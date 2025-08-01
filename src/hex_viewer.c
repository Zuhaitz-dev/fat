/**
 * @file hex_viewer.c
 * @author Zuhaitz (original)
 * @brief Implements the binary file hex dump generator.
 *
 * This file contains the logic for reading a file byte-by-byte and
 * formatting it into a standard hex dump view.
 */
#include "hex_viewer_api.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

/**
 * @brief Generates a hex dump of a given file's content.
 *
 * @param filepath The path to the binary file to dump.
 * @param dump_list A pointer to a StringList to be populated with the hex dump lines.
 * @return FAT_SUCCESS on success, or an error code on failure.
 */
FatResult hex_viewer_generate_dump(const char* filepath, StringList* dump_list) {
    FILE* f = NULL;
    FatResult result = FAT_SUCCESS;
    unsigned char buffer[16]; // Process the file in 16-byte chunks.
    size_t bytes_read;
    unsigned long offset = 0;
    char line_buffer[100]; // A buffer large enough for one formatted line.

    f = fopen(filepath, "rb");
    if (!f) {
        LOG_INFO("Could not open file '%s' for hex dump: %s", filepath, strerror(errno));
        return FAT_ERROR_FILE_READ;
    }

    // Read through the file until EOF.
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        int len = 0; // Current position in line_buffer.

        // 1 - Format the offset at the beginning of the line.
        len += snprintf(line_buffer + len, sizeof(line_buffer) - len, "%08lX: ", offset);

        // 2 - Format the hexadecimal representation of the bytes.
        for (size_t i = 0; i < 16; ++i) {
            if (i < bytes_read) {
                len += snprintf(line_buffer + len, sizeof(line_buffer) - len, "%02X ", buffer[i]);
            } else {
                // Pad with spaces if the last chunk is less than 16 bytes.
                len += snprintf(line_buffer + len, sizeof(line_buffer) - len, "   ");
            }
        }

        // 3 - Add a separator between the hex and ASCII sections.
        len += snprintf(line_buffer + len, sizeof(line_buffer) - len, " |");

        // 4 - Format the ASCII representation of the bytes.
        for (size_t i = 0; i < bytes_read; ++i) {
            // Use '.' for non-printable characters.
            len += snprintf(line_buffer + len, sizeof(line_buffer) - len, "%c", isprint(buffer[i]) ? buffer[i] : '.');
        }

        // Add the fully formatted line to our list.
        if (StringList_add(dump_list, line_buffer) != FAT_SUCCESS) {
            result = FAT_ERROR_MEMORY;
            goto cleanup; // Use goto to ensure file is closed on error.
        }
        offset += bytes_read;
    }

    // Check if the loop ended because of a read error.
    if (ferror(f)) {
        LOG_INFO("Error reading from file '%s' during hex dump: %s", filepath, strerror(errno));
        result = FAT_ERROR_FILE_READ;
    }

cleanup:
    if (f) {
        fclose(f);
    }
    return result;
}
