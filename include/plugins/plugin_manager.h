/**
 * @file plugin_manager.h
 * @author Zuhaitz (original)
 * @brief Defines the interface for the dynamic plugin loading system.
 *
 * The plugin manager is responsible for finding, loading, and providing access
 * to shared library (.so) plugins that handle different archive formats.
 */
#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#include "plugins/plugin_api.h"

/**
 * @brief Loads all plugins from the specified directory.
 *
 * This function scans a directory for shared library files (.so), opens them,
 * finds the `plugin_register` symbol, and stores the returned ArchivePlugin
 * interface. This should be called once at application startup.
 *
 * @param plugin_dir_path The path to the directory containing plugin .so files.
 */
void pm_load_plugins(const char* plugin_dir_path);

/**
 * @brief Finds the first loaded plugin that can handle a given file.
 *
 * It iterates through all successfully loaded plugins and calls their
 * `can_handle` function. The first plugin to return `true` is returned.
 *
 * @param filepath The path to the file that needs a handler.
 * @return A read-only pointer to the plugin's interface, or NULL if no
 * handler is found.
 */
const ArchivePlugin* pm_get_handler(const char* filepath);

#endif // PLUGIN_MANAGER_H
