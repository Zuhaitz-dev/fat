/**
 * @file logger.h
 * @author Zuhaitz (original)
 * @brief A simple file-based logging utility for debugging.
 *
 * This header defines the interface for a lightweight logger that writes
 * timestamped messages to a file. It is intended for developer use to trace
 * application flow and diagnose errors, not for user-facing messages.
 */
#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>
#include <stdbool.h>

/**
 * @brief Initializes the logger.
 *
 * Opens the specified log file in write mode, clearing any previous logs.
 * This must be called once at the beginning of the application's execution.
 *
 * @param log_file The path to the log file (e.g., "fat_log.txt").
 */
void logger_init(const char* log_file);

/**
 * @brief Closes the log file and cleans up resources.
 *
 * This must be called once at application exit to ensure the log file is
 * properly closed and all buffered messages are written.
 */
void logger_destroy(void);

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
void logger_log(const char* file, int line, const char* fmt, ...);

/**
 * @def LOG_INFO(...)
 * @brief A macro to simplify logging informational messages.
 *
 * This is the preferred way to log messages. It automatically passes the
 * file and line number to the logger_log function, reducing boilerplate code.
 *
 * Example:
 * LOG_INFO("Loaded %d plugins from %s", count, path);
 */
#define LOG_INFO(...)  logger_log(__FILE__, __LINE__, __VA_ARGS__)

#endif // LOGGER_H
