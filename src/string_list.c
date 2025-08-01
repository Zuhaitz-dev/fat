/**
 * @file string_list.c
 * @author Zuhaitz (original)
 * @brief Implementation of the dynamic string array (StringList).
 *
 * This file contains the logic for initializing, adding to, and freeing a
 * StringList, including its dynamic resizing behavior.
 */
#include "string_list.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Initializes a StringList to a safe, empty state.
 *
 * @param list Pointer to the StringList to initialize.
 */
void StringList_init(StringList* list) {
    list->lines = NULL;
    list->count = 0;
    list->capacity = 0;
}

/**
 * @brief Adds a copy of a string to the list.
 *
 * @param list Pointer to the StringList.
 * @param str The null-terminated string to add.
 * @return FAT_SUCCESS on success, or FAT_ERROR_MEMORY on an allocation failure.
 */
FatResult StringList_add(StringList* list, const char* str) {
    // If the list is full, grow its capacity.
    if (list->count >= list->capacity) {
        // Double the capacity, or start with 8 if it's new.
        size_t new_capacity = (list->capacity == 0) ? 8 : list->capacity * 2;
        char** new_lines = realloc(list->lines, new_capacity * sizeof(char*));
        if (!new_lines) {
            LOG_INFO("realloc failed in StringList_add");
            return FAT_ERROR_MEMORY;
        }
        list->lines = new_lines;
        list->capacity = new_capacity;
    }
    
    // Duplicate the incoming string and store the copy in the list.
    list->lines[list->count] = strdup(str);
    if (!list->lines[list->count]) {
        LOG_INFO("strdup failed in StringList_add");
        return FAT_ERROR_MEMORY;
    }
    
    list->count++;
    return FAT_SUCCESS;
}

/**
 * @brief Frees all memory associated with the list and its contents.
 *
 * @param list Pointer to the StringList to free.
 */
void StringList_free(StringList* list) {
    if (!list) return;
    // Free each individual string.
    for (size_t i = 0; i < list->count; i++) {
        free(list->lines[i]);
    }
    // Free the array of pointers.
    free(list->lines);
    // Reset the struct to a clean, initialized state to prevent use-after-free.
    StringList_init(list);
}
