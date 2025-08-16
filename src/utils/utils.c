#include "utils/utils.h"
#include <sys/stat.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else // Linux
#include <unistd.h>
#include <libgen.h> // for dirname
#endif


/**
 * @brief Checks if a directory exists at the given path.
 * @param path The path to check.
 * @return true if it is a directory, false otherwise.
 */
bool dir_exists(const char* path) {
    if (!path) return false;
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    }
    return false;
}

/**
 * @brief Gets the directory containing the application's executable.
 */
int get_executable_dir(char* buffer, size_t size) {
    char path[PATH_MAX];
#ifdef _WIN32
    if (GetModuleFileNameA(NULL, path, sizeof(path)) == 0) {
        return -1;
    }
    char* last_slash = strrchr(path, '\\');
    if (last_slash) {
        *last_slash = '\0';
    }
#elif defined(__APPLE__)
    uint32_t path_size = sizeof(path);
    if (_NSGetExecutablePath(path, &path_size) != 0) {
        return -1;
    }
    char* last_slash = strrchr(path, '/');
    if (last_slash) {
        *last_slash = '\0';
    }
#else // Linux
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len == -1) {
        return -1;
    }
    path[len] = '\0';
    // Use dirname to get the directory part.
    // Note: dirname might modify the path, so we work on a copy if needed,
    // but here it's fine as `path` is a local buffer.
    char* dir = dirname(path);
    strncpy(buffer, dir, size - 1);
    buffer[size - 1] = '\0';
    return 0;
#endif
    strncpy(buffer, path, size - 1);
    buffer[size - 1] = '\0';
    return 0;
}

/**
 * @brief Cleans up a temporary file if its path indicates it was created by FAT.
 */
void cleanup_temp_file_if_exists(const char* path) {
    if (!path) return;

    char temp_file_prefix[PATH_MAX];
#ifdef _WIN32
    char temp_dir[MAX_PATH];
    GetTempPathA(MAX_PATH, temp_dir);
    snprintf(temp_file_prefix, sizeof(temp_file_prefix), "%s\\fat-", temp_dir);
#else
    snprintf(temp_file_prefix, sizeof(temp_file_prefix), "/tmp/fat-");
#endif

    if (strncmp(path, temp_file_prefix, strlen(temp_file_prefix)) == 0) {
        remove(path);
    }
}
