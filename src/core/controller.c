/**
 * @file controller.c
 * @author Zuhaitz (original)
 * @brief Implements the core application setup and event loop.
 */
#include "core/controller.h"
#include "core/state.h"
#include "ui/ui.h"
#include "core/file.h"
#include "plugins/plugin_manager.h"
#include "utils/utils.h"
#include "utils/utf8_utils.h"
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#endif

/**
 * @brief Finds the configured command for the current file.
 *
 * It checks the user's configuration for a specific command associated with the
 * file's MIME type. If none is found, it falls back to the default command.
 *
 * @param state A pointer to the application state.
 * @return A read-only string containing the command to be executed, or NULL if none is set.
 */
static const char* find_command_for_file(AppState* state) {
    char* mime_type = get_file_mime_type(state->filepath);
    if (mime_type) {
        for (size_t i = 0; i < state->config.mime_commands_count; i++) {
            if (strcmp(mime_type, state->config.mime_commands[i].mime_type) == 0) {
                free(mime_type);
                return state->config.mime_commands[i].command;
            }
        }
        free(mime_type);
    }
    return state->config.default_command;
}

/**
 * @brief Processes a single character of user input and updates the state.
 */
FatResult process_input(AppState *state, int ch) {
    int page_size = getmaxy(state->right_pane) - 2;
    if (page_size < 1) page_size = 1;
    FatResult res = FAT_SUCCESS;

    // Handle multi-key 'g' prefix sequences first
    if (ch == 'g') {
        int next_ch = getch(); // Block and wait for the next key
        if (next_ch == 't') {
            // "gt" sequence for ACTION_JUMP_TO_LINE
            int target_line = ui_get_line_input(state);
            if (target_line > 0) {
                state->top_line = target_line - 1;
                if (state->top_line >= (int)state->content.count) {
                    state->top_line = state->content.count > 0 ? (int)state->content.count - 1 : 0;
                }
            }
        } else if (next_ch == 'g') {
            // "gg" sequence for ACTION_JUMP_TO_START
            state->top_line = 0;
            state->left_char = 0;
        }
        // If next_ch is something else, the sequence is ignored.
        return FAT_SUCCESS;
    }

    Action action = (ch >= 0 && ch < MAX_KEY_CODE) ? state->config.key_map[ch] : ACTION_NONE;

    if (action == ACTION_SELECT_THEME) {
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
    if (action == ACTION_TOGGLE_HELP) {
        ui_show_help(state);
        return FAT_SUCCESS;
    }
    if (action == ACTION_JUMP_TO_END) {
        state->top_line = state->content.count > 0 ? (int)state->content.count - 1 : 0;
        state->left_char = 0;
        return FAT_SUCCESS;
    }
    
    if (action == ACTION_OPEN_EXTERNAL || action == ACTION_OPEN_EXTERNAL_DEFAULT) {
        const char* command_to_run = NULL;
        char command_buffer[512] = {0};

        if(action == ACTION_OPEN_EXTERNAL_DEFAULT){
            command_to_run = find_command_for_file(state);
        }

        if(!command_to_run){
            ui_get_command_input(state, command_buffer, sizeof(command_buffer));
            if(command_buffer[0] != '\0'){
                command_to_run = command_buffer;
            }
        }

        if (command_to_run) {
            def_prog_mode();
            endwin();
            
            char full_command[1024];
            snprintf(full_command, sizeof(full_command), "%s \"%s\"", command_to_run, state->filepath);
            (void)system(full_command);
            
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
            switch (action) {
                case ACTION_SCROLL_DOWN:
                    if (state->top_line + 1 < (int)state->content.count) {
                        state->top_line++;
                    }
                    break;
                case ACTION_SCROLL_UP:
                    if (state->top_line > 0) {
                        state->top_line--;
                    }
                    break;
                case ACTION_PAGE_DOWN:
                    state->top_line += page_size;
                    if (state->top_line >= (int)state->content.count) {
                        state->top_line = state->content.count > 0 ? (int)state->content.count - 1 : 0;
                    }
                    break;
                case ACTION_PAGE_UP:
                    state->top_line -= page_size;
                    if (state->top_line < 0) {
                        state->top_line = 0;
                    }
                    break;
                case ACTION_CONFIRM: {
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
                case ACTION_GO_BACK:
                    if (state->breadcrumbs.count > 1) {
                        cleanup_temp_file_if_exists(state->breadcrumbs.lines[state->breadcrumbs.count - 1]);
                        state->breadcrumbs.count--;
                        return state_init(state, state->breadcrumbs.lines[state->breadcrumbs.count - 1]);
                    }
                    break;
                default:
                    break;
            }
            break;

        case VIEW_MODE_BINARY_HEX:
        case VIEW_MODE_NORMAL:
            {
                int visible_content_width = getmaxy(state->right_pane) - 7 - 1;
                int max_scroll_limit = (int)state->max_line_len - visible_content_width;
                if (max_scroll_limit < 0) max_scroll_limit = 0;

                switch(action) {
                    case ACTION_TOGGLE_VIEW_MODE:
                        if (state->view_mode == VIEW_MODE_NORMAL) {
                            return state_reload_content(state, VIEW_MODE_BINARY_HEX);
                        } else if (state->view_mode == VIEW_MODE_BINARY_HEX) {
                            return state_reload_content(state, VIEW_MODE_NORMAL);
                        }
                        break;
                    case ACTION_SCROLL_DOWN:
                        if (state->top_line + 1 < (int)state->content.count) state->top_line++;
                        break;
                    case ACTION_SCROLL_UP:
                        if (state->top_line > 0) state->top_line--;
                        break;
                    case ACTION_SCROLL_RIGHT:
                        if (!state->line_wrap_enabled && state->left_char < max_scroll_limit) {
                            const char *line = state->content.lines[state->top_line];
                            if (state->left_char < (int)strlen(line)) {
                                state->left_char += utf8_char_len(&line[state->left_char]);
                            }
                        }
                        break;
                    case ACTION_SCROLL_LEFT:
                        if (!state->line_wrap_enabled && state->left_char > 0) {
                            const char *line = state->content.lines[state->top_line];
                            state->left_char = utf8_prev_char_start(line, state->left_char);
                        }
                        break;
                    case ACTION_PAGE_DOWN:
                        state->top_line += page_size;
                        if ((size_t)(state->top_line) >= state->content.count) {
                             state->top_line = state->content.count > 0 ? (int)state->content.count - 1 : 0;
                        }
                        break;
                    case ACTION_PAGE_UP:
                        state->top_line -= page_size;
                        if (state->top_line < 0) state->top_line = 0;
                        break;
                    
                    case ACTION_SEARCH:
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

                    case ACTION_NEXT_MATCH:
                        if (state->search_term_active && state->search_results.count > 0) {
                            state->search_results.current_match_idx = (state->search_results.current_match_idx + 1) % state->search_results.count;
                            SearchMatch* match = &state->search_results.matches[state->search_results.current_match_idx];
                            state->top_line = (int)match->line_idx;
                        } else {
                            res = FAT_ERROR_UNSUPPORTED; // No active search
                        }
                        break;

                    case ACTION_PREV_MATCH:
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

                    case ACTION_TOGGLE_WRAP:
                        if (state->view_mode == VIEW_MODE_NORMAL) {
                            state->line_wrap_enabled = !state->line_wrap_enabled;
                        }
                        break;
                    case ACTION_GO_BACK:
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
                    default:
                        break;
                }
            }
            break;
    }
    return FAT_SUCCESS;
}
