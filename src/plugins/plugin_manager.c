/**
 * @file plugin_manager.c
 * @author Zuhaitz (original)
 * @brief Implements the dynamic plugin loading system.
 *
 * This file uses the `dlfcn.h` library (on POSIX systems) or the Windows API
 * to dynamically load shared objects at runtime, look up symbols, and build a
 * list of available archive handlers.
 */
#include "plugins/plugin_manager.h"
#include "utils/logger.h"
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <limits.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

/** @brief The maximum number of plugins that can be loaded. */
#define MAX_PLUGINS 16
/** @brief An array to store pointers to the loaded plugin interfaces. */
static ArchivePlugin* loaded_plugins[MAX_PLUGINS];
/** @brief The current number of loaded plugins. */
static int num_plugins = 0;

/**
 * @brief Checks if a plugin with the same name is already loaded.
 */
static bool is_plugin_already_loaded(const char* plugin_name) {
    for (int i = 0; i < num_plugins; i++) {
        if (strcmp(loaded_plugins[i]->plugin_name, plugin_name) == 0) {
            return true;
        }
    }
    return false;
}


/**
 * @brief Loads all plugins from the specified directory.
 *
 * @param plugin_dir_path The path to the directory containing plugin files.
 */
void pm_load_plugins(const char* plugin_dir_path) {
    DIR* d = opendir(plugin_dir_path);
    if (!d) {
        LOG_INFO("Could not open plugin directory '%s'. No plugins will be loaded.", plugin_dir_path);
        return;
    }

    struct dirent* dir;
    // Iterate through the directory entries.
    while ((dir = readdir(d)) != NULL && num_plugins < MAX_PLUGINS) {
        const char* dot = strrchr(dir->d_name, '.');
#ifdef _WIN32
        if (dot && strcmp(dot, ".dll") == 0) {
#else
        if (dot && (strcmp(dot, ".so") == 0 || strcmp(dot, ".dylib") == 0)) {
#endif
            char full_path[PATH_MAX];
            snprintf(full_path, sizeof(full_path), "%s/%s", plugin_dir_path, dir->d_name);

#ifdef _WIN32
            // Windows-specific library loading
            HMODULE handle = LoadLibraryA(full_path);
            if (!handle) {
                LOG_INFO("Error loading plugin %s: Error code %lu", full_path, GetLastError());
                continue;
            }

            // Look for the mandatory "plugin_register" function symbol.
            ArchivePlugin* (*reg_func)(void);
            *(void**)(&reg_func) = (void*)GetProcAddress(handle, "plugin_register");

            if (!reg_func) {
                LOG_INFO("Error: %s is not a valid plugin (missing 'plugin_register' symbol)", full_path);
                FreeLibrary(handle);
                continue;
            }
#else
            // POSIX-specific library loading
            void* handle = dlopen(full_path, RTLD_LAZY);
            if (!handle) {
                LOG_INFO("Error loading plugin %s: %s", full_path, dlerror());
                continue;
            }

            // Look for the mandatory "plugin_register" function symbol.
            ArchivePlugin* (*reg_func)(void);
            *(void**)(&reg_func) = dlsym(handle, "plugin_register");

            if (!reg_func) {
                LOG_INFO("Error: %s is not a valid plugin (missing 'plugin_register' symbol)", full_path);
                dlclose(handle);
                continue;
            }
#endif
            // Call the register function to get the plugin's interface struct.
            ArchivePlugin* new_plugin = reg_func();

            if (is_plugin_already_loaded(new_plugin->plugin_name)) {
                LOG_INFO("Plugin '%s' from '%s' was skipped because a plugin with the same name is already loaded.", new_plugin->plugin_name, full_path);
                #ifdef _WIN32
                    FreeLibrary(handle);
                #else
                    dlclose(handle);
                #endif
                continue;
            }
            
            loaded_plugins[num_plugins] = new_plugin;
            LOG_INFO("Successfully loaded plugin: %s (from %s)", loaded_plugins[num_plugins]->plugin_name, full_path);
            num_plugins++;
        }
    }
    closedir(d);
}

/**
 * @brief Finds the first loaded plugin that can handle a given file.
 *
 * @param filepath The path to the file that needs a handler.
 * @return A read-only pointer to the plugin's interface, or NULL if no handler is found.
 */
const ArchivePlugin* pm_get_handler(const char* filepath) {
    // Iterate through the loaded plugins and ask each one if it can handle the file.
    for (int i = 0; i < num_plugins; i++) {
        if (loaded_plugins[i]->can_handle(filepath)) {
            return loaded_plugins[i];
        }
    }
    // If no plugin claims the file, return NULL.
    return NULL;
}
