/**
 * @file plugin_api.h
 * @author Zuhaitz (original)
 * @brief Defines the Application Programming Interface (API) for archive plugins.
 *
 * This file specifies the "contract" that all dynamic plugins must adhere to.
 * The plugin manager uses the ArchivePlugin struct to interact with loaded
 * plugins in a standardized way, without needing to know the specifics of
 * how each archive format (ZIP, TAR, etc.) is handled.
 */
#ifndef PLUGIN_API_H
#define PLUGIN_API_H

#include "core/string_list.h"
#include "core/error.h"
#include <stdbool.h>

/**
 * @struct ArchivePlugin
 * @brief Defines the standard interface for an archive handling plugin.
 *
 * Every plugin must implement these functions and provide a pointer to a
 * static instance of this struct via its `plugin_register` function.
 */
typedef struct {
    /** The user-friendly name of the plugin (e.g., "ZIP Archive Handler"). */
    const char* plugin_name;

    /**
     * @brief A function to quickly check if this plugin can handle a given file.
     *
     * This is typically done by checking the file extension or magic numbers.
     *
     * @param filepath The path to the file to check.
     * @return `true` if the plugin can handle the file, `false` otherwise.
     */
    bool (*can_handle)(const char* filepath);

    /**
     * @brief A function to list the contents of the archive.
     *
     * @param filepath The path to the archive file.
     * @param list A pointer to a StringList to be populated with file/entry names.
     * @return FAT_SUCCESS on success, or an appropriate error code on failure.
     */
    FatResult (*list_contents)(const char* filepath, StringList* list);

    /**
     * @brief A function to extract a single entry from the archive to a temporary file.
     *
     * The application will then load this temporary file for viewing.
     *
     * @param archive_path The path to the archive file.
     * @param entry_name The name of the entry within the archive to extract.
     * @param out_temp_path A pointer to a `char*` that will be set to the path
     * of the new temporary file on success. The caller is
     * responsible for freeing this path string and deleting
     * the temporary file when it's no longer needed.
     * @return FAT_SUCCESS on success, or an appropriate error code on failure.
     */
    FatResult (*extract_entry)(const char* archive_path, const char* entry_name, char** out_temp_path);

} ArchivePlugin;

#endif // PLUGIN_API_H
