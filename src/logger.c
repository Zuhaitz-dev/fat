/**
 * @file logger.c
 * @author Zuhaitz (original)
 * @brief Implementation of the file-based logging utility.
 *
 * This file contains the logic for initializing, writing to, and closing the
 * application's log file.
 */
#include "logger.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

/** @brief The global file pointer for the log file. */
static FILE* log_fp = NULL;

/**
 * @brief Initializes the logger.
 *
 * Opens the specified log file in write mode, clearing any previous logs.
 * This must be called once at the beginning of the application's execution.
 *
 * @param log_file The path to the log file (e.g., "fat_log.txt").
 */
void logger_init(const char* log_file) {
    if (log_fp) {
        fclose(log_fp);
    }
    // Open in append mode ('a') to preserve old logs on each application run.
    log_fp = fopen(log_file, "a");
    if (!log_fp) {
        // This is a fatal initialization error. Print to stderr as a last resort,
        // as the logger itself has failed.
        perror("FATAL: Failed to open log file");
    }
}

/**
 * @brief Closes the log file and cleans up resources.
 *
 * This must be called once at application exit to ensure the log file is
 * properly closed and all buffered messages are written.
 */
void logger_destroy(void) {
    if (log_fp) {
        fprintf(log_fp, "Logger shutting down.\n");
        fclose(log_fp);
        log_fp = NULL;
    }
}

/**
 * @brief Logs a formatted message to the log file.
 *
 * This is the core logging function. It includes timestamp, source file, and
 * line number information automatically to provide context for the message.
 * It is typically not called directly; use the LOG_INFO macro instead.
 *
 * @param file The source file from which the log is generated (__FILE__).
 * @param line The line number from which the log is generated (__LINE__).
 * @param fmt The printf-style format string for the message.
 * @param ... Variable arguments corresponding to the format string.
 */
void logger_log(const char* file, int line, const char* fmt, ...) {
    if (!log_fp) {
        return; // Failed to initialize. Of course it will do something, right?
    }

    // Get current time to timestamp the log entry.
    time_t t = time(NULL);
    struct tm* lt = localtime(&t);
    char time_buf[20];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", lt);

    // Log the message to the file with full context.
    fprintf(log_fp, "%s [INFO] %s:%d: ", time_buf, file, line);
    va_list args;
    va_start(args, fmt);
    vfprintf(log_fp, fmt, args);
    va_end(args);
    fprintf(log_fp, "\n");

    // Flush the stream to ensure the log is written to disk immediately.
    // This is crucial for debugging crashes.
    fflush(log_fp);
}
