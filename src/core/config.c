/**
 * @file config.c
 * @author Zuhaitz (original)
 * @brief Implements the loading and management of user configuration settings.
 */
#include "core/config.h"
#include "core/state.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <dirent.h>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <wordexp.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif


/**
 * @brief Gets the full, OS-specific path to the configuration directory.
 */
int get_config_dir(char* buffer, size_t size) {
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        snprintf(buffer, size, "%s\\fat", path);
        return 0;
    }
    return -1;
#else
    wordexp_t p;
    if (wordexp("~/.config/fat", &p, 0) == 0) {
        strncpy(buffer, p.we_wordv[0], size - 1);
        buffer[size - 1] = '\0';
        wordfree(&p);
        return 0;
    }
    return -1;
#endif
}

/**
 * @brief Helper to copy a single file
 */
static void copy_file(const char* src, const char* dest) {
    FILE* src_file = fopen(src, "rb");
    if (!src_file) return;
    FILE* dest_file = fopen(dest, "wb");
    if (!dest_file) {
        fclose(src_file);
        return;
    }

    char buffer[4096];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
        fwrite(buffer, 1, n, dest_file);
    }

    fclose(src_file);
    fclose(dest_file);
}

/**
 * @brief Copies default themes from the system path to the user's config path.
 */
static void copy_default_themes(const char* user_themes_dir) {
    char system_themes_dir[PATH_MAX];
    snprintf(system_themes_dir, sizeof(system_themes_dir), "%s/share/fat/themes", INSTALL_PREFIX);

    if (!dir_exists(system_themes_dir)) {
        LOG_INFO("System themes directory not found at '%s', cannot copy defaults.", system_themes_dir);
        return;
    }

    DIR* d = opendir(system_themes_dir);
    if (!d) return;

    LOG_INFO("Copying default themes to %s", user_themes_dir);
    struct dirent* dir;
    while ((dir = readdir(d)) != NULL) {
        if (strstr(dir->d_name, ".json")) {
            char src_path[PATH_MAX];
            char dest_path[PATH_MAX];
            snprintf(src_path, sizeof(src_path), "%s/%s", system_themes_dir, dir->d_name);
            snprintf(dest_path, sizeof(dest_path), "%s/%s", user_themes_dir, dir->d_name);
            copy_file(src_path, dest_path);
        }
    }
    closedir(d);
}

/**
 * @brief Copies default plugins from the system path or dev path to the user's config path.
 */
static void copy_default_plugins(const char* user_plugins_dir) {
    char system_plugins_dir[PATH_MAX];
    const char* source_plugins_dir = NULL;

    // First, check the official installation directory
    snprintf(system_plugins_dir, sizeof(system_plugins_dir), "%s/lib/fat/plugins", INSTALL_PREFIX);
    if (dir_exists(system_plugins_dir)) {
        source_plugins_dir = system_plugins_dir;
    }
    // If not found, fall back to the local development directory
    else if (dir_exists("plugins")) {
        source_plugins_dir = "plugins";
    }

    if (!source_plugins_dir) {
        LOG_INFO("No source plugin directory found. Looked in '%s' and './plugins'. Cannot copy defaults.", system_plugins_dir);
        return;
    }

    DIR* d = opendir(source_plugins_dir);
    if (!d) return;

    LOG_INFO("Copying default plugins from '%s' to '%s'", source_plugins_dir, user_plugins_dir);
    struct dirent* dir;
    while ((dir = readdir(d)) != NULL) {
        const char* dot = strrchr(dir->d_name, '.');
        #ifdef _WIN32
            if (dot && strcmp(dot, ".dll") == 0) {
        #else
            if (dot && (strcmp(dot, ".so") == 0 || strcmp(dot, ".dylib") == 0)) {
        #endif
            char src_path[PATH_MAX];
            char dest_path[PATH_MAX];
            snprintf(src_path, sizeof(src_path), "%s/%s", source_plugins_dir, dir->d_name);
            snprintf(dest_path, sizeof(dest_path), "%s/%s", user_plugins_dir, dir->d_name);
            copy_file(src_path, dest_path);
        }
    }
    closedir(d);
}


/**
 * @brief Ensures the directory for the configuration file and subdirectories exist.
 */
static void ensure_config_subdirs_exist(const char* dir_path) {
#ifdef _WIN32
    CreateDirectoryA(dir_path, NULL);
    char sub_path[MAX_PATH];
    snprintf(sub_path, sizeof(sub_path), "%s\\themes", dir_path);
    CreateDirectoryA(sub_path, NULL);
    snprintf(sub_path, sizeof(sub_path), "%s\\plugins", dir_path);
    CreateDirectoryA(sub_path, NULL);
#else
    char* path_copy = strdup(dir_path);
    if (!path_copy) return;
    for (char* p = path_copy + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(path_copy, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
            *p = '/';
        }
    }
    mkdir(path_copy, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    free(path_copy);

    char sub_path[PATH_MAX];
    snprintf(sub_path, sizeof(sub_path), "%s/themes", dir_path);
    mkdir(sub_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    snprintf(sub_path, sizeof(sub_path), "%s/plugins", dir_path);
    mkdir(sub_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#endif
}

/**
 * @brief Helper to parse comma-separated values from the config file.
 */
static void parse_csv_to_stringlist(const char* value, StringList* list) {
    char* value_copy = strdup(value);
    if (!value_copy) return;

    char* token = strtok(value_copy, ",");
    while (token) {
        // Trim whitespace from token
        char* start = token;
        while (isspace((unsigned char)*start)) start++;
        char* end = start + strlen(start) - 1;
        while (end > start && isspace((unsigned char)*end)) *end-- = '\0';
        
        StringList_add(list, start);
        token = strtok(NULL, ",");
    }
    free(value_copy);
}

/**
 * @brief Loads user settings, creating defaults and copying themes on first run.
 */
void config_load(AppState* state) {
    state->config.default_theme_name = NULL;
    StringList_init(&state->config.text_mimes);
    StringList_init(&state->config.binary_mimes);

    char config_dir[PATH_MAX];
    if (get_config_dir(config_dir, sizeof(config_dir)) != 0) {
        LOG_INFO("Could not determine configuration directory path.");
        return;
    }

    ensure_config_subdirs_exist(config_dir);

    char config_file_path[PATH_MAX];
#ifdef _WIN32
    snprintf(config_file_path, sizeof(config_file_path), "%s\\fatrc", config_dir);
#else
    snprintf(config_file_path, sizeof(config_file_path), "%s/fatrc", config_dir);
#endif


    FILE* file = fopen(config_file_path, "r");
    if (!file) {
        LOG_INFO("Config file not found. Creating default and copying resources.", config_file_path);
        FILE* create_file = fopen(config_file_path, "w");
        if(create_file) {
            fprintf(create_file, "# FAT (File & Archive Tool) Configuration File\n\n");
            fprintf(create_file, "# Set the default theme by its name (without the .json extension).\n");
            fprintf(create_file, "# Example: default_theme = nord\n\n");
            fprintf(create_file, "# --- MIME Type Configuration ---\n");
            fprintf(create_file, "# Force files with these MIME types to be treated as text or binary.\n");
            fprintf(create_file, "# Values are comma-separated.\n");
            fprintf(create_file, "# Example: text_mimes = application/json, application/xml\n");
            fprintf(create_file, "# Example: binary_mimes = application/octet-stream\n");
            fprintf(create_file, "text_mimes = application/json\n");
            fprintf(create_file, "binary_mimes =\n");

            fclose(create_file);

            // Copy themes on first run
            char user_themes_dir[PATH_MAX];
            snprintf(user_themes_dir, sizeof(user_themes_dir), "%s/themes", config_dir);
            copy_default_themes(user_themes_dir);

            char user_plugins_dir[PATH_MAX];
            snprintf(user_plugins_dir, sizeof(user_plugins_dir), "%s/plugins", config_dir);
            copy_default_plugins(user_plugins_dir);
        }
        return;
    }

    char* line = NULL;
    size_t len = 0;
    while (getline(&line, &len, file) != -1) {
        char* start = line;
        while (isspace((unsigned char)*start)) start++;
        if (*start == '#' || *start == '\0') continue;

        char* equals = strchr(start, '=');
        if (equals) {
            *equals = '\0';
            char* key = start;
            char* value = equals + 1;

            char* key_end = key + strlen(key) - 1;
            while (key_end > key && isspace((unsigned char)*key_end)) *key_end-- = '\0';

            while (isspace((unsigned char)*value)) value++;
            char* value_end = value + strlen(value) - 1;
            while (value_end > value && isspace((unsigned char)*value_end)) *value_end-- = '\0';

            if (strcmp(key, "default_theme") == 0) {
                state->config.default_theme_name = strdup(value);
            } else if (strcmp(key, "text_mimes") == 0) {
                parse_csv_to_stringlist(value, &state->config.text_mimes);
            } else if (strcmp(key, "binary_mimes") == 0) {
                parse_csv_to_stringlist(value, &state->config.binary_mimes);
            }
        }
    }

    free(line);
    fclose(file);
    LOG_INFO("Loaded config from %s", config_file_path);
}

/**
 * @brief Frees memory allocated for configuration settings.
 */
void config_free(AppState* state) {
    if (!state) return;
    free(state->config.default_theme_name);
    state->config.default_theme_name = NULL;
    StringList_free(&state->config.text_mimes);
    StringList_free(&state->config.binary_mimes);
}
