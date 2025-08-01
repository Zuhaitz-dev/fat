/**
 * @file zip_plugin.c
 * @author Zuhaitz (original)
 * @brief A dynamic plugin for handling ZIP archives using libzip.
 */
#include "../include/plugin_api.h"
#include "../include/logger.h"
#include <zip.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <libgen.h>
#include <sys/types.h>
#ifdef _WIN32
#include <windows.h>
#endif

// **Plugin Implementation**

/**
 * @brief Checks if the plugin can handle the given file by attempting to open it as a ZIP archive.
 */
bool zip_can_handle(const char* filepath) {
    int err = 0;
    zip_t* za = zip_open(filepath, 0, &err);
    if (za) {
        zip_close(za);
        return true;
    }
    return false;
}

/**
 * @brief Lists the contents of a ZIP archive.
 */
FatResult zip_list_contents(const char* filepath, StringList* list) {
    int err = 0;
    zip_t* za = zip_open(filepath, 0, &err);
    if (!za) {
        LOG_INFO("Could not open zip file '%s'. Libzip error code: %d", filepath, err);
        return FAT_ERROR_ARCHIVE_ERROR;
    }

    FatResult result = FAT_SUCCESS;
    zip_int64_t num_entries = zip_get_num_entries(za, 0);
    for (zip_int64_t i = 0; i < num_entries; i++) {
        struct zip_stat sb;
        if (zip_stat_index(za, i, 0, &sb) == 0) {
            bool is_dir = (sb.name[strlen(sb.name) - 1] == '/') || (sb.size == 0);
            
            if (!is_dir) {
                if (StringList_add(list, sb.name) != FAT_SUCCESS) {
                    result = FAT_ERROR_MEMORY;
                    break;
                }
            }
        }
    }

    zip_close(za);
    return result;
}

/**
 * @brief Extracts a single entry from a ZIP archive to a temporary file.
 */
FatResult zip_extract_entry(const char* archive_path, const char* entry_name, char** out_temp_path) {
    zip_t* za = NULL;
    zip_file_t* zf = NULL;
    char* buffer = NULL;
    char* full_temp_path = NULL;
    FILE* temp_file = NULL;
    char temp_dir[256];

    #ifdef _WIN32
        GetTempPathA(sizeof(temp_dir), temp_dir);
    #else
        strcpy(temp_dir, "/tmp");
    #endif

    char* sanitized_name = strdup(entry_name);
    if (!sanitized_name) return FAT_ERROR_MEMORY;
    for (char* p = sanitized_name; *p; ++p) {
        if (*p == '/') *p = '_';
    }
    
    #ifdef _WIN32
        DWORD pid = GetCurrentProcessId();
    #else
        pid_t pid = getpid();
    #endif

    size_t path_len = strlen(temp_dir) + strlen("\\fat--") + strlen(sanitized_name) + 20; // Ample space
    full_temp_path = malloc(path_len);
    if(!full_temp_path) {
        free(sanitized_name);
        return FAT_ERROR_MEMORY;
    }
    #ifdef _WIN32
        snprintf(full_temp_path, path_len, "%s\\fat-%s-%lu", temp_dir, sanitized_name, pid);
    #else
        snprintf(full_temp_path, path_len, "%s/fat-%s-%d", temp_dir, sanitized_name, pid);
    #endif
    free(sanitized_name);

    FatResult result = FAT_SUCCESS;
    int err = 0;

    *out_temp_path = NULL;

    za = zip_open(archive_path, 0, &err);
    if (!za) {
        LOG_INFO("zip_open failed for '%s'. Libzip error: %d", archive_path, err);
        result = FAT_ERROR_ARCHIVE_ERROR;
        goto cleanup;
    }

    struct zip_stat sb;
    if (zip_stat(za, entry_name, 0, &sb) != 0) {
        LOG_INFO("zip_stat failed for entry '%s' in '%s'", entry_name, archive_path);
        result = FAT_ERROR_ARCHIVE_ERROR;
        goto cleanup;
    }

    zf = zip_fopen(za, entry_name, 0);
    if (!zf) {
        LOG_INFO("zip_fopen failed for entry '%s' in '%s'", entry_name, archive_path);
        result = FAT_ERROR_ARCHIVE_ERROR;
        goto cleanup;
    }

    buffer = malloc(sb.size);
    if (!buffer) {
        result = FAT_ERROR_MEMORY;
        goto cleanup;
    }

    if (zip_fread(zf, buffer, sb.size) != (zip_int64_t)sb.size) {
        LOG_INFO("zip_fread failed to read full entry '%s' from '%s'", entry_name, archive_path);
        result = FAT_ERROR_FILE_READ;
        goto cleanup;
    }
    
    temp_file = fopen(full_temp_path, "wb");
    if (!temp_file) {
        LOG_INFO("fopen for temp file failed: %s", strerror(errno));
        result = FAT_ERROR_FILE_WRITE;
        goto cleanup;
    }

    if (fwrite(buffer, 1, sb.size, temp_file) != sb.size) {
        LOG_INFO("Incomplete write to temporary file for %s", entry_name);
        result = FAT_ERROR_FILE_WRITE;
        goto cleanup;
    }

    *out_temp_path = full_temp_path;
    full_temp_path = NULL; // Transfer ownership

    if (!*out_temp_path) {
        result = FAT_ERROR_MEMORY;
        goto cleanup;
    }

cleanup:
    free(buffer);
    if (zf) zip_fclose(zf);
    if (za) zip_close(za);
    if (temp_file) fclose(temp_file);
    
    if (result != FAT_SUCCESS) {
        if(full_temp_path) remove(full_temp_path);
        free(*out_temp_path);
        *out_temp_path = NULL;
    }
    free(full_temp_path);
    return result;
}

// **Plugin Registration**

/**
 * @brief The static instance of the ArchivePlugin interface for this plugin.
 */
static ArchivePlugin zip_plugin_info = {
    .plugin_name = "ZIP Archive Handler",
    .can_handle = zip_can_handle,
    .list_contents = zip_list_contents,
    .extract_entry = zip_extract_entry
};

/**
 * @brief The registration function called by the plugin manager.
 */
ArchivePlugin* plugin_register() {
    return &zip_plugin_info;
}
