/**
 * @file utf8_utils.h
 * @author Zuhaitz (original)
 * @brief Defines utility functions for handling UTF-8 encoded strings.
 *
 * This file provides helper functions to correctly navigate and process
 * UTF-8 strings, which is crucial for handling multi-byte characters
 * in the user interface and content processing.
 */
#ifndef UTF8_UTILS_H
#define UTF8_UTILS_H

#include <stdlib.h>

/**
 * @brief Calculates the byte length of a single UTF-8 character.
 *
 * Given a pointer to the start of a character, this function determines
 * how many bytes (from 1 to 4) make up that single character.
 *
 * @param s A pointer to the null-terminated string starting with the character to measure.
 * @return The number of bytes in the character, or 0 if the string is null or empty.
 */
int utf8_char_len(const char *s);

/**
 * @brief Finds the starting byte position of the previous UTF-8 character in a string.
 *
 * This is essential for implementing horizontal scrolling (left arrow key)
 * correctly, as it moves the cursor backward one full character at a time,
 * regardless of how many bytes it occupies.
 *
 * @param s The full null-terminated string.
 * @param current_pos The current byte offset into the string.
 * @return The byte offset of the beginning of the previous character.
 */
int utf8_prev_char_start(const char *s, int current_pos);

#endif // UTF8_UTILS_H
