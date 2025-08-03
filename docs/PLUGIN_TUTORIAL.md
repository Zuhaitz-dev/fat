# How to Create a FAT Plugin

This tutorial will guide you through the process of creating a new plugin for the FAT (File & Archive Tool). The plugin system allows you to extend FAT to support new archive formats.

We will walk through the existing `zip_plugin.c` to explain each required step in detail.

## 1. Understanding the Plugin API

All FAT plugins are shared libraries that conform to the API defined in `include/plugin_api.h`. At its core is the `ArchivePlugin` struct, which requires you to provide pointers to three functions you will create:

- `can_handle`: A function that quickly checks if your plugin can open a given file.

- `list_contents`: This function opens the archive and populates a list with the names of all the files inside.

-`extract_entry`: This function extracts a single file from the archive into a temporary location on disk.

## 2. Step-by-Step: Exploring `zip_plugin.c`

Let's examine the real-world implementation of the ZIP plugin.

### 2.1. Implement `can_handle`

The goal of this function is to be fast. It should do the minimum work necessary to determine if it can handle the file. Instead of just checking the file extension (which can be unreliable), `zip_plugin.c` uses `libzip`'s `zip_open` function. If `zip_open` succeeds, we know it's a valid ZIP file.

```c
// From: plugins/zip_plugin.c
#include <zip.h>
#include <stdbool.h>

bool zip_can_handle(const char* filepath) {
    int err = 0;
    zip_t* za = zip_open(filepath, 0, &err);
    if (za) {
        zip_close(za);
        return true;
    }
    return false;
}
```

### 2.2. Implement `list_contents`

This function does the main work of reading the archive's directory. It opens the archive, gets the number of entries, and then loops through them, adding the name of each file to the `StringList` provided by FAT.

```c
// From: plugins/zip_plugin.c
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
            // We check to make sure it's not a directory entry
            if (sb.name[strlen(sb.name) - 1] != '/') {
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
```

### 2.3. Implement `extract_entry`

When the user wants to view a file, this function is called. It finds the specific entry, reads its entire content into a buffer, and writes that buffer to a new temporary file on disk. The path to this temporary file is returned to FAT, which then opens it.

```c
// From: plugins/zip_plugin.c
FatResult zip_extract_entry(const char* archive_path, const char* entry_name, char** out_temp_path) {
    // ... (code to generate a unique temporary file path) ...

    // Open the archive and find the specific entry
    za = zip_open(archive_path, 0, &err);
    // ...
    zf = zip_fopen(za, entry_name, 0);
    // ...

    // Allocate a buffer and read the compressed data
    buffer = malloc(sb.size);
    // ...
    zip_fread(zf, buffer, sb.size);

    // Open the temporary file and write the buffer to it
    temp_file = fopen(full_temp_path, "wb");
    // ...
    fwrite(buffer, 1, sb.size, temp_file);

    // Return the path of the new temporary file to FAT
    *out_temp_path = full_temp_path;

    // ... (cleanup code) ...
    return FAT_SUCCESS;
}
```

### 2.4. Implement `plugin_register`

Finally, this mandatory function connects your implementation to FAT. You create a static `ArchivePlugin` struct and populate it with pointers to your functions.

```c
// From: plugins/zip_plugin.c
static ArchivePlugin zip_plugin_info = {
    .plugin_name = "ZIP Archive Handler",
    .can_handle = zip_can_handle,
    .list_contents = zip_list_contents,
    .extract_entry = zip_extract_entry
};

ArchivePlugin* plugin_register() {
    return &zip_plugin_info;
}
```

## 3. Compiling Your Plugin

Compile your `.c` file into a shared library. The `Makefile` in the FAT project already contains the correct commands and flags for the existing plugins, which you can adapt.

```bash
# Example command for the zip_plugin on Linux
gcc -shared -fPIC -o zip_plugin.so zip_plugin.c \
    -I../include \
    -L../lib -lfat_utils \
    -lzip # Link against the libzip library
```

## 4. Packaging the `.fp` File

### 4.1. Create the `manifest.json`

This file describes your plugin. For the ZIP plugin, it looks like this:

```json
{
  "name": "ZIP Archive Handler",
  "author": "Zuhaitz",
  "version": "1.0.0",
  "description": "Adds support for reading .zip archives using libzip.",
  "fat_version_min": "v0.1.0-beta - (Betelgeuse)",
  "binary_path": "zip_plugin.so",
  "supported_extensions": [".zip"]
}
```

## 5. Installing the Plugin

Installation is simple:

1. Run `fat` at least once to create the `~/.config/fat` directory.

2. Copy your new package into the plugins folder:

```bash
cp zip.fp ~/.config/fat/plugins/
```

The next time you open `fat`, it will automatically load your plugin, and you'll be able to open `.zip` files!