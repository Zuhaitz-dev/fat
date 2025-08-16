/**
 * @file utils.h
 * @author Zuhaitz (original)
 * @brief Defines globally shared utility functions for the application.
 *
 * This header provides declarations for common helper functions that are used
 * across multiple modules, such as checking for the existence of a directory.
 */
#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Checks if a directory exists at the given path.
 *
 * This function uses `stat` to determine if a path points to a valid,
 * accessible directory.
 *
 * @param path The filesystem path to check.
 * @return `true` if the path is a directory, `false` otherwise.
 */
bool dir_exists(const char* path);

/**
 * @brief Gets the directory containing the application's executable.
 *
 * This function provides a reliable way to locate resource files (like themes
 * and default configs) that are stored relative to the executable, which is
 * crucial for development and portable builds.
 *
 * @param buffer The buffer to write the resulting path into.
 * @param size The size of the `buffer`.
 * @return 0 on success, -1 on failure.
 */
int get_executable_dir(char* buffer, size_t size);

/**
 * @brief Cleans up a temporary file if its path indicates it was created by FAT.
 *
 * This function checks if a given path matches the pattern of temporary files
 * created by the application (e.g., `/tmp/fat-*`) and deletes it if so. This
 * is used to clean up after viewing files from within archives.
 *
 * @param path The path to the file to potentially clean up.
 */
void cleanup_temp_file_if_exists(const char* path);


#endif // UTILS_H
