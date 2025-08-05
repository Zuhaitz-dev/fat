/**
 * @file error.c
 * @author Zuhaitz (original)
 * @brief Implements the conversion from error codes to human-readable strings.
 *
 * This provides the logic for the fat_result_to_string function, which is essential
 * for displaying meaningful error messages to the user.
 */
#include "core/error.h"

/**
 * @brief Converts a FatResult enum to a user-friendly, human-readable string.
 *
 * This function is primarily used to display error messages to the user in the UI.
 *
 * @param result The FatResult code to convert.
 * @return A constant, read-only string describing the error.
 */
const char* fat_result_to_string(FatResult result) {
    switch (result) {
        case FAT_SUCCESS:
            return "Success";
        case FAT_ERROR_GENERIC:
            return "An unknown error occurred.";
        case FAT_ERROR_MEMORY:
            return "Memory allocation failed.";
        case FAT_ERROR_FILE_NOT_FOUND:
            return "File or directory not found.";
        case FAT_ERROR_FILE_READ:
            return "Could not read from file.";
        case FAT_ERROR_FILE_WRITE:
            return "Could not write to file.";
        case FAT_ERROR_INVALID_ARGUMENT:
            return "Invalid argument provided to function.";
        case FAT_ERROR_PLUGIN_LOAD:
            return "Failed to load a plugin.";
        case FAT_ERROR_THEME_LOAD:
            return "Failed to load theme.";
        case FAT_ERROR_ARCHIVE_ERROR:
            return "An error occurred while handling an archive.";
        case FAT_ERROR_UNSUPPORTED:
            return "Operation not supported.";
        default:
            return "Unknown error code.";
    }
}
