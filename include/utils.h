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

#endif // UTILS_H
