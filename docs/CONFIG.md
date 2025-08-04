# FAT Configuration File (`fatrc`)

The `fatrc` file allows you to customize the behavior of the FAT (File & Archive Tool). On its first run, FAT creates a default configuration file in your user config directory.

-   **Linux/macOS**: `~/.config/fat/fatrc`
-   **Windows**: `%APPDATA%\fat\fatrc`

This file uses a simple `key = value` format. Lines beginning with `#` are treated as comments and are ignored.

## Available Settings

### `default_theme`

This setting allows you to specify which theme FAT should use at startup. The value should be the name of the theme file without the `.json` extension.

**Example:**

```
# Use the Gruvbox theme by default
default_theme = gruvbox
```

---

### `text_mimes`

A comma-separated list of MIME types that you want to **always** force into Text Mode, overriding FAT's default behavior. This is useful for file types that might otherwise be incorrectly identified as binary (like certain XML or JSON-based formats).

**Example:**

```
# Always treat JSON, GeoJSON, and XML files as text
text_mimes = application/json, application/geo+json, application/xml
```

---

### `binary_mimes`

A comma-separated list of MIME types that you want to **always** force into Hex Mode. This gives you control over file types that might not be in FAT's default list of binary formats.

**Example:**

```
# Always treat SQLite databases and generic binary streams as hex
binary_mimes = application/x-sqlite3, application/octet-stream
```
