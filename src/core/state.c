/**
 * @file state.c
 * @author Zuhaitz (original)
 * @brief Implements the core state management logic for the application.
 */
#include "core/state.h"
#include "core/file.h"
#include "plugins/plugin_manager.h"
#include "plugins/hex_viewer_api.h"
#include "utils/logger.h"
#include "core/config.h"
#include "utils/utils.h"
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

        config_load(state);

        // **Discover Themes (User, System, and Dev)**
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

        // Dev fallback using executable path
        char exe_dir[PATH_MAX];
        if (get_executable_dir(exe_dir, sizeof(exe_dir)) == 0) {
            char dev_themes_dir[PATH_MAX];
            snprintf(dev_themes_dir, sizeof(dev_themes_dir), "%s/../../themes", exe_dir);
            if (dir_exists(dev_themes_dir)) {
                theme_discover(dev_themes_dir, &state->theme_paths);
            }
        }

        // **Load Plugins (User, System, and Dev)**
        if (get_config_dir(user_config_dir, sizeof(user_config_dir)) == 0) {
            char user_plugins_dir[PATH_MAX];
            snprintf(user_plugins_dir, sizeof(user_plugins_dir), "%s/plugins", user_config_dir);
            if (dir_exists(user_plugins_dir)) {
                pm_load_plugins(user_plugins_dir);
            }
        }

        char system_plugins_dir[PATH_MAX];
        snprintf(system_plugins_dir, sizeof(system_plugins_dir), "%s/lib/fat/plugins", INSTALL_PREFIX);
        if (dir_exists(system_plugins_dir)) {
             pm_load_plugins(system_plugins_dir);
        } else if (get_executable_dir(exe_dir, sizeof(exe_dir)) == 0) { // Dev fallback
            char dev_plugins_dir[PATH_MAX];
            snprintf(dev_plugins_dir, sizeof(dev_plugins_dir), "%s/../../plugins", exe_dir);
            if (dir_exists(dev_plugins_dir)) {
                pm_load_plugins(dev_plugins_dir);
            }
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

    // Initialize search state
    state->search_term_active = false;
    state->search_results.matches = NULL;
    state->search_results.count = 0;
    state->search_results.capacity = 0;
    state->search_results.current_match_idx = 0;


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

    // **Hardcoded Fallback Theme**
    if (state->theme == NULL) {
        LOG_INFO("No themes found or loaded. Applying hardcoded monochrome fallback.");
        state->theme = calloc(1, sizeof(Theme));
        if (state->theme) {
            state->theme->name = strdup("Monochrome");
            state->theme->author = strdup("Zuhaitz");
            state->theme->colors[THEME_ELEMENT_BORDER].fg = COLOR_WHITE;
            state->theme->colors[THEME_ELEMENT_BORDER].bg = -1;
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

    if (state->force_view_mode == FORCE_VIEW_TEXT) {
        state->view_mode = VIEW_MODE_NORMAL;
        res = read_file_content(filepath, &state->content);
    } else if (state->force_view_mode == FORCE_VIEW_HEX) {
        state->view_mode = VIEW_MODE_BINARY_HEX;
        res = hex_viewer_generate_dump(filepath, &state->content);
    } else {
        const ArchivePlugin* handler = pm_get_handler(filepath);
        if (handler) {
            state->view_mode = VIEW_MODE_ARCHIVE;
            res = handler->list_contents(filepath, &state->content);
        } else {
            magic_t magic_cookie = magic_open(MAGIC_MIME_TYPE);
            bool is_binary = false;
            if (magic_cookie && magic_load(magic_cookie, NULL) == 0) {
                const char* magic_full = magic_file(magic_cookie, filepath);
                if (magic_full) {
                    bool is_forced_text = false;
                    for (size_t i = 0; i < state->config.text_mimes.count; i++) {
                        if (strcmp(magic_full, state->config.text_mimes.lines[i]) == 0) {
                            is_forced_text = true;
                            break;
                        }
                    }

                    bool is_forced_binary = false;
                    for (size_t i = 0; i < state->config.binary_mimes.count; i++) {
                        if (strcmp(magic_full, state->config.binary_mimes.lines[i]) == 0) {
                            is_forced_binary = true;
                            break;
                        }
                    }

                    if (is_forced_text) {
                        is_binary = false;
                    } else if (is_forced_binary) {
                        is_binary = true;
                    } else if ((strncmp(magic_full, "application/", 12) == 0 && strcmp(magic_full, "application/json") != 0) ||
                               strncmp(magic_full, "image/", 6) == 0 ||
                               strncmp(magic_full, "video/", 6) == 0) {
                        is_binary = true;
                    }
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
 * @brief Reloads the content for the current file in a new view mode.
 */
FatResult state_reload_content(AppState *state, ViewMode new_mode) {
    FatResult res = FAT_SUCCESS;

    // Free the old content and metadata related to content size
    StringList_free(&state->content);
    if (state->metadata.count > 0) {
        free(state->metadata.lines[state->metadata.count - 1]);
        state->metadata.count--;
    }

    state->view_mode = new_mode;

    // Reload content based on the new mode
    if (new_mode == VIEW_MODE_NORMAL) {
        res = read_file_content(state->filepath, &state->content);
    } else if (new_mode == VIEW_MODE_BINARY_HEX) {
        res = hex_viewer_generate_dump(state->filepath, &state->content);
    }

    if (res != FAT_SUCCESS) {
        return res;
    }
    
    // Update metadata with the new line count
    char count_buffer[128];
    snprintf(count_buffer, sizeof(count_buffer), "Lines: %zu", state->content.count);
    StringList_add(&state->metadata, count_buffer);

    // Recalculate max line length
    state->max_line_len = 0;
    for (size_t i = 0; i < state->content.count; i++) {
        size_t len = strlen(state->content.lines[i]);
        if (len > state->max_line_len) state->max_line_len = len;
    }
    
    // Reset view state
    state->top_line = 0;
    state->left_char = 0;
    state->search_term_active = false;

    return FAT_SUCCESS;
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
    free(state->search_results.matches);
    state->search_results.matches = NULL;
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

/**
 * @brief Performs a search for the current search term and populates the search_results list.
 */
FatResult state_perform_search(AppState *state) {
    // Clear previous results
    free(state->search_results.matches);
    state->search_results.matches = NULL;
    state->search_results.count = 0;
    state->search_results.capacity = 0;
    state->search_results.current_match_idx = 0;

    if (!state->search_term_active || state->search_term[0] == '\0') {
        return FAT_SUCCESS;
    }

    for (size_t i = 0; i < state->content.count; i++) {
        const char* line = state->content.lines[i];
        const char* ptr = line;
        while ((ptr = strstr(ptr, state->search_term)) != NULL) {
            if (state->search_results.count >= state->search_results.capacity) {
                size_t new_capacity = (state->search_results.capacity == 0) ? 8 : state->search_results.capacity * 2;
                SearchMatch* new_matches = realloc(state->search_results.matches, new_capacity * sizeof(SearchMatch));
                if (!new_matches) {
                    return FAT_ERROR_MEMORY;
                }
                state->search_results.matches = new_matches;
                state->search_results.capacity = new_capacity;
            }

            state->search_results.matches[state->search_results.count].line_idx = i;
            state->search_results.matches[state->search_results.count].char_idx = (size_t)(ptr - line);
            state->search_results.count++;
            ptr++; // Move past the beginning of the current match
        }
    }

    if (state->search_results.count > 0) {
        // Jump to the first match
        state->top_line = (int)state->search_results.matches[0].line_idx;
        return FAT_SUCCESS;
    }

    return FAT_ERROR_FILE_NOT_FOUND;
}
