#include "utils/utils.h"
#include <sys/stat.h>

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
