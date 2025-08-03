/**
 * @file state.c
 * @author Zuhaitz (original)
 * @brief Implements the core state management logic for the application.
 */
#include "state.h"
#include "file.h"
#include "plugin_manager.h"
#include "hex_viewer_api.h"
#include "logger.h"
#include "config.h"
#include "utils.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <magic.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif


/**
 * @brief Initializes or re-initializes the application state for a given file.
 */
FatResult state_init(AppState *state, const char *filepath) {
    FatResult res = FAT_SUCCESS;
    Theme* current_theme = state->theme;
    state->theme = NULL;

    state_destroy_view(state);

    if (state->breadcrumbs.count == 0) {
        StringList_init(&state->breadcrumbs);
        StringList_init(&state->theme_paths);

        // This now handles the entire first-run process correctly.
        config_load(state);

        // --- Discover Themes (User, System, and Dev) ---
        char user_config_dir[PATH_MAX];
        if (get_config_dir(user_config_dir, sizeof(user_config_dir)) == 0) {
            char user_themes_dir[PATH_MAX];
            snprintf(user_themes_dir, sizeof(user_themes_dir), "%s/themes", user_config_dir);
            if (dir_exists(user_themes_dir)) {
                theme_discover(user_themes_dir, &state->theme_paths);
            }
        }
        
        char system_themes_dir[PATH_MAX];
        snprintf(system_themes_dir, sizeof(system_themes_dir), "%s/share/fat/themes", INSTALL_PREFIX);
        if (dir_exists(system_themes_dir)) {
            theme_discover(system_themes_dir, &state->theme_paths);
        }

        if (dir_exists("themes")) { // Dev fallback
             theme_discover("themes", &state->theme_paths);
        }

        // --- Load Plugins (System or Dev) ---
        char system_plugins_dir[PATH_MAX];
        snprintf(system_plugins_dir, sizeof(system_plugins_dir), "%s/lib/fat/plugins", INSTALL_PREFIX);
        if (dir_exists(system_plugins_dir)) {
             pm_load_plugins(system_plugins_dir);
        } else if (dir_exists("plugins")) { // Dev fallback
             pm_load_plugins("plugins");
        }
    }

    state->filepath = strdup(filepath);
    if (!state->filepath) return FAT_ERROR_MEMORY;

    if (state->breadcrumbs.count == 0 || strcmp(state->breadcrumbs.lines[state->breadcrumbs.count - 1], filepath) != 0) {
        if (StringList_add(&state->breadcrumbs, filepath) != FAT_SUCCESS) {
            res = FAT_ERROR_MEMORY;
            goto cleanup;
        }
    }

    state->mode = MODE_NORMAL;
    state->line_wrap_enabled = false;
    state->top_line = 0;
    state->left_char = 0;
    state->search_term_active = false;
    state->current_search_line_idx = -1;
    state->current_search_char_idx = -1;
    state->search_direction = 1;
    state->search_wrapped = false;

    state->theme = current_theme;
    if (state->theme == NULL && state->theme_paths.count > 0) {
        bool theme_loaded = false;
        if (state->config.default_theme_name) {
            for (size_t i = 0; i < state->theme_paths.count; i++) {
                const char* theme_path = state->theme_paths.lines[i];
                const char* basename = strrchr(theme_path, '/');
                if (!basename) basename = strrchr(theme_path, '\\');
                basename = basename ? basename + 1 : theme_path;

                if (strncmp(basename, state->config.default_theme_name, strlen(state->config.default_theme_name)) == 0) {
                    if (theme_load(theme_path, &state->theme) == FAT_SUCCESS) {
                        theme_loaded = true;
                    }
                    break;
                }
            }
        }
        if (!theme_loaded) {
             theme_load(state->theme_paths.lines[0], &state->theme);
        }
    }
    
    // **NEW: Hardcoded Fallback Theme**
    if (state->theme == NULL) {
        LOG_INFO("No themes found or loaded. Applying hardcoded monochrome fallback.");
        state->theme = calloc(1, sizeof(Theme));
        if (state->theme) {
            state->theme->name = strdup("Monochrome");
            state->theme->author = strdup("Zuhaitz");

            // Define the monochrome theme colors directly
            state->theme->colors[THEME_ELEMENT_BORDER].fg = COLOR_WHITE;
            state->theme->colors[THEME_ELEMENT_BORDER].bg = -1; // Default background

            state->theme->colors[THEME_ELEMENT_TITLE].fg = COLOR_WHITE;
            state->theme->colors[THEME_ELEMENT_TITLE].bg = -1;

            state->theme->colors[THEME_ELEMENT_METADATA_LABEL].fg = COLOR_WHITE;
            state->theme->colors[THEME_ELEMENT_METADATA_LABEL].bg = -1;

            state->theme->colors[THEME_ELEMENT_LINE_NUM].fg = COLOR_WHITE;
            state->theme->colors[THEME_ELEMENT_LINE_NUM].bg = -1;

            state->theme->colors[THEME_ELEMENT_STATUSBAR].fg = COLOR_BLACK;
            state->theme->colors[THEME_ELEMENT_STATUSBAR].bg = COLOR_WHITE;

            state->theme->colors[THEME_ELEMENT_SEARCH_HIGHLIGHT].fg = COLOR_BLACK;
            state->theme->colors[THEME_ELEMENT_SEARCH_HIGHLIGHT].bg = COLOR_WHITE;
            
            state->theme->colors[THEME_ELEMENT_HELP_BORDER].fg = COLOR_BLACK;
            state->theme->colors[THEME_ELEMENT_HELP_BORDER].bg = COLOR_WHITE;

            state->theme->colors[THEME_ELEMENT_HELP_KEY].fg = COLOR_BLACK;
            state->theme->colors[THEME_ELEMENT_HELP_KEY].bg = COLOR_WHITE;
        }
    }

    if (state->theme) {
        theme_apply(state->theme);
    }

    res = get_file_info(filepath, &state->metadata);
    if (res != FAT_SUCCESS) goto cleanup;

    const ArchivePlugin* handler = pm_get_handler(filepath);
    if (handler) {
        state->view_mode = VIEW_MODE_ARCHIVE;
        res = handler->list_contents(filepath, &state->content);
    } else {
        magic_t magic_cookie = magic_open(MAGIC_MIME_TYPE);
        bool is_binary = false;
        if (magic_cookie && magic_load(magic_cookie, NULL) == 0) {
            const char* magic_full = magic_file(magic_cookie, filepath);
            if (magic_full && 
               (strncmp(magic_full, "application/", 12) == 0 && strcmp(magic_full, "application/json") != 0) || // MODIFIED LINE
                strncmp(magic_full, "image/", 6) == 0 || 
                strncmp(magic_full, "video/", 6) == 0) {
                is_binary = true;
            }
        }
        if (magic_cookie) magic_close(magic_cookie);

        if (is_binary) {
            state->view_mode = VIEW_MODE_BINARY_HEX;
            res = hex_viewer_generate_dump(filepath, &state->content);
        } else {
            state->view_mode = VIEW_MODE_NORMAL;
            res = read_file_content(filepath, &state->content);
        }
    }
    if (res != FAT_SUCCESS) goto cleanup;

    char count_buffer[128];
    snprintf(count_buffer, sizeof(count_buffer), "%s: %zu",
        state->view_mode == VIEW_MODE_ARCHIVE ? "Entries" : "Lines",
        state->content.count);
    if (StringList_add(&state->metadata, count_buffer) != FAT_SUCCESS) {
        res = FAT_ERROR_MEMORY;
        goto cleanup;
    }

    state->max_line_len = 0;
    for (size_t i = 0; i < state->content.count; i++) {
        size_t len = strlen(state->content.lines[i]);
        if (len > state->max_line_len) state->max_line_len = len;
    }

    return FAT_SUCCESS;

cleanup:
    state_destroy_view(state);
    return res;
}

/**
 * @brief Frees resources associated with the current view only.
 */
void state_destroy_view(AppState *state) {
    if (!state) return;
    StringList_free(&state->metadata);
    StringList_free(&state->content);
    state->search_term_active = false;
    free(state->filepath);
    state->filepath = NULL;
}

/**
 * @brief Frees all resources for the entire application lifetime before exit.
 */
void full_app_reset(AppState* state) {
    if (!state) return;

    state_destroy_view(state);

    if (state->left_pane) { delwin(state->left_pane); state->left_pane = NULL; }
    if (state->right_pane) { delwin(state->right_pane); state->right_pane = NULL; }
    if (state->status_bar) { delwin(state->status_bar); state->status_bar = NULL; }

    StringList_free(&state->theme_paths);
    theme_free(state->theme);
    config_free(state);

    char temp_dir_path[PATH_MAX];
    #ifdef _WIN32
        GetTempPathA(PATH_MAX, temp_dir_path);
    #else
        strncpy(temp_dir_path, "/tmp", sizeof(temp_dir_path) - 1);
        temp_dir_path[sizeof(temp_dir_path) - 1] = '\0';
    #endif

    char temp_file_prefix[PATH_MAX];
    #ifdef _WIN32
        snprintf(temp_file_prefix, sizeof(temp_file_prefix), "%s\\fat-", temp_dir_path);
    #else
        snprintf(temp_file_prefix, sizeof(temp_file_prefix), "%s/fat-", temp_dir_path);
    #endif

    for(size_t i = 0; i < state->breadcrumbs.count; ++i) {
        if (strncmp(state->breadcrumbs.lines[i], temp_file_prefix, strlen(temp_file_prefix)) == 0) {
            remove(state->breadcrumbs.lines[i]);
        }
    }

    StringList_free(&state->breadcrumbs);
}
