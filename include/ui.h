/**
 * @file ui.h
 * @author Zuhaitz (original)
 * @brief Defines the interface for all user interface (UI) operations.
 *
 * This header declares the functions responsible for initializing and tearing down
 * the ncurses environment, drawing all UI components (panes, status bar), and
 * handling user-facing interactions like the help screen and search input.
 */
#ifndef UI_H
#define UI_H

#include "state.h"

/**
 * @brief Initializes the ncurses screen and sets up initial terminal modes.
 *
 * This must be called once at the very beginning of the application. It sets
 * up the terminal for cbreak mode, disables echo, enables the keypad, etc.
 */
void ui_init();

/**
 * @brief Restores the terminal to its original state and cleans up ncurses.
 *
 * This must be called once at the very end of the application to ensure the
 * terminal is usable after the program exits.
 */
void ui_destroy();

/**
 * @brief Draws the entire application screen based on the current AppState.
 *
 * This is the main drawing function. It calls helper functions to draw the
 * metadata pane, the content pane, and the status bar.
 *
 * @param state A read-only pointer to the current application state.
 */
void ui_draw(const AppState *state);

/**
 * @brief Handles a terminal resize event.
 *
 * This function recalculates the dimensions of all ncurses windows and redraws
 * the screen to fit the new terminal size.
 *
 * @param state A pointer to the application state, which will be updated with
 * the new window dimensions.
 */
void ui_handle_resize(AppState *state);

/**
 * @brief Displays the help window with keybindings.
 *
 * This function creates a new window in the center of the screen, displays
 * the relevant keybindings for the current view mode, and waits for a key

 * press before closing.
 *
 * @param state A read-only pointer to the current application state, used to
 * determine which set of keybindings to show.
 */
void ui_show_help(const AppState* state);


/**
 * @brief Gets search input from the user via the status bar.
 *
 * This function takes control of the input loop to allow the user to type
 * a search term directly into the status bar.
 *
 * @param state A pointer to the application state, where the entered search
 * term will be stored.
 */
void ui_get_search_input(AppState *state);

/**
 * @brief Gets a line number from the user via the status bar.
 *
 * This function takes control of the input loop to allow the user to type
 * a line number to jump to.
 *
 * @param state A pointer to the application state.
 * @return The line number entered by the user, or -1 if cancelled.
 */
int ui_get_line_input(AppState *state);


/**
 * @brief Displays a message to the user in the status bar.
 *
 * This is used for showing error messages or other notifications. It waits for
 * a key press before returning, allowing the user to read the message.
 *
 * @param state A read-only pointer to the current application state.
 * @param message The message string to display.
 */
void ui_show_message(const AppState *state, const char *message);

/**
 * @brief Displays a theme selection menu to the user.
 *
 * Creates a new window in the center of the screen listing all available themes
 * and allows the user to select one with the arrow keys.
 *
 * @param state A read-only pointer to the current application state, used to get
 * the list of available theme paths.
 * @return The index of the selected theme in the `state->theme_paths` list,
 * or -1 if the user cancelled the selection.
 */
int ui_show_theme_selector(const AppState* state);

#endif //UI_H
