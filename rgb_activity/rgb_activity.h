#pragma once

#include <stdbool.h>
#include <stdint.h>

#include QMK_KEYBOARD_H

/**
 * @brief RGB Activity Management Module
 *
 * This module handles RGB matrix fade-in and fade-out effects based on keyboard activity.
 */

/**
 * @brief Update RGB activity state based on current inactivity time
 *
 * This function should be called regularly (e.g., from rgb_matrix_indicators_user)
 * to handle fade-in and fade-out logic.
 *
 * @param inactivity_time_ms Time in milliseconds since last input activity
 */
void rgb_activity_update(uint32_t inactivity_time_ms);

/**
 * @brief Notify the system of keyboard activity
 *
 * This function should be called when a key is pressed to trigger fade-in
 * if the RGB matrix is currently faded or fading out.
 */
void rgb_activity_on_keypress(void);

/**
 * @brief Check if RGB activity management is currently fading in
 *
 * @return true if currently fading in, false otherwise
 */
bool rgb_activity_is_fading_in(void);

/**
 * @brief Check if RGB activity management is currently fading out
 *
 * @return true if currently fading out, false otherwise
 */
bool rgb_activity_is_fading_out(void);

/**
 * @brief Get the current fade state as a string (for debugging)
 *
 * @return String representation of current fade state
 */
const char* rgb_activity_get_state_string(void);
