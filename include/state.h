/**
 * @file state.h
 * @author Zuhaitz (original)
 * @brief Defines the core application state structure and its management functions.
 */
#ifndef STATE_H
#define STATE_H

#include <ncurses.h>
#include <limits.h>
#include <stdbool.h>
#include "string_list.h"
#include "theme.h"
#include "error.h"

/**
 * @struct AppConfig
 * @brief Holds settings loaded from the user's configuration file.
 */
typedef struct {
    char* default_theme_name;   /**< The name of the theme to load by default. */
} AppConfig;


/**
 * @enum AppMode
 * @brief Represents the application's current input mode.
 */
typedef enum {
    MODE_NORMAL,        /**< Default mode for navigation and viewing. */
    MODE_SEARCH_INPUT   /**< Mode for when the user is typing in the search bar. */
} AppMode;

/**
 * @enum ViewMode
 * @brief Represents the type of content currently being displayed.
 */
typedef enum {
    VIEW_MODE_NORMAL,       /**< Displaying a regular text file. */
    VIEW_MODE_ARCHIVE,      /**< Displaying the list of entries in an archive. */
    VIEW_MODE_BINARY_HEX    /**< Displaying a hex dump of a binary file. */
} ViewMode;

/**
 * @struct AppState
 * @brief The central data structure holding the entire application state.
 */
typedef struct {
    // --- UI Pointers (managed by main.c and ui.c) ---
    WINDOW *left_pane;      /**< ncurses window for file metadata. */
    WINDOW *right_pane;     /**< ncurses window for file content. */
    WINDOW *status_bar;     /**< ncurses window for the status bar. */

    // --- View-Specific Data (managed by state.c) ---
    StringList metadata;    /**< Metadata for the current file (for left pane). */
    StringList content;     /**< Content of the current file/archive (for right pane). */
    char *filepath;         /**< The path to the currently loaded file. */
    size_t max_line_len;    /**< The length of the longest line in the current content. */

    // --- View State ---
    int top_line;           /**< The index of the content line at the top of the right pane. */
    int left_char;          /**< The index of the character at the left of the right pane (for horizontal scrolling). */
    ViewMode view_mode;     /**< The current view mode (Normal, Archive, Hex). */
    AppMode mode;           /**< The current input mode (Normal, Search). */
    Theme *theme;           /**< The currently active theme. */
    bool line_wrap_enabled; /**< Flag for whether line wrapping is active. */

    // --- App-Lifetime Data ---
    StringList theme_paths;             /**< A list of full paths to all discovered themes. */
    char themes_dir_path[PATH_MAX];     /**< The path to the themes directory. */
    StringList breadcrumbs;             /**< Navigation history (a stack of file paths). */
    AppConfig config;                   /**< Holds user-defined configuration settings. */

    // --- Search State ---
    char search_term[256];              /**< The current search term entered by the user. */
    bool search_term_active;            /**< True if a search term is currently active. */
    int current_search_line_idx;        /**< The line index of the currently highlighted search match. */
    int current_search_char_idx;        /**< The character offset within the line of the current match. */
    int search_direction;               /**< 1 for forward search, -1 for backward search. */
    bool search_wrapped;                /**< True if search wrapped around the file. */

} AppState;

/**
 * @brief Initializes or re-initializes the application state for a given file.
 * @param state A pointer to the application state to modify.
 * @param filepath The path to the new file to load.
 * @return FAT_SUCCESS on success, or an error code on failure.
 */
FatResult state_init(AppState *state, const char *filepath);

/**
 * @brief Frees resources associated with the current view only.
 * @param state The application state.
 */
void state_destroy_view(AppState *state);

/**
 * @brief Frees all resources for the entire application lifetime before exit.
 * @param state The application state.
 */
void full_app_reset(AppState* state);

#endif //STATE_H
