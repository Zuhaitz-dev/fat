/**
 * @file utf8_utils.c
 * @author Zuhaitz (original)
 * @brief Implements utility functions for handling UTF-8 encoded strings.
 */
#include "utils/utf8_utils.h"

/**
 * @brief Calculates the byte length of a UTF-8 character.
 * @param s A pointer to the start of the character.
 * @return The number of bytes (1-4) for the character.
 */
int utf8_char_len(const char *s) {
    if (!s || *s == '\0') return 0;
    unsigned char c = *s;
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1; // Fallback for invalid byte
}

/**
 * @brief Finds the starting byte position of the previous UTF-8 character.
 * @param s The full string.
 * @param current_pos The current byte offset into the string.
 * @return The byte offset of the beginning of the previous character.
 */
int utf8_prev_char_start(const char *s, int current_pos) {
    if (current_pos <= 0) return 0;
    int pos = current_pos - 1;
    // Move backwards until we find a byte that is NOT a continuation byte
    while (pos > 0 && (s[pos] & 0xC0) == 0x80) {
        pos--;
    }
    return pos;
}
