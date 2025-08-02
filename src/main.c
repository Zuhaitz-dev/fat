/**
 * @file main.c
 * @author Zuhaitz (original)
 * @brief The main entry point and event loop for the FAT TUI application.
 */
#include "state.h"
#include "ui.h"
#include "plugin_manager.h"
#include "logger.h"
#include "error.h"
#include "utf8_utils.h"
#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

// **Application Constants**
#define MIN_TERM_WIDTH 80
#define MIN_TERM_HEIGHT 20


// **Forward Declarations**
static FatResult process_input(AppState *state, int ch);
static FatResult find_next_search_match(AppState *state);
static FatResult find_prev_search_match(AppState *state);
static void cleanup_temp_file_if_exists(const char* path);
static bool check_terminal_size(void);

/**
 * @brief The main entry point of the application.
 */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    logger_init("fat_log.txt");
    LOG_INFO("Application starting.");
    setlocale(LC_ALL, "");

    ui_init();

    if (!check_terminal_size()) {
        ui_destroy();
        logger_destroy();
        return 0;
    }

    AppState state = {0};

    int height, width;
    getmaxyx(stdscr, height, width);
    int mid = width / 3;
    state.left_pane = newwin(height - 1, mid, 0, 0);
    state.right_pane = newwin(height - 1, width - mid, 0, mid);
    state.status_bar = newwin(1, width, height - 1, 0);

    if (!state.left_pane || !state.right_pane || !state.status_bar) {
        ui_destroy();
        LOG_INFO("FATAL: Failed to create ncurses windows.");
        fprintf(stderr, "Failed to create ncurses windows.\n");
        logger_destroy();
        return 1;
    }

    FatResult res = state_init(&state, argv[1]);
    if (res != FAT_SUCCESS) {
        ui_destroy();
        LOG_INFO("FATAL: Initial state setup failed with code %d.", res);
        fprintf(stderr, "Initialization failed: %s\n", fat_result_to_string(res));
        logger_destroy();
        return 1;
    }

    // To avoid possible warnings that break the UI. 
    // To see the warnings, you can simply comment these two lines.
    clear();
    refresh();
    ui_draw(&state);

    int ch;
    while ((ch = getch()) != 'q') {
        if (!check_terminal_size()) {
            break;
        }

        if (ch == KEY_RESIZE) {
            ui_handle_resize(&state);
            ui_draw(&state);
            continue;
        }

        res = process_input(&state, ch);
        if (res != FAT_SUCCESS) {
            ui_show_message(&state, fat_result_to_string(res));
        }
        ui_draw(&state);
    }

    full_app_reset(&state);
    ui_destroy();
    LOG_INFO("Application shutting down cleanly.");
    logger_destroy();
    return 0;
}

/**
 * @brief Continuously checks terminal size, pausing execution until it's valid.
 */
static bool check_terminal_size(void) {
    int height, width;
    getmaxyx(stdscr, height, width);

    if (width >= MIN_TERM_WIDTH && height >= MIN_TERM_HEIGHT) {
        return true;
    }

    nodelay(stdscr, TRUE);
    while (1) {
        getmaxyx(stdscr, height, width);
        if (width >= MIN_TERM_WIDTH && height >= MIN_TERM_HEIGHT) {
            nodelay(stdscr, FALSE);
            clear();
            refresh();
            return true;
        }

        clear();
        if (width >= 45) {
            char msg1[100], msg2[100];
            snprintf(msg1, sizeof(msg1), "Terminal is too small.");
            snprintf(msg2, sizeof(msg2), "Current: %dx%d, Required: %dx%d", width, height, MIN_TERM_WIDTH, MIN_TERM_HEIGHT);

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
 * @brief Cleans up a temporary file if its path indicates it was created by FAT.
 */
static void cleanup_temp_file_if_exists(const char* path) {
    if (!path) return;

    char temp_file_prefix[PATH_MAX];
#ifdef _WIN32
    char temp_dir[MAX_PATH];
    GetTempPathA(MAX_PATH, temp_dir);
    snprintf(temp_file_prefix, sizeof(temp_file_prefix), "%s\\fat-", temp_dir);
#else
    snprintf(temp_file_prefix, sizeof(temp_file_prefix), "/tmp/fat-"); // <-- REMOVED EXTRA ARG
#endif

    if (strncmp(path, temp_file_prefix, strlen(temp_file_prefix)) == 0) {
        remove(path);
    }
}


/**
 * @brief Processes a single character of user input and updates the state.
 */
static FatResult process_input(AppState *state, int ch) {
    int page_size = getmaxy(state->right_pane) - 2;
    if (page_size < 1) page_size = 1;
    FatResult res = FAT_SUCCESS;

    if (ch == KEY_F(2)) {
        int selected_idx = ui_show_theme_selector(state);
        if (selected_idx != -1) {
            const char* new_theme_path = state->theme_paths.lines[selected_idx];
            theme_free(state->theme);
            state->theme = NULL;
            res = theme_load(new_theme_path, &state->theme);
            if (res == FAT_SUCCESS && state->theme) {
                theme_apply(state->theme);
            }
        }
        return res;
    }
    if (ch == '?') {
        ui_show_help(state);
        return FAT_SUCCESS;
    }
    // Update 'o' key to go to a line, and add 'G' for end and 'gg' for beginning.
    if (ch == 'o') {
        int target_line = ui_get_line_input(state);
        if (target_line > 0) {
            state->top_line = target_line - 1;
            if (state->top_line >= (int)state->content.count) {
                state->top_line = state->content.count > 0 ? (int)state->content.count - 1 : 0;
            }
        }
        return FAT_SUCCESS;
    }

    if (ch == 'g') {
        int next_ch = getch(); // Read next key
        if (next_ch == 'g') { // 'gg'
            state->top_line = 0;
            state->left_char = 0;
            return FAT_SUCCESS;
        }
        return FAT_SUCCESS;
    }
    // New 'G' keybinding to go to the end
    if (ch == 'G') {
        state->top_line = state->content.count > 0 ? (int)state->content.count - 1 : 0;
        state->left_char = 0;
        return FAT_SUCCESS;
    }

    switch (state->view_mode) {
        case VIEW_MODE_ARCHIVE:
            switch (ch) {
                case KEY_DOWN:
                case 'j':
                    if (state->top_line + 1 < (int)state->content.count) state->top_line++; break;
                case KEY_UP:
                case 'k':
                    if (state->top_line > 0) state->top_line--; break;
                case KEY_NPAGE:
                    state->top_line += page_size;
                    if (state->top_line >= (int)state->content.count) state->top_line = state->content.count > 0 ? (int)state->content.count - 1 : 0;
                    break;
                case KEY_PPAGE:
                    state->top_line -= page_size;
                    if (state->top_line < 0) state->top_line = 0;
                    break;
                case KEY_HOME: state->top_line = 0; break;
                case KEY_END: state->top_line = state->content.count > 0 ? (int)state->content.count - 1 : 0; break;

                case '\n':
                case KEY_ENTER: {
                    if (state->content.count == 0) break;
                    state->search_term_active = false;
                    const char* entry_name = state->content.lines[state->top_line];
                    const ArchivePlugin* handler = pm_get_handler(state->filepath);
                    char* temp_file_path = NULL;
                    if (handler) {
                        res = handler->extract_entry(state->filepath, entry_name, &temp_file_path);
                        if (res == FAT_SUCCESS && temp_file_path) {
                            res = state_init(state, temp_file_path);
                            free(temp_file_path);
                        }
                    }
                    return res;
                }
                case 27:
                    if (state->breadcrumbs.count > 1) {
                        cleanup_temp_file_if_exists(state->breadcrumbs.lines[state->breadcrumbs.count - 1]);
                        state->breadcrumbs.count--;
                        return state_init(state, state->breadcrumbs.lines[state->breadcrumbs.count - 1]);
                    }
                    break;
            }
            break;

        case VIEW_MODE_BINARY_HEX:
            {
                int visible_content_width = getmaxx(state->right_pane) - 7 - 1;
                int max_scroll_limit = (int)state->max_line_len - visible_content_width;
                if (max_scroll_limit < 0) max_scroll_limit = 0;

                switch(ch) {
                    case KEY_DOWN:
                    case 'j': if (state->top_line + 1 < (int)state->content.count) state->top_line++; break;
                    case KEY_UP:
                    case 'k': if (state->top_line > 0) state->top_line--; break;
                    case KEY_RIGHT:
                    case 'l':
                        if (state->left_char < max_scroll_limit) state->left_char++;
                        break;
                    case KEY_LEFT:
                    case 'h':
                        if (state->left_char > 0) state->left_char--;
                        break;
                    case KEY_NPAGE:
                        state->top_line += page_size;
                        if ((size_t)(state->top_line) >= state->content.count) {
                             state->top_line = state->content.count > 0 ? (int)state->content.count - 1 : 0;
                        }
                        break;
                    case KEY_PPAGE:
                        state->top_line -= page_size;
                        if (state->top_line < 0) state->top_line = 0;
                        break;
                    case KEY_HOME: state->top_line = 0; state->left_char = 0; break;
                    case KEY_END:
                         state->top_line = state->content.count > 0 ? (int)state->content.count - 1 : 0;
                         state->left_char = 0;
                         break;
                    case 27:
                        if (state->breadcrumbs.count > 1) {
                            cleanup_temp_file_if_exists(state->breadcrumbs.lines[state->breadcrumbs.count - 1]);
                            state->breadcrumbs.count--;
                            return state_init(state, state->breadcrumbs.lines[state->breadcrumbs.count - 1]);
                        }
                        break;
                    default:
                         break;
                }
            }
            break;

        case VIEW_MODE_NORMAL:
            {
                int visible_content_width = getmaxx(state->right_pane) - 7 - 1;
                int max_scroll_limit = (int)state->max_line_len - visible_content_width;
                if (max_scroll_limit < 0) max_scroll_limit = 0;

                switch(ch) {
                    case KEY_DOWN:
                    case 'j': if (state->top_line + 1 < (int)state->content.count) state->top_line++; break;
                    case KEY_UP:
                    case 'k': if (state->top_line > 0) state->top_line--; break;
                    case KEY_RIGHT:
                    case 'l':
                        if (!state->line_wrap_enabled && state->left_char < max_scroll_limit) {
                            const char *line = state->content.lines[state->top_line];
                            if (state->left_char < (int)strlen(line)) {
                                state->left_char += utf8_char_len(&line[state->left_char]);
                            }
                        }
                        break;
                    case KEY_LEFT:
                    case 'h':
                        if (!state->line_wrap_enabled && state->left_char > 0) {
                            const char *line = state->content.lines[state->top_line];
                            state->left_char = utf8_prev_char_start(line, state->left_char);
                        }
                        break;
                    case KEY_NPAGE:
                        state->top_line += page_size;
                        if ((size_t)(state->top_line) >= state->content.count) {
                             state->top_line = state->content.count > 0 ? (int)state->content.count - 1 : 0;
                        }
                        break;
                    case KEY_PPAGE:
                        state->top_line -= page_size;
                        if (state->top_line < 0) state->top_line = 0;
                        break;
                    case KEY_HOME: state->top_line = 0; state->left_char = 0; break;
                    case KEY_END:
                         state->top_line = state->content.count > 0 ? (int)state->content.count - 1 : 0;
                         state->left_char = 0;
                         break;

                    case '/':
                        state->search_term_active = false;
                        ui_get_search_input(state);
                        state->search_term_active = (state->search_term[0] != '\0');
                        state->search_direction = 1;
                        state->search_wrapped = false;
                        state->current_search_line_idx = state->top_line;
                        state->current_search_char_idx = state->left_char;
                        res = find_next_search_match(state);
                        break;
                    case 'n':
                        if (state->search_term_active) {
                            state->search_direction = 1;
                            res = find_next_search_match(state);
                        } else {
                            res = FAT_ERROR_UNSUPPORTED;
                        }
                        if (res == FAT_ERROR_FILE_NOT_FOUND && state->search_wrapped) {
                            ui_show_message(state, "No more matches found (wrapped around).");
                            res = FAT_SUCCESS;
                        } else if (res == FAT_ERROR_FILE_NOT_FOUND) {
                            ui_show_message(state, "No matches found.");
                            res = FAT_SUCCESS;
                        }

                        break;
                    case 'N':
                        if (state->search_term_active) {
                            state->search_direction = -1;
                            res = find_prev_search_match(state);
                        } else {
                            res = FAT_ERROR_UNSUPPORTED;
                        }
                        if (res == FAT_ERROR_FILE_NOT_FOUND && state->search_wrapped) {
                            ui_show_message(state, "No more matches found (wrapped around).");
                            res = FAT_SUCCESS;
                        } else if (res == FAT_ERROR_FILE_NOT_FOUND) {
                            ui_show_message(state, "No matches found.");
                            res = FAT_SUCCESS;
                        }
                        break;
                    case 'w':
                        if (state->view_mode == VIEW_MODE_NORMAL) {
                            state->line_wrap_enabled = !state->line_wrap_enabled;
                        }
                        break;
                    case 27:
                        if (state->breadcrumbs.count > 1) {
                            cleanup_temp_file_if_exists(state->breadcrumbs.lines[state->breadcrumbs.count - 1]);
                            state->breadcrumbs.count--;
                            return state_init(state, state->breadcrumbs.lines[state->breadcrumbs.count - 1]);
                        } else if (state->search_term_active) {
                            state->search_term[0] = '\0';
                            state->search_term_active = false;
                            state->current_search_line_idx = -1;
                            state->current_search_char_idx = -1;
                            state->search_wrapped = false;
                            return FAT_SUCCESS;
                        }
                        break;
                }
            }
            break;
    }
    return FAT_SUCCESS;
}

/**
 * @brief Finds the next occurrence of the search term in the content.
 */
static FatResult find_next_search_match(AppState *state) {
    if (!state->search_term_active || state->search_term[0] == '\0') return FAT_ERROR_UNSUPPORTED;

    int start_line = state->current_search_line_idx;
    int start_char = state->current_search_char_idx + (state->current_search_char_idx != -1 ? (int)strlen(state->search_term) : 0);

    size_t content_count = state->content.count;

    for (int i = start_line; i < (int)content_count; ++i) {
        const char* line = state->content.lines[i];
        const char* search_start_ptr = (i == start_line && start_char != -1) ? line + start_char : line;

        if (search_start_ptr > line + strlen(line)) {
            search_start_ptr = line;
        }

        const char* match = strstr(search_start_ptr, state->search_term);
        if (match) {
            state->current_search_line_idx = i;
            state->current_search_char_idx = (int)(match - line);
            state->top_line = i;
            state->search_wrapped = false;
            return FAT_SUCCESS;
        }
        start_char = 0;
    }

    if (!state->search_wrapped) {
        state->search_wrapped = true;
        state->current_search_line_idx = 0;
        state->current_search_char_idx = -1;
        LOG_INFO("Search wrapped to beginning.");
        return find_next_search_match(state);
    }

    return FAT_ERROR_FILE_NOT_FOUND;
}

/**
 * @brief Finds the previous occurrence of the search term in the content.
 */
static FatResult find_prev_search_match(AppState *state) {
    if (!state->search_term_active || state->search_term[0] == '\0') return FAT_ERROR_UNSUPPORTED;

    int start_line = state->current_search_line_idx;
    int start_char = state->current_search_char_idx;

    if (start_line == -1 || start_char == -1) {
        start_line = (int)state->content.count - 1;
        start_char = (int)strlen(state->content.lines[start_line]);
    }

    for (int i = start_line; i >= 0; --i) {
        const char* line = state->content.lines[i];
        int current_len = (int)strlen(line);
        int search_end_char = (i == start_line) ? start_char : current_len;

        if (search_end_char > current_len) search_end_char = current_len;

        for (int j = search_end_char - 1; j >= 0; --j) {
            if (j + strlen(state->search_term) <= (size_t)current_len &&
                strncmp(line + j, state->search_term, strlen(state->search_term)) == 0) {

                state->current_search_line_idx = i;
                state->current_search_char_idx = j;
                state->top_line = i;
                state->search_wrapped = false;
                return FAT_SUCCESS;
            }
        }
        start_char = -1;
    }

    if (!state->search_wrapped) {
        state->search_wrapped = true;
        state->current_search_line_idx = (int)state->content.count - 1;
        state->current_search_char_idx = (int)strlen(state->content.lines[state->current_search_line_idx]);
        LOG_INFO("Search wrapped to end.");
        return find_prev_search_match(state);
    }

    return FAT_ERROR_FILE_NOT_FOUND;
}
