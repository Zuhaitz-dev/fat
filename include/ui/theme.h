/**
 * @file theme.h
 * @author Zuhaitz (original)
 * @brief Defines the data structures and functions for handling UI themes.
 *
 * This file specifies how themes are represented in memory (the Theme struct)
 * and the functions for loading them from JSON files, applying them to the
 * ncurses environment, and discovering available theme files.
 */
#ifndef THEME_H
#define THEME_H

#include <ncurses.h>
#include "core/string_list.h"
#include "core/error.h"

/**
 * @enum ThemeElement
 * @brief Maps logical UI elements to ncurses color pairs.
 *
 * This enum provides a clear, readable way to reference different themed
 * elements of the UI. The order MUST match the `element_names` array in theme.c
 * to ensure correct parsing from JSON files.
 */
typedef enum {
    THEME_ELEMENT_BORDER,
    THEME_ELEMENT_TITLE,
    THEME_ELEMENT_METADATA_LABEL,
    THEME_ELEMENT_LINE_NUM,
    THEME_ELEMENT_STATUSBAR,
    THEME_ELEMENT_SEARCH_HIGHLIGHT,
    THEME_ELEMENT_HELP_BORDER,
    THEME_ELEMENT_HELP_KEY,
    THEME_ELEMENT_COUNT // Keep this last for easy iteration and array sizing.
} ThemeElement;

/**
 * @struct ThemeColor
 * @brief Represents an ncurses color pair with foreground and background colors.
 */
typedef struct {
    short fg; /**< The foreground color (e.g., COLOR_RED). */
    short bg; /**< The background color (e.g., COLOR_BLACK). */
} ThemeColor;

/**
 * @struct Theme
 * @brief Represents a full UI theme, including its name and all its color definitions.
 */
typedef struct {
    char* name;     /**< The display name of the theme (e.g., "Nord"). */
    char* author;   /**< The author of the theme. */
    ThemeColor colors[THEME_ELEMENT_COUNT]; /**< An array of colors for each UI element. */
} Theme;

/**
 * @brief Loads a theme from a JSON file and creates a Theme struct.
 *
 * @param filepath The path to the theme's .json file.
 * @param out_theme A pointer to a `Theme*` that will be allocated and populated on success.
 * The caller is responsible for freeing this memory with `theme_free`.
 * @return FAT_SUCCESS on success, or FAT_ERROR_THEME_LOAD on failure.
 */
FatResult theme_load(const char* filepath, Theme** out_theme);

/**
 * @brief Applies the colors of a loaded theme to the ncurses environment.
 *
 * This function calls `init_pair` for each color in the theme, setting up the
 * color pairs that the UI drawing functions will use.
 *
 * @param theme A pointer to the loaded theme to apply.
 */
void theme_apply(const Theme* theme);

/**
 * @brief Frees the memory allocated for a Theme struct.
 *
 * @param theme The theme to free. Does nothing if the theme is NULL.
 */
void theme_free(Theme* theme);

/**
 * @brief Discovers all .json theme files in a given directory.
 *
 * @param themes_dir_path The path to the directory containing theme files.
 * @param list A pointer to a StringList to be populated with the full paths of found themes.
 * @return FAT_SUCCESS on success, or FAT_ERROR_FILE_NOT_FOUND on failure.
 */
FatResult theme_discover(const char* themes_dir_path, StringList* list);

#endif // THEME_H
