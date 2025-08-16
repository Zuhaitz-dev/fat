/**
 * @file main.c
 * @author Zuhaitz (original)
 * @brief The main entry point for the FAT TUI application.
 */
#include "core/state.h"
#include "ui/ui.h"
#include "utils/logger.h"
#include "core/error.h"
#include "core/config.h"
#include "core/controller.h"
#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#ifdef _WIN32
#include <windows.h>
#endif

// **Forward Declarations**
static void print_help(const char* executable_name);

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
    
    AppState state = {0};
    state.force_view_mode = force_mode; // Pass the forced mode to the state
    
    // Load configuration to get terminal size requirements before checking
    config_load(&state);


    if (!check_terminal_size(&state)) {
        ui_destroy();
        logger_destroy();
        return 0;
    }


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
    while (true) {
        ch = getch();
        
        if (state.config.key_map[ch] == ACTION_QUIT) {
            break;
        }

        if (!check_terminal_size(&state)) {
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
