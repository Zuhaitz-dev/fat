/**
 * @file config.c
 * @author Zuhaitz (original)
 * @brief Implements the loading and management of user configuration settings.
 */
#include "core/config.h"
#include "core/state.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include "utils/cJSON.h"
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


// **Forward Declarations**
static void config_load_keybindings(AppState* state);
static void config_free_keybindings(AppConfig* config);

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
 * @brief Copies default configuration files from the system path to the user's config path.
 */
static void copy_default_configs(const char* user_config_dir) {
    char system_defaults_dir[PATH_MAX];
    snprintf(system_defaults_dir, sizeof(system_defaults_dir), "%s/share/fat/defaults", INSTALL_PREFIX);

    if (!dir_exists(system_defaults_dir)) {
        LOG_INFO("System defaults directory not found at '%s', cannot copy defaults.", system_defaults_dir);
        return;
    }

    // Copy keybindings.json
    char src_path[PATH_MAX];
    char dest_path[PATH_MAX];
    snprintf(src_path, sizeof(src_path), "%s/keybindings.json", system_defaults_dir);
    snprintf(dest_path, sizeof(dest_path), "%s/keybindings.json", user_config_dir);
    copy_file(src_path, dest_path);
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
    // Set default values first
    state->config.default_theme_name = NULL;
    state->config.min_term_width = 80;
    state->config.min_term_height = 20;
    state->config.mime_commands = NULL;
    state->config.mime_commands_count = 0;
    state->config.default_command = NULL;
    StringList_init(&state->config.text_mimes);
    StringList_init(&state->config.binary_mimes);

    config_load_keybindings(state); // Load keybindings

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
            fprintf(create_file, "# --- Terminal Size Configuration ---\n");
            fprintf(create_file, "# Set the minimum required terminal dimensions.\n");
            fprintf(create_file, "# Default is 80x20 if not specified.\n");
            fprintf(create_file, "min_term_width = 80\n");
            fprintf(create_file, "min_term_height = 20\n\n");
            fprintf(create_file, "# --- MIME Type Configuration ---\n");
            fprintf(create_file, "# Force files with these MIME types to be treated as text or binary.\n");
            fprintf(create_file, "# Values are comma-separated.\n");
            fprintf(create_file, "# Example: text_mimes = application/json, application/xml\n");
            fprintf(create_file, "# Example: binary_mimes = application/octet-stream\n");
            fprintf(create_file, "text_mimes = application/json\n");
            fprintf(create_file, "binary_mimes =\n\n");
            fprintf(create_file, "# --- Default External Commands ---\n");
            fprintf(create_file, "# Set a default command for all file types.\n");
            fprintf(create_file, "# default_command = vim\n\n");
            fprintf(create_file, "# Set specific commands for MIME types.\n");
            fprintf(create_file, "# mime.image/png = feh\n");
            fprintf(create_file, "# mime.application/pdf = zathura\n");

            fclose(create_file);

            // Copy themes and configs on first run
            char user_themes_dir[PATH_MAX];
            snprintf(user_themes_dir, sizeof(user_themes_dir), "%s/themes", config_dir);
            copy_default_themes(user_themes_dir);

            char user_plugins_dir[PATH_MAX];
            snprintf(user_plugins_dir, sizeof(user_plugins_dir), "%s/plugins", config_dir);
            copy_default_plugins(user_plugins_dir);

            copy_default_configs(config_dir);
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
            } else if (strcmp(key, "min_term_width") == 0) {
                int width = atoi(value);
                if (width > 0) {
                    state->config.min_term_width = width;
                }
            } else if (strcmp(key, "min_term_height") == 0) {
                int height = atoi(value);
                if (height > 0) {
                    state->config.min_term_height = height;
                }
            } else if (strcmp(key, "default_command") == 0) {
                state->config.default_command = strdup(value);
            } else if (strncmp(key, "mime.", 5) == 0) {
                char* mime_type = key + 5;
                char* command = value;
                char* description = NULL;
                char* hash = strchr(value, '#');
                if(hash){
                    *hash = '\0';
                    description = hash + 1;
                    while(isspace((unsigned char)*description)) description++;
                }

                state->config.mime_commands_count++;
                state->config.mime_commands = realloc(state->config.mime_commands, state->config.mime_commands_count * sizeof(MimeCommand));
                state->config.mime_commands[state->config.mime_commands_count - 1].mime_type = strdup(mime_type);
                state->config.mime_commands[state->config.mime_commands_count - 1].command = strdup(command);
                state->config.mime_commands[state->config.mime_commands_count - 1].description = description ? strdup(description) : NULL;
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

    for(size_t i = 0; i < state->config.mime_commands_count; i++){
        free(state->config.mime_commands[i].mime_type);
        free(state->config.mime_commands[i].command);
        free(state->config.mime_commands[i].description);
    }
    free(state->config.mime_commands);
    free(state->config.default_command);

    config_free_keybindings(&state->config);
}


// ==============================================================================
// Keybinding Configuration
// ==============================================================================

/**
 * @brief Helper to map a key name string to an ncurses integer code.
 */
static int key_string_to_ncurses(const char* key_str) {
    if (!key_str) return -1;
    if (strcmp(key_str, "KEY_DOWN") == 0) return KEY_DOWN;
    if (strcmp(key_str, "KEY_UP") == 0) return KEY_UP;
    if (strcmp(key_str, "KEY_LEFT") == 0) return KEY_LEFT;
    if (strcmp(key_str, "KEY_RIGHT") == 0) return KEY_RIGHT;
    if (strcmp(key_str, "KEY_NPAGE") == 0) return KEY_NPAGE;
    if (strcmp(key_str, "KEY_PPAGE") == 0) return KEY_PPAGE;
    if (strcmp(key_str, "KEY_HOME") == 0) return KEY_HOME;
    if (strcmp(key_str, "KEY_END") == 0) return KEY_END;
    if (strcmp(key_str, "KEY_ENTER") == 0) return KEY_ENTER;
    if (strcmp(key_str, "KEY_BACKSPACE") == 0) return KEY_BACKSPACE;
    if (strcmp(key_str, "KEY_ESC") == 0) return 27; // ncurses doesn't have a KEY_ESC
    if (strncmp(key_str, "KEY_F(", 6) == 0) {
        int f_num = atoi(key_str + 6);
        if (f_num > 0 && f_num <= 64) return KEY_F(f_num);
    }
    if (strlen(key_str) == 1) return (int)key_str[0];
    if (strcmp(key_str, "\n") == 0) return '\n';

    return -1; // Not a recognized key
}

/**
 * @brief Helper to map an action name string to an Action enum.
 */
static Action action_name_to_enum(const char* name) {
    if (strcmp(name, "quit") == 0) return ACTION_QUIT;
    if (strcmp(name, "scroll_up") == 0) return ACTION_SCROLL_UP;
    if (strcmp(name, "scroll_down") == 0) return ACTION_SCROLL_DOWN;
    if (strcmp(name, "scroll_left") == 0) return ACTION_SCROLL_LEFT;
    if (strcmp(name, "scroll_right") == 0) return ACTION_SCROLL_RIGHT;
    if (strcmp(name, "page_up") == 0) return ACTION_PAGE_UP;
    if (strcmp(name, "page_down") == 0) return ACTION_PAGE_DOWN;
    if (strcmp(name, "jump_to_start") == 0) return ACTION_JUMP_TO_START;
    if (strcmp(name, "jump_to_end") == 0) return ACTION_JUMP_TO_END;
    if (strcmp(name, "jump_to_line") == 0) return ACTION_JUMP_TO_LINE;
    if (strcmp(name, "toggle_wrap") == 0) return ACTION_TOGGLE_WRAP;
    if (strcmp(name, "search") == 0) return ACTION_SEARCH;
    if (strcmp(name, "next_match") == 0) return ACTION_NEXT_MATCH;
    if (strcmp(name, "prev_match") == 0) return ACTION_PREV_MATCH;
    if (strcmp(name, "toggle_view_mode") == 0) return ACTION_TOGGLE_VIEW_MODE;
    if (strcmp(name, "open_external") == 0) return ACTION_OPEN_EXTERNAL;
    if (strcmp(name, "open_external_default") == 0) return ACTION_OPEN_EXTERNAL_DEFAULT;
    if (strcmp(name, "go_back") == 0) return ACTION_GO_BACK;
    if (strcmp(name, "select_theme") == 0) return ACTION_SELECT_THEME;
    if (strcmp(name, "toggle_help") == 0) return ACTION_TOGGLE_HELP;
    if (strcmp(name, "confirm") == 0) return ACTION_CONFIRM;
    return ACTION_NONE;
}

/**
 * @brief Frees all memory associated with the keybindings.
 */
static void config_free_keybindings(AppConfig* config) {
    for (int i = 0; i < ACTION_COUNT; i++) {
        free(config->keybindings[i].name);
        free(config->keybindings[i].description);
        StringList_free(&config->keybindings[i].keys);
        StringList_free(&config->keybindings[i].modes);
    }
}

/**
 * @brief Populates the key_map for fast lookup.
 */
static void populate_key_map(AppConfig* config) {
    // Initialize map
    for (int i = 0; i < MAX_KEY_CODE; i++) {
        config->key_map[i] = ACTION_NONE;
    }

    // Populate with loaded keybindings
    for (int i = 0; i < ACTION_COUNT; i++) {
        Keybinding* kb = &config->keybindings[i];
        for (size_t j = 0; j < kb->keys.count; j++) {
            int key_code = key_string_to_ncurses(kb->keys.lines[j]);
            if (key_code >= 0 && key_code < MAX_KEY_CODE) {
                config->key_map[key_code] = kb->action;
            }
        }
    }
}

/**
 * @brief Parses a keybindings.json file and updates the AppConfig.
 */
static void parse_keybindings_json(const char* filepath, AppConfig* config) {
    FILE* f = fopen(filepath, "rb");
    if (!f) {
        LOG_INFO("Failed to open keybindings file at path: %s", filepath);
        return;
    }

    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buffer = malloc(length + 1);
    if (!buffer) {
        fclose(f);
        return;
    }
    if (fread(buffer, 1, length, f) != (size_t)length) {
        // Handle read error if necessary
    }
    fclose(f);
    buffer[length] = '\0';

    cJSON* json = cJSON_Parse(buffer);
    free(buffer);
    if (!json) {
        LOG_INFO("Failed to parse keybindings file: %s", filepath);
        return;
    }

    cJSON* actions = cJSON_GetObjectItemCaseSensitive(json, "actions");
    cJSON* action_obj;
    cJSON_ArrayForEach(action_obj, actions) {
        cJSON* name_json = cJSON_GetObjectItemCaseSensitive(action_obj, "name");
        if (!cJSON_IsString(name_json)) continue;

        Action action = action_name_to_enum(name_json->valuestring);
        if (action == ACTION_NONE) continue;

        Keybinding* kb = &config->keybindings[action];
        
        // Free old data before overriding
        free(kb->name);
        free(kb->description);
        StringList_free(&kb->keys);
        StringList_free(&kb->modes);

        kb->action = action;
        kb->name = strdup(name_json->valuestring);
        
        cJSON* desc_json = cJSON_GetObjectItemCaseSensitive(action_obj, "description");
        if (cJSON_IsString(desc_json)) {
            kb->description = strdup(desc_json->valuestring);
        }

        cJSON* keys_array = cJSON_GetObjectItemCaseSensitive(action_obj, "keys");
        cJSON* key_str_json;
        cJSON_ArrayForEach(key_str_json, keys_array) {
            if (cJSON_IsString(key_str_json)) {
                StringList_add(&kb->keys, key_str_json->valuestring);
            }
        }

        cJSON* modes_array = cJSON_GetObjectItemCaseSensitive(action_obj, "modes");
        cJSON* mode_str_json;
        cJSON_ArrayForEach(mode_str_json, modes_array) {
            if (cJSON_IsString(mode_str_json)) {
                StringList_add(&kb->modes, mode_str_json->valuestring);
            }
        }
    }

    cJSON_Delete(json);
    LOG_INFO("Loaded keybindings from %s", filepath);
}

/**
 * @brief Initializes and loads keybindings from default and user files.
 */
static void config_load_keybindings(AppState* state) {
    // 1 - Initialize the keybinding array
    for (int i = 0; i < ACTION_COUNT; i++) {
        state->config.keybindings[i].action = (Action)i;
        state->config.keybindings[i].name = NULL;
        state->config.keybindings[i].description = NULL;
        StringList_init(&state->config.keybindings[i].keys);
        StringList_init(&state->config.keybindings[i].modes);
    }
    
    // 2 - Parse default, system, and user keybinding files. Each subsequent
    //    file will override settings from the previous one.
    char keybinding_path[PATH_MAX];
    
    // Dev path (lowest priority), relative to the executable
    char exe_dir[PATH_MAX];
    if (get_executable_dir(exe_dir, sizeof(exe_dir)) == 0) {
        char dev_defaults_dir[PATH_MAX];

        // Check for the defaults dir in two common locations
        snprintf(dev_defaults_dir, sizeof(dev_defaults_dir), "%s/../../defaults", exe_dir);
        if (!dir_exists(dev_defaults_dir)) {
            // If not found, try another common location (e.g., if running from root)
            snprintf(dev_defaults_dir, sizeof(dev_defaults_dir), "%s/../defaults", exe_dir);
        }

        if (dir_exists(dev_defaults_dir)) {
            snprintf(keybinding_path, sizeof(keybinding_path), "%s/keybindings.json", dev_defaults_dir);
            LOG_INFO("Attempting to load dev keybindings from: %s", keybinding_path);
            parse_keybindings_json(keybinding_path, &state->config);
        }
    }

    // System path
    snprintf(keybinding_path, sizeof(keybinding_path), "%s/share/fat/defaults/keybindings.json", INSTALL_PREFIX);
    parse_keybindings_json(keybinding_path, &state->config);

    // User path (highest priority)
    char config_dir[PATH_MAX];
    if (get_config_dir(config_dir, sizeof(config_dir)) == 0) {
        snprintf(keybinding_path, sizeof(keybinding_path), "%s/keybindings.json", config_dir);
        parse_keybindings_json(keybinding_path, &state->config);
    }
    
    // 3 - Populate the fast lookup map
    populate_key_map(&state->config);
}
