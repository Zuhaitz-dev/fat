/**
 * @file error.h
 * @author Zuhaitz (original)
 * @brief Defines the centralized error handling codes for the application.
 *
 * This file establishes a standard set of error codes (FatResult) that are used
 * as return types for functions that can fail. This provides a consistent
 * and specific way to handle errors throughout the FAT TUI application.
 */
#ifndef ERROR_H
#define ERROR_H

/**
 * @enum FatResult
 * @brief Defines the result type for functions that can fail.
 *
 * Using a specific enum for function results makes error handling more robust
 * and explicit than relying on simple integer codes or NULL pointers.
 */
typedef enum {
    FAT_SUCCESS = 0,                /**< Operation was successful. */
    FAT_ERROR_GENERIC,              /**< A generic, unspecified error occurred. */
    FAT_ERROR_MEMORY,               /**< A memory allocation (malloc, realloc, etc.) failed. */
    FAT_ERROR_FILE_NOT_FOUND,       /**< The specified file or directory could not be found. */
    FAT_ERROR_FILE_READ,            /**< An error occurred while reading from a file. */
    FAT_ERROR_FILE_WRITE,           /**< An error occurred while writing to a file. */
    FAT_ERROR_INVALID_ARGUMENT,     /**< An invalid argument was provided to a function. */
    FAT_ERROR_PLUGIN_LOAD,          /**< A plugin failed to load or initialize. */
    FAT_ERROR_THEME_LOAD,           /**< A theme file could not be loaded or parsed. */
    FAT_ERROR_ARCHIVE_ERROR,        /**< An error occurred during archive processing (zip, tar). */
    FAT_ERROR_UNSUPPORTED           /**< The requested operation is not supported for the given item. */
} FatResult;

/**
 * @brief Converts a FatResult enum to a user-friendly, human-readable string.
 *
 * This function is primarily used to display error messages to the user in the UI.
 *
 * @param result The FatResult code to convert.
 * @return A constant, read-only string describing the error.
 */
const char* fat_result_to_string(FatResult result);

#endif // ERROR_H
