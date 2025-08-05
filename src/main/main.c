/**
 * @file main.c
 * @author Zuhaitz (original)
 * @brief The main entry point and event loop for the FAT TUI application.
 */
#include "core/state.h"
#include "ui/ui.h"
#include "plugins/plugin_manager.h"
#include "utils/logger.h"
#include "core/error.h"
#include "utils/utf8_utils.h"
#include "core/config.h"
#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#ifdef _WIN32
#include <windows.h>
#endif

// **Application Constants**
#define MIN_TERM_WIDTH 80
#define MIN_TERM_HEIGHT 20


// **Forward Declarations**
static void print_help(const char* executable_name);
static FatResult process_input(AppState *state, int ch);
static void cleanup_temp_file_if_exists(const char* path);
static bool check_terminal_size(void);

/**
 * @brief Prints the help message to the console and exits.
 */
static void print_help(const char* executable_name) {
    printf("FAT (File & Archive Tool) %s\n", FAT_VERSION);
    printf("A TUI file and archive viewer for your terminal.\n\n");
    printf("USAGE:\n");
    printf("  %s [OPTIONS] <FILE>\n\n", executable_name);
    printf("OPTIONS:\n");
    printf("  --force-text    Force the file to be opened in text mode.\n");
    printf("  --force-hex     Force the file to be opened in hex mode.\n");
    printf("  -h, --help      Show this help message and exit.\n");
}


/**
 * @brief The main entry point of the application.
 */
int main(int argc, char *argv[]) {
    ForceViewMode force_mode = FORCE_VIEW_NONE;
    char* filepath = NULL;

        // **Argument Parsing Logic**
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--force-text") == 0) {
            force_mode = FORCE_VIEW_TEXT;
        } else if (strcmp(argv[i], "--force-hex") == 0) {
            force_mode = FORCE_VIEW_HEX;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_help(argv[0]);
            return 1;
        } else {
            if (filepath != NULL) {
                fprintf(stderr, "Error: Multiple files specified. Only one file can be opened at a time.\n");
                return 1;
            }
            filepath = argv[i];
        }
    }

    if (filepath == NULL) {
        fprintf(stderr, "Usage: %s [OPTIONS] <FILE>\n", argv[0]);
        return 1;
    }

    char config_dir[PATH_MAX];
    char log_path[PATH_MAX];
    if (get_config_dir(config_dir, sizeof(config_dir)) == 0) {
        snprintf(log_path, sizeof(log_path), "%s/fat.log", config_dir);
    } else {
        snprintf(log_path, sizeof(log_path), "fat.log");
    }

    logger_init(log_path);
    LOG_INFO("Application starting.");
    setlocale(LC_ALL, "");

    ui_init();

    if (!check_terminal_size()) {
        ui_destroy();
        logger_destroy();
        return 0;
    }

    AppState state = {0};
    state.force_view_mode = force_mode; // Pass the forced mode to the state

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

    FatResult res = state_init(&state, filepath);
    if (res != FAT_SUCCESS) {
        ui_destroy();
        LOG_INFO("FATAL: Initial state setup failed with code %d.", res);
        fprintf(stderr, "Initialization failed: %s\n", fat_result_to_string(res));
        logger_destroy();
        return 1;
    }

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
    snprintf(temp_file_prefix, sizeof(temp_file_prefix), "/tmp/fat-");
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
        int next_ch = getch();
        if (next_ch == 'g') {
            state->top_line = 0;
            state->left_char = 0;
            return FAT_SUCCESS;
        }
        return FAT_SUCCESS;
    }
    if (ch == 'G') {
        state->top_line = state->content.count > 0 ? (int)state->content.count - 1 : 0;
        state->left_char = 0;
        return FAT_SUCCESS;
    }
    
    if (ch == 'O') {
        char command_buffer[512];
        ui_get_command_input(state, command_buffer, sizeof(command_buffer));
        if (command_buffer[0] != '\0') {
            def_prog_mode();
            endwin();
            
            char full_command[1024];
            snprintf(full_command, sizeof(full_command), "%s \"%s\"", command_buffer, state->filepath);
            (void)system(full_command); // Execute the command
            
            reset_prog_mode();
            refresh();

            if (state->view_mode != VIEW_MODE_ARCHIVE) {
                state_reload_content(state, state->view_mode);
            }
        }
        return FAT_SUCCESS;
    }


    switch (state->view_mode) {
        case VIEW_MODE_ARCHIVE:
             switch (ch) {
                case KEY_DOWN:
                case 'j':
                    if (state->top_line + 1 < (int)state->content.count) {
                        state->top_line++;
                    }
                    break;
                case KEY_UP:
                case 'k':
                    if (state->top_line > 0) {
                        state->top_line--;
                    }
                    break;
                case KEY_NPAGE:
                    state->top_line += page_size;
                    if (state->top_line >= (int)state->content.count) {
                        state->top_line = state->content.count > 0 ? (int)state->content.count - 1 : 0;
                    }
                    break;
                case KEY_PPAGE:
                    state->top_line -= page_size;
                    if (state->top_line < 0) {
                        state->top_line = 0;
                    }
                    break;
                case KEY_HOME:
                    state->top_line = 0;
                    break;
                case KEY_END:
                    state->top_line = state->content.count > 0 ? (int)state->content.count - 1 : 0;
                    break;
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
        case VIEW_MODE_NORMAL:
            {
                int visible_content_width = getmaxx(state->right_pane) - 7 - 1;
                int max_scroll_limit = (int)state->max_line_len - visible_content_width;
                if (max_scroll_limit < 0) max_scroll_limit = 0;

                switch(ch) {
                    case 't':
                        if (state->view_mode == VIEW_MODE_NORMAL) {
                            return state_reload_content(state, VIEW_MODE_BINARY_HEX);
                        } else if (state->view_mode == VIEW_MODE_BINARY_HEX) {
                            return state_reload_content(state, VIEW_MODE_NORMAL);
                        }
                        break;
                    case KEY_DOWN:
                    case 'j':
                        if (state->top_line + 1 < (int)state->content.count) state->top_line++;
                        break;
                    case KEY_UP:
                    case 'k':
                        if (state->top_line > 0) state->top_line--;
                        break;
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
                    case KEY_HOME:
                        state->top_line = 0;
                        state->left_char = 0;
                        break;
                    case KEY_END:
                         state->top_line = state->content.count > 0 ? (int)state->content.count - 1 : 0;
                         state->left_char = 0;
                         break;

                    case '/':
                        ui_get_search_input(state); // This function now handles everything
                        if (state->search_term_active) {
                           res = state_perform_search(state);
                           if (res == FAT_ERROR_FILE_NOT_FOUND) {
                               ui_show_message(state, "No matches found.");
                               state->search_term_active = false;
                               res = FAT_SUCCESS;
                           }
                        }
                        break;

                    case 'n':
                        if (state->search_term_active && state->search_results.count > 0) {
                            state->search_results.current_match_idx = (state->search_results.current_match_idx + 1) % state->search_results.count;
                            SearchMatch* match = &state->search_results.matches[state->search_results.current_match_idx];
                            state->top_line = (int)match->line_idx;
                        } else {
                            res = FAT_ERROR_UNSUPPORTED; // No active search
                        }
                        break;

                    case 'N':
                        if (state->search_term_active && state->search_results.count > 0) {
                            if (state->search_results.current_match_idx == 0) {
                                state->search_results.current_match_idx = state->search_results.count - 1;
                            } else {
                                state->search_results.current_match_idx--;
                            }
                            SearchMatch* match = &state->search_results.matches[state->search_results.current_match_idx];
                            state->top_line = (int)match->line_idx;
                        } else {
                            res = FAT_ERROR_UNSUPPORTED; // No active search
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
                            free(state->search_results.matches);
                            state->search_results.matches = NULL;
                            state->search_results.count = 0;
                            state->search_results.capacity = 0;
                            return FAT_SUCCESS;
                        }
                        break;
                }
            }
            break;
    }
    return FAT_SUCCESS;
}
