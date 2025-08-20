/**
 * @file gz_plugin.c
 * @author Zuhaitz (original)
 * @brief A dynamic plugin for handling GZIP compressed files using zlib.
 */
#include "../include/plugins/plugin_api.h"
#include "../include/utils/logger.h"
#include <zlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <libgen.h>

#define CHUNK_SIZE 16384 // 16KB chunk for decompression

// **Plugin Implementation**

/**
 * @brief Checks if the plugin can handle the given file by its extension and magic number.
 */
bool gz_can_handle(const char* filepath) {
    const char* dot = strrchr(filepath, '.');
    if (dot && strcmp(dot, ".gz") == 0) {
        FILE* f = fopen(filepath, "rb");
        if (!f) return false;
        unsigned char magic[2];
        bool result = false;
        if (fread(magic, 1, 2, f) == 2) {
            // Gzip magic number is 0x1F 0x8B
            if (magic[0] == 0x1f && magic[1] == 0x8b) {
                result = true;
            }
        }
        fclose(f);
        return result;
    }
    return false;
}

/**
 * @brief "Lists" the contents of a GZIP file, which is just the original filename.
 */
FatResult gz_list_contents(const char* filepath, StringList* list) {
    char* base = basename((char*)filepath);
    size_t len = strlen(base);
    if (len > 3 && strcmp(base + len - 3, ".gz") == 0) {
        char* original_name = strdup(base);
        if (!original_name) return FAT_ERROR_MEMORY;
        original_name[len - 3] = '\0'; // Remove .gz extension
        FatResult result = StringList_add(list, original_name);
        free(original_name);
        return result;
    }
    return StringList_add(list, "decompressed_file"); // Fallback name
}

/**
 * @brief Extracts the GZIP file to a temporary location.
 */
FatResult gz_extract_entry(const char* archive_path, const char* entry_name, char** out_temp_path) {
    (void)entry_name; // Unused, since there's only one "entry"

    gzFile gz_file = gzopen(archive_path, "rb");
    if (!gz_file) {
        LOG_INFO("gzopen failed for '%s'", archive_path);
        return FAT_ERROR_ARCHIVE_ERROR;
    }

    // Create a temporary file path
    char* temp_path_template = strdup("/tmp/fat-XXXXXX");
    if (!temp_path_template) {
        gzclose(gz_file);
        return FAT_ERROR_MEMORY;
    }

    int temp_fd = mkstemp(temp_path_template);
    if (temp_fd == -1) {
        LOG_INFO("mkstemp failed for temporary file: %s", strerror(errno));
        gzclose(gz_file);
        free(temp_path_template);
        return FAT_ERROR_FILE_WRITE;
    }

    FILE* temp_file = fdopen(temp_fd, "wb");
    if (!temp_file) {
        LOG_INFO("fdopen failed for temporary file: %s", strerror(errno));
        close(temp_fd);
        gzclose(gz_file);
        free(temp_path_template);
        return FAT_ERROR_FILE_WRITE;
    }

    unsigned char buffer[CHUNK_SIZE];
    int bytes_read;
    while ((bytes_read = gzread(gz_file, buffer, CHUNK_SIZE)) > 0) {
        if (fwrite(buffer, 1, bytes_read, temp_file) != (size_t)bytes_read) {
            LOG_INFO("Incomplete write to temporary file for %s", archive_path);
            fclose(temp_file);
            gzclose(gz_file);
            remove(temp_path_template);
            free(temp_path_template);
            return FAT_ERROR_FILE_WRITE;
        }
    }

    if (bytes_read < 0) {
        int err_no = 0;
        const char *err_str = gzerror(gz_file, &err_no);
        if(err_no != Z_OK) {
            LOG_INFO("gzread error while decompressing '%s': %s", archive_path, err_str);
            fclose(temp_file);
            gzclose(gz_file);
            remove(temp_path_template);
            free(temp_path_template);
            return FAT_ERROR_FILE_READ;
        }
    }

    fclose(temp_file);
    gzclose(gz_file);

    *out_temp_path = temp_path_template;

    return FAT_SUCCESS;
}


// **Plugin Registration**

/**
 * @brief The static instance of the ArchivePlugin interface for this plugin.
 */
static ArchivePlugin gz_plugin_info = {
    .plugin_name = "GZIP Decompressor",
    .can_handle = gz_can_handle,
    .list_contents = gz_list_contents,
    .extract_entry = gz_extract_entry
};

/**
 * @brief The registration function called by the plugin manager.
 */
ArchivePlugin* plugin_register() {
    return &gz_plugin_info;
}
