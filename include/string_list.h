/**
 * @file string_list.h
 * @author Zuhaitz (original)
 * @brief Defines a dynamic array of strings (StringList).
 *
 * This utility provides a simple, C-style replacement for C++'s std::vector<std::string>.
 * It handles dynamic memory allocation, allowing lists of strings to grow as needed.
 * It is used extensively throughout the application to manage collections of lines,
 * file paths, and metadata.
 */
#ifndef STRING_LIST_H
#define STRING_LIST_H

#include <stdlib.h> // For size_t
#include "error.h"  // For FatResult

/**
 * @struct StringList
 * @brief A dynamic array of strings.
 *
 * Manages a list of heap-allocated strings, automatically resizing as
 * new strings are added.
 */
typedef struct {
    char** lines;       /**< The array of string pointers. */
    size_t count;       /**< The number of strings currently in the list. */
    size_t capacity;    /**< The number of string pointers the array can currently hold. */
} StringList;


/**
 * @brief Initializes a StringList to a safe, empty state.
 *
 * This must be called on a new StringList before it is used to ensure its
 * members are set to NULL/0, preventing invalid memory access.
 *
 * @param list Pointer to the StringList to initialize.
 */
void StringList_init(StringList* list);

/**
 * @brief Adds a copy of a string to the list.
 *
 * This function handles memory allocation for the new string and resizes the
 * internal array if necessary. The provided string is duplicated, so the
 * original can be freed or go out of scope safely.
 *
 * @param list Pointer to the StringList.
 * @param str The null-terminated string to add.
 * @return FAT_SUCCESS on success, or FAT_ERROR_MEMORY on an allocation failure.
 */
FatResult StringList_add(StringList* list, const char* str);

/**
 * @brief Frees all memory associated with the list and its contents.
 *
 * This function iterates through each string in the list, frees it, then frees
 * the list's internal array. Finally, it re-initializes the list to a safe
 * empty state.
 *
 * @param list Pointer to the StringList to free.
 */
void StringList_free(StringList* list);

#endif // STRING_LIST_H
