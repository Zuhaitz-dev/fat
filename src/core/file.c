/**
 * @file file.c
 * @author Zuhaitz (original)
 * @brief Implements file content and metadata reading functions.
 *
 * This file contains the logic for interacting with the filesystem to get
 * file details and read text content, using standard C libraries, POSIX
 * functions (stat), and libmagic for MIME type detection.
 */
#include "core/file.h"
#include "utils/logger.h"
#include <sys/stat.h>
#include <magic.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/**
 * @brief Gets file metadata (name, size, type, etc.) using stat and libmagic.
 *
 * @param path The path to the file to inspect.
 * @param info A pointer to a StringList that will be populated with metadata.
 * @return FAT_SUCCESS on success, or an error code on failure.
 */
FatResult get_file_info(const char* path, StringList* info) {
    struct stat st;
    magic_t magic_cookie = NULL;
    char buffer[512];

    // Use stat to get most of the file information.
    if (stat(path, &st) != 0) {
        LOG_INFO("Cannot stat file '%s': %s", path, strerror(errno));
        return FAT_ERROR_FILE_NOT_FOUND;
    }

    // **Format and add each piece of metadata to the StringList**

    // File Name (basename)
    const char* basename = strrchr(path, '/');
    basename = basename ? basename + 1 : path;
    snprintf(buffer, sizeof(buffer), "File: %s", basename);
    if (StringList_add(info, buffer) != FAT_SUCCESS) return FAT_ERROR_MEMORY;

    // MIME Type (using libmagic)
    magic_cookie = magic_open(MAGIC_MIME_TYPE | MAGIC_ERROR);
    if (magic_cookie == NULL) {
        LOG_INFO("magic_open failed");
    } else {
        if (magic_load(magic_cookie, NULL) == 0) {
            const char* magic_full = magic_file(magic_cookie, path);
            snprintf(buffer, sizeof(buffer), "Type: %s", magic_full ? magic_full : "unknown");
            if (StringList_add(info, buffer) != FAT_SUCCESS) {
                magic_close(magic_cookie);
                return FAT_ERROR_MEMORY;
            }
        } else {
            LOG_INFO("magic_load failed: %s", magic_error(magic_cookie));
        }
        magic_close(magic_cookie);
    }
    
    // Size
    snprintf(buffer, sizeof(buffer), "Size: %ld bytes", (long)st.st_size);
    if (StringList_add(info, buffer) != FAT_SUCCESS) return FAT_ERROR_MEMORY;

    // Modified Time
    char timebuf[100];
    if (strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", localtime(&st.st_mtime)) > 0) {
        snprintf(buffer, sizeof(buffer), "Modified: %s", timebuf);
        if (StringList_add(info, buffer) != FAT_SUCCESS) return FAT_ERROR_MEMORY;
    }

    // Permissions (in rwxrwxrwx format)
    char perm[11];
    snprintf(perm, sizeof(perm), "%c%c%c%c%c%c%c%c%c%c",
             S_ISDIR(st.st_mode) ? 'd' : '-', (st.st_mode & S_IRUSR) ? 'r' : '-',
             (st.st_mode & S_IWUSR) ? 'w' : '-', (st.st_mode & S_IXUSR) ? 'x' : '-',
             (st.st_mode & S_IRGRP) ? 'r' : '-', (st.st_mode & S_IWGRP) ? 'w' : '-',
             (st.st_mode & S_IXGRP) ? 'x' : '-', (st.st_mode & S_IROTH) ? 'r' : '-',
             (st.st_mode & S_IWOTH) ? 'w' : '-', (st.st_mode & S_IXOTH) ? 'x' : '-');
    snprintf(buffer, sizeof(buffer), "Perms: %s", perm);
    if (StringList_add(info, buffer) != FAT_SUCCESS) return FAT_ERROR_MEMORY;

    return FAT_SUCCESS;
}

/**
 * @brief Reads the content of a text file into a StringList, one line per entry.
 *
 * @param path The path to the text file to read.
 * @param list A pointer to a StringList that will be populated with the file's content.
 * @return FAT_SUCCESS on success, or an error code on failure.
 */
FatResult read_file_content(const char* path, StringList* list) {
    FatResult result = FAT_SUCCESS;
    FILE* file = NULL;
    char* line = NULL;
    size_t len = 0;

    file = fopen(path, "r");
    if (!file) {
        LOG_INFO("Could not open file '%s': %s", path, strerror(errno));
        return FAT_ERROR_FILE_READ;
    }

    // Read the file line by line using getline.
    while (getline(&line, &len, file) != -1) {
        // Trim the trailing newline character, if it exists.
        if (strlen(line) > 0 && line[strlen(line) - 1] == '\n') {
            line[strlen(line) - 1] = '\0';
        }
        if (StringList_add(list, line) != FAT_SUCCESS) {
            result = FAT_ERROR_MEMORY;
            goto cleanup; // Use goto to ensure resources are freed on error.
        }
    }

    // Check if the loop terminated due to a read error.
    if (ferror(file)) {
        LOG_INFO("Error reading from file '%s': %s", path, strerror(errno));
        result = FAT_ERROR_FILE_READ;
    }

cleanup:
    free(line); // getline allocates memory for 'line', which must be freed.
    if (file) {
        fclose(file);
    }
    return result;
}
