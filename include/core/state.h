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
#include "ui/theme.h"
#include "core/error.h"

/**
 * @struct AppConfig
 * @brief Holds settings loaded from the user's configuration file.
 */
typedef struct {
    char* default_theme_name;   /**< The name of the theme to load by default. */
    StringList text_mimes;      /**< List of MIME types to always treat as text. */
    StringList binary_mimes;    /**< List of MIME types to always treat as binary. */
} AppConfig;


/**
 * @enum AppMode
 * @brief Represents the application's current input mode.
 */
typedef enum {
    MODE_NORMAL,        /**< Default mode for navigation and viewing. */
    MODE_SEARCH_INPUT,  /**< Mode for when the user is typing in the search bar. */
    MODE_COMMAND_INPUT  /**< Mode for when the user is typing in the command bar. */
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
 * @enum ForceViewMode
 * @brief Represents if the user has forced a specific view mode from the command line.
 */
typedef enum {
    FORCE_VIEW_NONE,
    FORCE_VIEW_TEXT,
    FORCE_VIEW_HEX
} ForceViewMode;

/**
 * @struct SearchMatch
 * @brief Stores the location of a single search match.
 */
typedef struct {
    size_t line_idx;    /**< The line number of the match. */
    size_t char_idx;    /**< The byte offset of the match within the line. */
} SearchMatch;

/**
 * @struct SearchMatchList
 * @brief A dynamic array of SearchMatch structs.
 */
typedef struct {
    SearchMatch* matches;       /**< The array of matches. */
    size_t count;               /**< The number of matches found. */
    size_t capacity;            /**< The allocated capacity of the array. */
    size_t current_match_idx;   /**< The index of the currently active match. */
} SearchMatchList;

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
    AppMode mode;           /**< The current input mode (Normal, Search, Command). */
    Theme *theme;           /**< The currently active theme. */
    bool line_wrap_enabled; /**< Flag for whether line wrapping is active. */

    // --- App-Lifetime Data ---
    ForceViewMode force_view_mode;      /**< A flag to force a view mode, set at startup. */
    StringList theme_paths;             /**< A list of full paths to all discovered themes. */
    char themes_dir_path[PATH_MAX];     /**< The path to the themes directory. */
    StringList breadcrumbs;             /**< Navigation history (a stack of file paths). */
    AppConfig config;                   /**< Holds user-defined configuration settings. */

    // --- Search State ---
    char search_term[256];              /**< The current search term entered by the user. */
    bool search_term_active;            /**< True if a search term is currently active. */
    SearchMatchList search_results;     /**< A list of all found matches for the current term. */

} AppState;

/**
 * @brief Initializes or re-initializes the application state for a given file.
 * @param state A pointer to the application state to modify.
 * @param filepath The path to the new file to load.
 * @return FAT_SUCCESS on success, or an error code on failure.
 */
FatResult state_init(AppState *state, const char *filepath);

/**
 * @brief Reloads the content for the current file in a new view mode.
 * @param state A pointer to the application state to modify.
 * @param new_mode The view mode to switch to.
 * @return FAT_SUCCESS on success, or an error code on failure.
 */
FatResult state_reload_content(AppState *state, ViewMode new_mode);

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

/**
 * @brief Performs a search for the current search term and populates the search_results list.
 * @param state A pointer to the application state.
 * @return FAT_SUCCESS if matches are found, FAT_ERROR_FILE_NOT_FOUND if no matches are found.
 */
FatResult state_perform_search(AppState *state);

#endif //STATE_H
