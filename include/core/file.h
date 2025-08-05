/**
 * @file file.h
 * @author Zuhaitz (original)
 * @brief Defines functions for reading file content and metadata.
 *
 * This header provides the interface for core file operations, such as
 * reading a text file line-by-line or gathering detailed information
 * (like size, type, and permissions) about a file.
 */
#ifndef FILE_H
#define FILE_H

#include "core/string_list.h"
#include "core/error.h"

/**
 * @brief Gets file metadata (name, size, type, etc.) using stat and libmagic.
 *
 * This function populates a StringList with human-readable lines of file
 * information, which is then displayed in the UI's left pane.
 *
 * @param path The path to the file to inspect.
 * @param info A pointer to a StringList that will be populated with metadata.
 * The list is not initialized by this function; it should be
 * initialized before calling.
 * @return FAT_SUCCESS on success, or an error code (e.g., FAT_ERROR_FILE_NOT_FOUND)
 * on failure.
 */
FatResult get_file_info(const char *path, StringList *info);

/**
 * @brief Reads the content of a text file into a StringList, one line per entry.
 *
 * This function is used to load the content of regular text files for display
 * in the UI's right pane. It handles newline characters automatically.
 *
 * @param path The path to the text file to read.
 * @param list A pointer to a StringList that will be populated with the file's content.
 * The list should be initialized before calling.
 * @return FAT_SUCCESS on success, or an error code (e.g., FAT_ERROR_FILE_READ)
 * on failure.
 */
FatResult read_file_content(const char *path, StringList *list);

#endif //FILE_H
