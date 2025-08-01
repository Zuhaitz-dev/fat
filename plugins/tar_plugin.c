/**
 * @file tar_plugin.c
 * @author Zuhaitz (original)
 * @brief A dynamic plugin for handling TAR archives using libtar.
 */
#include "../include/plugin_api.h"
#include "../include/logger.h"
#include <libtar.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#if defined(__linux__) || defined(__APPLE__)
#include <fcntl.h>
#endif
#include <errno.h>
#include <libgen.h>
#include <sys/types.h>
#ifdef _WIN32
#include <windows.h>
#endif

#ifndef O_RDONLY
#define O_RDONLY 0
#endif

// **Plugin Implementation**

/**
 * @brief Checks if the plugin can handle the given file by attempting to open it as a TAR archive.
 */
bool tar_can_handle(const char* filepath) {
    TAR* t = NULL;
    if (tar_open(&t, (char*)filepath, NULL, O_RDONLY, 0, 0) != 0) {
        return false;
    }
    if (th_read(t) != 0) {
        tar_close(t);
        return false;
    }
    tar_close(t);
    return true;
}

/**
 * @brief Lists the contents of a TAR archive.
 */
FatResult tar_list_contents(const char* filepath, StringList* list) {
    TAR* t = NULL;
    FatResult result = FAT_SUCCESS;

    if (tar_open(&t, (char*)filepath, NULL, O_RDONLY, 0, 0) == -1) {
        LOG_INFO("tar_open failed for '%s'", filepath);
        return FAT_ERROR_ARCHIVE_ERROR;
    }

    while (th_read(t) == 0) {
        if (TH_ISREG(t)) {
            if (StringList_add(list, th_get_pathname(t)) != FAT_SUCCESS) {
                result = FAT_ERROR_MEMORY;
                goto cleanup;
            }
        }
        if (TH_ISREG(t) && tar_skip_regfile(t) != 0) {
            LOG_INFO("tar_skip_regfile failed for an entry in '%s'", filepath);
            result = FAT_ERROR_ARCHIVE_ERROR;
            goto cleanup;
        }
    }

cleanup:
    if (t) {
        tar_close(t);
    }
    return result;
}

/**
 * @brief Extracts a single entry from a TAR archive to a temporary file.
 */
FatResult tar_extract_entry(const char* archive_path, const char* entry_name, char** out_temp_path) {
    TAR* t = NULL;
    FatResult result = FAT_SUCCESS;
    char* full_temp_path = NULL;
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
    if (!full_temp_path) {
        free(sanitized_name);
        return FAT_ERROR_MEMORY;
    }
    #ifdef _WIN32
        snprintf(full_temp_path, path_len, "%s\\fat-%s-%lu", temp_dir, sanitized_name, pid);
    #else
        snprintf(full_temp_path, path_len, "%s/fat-%s-%d", temp_dir, sanitized_name, pid);
    #endif
    free(sanitized_name);

    bool found = false;
    *out_temp_path = NULL;

    if (tar_open(&t, (char*)archive_path, NULL, O_RDONLY, 0, 0) == -1) {
        LOG_INFO("tar_open failed for '%s'", archive_path);
        result = FAT_ERROR_ARCHIVE_ERROR;
        goto cleanup;
    }
    
    while (th_read(t) == 0) {
        if (strcmp(th_get_pathname(t), entry_name) == 0) {
            if (TH_ISREG(t)) {
                if (tar_extract_file(t, full_temp_path) == 0) {
                    found = true;
                } else {
                    LOG_INFO("tar_extract_file failed for entry '%s' in '%s'", entry_name, archive_path);
                    result = FAT_ERROR_ARCHIVE_ERROR;
                }
            } else {
                 LOG_INFO("Entry '%s' in '%s' is not a regular file.", entry_name, archive_path);
                 result = FAT_ERROR_UNSUPPORTED;
            }
            break; 
        }
        if (TH_ISREG(t)) {
            tar_skip_regfile(t);
        }
    }
    
    if (result == FAT_SUCCESS && found) {
        *out_temp_path = full_temp_path;
        full_temp_path = NULL; 
        if (!*out_temp_path) {
            result = FAT_ERROR_MEMORY;
        }
    }

cleanup:
    if (t) {
        tar_close(t);
    }
    if (result != FAT_SUCCESS || !found) {
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
static ArchivePlugin tar_plugin_info = {
    .plugin_name = "TAR Archive Handler",
    .can_handle = tar_can_handle,
    .list_contents = tar_list_contents,
    .extract_entry = tar_extract_entry
};

/**
 * @brief The registration function called by the plugin manager.
 */
ArchivePlugin* plugin_register() {
    return &tar_plugin_info;
}
