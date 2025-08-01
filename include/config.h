/**
 * @file config.h
 * @author Zuhaitz (original)
 * @brief Defines the interface for loading and managing user configuration.
 *
 * This file declares the functions responsible for finding the user's
 * configuration directory, creating default files on the first run, loading
 * settings, and freeing associated resources at shutdown.
 */
#ifndef CONFIG_H
#define CONFIG_H

#include "state.h"
#include <stddef.h>

/**
 * @brief Gets the full, OS-specific path to the user's configuration directory.
 *
 * This function determines the conventional location for configuration files
 * based on the operating system (e.g., ~/.config/fat on Linux).
 *
 * @param buffer The buffer to write the resulting path into.
 * @param size The size of the `buffer`.
 * @return 0 on success, -1 on failure.
 */
int get_config_dir(char* buffer, size_t size);

/**
 * @brief Loads user settings and handles the first-run setup.
 *
 * This function is called once at startup. It locates the configuration
 * directory, and if it's the first time the app is run, it creates the
 * necessary subdirectories and copies the default themes and plugins for the user.
 * It then parses the `fatrc` file for any user-defined settings.
 *
 * @param state A pointer to the application state to populate with loaded settings.
 */
void config_load(AppState* state);

/**
 * @brief Frees memory allocated for configuration settings.
 *
 * This function is called once at shutdown to release any resources
 * that were allocated by `config_load`, such as the `default_theme_name` string.
 *
 * @param state The application state containing the config to free.
 */
void config_free(AppState* state);

#endif // CONFIG_H
