/**
 * @file controller.h
 * @author Zuhaitz (original)
 * @brief Defines the core application setup and event loop.
 */
#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "core/state.h"

/**
 * @brief Processes a single character of user input and updates the state.
 *
 * This function is the core of the event loop, translating key presses into
 * actions based on the current application and view mode.
 *
 * @param state A pointer to the application state to modify.
 * @param ch The character code of the key pressed by the user.
 * @return FAT_SUCCESS on success, or an error code if the input resulted in an error.
 */
FatResult process_input(AppState *state, int ch);

#endif // CONTROLLER_H
