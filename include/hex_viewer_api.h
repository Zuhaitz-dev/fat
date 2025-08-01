/**
 * @file hex_viewer_api.h
 * @author Zuhaitz (original)
 * @brief Defines the interface for the binary file hex viewer.
 *
 * This module is responsible for taking a binary file and converting its
 * content into a traditional hex dump format (offset, hex bytes, ASCII representation)
 * for display in the UI.
 */
#ifndef HEX_VIEWER_API_H
#define HEX_VIEWER_API_H

#include "string_list.h"
#include "error.h"

/**
 * @brief Generates a hex dump of a given file's content.
 *
 * Reads the file in 16-byte chunks and formats each chunk into a single
 * string line, which is then added to the output StringList.
 *
 * @param filepath The path to the binary file to dump.
 * @param dump_list A pointer to a StringList that will be populated with the
 * hex dump lines. The list should be initialized before calling.
 * @return FAT_SUCCESS on success, or an error code (e.g., FAT_ERROR_FILE_READ)
 * on failure.
 */
FatResult hex_viewer_generate_dump(const char* filepath, StringList* dump_list);

#endif // HEX_VIEWER_API_H
