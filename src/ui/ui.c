/**
 * @file ui.c
 * @author Zuhaitz (original)
 * @brief Implements all ncurses-based user interface drawing and interaction logic.
 *
 * This file is responsible for all direct interaction with the ncurses library.
 * It takes the data from the AppState struct and translates it into what the
 * user sees on the screen.
 */
#include "ui/ui.h"
#include "ui/theme.h"
#include "core/error.h"
#include "utils/utf8_utils.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

/**
 * @brief Color pair definition for window borders.
 */
#define COLOR_PAIR_BORDER          (THEME_ELEMENT_BORDER + 1)
/**
 * @brief Color pair definition for window titles.
 */
#define COLOR_PAIR_TITLE           (THEME_ELEMENT_TITLE + 1)
/**
 * @brief Color pair definition for metadata labels.
 */
#define COLOR_PAIR_METADATA_LABEL  (THEME_ELEMENT_METADATA_LABEL + 1)
/**
 * @brief Color pair definition for line numbers.
 */
#define COLOR_PAIR_LINE_NUM        (THEME_ELEMENT_LINE_NUM + 1)
/**
 * @brief Color pair definition for the status bar.
 */
#define COLOR_PAIR_STATUSBAR       (THEME_ELEMENT_STATUSBAR + 1)
/**
 * @brief Color pair definition for search highlights.
 */
#define COLOR_PAIR_SEARCH_HIGHLIGHT (THEME_ELEMENT_SEARCH_HIGHLIGHT + 1)
/**
 * @brief Color pair definition for help window borders.
 */
#define COLOR_PAIR_HELP_BORDER     (THEME_ELEMENT_HELP_BORDER + 1)
/**
 * @brief Color pair definition for help key text.
 */
#define COLOR_PAIR_HELP_KEY        (THEME_ELEMENT_HELP_KEY + 1)

// Private Helper Function Prototypes
// (Detailed info for these functions will be with their definitions)
static void draw_metadata_pane(WINDOW* win, const StringList* metadata);
static void draw_content_pane(WINDOW* win, const AppState* state);
static void draw_statusbar(const AppState *state);
static void print_segment(WINDOW* win, int y, int x, const char* text, int len,
                   bool is_active, bool is_highlight, bool is_current_match);

/**
 * @brief Calculates the number of displayable characters and corresponding bytes for a given screen width.
 *
 * This helper function iterates through a UTF-8 string to determine how many characters
 * can be displayed within a specified `max_screen_width`. It also calculates the total
 * number of bytes advanced to reach that point. This is crucial for handling variable-width
 * UTF-8 characters correctly in a fixed-width terminal display.
 *
 * @param text The input UTF-8 string.
 * @param max_screen_width The maximum number of character columns available on the screen.
 * @param bytes_advanced A pointer to an integer that will be updated with the number of bytes consumed.
 * @return The number of characters that can be displayed within the given width.
 */
static int get_display_chars_and_bytes(const char* text, int max_screen_width, int* bytes_advanced) {
    int chars_count = 0;
    int current_bytes = 0;
    const char* ptr = text;

    *bytes_advanced = 0;

    while (*ptr != '\0' && chars_count < max_screen_width) {
        int char_len = utf8_char_len(ptr);
        if (char_len == 0) { // Should not happen for valid UTF-8, but defensive
            break;
        }
        ptr += char_len;
        current_bytes += char_len;
        chars_count++;
    }
    *bytes_advanced = current_bytes;
    return chars_count;
}

/**
 * @brief Calculates the number of characters in a UTF-8 string up to a maximum number of bytes.
 *
 * This function is used to determine the character length of a substring when given a byte limit.
 * It's useful for scenarios where you need to process a specific byte-length portion of a UTF-8 string
 * and need to know how many displayable characters it contains.
 *
 * @param text The input UTF-8 string.
 * @param max_bytes The maximum number of bytes to consider.
 * @return The number of characters within the specified byte limit.
 */
static int get_char_len_from_bytes(const char* text, int max_bytes) {
    int chars_count = 0;
    int current_bytes = 0;
    const char* ptr = text;
    while (*ptr != '\0' && current_bytes < max_bytes) {
        int char_len = utf8_char_len(ptr);
        if (char_len == 0) break;
        ptr += char_len;
        current_bytes += char_len;
        chars_count++;
    }
    return chars_count;
}


// **Public API Functions**

/**
 * @brief Initializes the ncurses screen and sets up initial terminal modes.
 *
 * This function performs the necessary setup for the ncurses library,
 * including initializing the screen, setting character input modes (cbreak, noecho),
 * hiding the cursor, enabling keypad input, and starting color support.
 */
void ui_init() {
    setenv("ESCDELAY", "25", 1); // Set a shorter ESC delay for faster key recognition
    initscr();                   // Initialize the screen
    cbreak();                    // Line buffering disabled, pass everything to program
    noecho();                    // Don't echo input characters
    curs_set(0);                 // Hide the cursor
    keypad(stdscr, TRUE);        // Enable function keys (e.g., arrow keys, F1-F12)
    start_color();               // Enable color support
    refresh();                   // Refresh the screen to apply initial settings
}

/**
 * @brief Restores the terminal to its original state.
 *
 * This function cleans up the ncurses environment, reverting the terminal
 * to its state before `ui_init()` was called.
 */
void ui_destroy() {
    endwin();
}

/**
 * @brief Draws the entire application screen based on the current AppState.
 *
 * This function orchestrates the drawing of all UI components, including
 * the metadata pane, content pane, and status bar. It then calls `doupdate()`
 * to refresh the physical screen efficiently.
 *
 * @param state A read-only pointer to the current application state.
 */
void ui_draw(const AppState *state) {
    draw_metadata_pane(state->left_pane, &state->metadata);
    draw_content_pane(state->right_pane, state);
    draw_statusbar(state);
    doupdate(); // Update the physical screen with all changes
}

/**
 * @brief Handles a terminal resize event.
 *
 * This function recalculates the dimensions and positions of the sub-windows
 * (`left_pane`, `right_pane`, `status_bar`) based on the new terminal size.
 * It also clears the screen and touches all windows to ensure a full repaint
 * and prevent visual artifacts after resizing.
 *
 * @param state A pointer to the application state, which contains the window pointers.
 */
void ui_handle_resize(AppState *state) {
    int height, width;
    getmaxyx(stdscr, height, width);
    int mid = width / 3; // Divide screen into a left third and right two-thirds

    // 1 - Clear the main screen buffer to prevent artifacts.
    clear();

    // 2 - Resize and move the sub-windows to their new coordinates.
    wresize(state->left_pane, height - 1, mid);
    mvwin(state->left_pane, 0, 0);

    wresize(state->right_pane, height - 1, width - mid);
    mvwin(state->right_pane, 0, mid);

    wresize(state->status_bar, 1, width);
    mvwin(state->status_bar, height - 1, 0);

    // 3 - Tell ncurses that the main window and all its sub-windows have been
    // "touched" (modified) and need a full repaint on the next draw cycle.
    // This is the key to preventing visual glitches.
    touchwin(stdscr);
    wnoutrefresh(stdscr);
}

/**
 * @brief Continuously checks terminal size, pausing execution until it's valid.
 */
bool check_terminal_size(const AppState* state) {
    int height, width;
    getmaxyx(stdscr, height, width);

    int min_width = state->config.min_term_width;
    int min_height = state->config.min_term_height;

    if (width >= min_width && height >= min_height) {
        return true;
    }

    nodelay(stdscr, TRUE);
    while (1) {
        getmaxyx(stdscr, height, width);
        if (width >= min_width && height >= min_height) {
            nodelay(stdscr, FALSE);
            clear();
            refresh();
            return true;
        }

        clear();
        if (width >= 45) {
            char msg1[100], msg2[100];
            snprintf(msg1, sizeof(msg1), "Terminal is too small.");
            snprintf(msg2, sizeof(msg2), "Current: %dx%d, Required: %dx%d", width, height, min_width, min_height);

            mvprintw(height / 2 - 1, (width - strlen(msg1)) / 2, "%s", msg1);
            mvprintw(height / 2, (width - strlen(msg2)) / 2, "%s", msg2);
            mvprintw(height - 2, (width - strlen("Resize window or press 'q' to quit.")) / 2, "Resize window or press 'q' to quit.");
        }
        refresh();

        int ch = getch();
        if (ch == 'q' || ch == 'Q') {
            nodelay(stdscr, FALSE);
            return false;
        }
        napms(50);
    }
}


/**
 * @brief Displays a message to the user in the status bar and waits for a key press.
 *
 * This function temporarily displays a given message in the status bar,
 * typically used for informational or error messages. It then waits for
 * the user to press any key before returning, ensuring the message is seen.
 *
 * @param state A read-only pointer to the current application state, used to access the status bar window.
 * @param message The message string to display.
 */
void ui_show_message(const AppState *state, const char *message) {
    if (!state || !state->status_bar || !message) return;

    WINDOW *bar = state->status_bar;
    werase(bar); // Clear the status bar
    mvwprintw(bar, 0, 1, "MSG: %s (Press any key)", message);
    wrefresh(bar); // Refresh the status bar to show the message

    // Clear input buffer and wait for a key press
    nodelay(stdscr, TRUE);  // Make getch() non-blocking
    while(getch() != ERR);  // Consume any pending input
    nodelay(stdscr, FALSE); // Make getch() blocking again
    getch(); // Wait for user to press a key
}

/**
 * @brief Displays a mode-aware help window with relevant keybindings.
 */
void ui_show_help(const AppState* state) {
    // Determine the current mode as a string
    const char* current_mode_str = "normal";
    switch (state->view_mode) {
        case VIEW_MODE_ARCHIVE:     current_mode_str = "archive"; break;
        case VIEW_MODE_BINARY_HEX:  current_mode_str = "binary";  break;
        default:                    current_mode_str = "normal";  break;
    }

    // Create a temporary list of keybindings for the current mode
    StringList relevant_keys;
    StringList relevant_descs;
    StringList_init(&relevant_keys);
    StringList_init(&relevant_descs);

    int max_key_len = 0;

    for (int i = 0; i < ACTION_COUNT; i++) {
        const Keybinding* kb = &state->config.keybindings[i];
        bool mode_is_relevant = false;
        for (size_t j = 0; j < kb->modes.count; j++) {
            if (strcmp(kb->modes.lines[j], current_mode_str) == 0) {
                mode_is_relevant = true;
                break;
            }
        }

        if (mode_is_relevant && kb->keys.count > 0 && kb->description) {
            char keys_str[64] = {0};
            for (size_t k = 0; k < kb->keys.count; k++) {
                strcat(keys_str, kb->keys.lines[k]);
                if (k < kb->keys.count - 1) {
                    strcat(keys_str, ", ");
                }
            }
            StringList_add(&relevant_keys, keys_str);
            StringList_add(&relevant_descs, kb->description);

            int current_key_len = (int)strlen(keys_str);
            if (current_key_len > max_key_len) {
                max_key_len = current_key_len;
            }
        }
    }

    // Dynamically calculate window size
    int height, width;
    getmaxyx(stdscr, height, width);
    int help_h = (int)relevant_keys.count + 4; // 2 for borders, 2 for title/footer
    int help_w = max_key_len + 45; // 4 for padding, 1 for ':', 38 for desc, 2 for border
    if (help_h > height - 2) help_h = height - 2;
    if (help_w > width - 2) help_w = width - 2;

    int start_y = (height - help_h) / 2;
    int start_x = (width - help_w) / 2;
    WINDOW* help_win = newwin(help_h, help_w, start_y, start_x);

    wbkgd(help_win, COLOR_PAIR(COLOR_PAIR_STATUSBAR));
    wattron(help_win, COLOR_PAIR(COLOR_PAIR_HELP_BORDER));
    box(help_win, 0, 0);
    wattroff(help_win, COLOR_PAIR(COLOR_PAIR_HELP_BORDER));

    char title[64];
    snprintf(title, sizeof(title), "Keybindings (%s mode)", current_mode_str);
    wattron(help_win, A_BOLD);
    mvwprintw(help_win, 1, (help_w - (int)strlen(title)) / 2, "%s", title);
    wattroff(help_win, A_BOLD);

    for (size_t i = 0; i < relevant_keys.count; i++) {
        int line_y = (int)i + 2;
        if (line_y >= help_h - 1) break; // Don't draw past window boundary

        wattron(help_win, COLOR_PAIR(COLOR_PAIR_HELP_KEY) | A_BOLD);
        mvwprintw(help_win, line_y, 3, "%-*s", max_key_len, relevant_keys.lines[i]);
        wattroff(help_win, COLOR_PAIR(COLOR_PAIR_HELP_KEY) | A_BOLD);

        wprintw(help_win, " : %s", relevant_descs.lines[i]);
    }

    mvwprintw(help_win, help_h - 2, (help_w - 21) / 2, "Press any key to close");
    wrefresh(help_win);
    getch();
    delwin(help_win);

    StringList_free(&relevant_keys);
    StringList_free(&relevant_descs);
    touchwin(stdscr);
    wnoutrefresh(stdscr);
    doupdate();
}


/**
 * @brief Gets search input from the user via the status bar.
 *
 * This function switches the application to search input mode, displays a prompt
 * in the status bar, and captures user input for a search term. It handles
 * backspace, printable characters, and termination by Enter or Escape.
 * The cursor is made visible during input.
 *
 * @param state A pointer to the application state, where the search term will be stored.
 */
void ui_get_search_input(AppState *state) {
    WINDOW *bar = state->status_bar;
    char temp_buffer[256] = {0};
    strncpy(temp_buffer, state->search_term, sizeof(temp_buffer) - 1); // Pre-fill with existing search term

    int pos = (int)strlen(temp_buffer); // Current cursor position in the buffer

    state->mode = MODE_SEARCH_INPUT;
    ui_draw(state); // Redraw to show search input mode in status bar

    curs_set(1); // Show cursor
    keypad(bar, TRUE); // Enable keypad for the status bar window
    int ch;
    while (1) {
        mvwprintw(bar, 0, 9, "%-s", temp_buffer); // Display current buffer content
        wclrtoeol(bar); // Clear to end of line
        wmove(bar, 0, 9 + pos); // Move cursor to current input position

        ch = wgetch(bar); // Get character from status bar window
        if (ch == '\n' || ch == KEY_ENTER) break; // Enter key confirms input
        if (ch == 27) { // Escape key cancels input
            temp_buffer[0] = '\0'; // Clear search term
            break;
        }
        if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') { // Handle backspace
            if (pos > 0) { pos--; temp_buffer[pos] = '\0'; }
        } else if (isprint(ch) && (size_t)pos < (sizeof(temp_buffer) - 1)) { // Handle printable characters
            temp_buffer[pos] = (char)ch;
            pos++;
            temp_buffer[pos] = '\0';
        }
    }
    curs_set(0); // Hide cursor
    keypad(bar, FALSE); // Disable keypad for the status bar window
    state->mode = MODE_NORMAL; // Revert to normal mode
    strncpy(state->search_term, temp_buffer, sizeof(state->search_term) - 1); // Copy input to app state
    state->search_term[sizeof(state->search_term) - 1] = '\0'; // Ensure null-termination
    state->search_term_active = (state->search_term[0] != '\0');
}

/**
 * @brief Gets a shell command from the user via the status bar.
 */
void ui_get_command_input(AppState *state, char* buffer, size_t buffer_size) {
    WINDOW *bar = state->status_bar;
    buffer[0] = '\0';
    int pos = 0;

    state->mode = MODE_COMMAND_INPUT;
    ui_draw(state); // Redraw to show command input mode

    curs_set(1);
    keypad(bar, TRUE);
    int ch;
    while (1) {
        mvwprintw(bar, 0, 11, "Open with: %-s", buffer);
        wclrtoeol(bar);
        wmove(bar, 0, 11 + strlen("Open with: ") + pos);

        ch = wgetch(bar);
        if (ch == '\n' || ch == KEY_ENTER) break;
        if (ch == 27) { // Escape
            buffer[0] = '\0';
            break;
        }
        if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
            if (pos > 0) { pos--; buffer[pos] = '\0'; }
        } else if (isprint(ch) && (size_t)pos < (buffer_size - 1)) {
            buffer[pos] = (char)ch;
            pos++;
            buffer[pos] = '\0';
        }
    }
    curs_set(0);
    keypad(bar, FALSE);
    state->mode = MODE_NORMAL;
}


/**
 * @brief Gets a line number from the user via the status bar.
 *
 * This function prompts the user to enter a line number in the status bar.
 * It validates input to ensure only digits are accepted. The input can be
 * confirmed with Enter or cancelled with Escape.
 *
 * @param state A pointer to the application state, used to access the status bar.
 * @return The line number entered by the user, or -1 if cancelled or no valid number entered.
 */
int ui_get_line_input(AppState *state) {
    WINDOW *bar = state->status_bar;
    char temp_buffer[16] = {0}; // Buffer for line number input
    int pos = 0; // Current cursor position

    wbkgd(bar, COLOR_PAIR(COLOR_PAIR_STATUSBAR)); // Set background
    werase(bar); // Clear status bar
    wattron(bar, A_BOLD);
    mvwprintw(bar, 0, 1, "[GO TO LINE]"); // Display prompt
    wattroff(bar, A_BOLD);
    wrefresh(bar); // Refresh to show prompt

    curs_set(1);        // Show cursor
    keypad(bar, TRUE);  // Enable keypad for status bar
    int ch;
    while (1) {
        mvwprintw(bar, 0, 15, "%-s", temp_buffer); // Display current buffer
        wclrtoeol(bar); // Clear to end of line
        wmove(bar, 0, 15 + pos); // Move cursor

        ch = wgetch(bar); // Get character
        if (ch == '\n' || ch == KEY_ENTER) break; // Enter confirms
        if (ch == 27) { // Escape cancels
            return -1;
        }
        if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') { // Handle backspace
            if (pos > 0) { pos--; temp_buffer[pos] = '\0'; }
        } else if (isdigit(ch) && (size_t)pos < (sizeof(temp_buffer) - 1)) { // Only allow digits
            temp_buffer[pos] = (char)ch;
            pos++;
            temp_buffer[pos] = '\0';
        }
    }
    curs_set(0);        // Hide cursor
    keypad(bar, FALSE); // Disable keypad

    if (strlen(temp_buffer) > 0) {
        return atoi(temp_buffer); // Convert string to integer
    }
    return -1; // No number entered
}

/**
 * @brief Displays a theme selection menu to the user.
 *
 * This function creates a modal window that lists available themes. The user
 * can navigate the list using Up/Down arrow keys and select a theme with Enter,
 * or cancel with 'q' or Escape.
 *
 * @param state A read-only pointer to the current application state, containing theme paths.
 * @return The index of the selected theme (0-based), or -1 if cancelled or no themes found.
 */
int ui_show_theme_selector(const AppState* state) {
    if (state->theme_paths.count == 0) {
        ui_show_message(state, "No themes found in themes/ directory.");
        return -1;
    }

    int height, width; getmaxyx(stdscr, height, width);
    size_t count = state->theme_paths.count;
    // Calculate selector height, clamped to screen size
    int selector_h = (int)((count < 10) ? count + 4 : 14);
    if (selector_h > height - 4) selector_h = height - 4;
    int selector_w = 40;
    int start_y = (height - selector_h) / 2;
    int start_x = (width - selector_w) / 2;

    WINDOW* win = newwin(selector_h, selector_w, start_y, start_x);
    keypad(win, TRUE); // Enable keypad for the selector window
    wbkgd(win, COLOR_PAIR(COLOR_PAIR_STATUSBAR)); // Set background
    box(win, 0, 0); // Draw border

    mvwprintw(win, 1, (selector_w - 13) / 2, "Select Theme"); // Title

    int current_selection = 0;
    int choice = -1;
    int ch;

    while(1) {
        // Redraw options
        for (size_t i = 0; i < count; i++) {
            if ((int)i + 2 >= selector_h - 1) break; // Don't print outside window
            if ((int)i == current_selection) wattron(win, A_REVERSE); // Highlight selected item

            const char* basename = strrchr(state->theme_paths.lines[i], '/'); // Get filename from path
            basename = basename ? basename + 1 : state->theme_paths.lines[i];
            mvwprintw(win, (int)i + 2, 2, "%.*s", selector_w - 4, basename); // Print theme name

            if ((int)i == current_selection) wattroff(win, A_REVERSE); // Remove highlight
        }
        wrefresh(win); // Refresh selector window

        ch = wgetch(win); // Get user input
        switch(ch) {
            case KEY_UP: current_selection = (current_selection - 1 + (int)count) % (int)count; break;
            case KEY_DOWN: current_selection = (current_selection + 1) % (int)count; break;
            case '\n': case KEY_ENTER: choice = current_selection; goto end_loop; // Select and exit
            case 'q': case 27: choice = -1; goto end_loop; // Cancel and exit
        }
    }

end_loop:
    delwin(win);          // Delete the selector window
    touchwin(stdscr);     // Mark main window as changed
    wnoutrefresh(stdscr); // Refresh main window
    doupdate();           // Update physical screen
    return choice;
}

/**
 * @brief Helper to print a segment of a line with various attributes.
 *
 * This function is a low-level helper for `draw_content_pane`. It prints a specified
 * number of characters from a given text string to an ncurses window at a specific
 * coordinate. It applies visual attributes such as reverse video (for active lines),
 * search highlighting, and bold (for the current search match).
 *
 * @param win The ncurses window to print to.
 * @param y The y-coordinate (row) to start printing.
 * @param x The x-coordinate (column) to start printing.
 * @param text The string to print.
 * @param len The number of characters (not bytes) to print from the text.
 * @param is_active True if the line is the currently active line (for reverse video).
 * @param is_highlight True if the segment should be highlighted (e.g., search match).
 * @param is_current_match True if the segment is the currently selected search match (for bold).
 */
static void print_segment(WINDOW* win, int y, int x, const char* text, int len,
                   bool is_active, bool is_highlight, bool is_current_match) {
    // Ensure we don't try to print beyond the window width.
    int max_x = getmaxx(win);
    if (x >= max_x - 1 || len <= 0) return;

    if (is_active) wattron(win, A_REVERSE); // Apply reverse video for active line
    if (is_highlight) {
        wattron(win, COLOR_PAIR(COLOR_PAIR_SEARCH_HIGHLIGHT)); // Apply search highlight color
        if (is_current_match) wattron(win, A_BOLD); // Apply bold for current match
    }

    // Pass 'len' directly as the number of characters to print.
    // ncurses handles UTF-8 characters correctly with mvwaddnstr when given character count.
    mvwaddnstr(win, y, x, text, len);

    // Turn off attributes in reverse order of application
    if (is_highlight) {
        if (is_current_match) wattroff(win, A_BOLD);
        wattroff(win, COLOR_PAIR(COLOR_PAIR_SEARCH_HIGHLIGHT));
    }
    if (is_active) wattroff(win, A_REVERSE);
}


/**
 * @brief Draws the left pane with file metadata, truncating long lines.
 *
 * This function is responsible for rendering the metadata pane. It draws a border,
 * a "File Info" title, and then iterates through the `metadata` StringList.
 * Each metadata line is parsed (if it contains a ':') to separate label and value.
 * Long value strings are truncated with an ellipsis to fit within the pane's width.
 *
 * @param win The ncurses window to draw to (left pane).
 * @param metadata A read-only pointer to the StringList containing metadata entries.
 */
static void draw_metadata_pane(WINDOW* win, const StringList* metadata) {
    werase(win); // Clear the window
    wattron(win, COLOR_PAIR(COLOR_PAIR_BORDER));
    box(win, 0, 0); // Draw border
    wattroff(win, COLOR_PAIR(COLOR_PAIR_BORDER));

    wattron(win, A_BOLD | COLOR_PAIR(COLOR_PAIR_TITLE));
    mvwprintw(win, 1, 2, "File Info"); // Title
    mvwhline(win, 2, 1, ACS_HLINE, getmaxx(win) - 2); // Separator line
    wattroff(win, A_BOLD | COLOR_PAIR(COLOR_PAIR_TITLE));

    int max_w = getmaxx(win); // Max width of the window

    for (size_t i = 0; i < metadata->count; i++) {
        if ((int)i + 4 >= getmaxy(win) - 1) break; // Stop if we run out of vertical space
        char* line = metadata->lines[i];
        char* separator = strchr(line, ':'); // Look for label-value separator

        if (separator) {
            int label_len = (int)(separator - line);
            mvwprintw(win, (int)i + 4, 2, "%.*s", label_len, line); // Print label
            wprintw(win, ":");

            const char* value = separator + 1;
            while (*value == ' ') { // Skip leading spaces in value
                value++;
            }

            int start_x = 2 + label_len + 1; // Starting X for value
            int available_width = max_w - start_x - 2; // Width available for value
            if (available_width <= 0) continue;

            if ((int)strlen(value) > available_width) {
                if (available_width > 3) { // Ensure enough space for "..."
                    wprintw(win, " %.*s...", available_width - 4, value); // Truncate with ellipsis
                }
            } else {
                wprintw(win, " %s", value); // Print full value
            }

        } else { // No separator, treat as a single line
            int available_width = max_w - 4; // Width available for line
            if (available_width > 3 && (int)strlen(line) > available_width) {
                 mvwprintw(win, (int)i + 4, 2, "%.*s...", available_width - 3, line); // Truncate with ellipsis
            } else {
                 mvwprintw(win, (int)i + 4, 2, "%.*s", available_width, line); // Print full line
            }
        }
    }
    wnoutrefresh(win); // Mark window for refresh
}

/**
 * @brief Draws the status bar at the bottom of the screen.
 *
 * This function updates the content of the status bar based on the current
 * application state. It displays the current mode (Normal, Archive, Binary, Search),
 * a "WRAP" indicator if line wrapping is enabled, the current file path, and
 * the current line/entry number along with search match information if applicable.
 *
 * @param state A read-only pointer to the current application state.
 */
static void draw_statusbar(const AppState *state) {
    WINDOW *win = state->status_bar;
    int width = getmaxx(win);
    wbkgd(win, COLOR_PAIR(COLOR_PAIR_STATUSBAR)); // Set background color
    werase(win); // Clear the status bar

    wattron(win, A_BOLD);
    if (state->mode == MODE_SEARCH_INPUT) {
        mvwprintw(win, 0, 1, "[SEARCH]");
    } else if (state->mode == MODE_COMMAND_INPUT) {
        mvwprintw(win, 0, 1, "[COMMAND]");
    }
    else {
        switch (state->view_mode) {
            case VIEW_MODE_ARCHIVE: mvwprintw(win, 0, 1, "[ARCHIVE]"); break;
            case VIEW_MODE_BINARY_HEX: mvwprintw(win, 0, 1, "[BINARY]"); break;
            default: mvwprintw(win, 0, 1, "[NORMAL]"); break;
        }
    }
    wattroff(win, A_BOLD);

    if (state->line_wrap_enabled && state->view_mode == VIEW_MODE_NORMAL) {
        wattron(win, A_BOLD | A_REVERSE);
        mvwprintw(win, 0, 11, " WRAP ");
        wattroff(win, A_BOLD | A_REVERSE);
    }

    // Display file path
    mvwprintw(win, 0, 19, "%.*s", width - 40, state->filepath ? state->filepath : "");

    char right_status[64]; // Buffer for right-aligned status text
    const char* label = (state->view_mode == VIEW_MODE_ARCHIVE) ? "Entry" : "Line";

    // Format search match and line/entry info
    if (state->search_term_active && state->search_results.count > 0) {
        snprintf(right_status, sizeof(right_status), "Match %zu/%zu | %s %d/%zu",
                 state->search_results.current_match_idx + 1, state->search_results.count,
                 label, state->top_line + 1, state->content.count);
    } else {
        snprintf(right_status, sizeof(right_status), "%s %d/%zu",
                 label, state->top_line + 1, state->content.count);
    }
    mvwprintw(win, 0, width - (int)strlen(right_status) - 1, "%s", right_status); // Print right-aligned

    // Display search term if in search input mode
    if (state->mode == MODE_SEARCH_INPUT) {
        mvwprintw(win, 0, 9, "/%s", state->search_term);
    }
    wnoutrefresh(win); // Mark window for refresh
}

/**
 * @brief Draws the main content pane (right side) with UTF-8 and line-wrap awareness.
 *
 * This is the core function for displaying file content. It handles:
 * - Drawing the pane border and version string.
 * - Calculating line number width and content width.
 * - Iterating through visible lines of content.
 * - Drawing line numbers, applying reverse video for the active line.
 * - Implementing optional line wrapping for text content.
 * - Applying search highlighting for all matches and bolding the current match.
 * - Handling horizontal scrolling when line wrapping is disabled, ensuring the current
 * search match remains visible if present.
 *
 * @param win The :ncurses window for the content pane.
 * @param state A read-only pointer to the current application state, containing content, scroll, and search info.
 */
static void draw_content_pane(WINDOW* win, const AppState* state) {
    werase(win); // Clear the window
    wattron(win, COLOR_PAIR(COLOR_PAIR_BORDER));
    box(win, 0, 0); // Draw border
    wattroff(win, COLOR_PAIR(COLOR_PAIR_BORDER));

    int width = getmaxx(win);
    const char* version_str = FAT_VERSION; // Application version string
    int version_len = (int)strlen(version_str);
    if (width > version_len + 4) { // Ensure enough space for version string
        wattron(win, A_BOLD | COLOR_PAIR(COLOR_PAIR_TITLE));
        mvwprintw(win, 0, width - version_len - 2, " %s ", version_str); // Print version top-right
        wattroff(win, A_BOLD | COLOR_PAIR(COLOR_PAIR_TITLE));
    }

    int height = getmaxy(win);
    int line_num_width = 7; // Fixed width for line numbers (e.g., "12345 ")
    int content_width = width - line_num_width - 1; // Available width for content
    if (content_width < 1) { // Prevent division by zero or negative width
        wnoutrefresh(win);
        return;
    }

    int y = 1; // Starting Y coordinate for content (below border/title)
    // Loop through logical lines of content that are visible on screen
    for (int line_idx = state->top_line; line_idx < (int)state->content.count && y < height - 1; ) {
        bool is_active_line = (line_idx == state->top_line); // Check if this is the currently selected line
        const char* full_line = state->content.lines[line_idx];

        // Draw line number and initial reverse video if active line
        if (is_active_line) wattron(win, A_REVERSE); // Apply reverse video for active line
        wattron(win, COLOR_PAIR(COLOR_PAIR_LINE_NUM));
        mvwprintw(win, y, 1, "%*d ", line_num_width - 2, line_idx + 1); // Print line number
        wattroff(win, COLOR_PAIR(COLOR_PAIR_LINE_NUM));
        if (is_active_line) wattroff(win, A_REVERSE); // Turn off reverse video for line number


        if (state->line_wrap_enabled) {
            // **Line Wrapping with Search Highlighting**
            const char* current_ptr = full_line; // Pointer to the current position in the logical line
            
            do {
                if (y >= height - 1) break; // Stop if we run out of screen rows

                int screen_x_in_segment = 0; // Screen X position within the current wrapped segment
                const char* segment_start_ptr = current_ptr; // Start of the current wrapped segment
                int chars_to_take = 0; // Number of characters to take for this wrapped segment
                int bytes_advanced_for_segment = 0;

                // Calculate how many characters fit on the current screen line
                chars_to_take = get_display_chars_and_bytes(current_ptr, content_width, &bytes_advanced_for_segment);
                
                const char* segment_end_ptr = current_ptr + bytes_advanced_for_segment;

                // If the line is longer than content_width, try to find a suitable wrap point (space)
                if (*segment_end_ptr != '\0' && chars_to_take == content_width) {
                    const char* last_space_ptr = NULL;
                    int last_space_chars_count = 0;
                    
                    // Iterate backwards from the end of the potential segment to find a space
                    const char* search_back_ptr = segment_start_ptr;
                    int current_char_in_segment = 0;
                    while (search_back_ptr < segment_end_ptr) {
                        if (isspace((unsigned char)*search_back_ptr)) {
                            last_space_ptr = search_back_ptr;
                            last_space_chars_count = current_char_in_segment;
                        }
                        search_back_ptr += utf8_char_len(search_back_ptr);
                        current_char_in_segment++;
                    }

                    // If a space was found and it's not too close to the beginning (e.g., within first half)
                    // Then wrap at that space. Otherwise, just break at the character limit.
                    if (last_space_ptr != NULL && last_space_chars_count > content_width / 2) {
                        chars_to_take = last_space_chars_count;
                        current_ptr = last_space_ptr + utf8_char_len(last_space_ptr); // Start next segment after the space
                    } else {
                        // No good space, break at character limit
                        current_ptr = segment_end_ptr;
                    }
                } else {
                    current_ptr = segment_end_ptr; // End of line or fits entirely
                }

                // Now, print the segment, applying search highlighting
                const char* print_segment_ptr = segment_start_ptr;
                int chars_remaining_in_segment = chars_to_take;
                int current_screen_col = line_num_width; // Start printing content after line number

                if (state->search_term_active && state->search_results.count > 0) {
                    const char* term = state->search_term;
                    size_t term_len_bytes = strlen(term);
                    size_t term_len_chars = get_char_len_from_bytes(term, (int)term_len_bytes);

                    while (chars_remaining_in_segment > 0 && current_screen_col < width -1) {
                        const char* match_in_segment = strstr(print_segment_ptr, term);
                        size_t match_byte_offset = (match_in_segment) ? (size_t)(match_in_segment - full_line) : -1;

                        // Check if match is within the current logical segment being drawn AND if it's the current match
                        if (match_in_segment != NULL && match_in_segment < current_ptr) {
                            // Calculate pre-match characters
                            int pre_match_bytes = (int)(match_in_segment - print_segment_ptr);
                            int pre_match_chars = get_char_len_from_bytes(print_segment_ptr, pre_match_bytes);

                            if (pre_match_chars > 0) {
                                int chars_to_print_now = pre_match_chars;
                                if (current_screen_col + chars_to_print_now > width - 1) { // Check against window width
                                    chars_to_print_now = (width - 1) - current_screen_col;
                                }
                                if (chars_to_print_now > 0) {
                                    print_segment(win, y, current_screen_col, print_segment_ptr, chars_to_print_now, is_active_line, false, false);
                                    current_screen_col += chars_to_print_now;
                                }
                            }
                            
                            if (current_screen_col >= width - 1) break; // Filled screen width

                            // Print the match itself
                            bool is_current_match = (state->search_results.matches[state->search_results.current_match_idx].line_idx == (size_t)line_idx &&
                                                     state->search_results.matches[state->search_results.current_match_idx].char_idx == match_byte_offset);

                            int match_chars_to_print = (int)term_len_chars;
                            if (current_screen_col + match_chars_to_print > width - 1) { // Check against window width
                                match_chars_to_print = (width - 1) - current_screen_col;
                            }
                            if (match_chars_to_print > 0) {
                                print_segment(win, y, current_screen_col, match_in_segment, match_chars_to_print, is_active_line, true, is_current_match);
                                current_screen_col += match_chars_to_print;
                            }

                            // Advance print_segment_ptr past the match (in bytes)
                            print_segment_ptr = match_in_segment + term_len_bytes;
                            chars_remaining_in_segment -= (pre_match_chars + (int)term_len_chars);

                        } else {
                            // No more matches in this remaining segment, print the rest
                            int chars_to_print_now = chars_remaining_in_segment;
                            if (current_screen_col + chars_to_print_now > width - 1) { // Check against window width
                                chars_to_print_now = (width - 1) - current_screen_col;
                            }
                            if (chars_to_print_now > 0) {
                                print_segment(win, y, current_screen_col, print_segment_ptr, chars_to_print_now, is_active_line, false, false);
                            }
                            break; // Exit inner loop
                        }
                    }
                } else {
                    // No search or no match, just print the segment
                    print_segment(win, y, line_num_width, segment_start_ptr, chars_to_take, is_active_line, false, false);
                }

                y++; // Move to the next screen line for the next wrapped segment

                // If there's more content for this logical line, draw an indent for the next wrapped part
                if (*current_ptr != '\0' && y < height - 1) {
                    if (is_active_line) wattron(win, A_REVERSE);
                    mvwprintw(win, y, 1, "%*s", line_num_width - 1, ""); // Indent wrapped lines
                    if (is_active_line) wattroff(win, A_REVERSE);
                }

            } while (*current_ptr != '\0'); // Continue wrapping until the end of the full_line
            line_idx++; // Move to the next logical line
        } else {
            // **No Line Wrapping**
            int effective_left_char = state->left_char;

            // If this line contains the active match, ensure it is visible
            if (state->search_term_active && state->search_results.count > 0) {
                SearchMatch* current_match = &state->search_results.matches[state->search_results.current_match_idx];
                if (current_match->line_idx == (size_t)line_idx) {
                    int match_char_pos = get_char_len_from_bytes(full_line, (int)current_match->char_idx);
                    int term_char_len = get_char_len_from_bytes(state->search_term, (int)strlen(state->search_term));

                    if (match_char_pos < effective_left_char) {
                        effective_left_char = match_char_pos;
                    }
                    if (match_char_pos + term_char_len > effective_left_char + content_width) {
                        effective_left_char = match_char_pos + term_char_len - content_width;
                    }
                    if (effective_left_char < 0) effective_left_char = 0;
                }
            }

            int screen_x = 0; // Current column position on screen for content
            const char* current_print_ptr = full_line; // Pointer to the current character in the line to print
            int current_char_pos_in_line = 0; // Character offset from start of full_line

            // Skip characters that are off-screen to the left due to horizontal scrolling
            while (current_char_pos_in_line < effective_left_char && *current_print_ptr != '\0') {
                int char_len = utf8_char_len(current_print_ptr);
                current_print_ptr += char_len;
                current_char_pos_in_line++;
            }

            // Print line with highlighting
            const char* term = state->search_term;
            size_t term_len_bytes = (state->search_term_active) ? strlen(term) : 0;
            size_t term_len_chars = (state->search_term_active) ? get_char_len_from_bytes(term, (int)term_len_bytes) : 0;

            while (*current_print_ptr != '\0' && screen_x < content_width) {
                const char* match = (state->search_term_active && term_len_bytes > 0) ? strstr(current_print_ptr, term) : NULL;
                size_t match_byte_offset = (match) ? (size_t)(match - full_line) : -1;

                if (match) {
                    // Print text before the match
                    int pre_match_bytes = (int)(match - current_print_ptr);
                    int pre_match_chars = get_char_len_from_bytes(current_print_ptr, pre_match_bytes);
                    if (pre_match_chars > 0) {
                        print_segment(win, y, line_num_width + screen_x, current_print_ptr, pre_match_chars, is_active_line, false, false);
                        screen_x += pre_match_chars;
                    }

                    // Check if this is the currently selected match
                    bool is_current_match = (state->search_results.count > 0 &&
                                             state->search_results.matches[state->search_results.current_match_idx].line_idx == (size_t)line_idx &&
                                             state->search_results.matches[state->search_results.current_match_idx].char_idx == match_byte_offset);

                    print_segment(win, y, line_num_width + screen_x, match, (int)term_len_chars, is_active_line, true, is_current_match);
                    screen_x += (int)term_len_chars;
                    current_print_ptr = match + term_len_bytes;
                } else {
                    // No more matches on this line, print the rest
                    int bytes_to_advance = 0;
                    int chars_to_print = get_display_chars_and_bytes(current_print_ptr, content_width - screen_x, &bytes_to_advance);
                    print_segment(win, y, line_num_width + screen_x, current_print_ptr, chars_to_print, is_active_line, false, false);
                    break; // End of line
                }
            }

            line_idx++; // Move to the next logical line
            y++;
        }
    }
    wnoutrefresh(win); // Mark window for refresh
}
