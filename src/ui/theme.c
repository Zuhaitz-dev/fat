/**
 * @file theme.c
 * @author Zuhaitz (original)
 * @brief Implements the logic for loading, applying, and discovering UI themes.
 */
#include "ui/theme.h"
#include "utils/logger.h"
#include "utils/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>
#include <errno.h>
#include <libgen.h> // For basename

/**
 * @brief Helper function to convert a color name string to an ncurses color constant.
 */
static short color_string_to_ncurses(const char* color_str) {
    if (strcmp(color_str, "black") == 0) return COLOR_BLACK;
    if (strcmp(color_str, "red") == 0) return COLOR_RED;
    if (strcmp(color_str, "green") == 0) return COLOR_GREEN;
    if (strcmp(color_str, "yellow") == 0) return COLOR_YELLOW;
    if (strcmp(color_str, "blue") == 0) return COLOR_BLUE;
    if (strcmp(color_str, "magenta") == 0) return COLOR_MAGENTA;
    if (strcmp(color_str, "cyan") == 0) return COLOR_CYAN;
    if (strcmp(color_str, "white") == 0) return COLOR_WHITE;
    if (strcmp(color_str, "default") == 0) return -1;
    return -1;
}

/**
 * @brief Helper function to parse a single color object from cJSON.
 */
static void parse_theme_color(const cJSON* color_obj, ThemeColor* tc) {
    tc->fg = -1;
    tc->bg = -1;
    if (!color_obj) return;

    const cJSON* fg_json = cJSON_GetObjectItemCaseSensitive(color_obj, "fg");
    if (cJSON_IsString(fg_json) && (fg_json->valuestring != NULL)) {
        tc->fg = color_string_to_ncurses(fg_json->valuestring);
    }
    const cJSON* bg_json = cJSON_GetObjectItemCaseSensitive(color_obj, "bg");
    if (cJSON_IsString(bg_json) && (bg_json->valuestring != NULL)) {
        tc->bg = color_string_to_ncurses(bg_json->valuestring);
    }
}

/**
 * @brief Loads a theme from a JSON file and creates a Theme struct.
 */
FatResult theme_load(const char* filepath, Theme** out_theme) {
    FILE* f = NULL;
    char* buffer = NULL;
    cJSON* json = NULL;
    Theme* theme = NULL;
    FatResult result = FAT_SUCCESS;

    *out_theme = NULL;

    f = fopen(filepath, "rb");
    if (!f) {
        LOG_INFO("Could not open theme file '%s': %s", filepath, strerror(errno));
        return FAT_ERROR_THEME_LOAD;
    }

    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);

    buffer = malloc(length + 1);
    if (!buffer) {
        result = FAT_ERROR_MEMORY;
        goto cleanup;
    }
    
    if (fread(buffer, 1, length, f) != (size_t)length) {
        LOG_INFO("Incomplete read of theme file '%s'", filepath);
    }
    buffer[length] = '\0';

    json = cJSON_Parse(buffer);
    if (!json) {
        LOG_INFO("Error parsing theme file '%s': %s", filepath, cJSON_GetErrorPtr());
        result = FAT_ERROR_THEME_LOAD;
        goto cleanup;
    }

    theme = calloc(1, sizeof(Theme));
    if (!theme) {
        result = FAT_ERROR_MEMORY;
        goto cleanup;
    }

    const char* element_names[THEME_ELEMENT_COUNT] = {
        "border", "title", "metadata_label", "line_num", "statusbar",
        "search_highlight", "help_border", "help_key"
    };

    cJSON* name_json = cJSON_GetObjectItemCaseSensitive(json, "name");
    if (cJSON_IsString(name_json)) theme->name = strdup(name_json->valuestring);

    cJSON* author_json = cJSON_GetObjectItemCaseSensitive(json, "author");
    if (cJSON_IsString(author_json)) theme->author = strdup(author_json->valuestring);

    cJSON* colors_obj = cJSON_GetObjectItemCaseSensitive(json, "colors");
    if (cJSON_IsObject(colors_obj)) {
        for (int i = 0; i < THEME_ELEMENT_COUNT; ++i) {
            cJSON* color_obj = cJSON_GetObjectItemCaseSensitive(colors_obj, element_names[i]);
            parse_theme_color(color_obj, &theme->colors[i]);
        }
    }

    *out_theme = theme;
    theme = NULL;

cleanup:
    free(buffer);
    if (f) fclose(f);
    if (json) cJSON_Delete(json);
    if (theme) theme_free(theme);
    return result;
}

/**
 * @brief Applies the colors of a loaded theme to the ncurses environment.
 */
void theme_apply(const Theme* theme) {
    if (!theme) return;
    start_color();

    for (int i = 0; i < THEME_ELEMENT_COUNT; ++i) {
        short fg = theme->colors[i].fg;
        short bg = theme->colors[i].bg;

        if (fg == -1) fg = COLOR_WHITE;
        if (bg == -1) bg = COLOR_BLACK;

        init_pair(i + 1, fg, bg);
    }
}

/**
 * @brief Frees the memory allocated for a Theme struct.
 */
void theme_free(Theme* theme) {
    if (!theme) return;
    free(theme->name);
    free(theme->author);
    free(theme);
}

/**
 * @brief Helper to check if a StringList already contains a theme with the same filename.
 */
static bool list_contains_theme(const StringList* list, const char* new_theme_filename) {
    for (size_t i = 0; i < list->count; ++i) {
        char* existing_path_copy = strdup(list->lines[i]);
        if (existing_path_copy) {
            const char* existing_filename = basename(existing_path_copy);
            if (strcmp(existing_filename, new_theme_filename) == 0) {
                free(existing_path_copy);
                return true;
            }
            free(existing_path_copy);
        }
    }
    return false;
}

/**
 * @brief Discovers all .json theme files in a given directory, avoiding duplicates.
 */
FatResult theme_discover(const char* themes_dir_path, StringList* list) {
    DIR* d = opendir(themes_dir_path);
    if (!d) {
        LOG_INFO("Could not open themes directory '%s': %s", themes_dir_path, strerror(errno));
        return FAT_ERROR_FILE_NOT_FOUND;
    }

    FatResult result = FAT_SUCCESS;
    struct dirent* dir;
    while ((dir = readdir(d)) != NULL) {
        const char* dot = strrchr(dir->d_name, '.');
        if (dot && strcmp(dot, ".json") == 0) {
            if (!list_contains_theme(list, dir->d_name)) {
                char full_path[PATH_MAX];
                snprintf(full_path, sizeof(full_path), "%s/%s", themes_dir_path, dir->d_name);
                if (StringList_add(list, full_path) != FAT_SUCCESS) {
                    result = FAT_ERROR_MEMORY;
                    break;
                }
            }
        }
    }
    closedir(d);
    return result;
}
