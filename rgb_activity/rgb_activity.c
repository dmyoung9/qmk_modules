// Copyright 2024 David Young (@dmyoung9)
// SPDX-License-Identifier: GPL-2.0-or-later
#include <stdbool.h>
#include <stdint.h>

#include QMK_KEYBOARD_H

#include "rgb_activity.h"

// RGB activity state tracking
typedef enum {
    RGB_ACTIVITY_NORMAL,    // Normal operation, no fading
    RGB_ACTIVITY_FADING_OUT, // Currently fading out due to inactivity
    RGB_ACTIVITY_FADING_IN   // Currently fading in due to activity
} rgb_activity_state_t;

// Internal state variables
static rgb_activity_state_t current_state = RGB_ACTIVITY_NORMAL;
static uint8_t original_brightness = 0;
static uint8_t original_saturation = 0;
static uint32_t last_fade_out_time = 0;
static uint32_t last_fade_in_time = 0;
static bool state_saved = false;

// Internal helper functions
static void save_original_state(void);
static void handle_fade_out_step(void);
static void handle_fade_in_step(void);
static void start_fade_out(void);
static void start_fade_in(void);

void keyboard_post_init_rgb_activity(void) {
    current_state = RGB_ACTIVITY_NORMAL;
    original_brightness = 0;
    original_saturation = 0;
    last_fade_out_time = 0;
    last_fade_in_time = 0;
    state_saved = false;
}

void rgb_activity_update(uint32_t inactivity_time_ms) {
    // Save original brightness on first run or when in normal state
    if (!state_saved && current_state == RGB_ACTIVITY_NORMAL) {
        save_original_state();
    }

    // Handle fade-in process (only when there's recent activity)
    if (inactivity_time_ms < RGB_FADE_START_TIMEOUT) {
        if (current_state == RGB_ACTIVITY_FADING_IN) {
            handle_fade_in_step();
        }
    } else {
        // Stop fade-in if activity has stopped
        if (current_state == RGB_ACTIVITY_FADING_IN) {
            current_state = RGB_ACTIVITY_NORMAL;
            state_saved = false;
        }
    }

    // Handle fade-out logic (only if not fading in)
    if (current_state != RGB_ACTIVITY_FADING_IN &&
        inactivity_time_ms >= RGB_FADE_START_TIMEOUT &&
        inactivity_time_ms < RGB_MATRIX_TIMEOUT) {

        if (current_state != RGB_ACTIVITY_FADING_OUT) {
            start_fade_out();
        }

        handle_fade_out_step();
    }
}

bool process_record_rgb_activity(uint16_t keycode, keyrecord_t *record) {
    if (!record->event.pressed) return true;

    if (((current_state == RGB_ACTIVITY_FADING_OUT || rgb_matrix_get_val() < original_brightness) || (current_state == RGB_ACTIVITY_FADING_OUT || rgb_matrix_get_sat() < original_saturation)) && state_saved) {
        start_fade_in();
    }

    return true;
}

bool rgb_activity_is_fading_in(void) {
    return current_state == RGB_ACTIVITY_FADING_IN;
}

bool rgb_activity_is_fading_out(void) {
    return current_state == RGB_ACTIVITY_FADING_OUT;
}

const char* rgb_activity_get_state_string(void) {
    switch (current_state) {
        case RGB_ACTIVITY_NORMAL:    return "NORMAL";
        case RGB_ACTIVITY_FADING_OUT: return "FADING_OUT";
        case RGB_ACTIVITY_FADING_IN:  return "FADING_IN";
        default:                     return "UNKNOWN";
    }
}

// Internal helper function implementations
static void save_original_state(void) {
    original_brightness = rgb_matrix_get_val();
    original_saturation = rgb_matrix_get_sat();
    state_saved = true;
}

static void start_fade_out(void) {
    current_state = RGB_ACTIVITY_FADING_OUT;
    last_fade_out_time = timer_read32();
}

static void start_fade_in(void) {
    current_state = RGB_ACTIVITY_FADING_IN;
    last_fade_in_time = timer_read32();
}

static void handle_fade_out_step(void) {
    uint32_t current_time = timer_read32();

    // Check if it's time for the next fade step
    if (current_time - last_fade_out_time >= RGB_FADE_STEP_INTERVAL) {
        uint8_t current_val = rgb_matrix_get_val();
        uint8_t current_sat = rgb_matrix_get_sat();
        if (current_val > RGB_FADE_MIN_BRIGHTNESS || current_sat > RGB_FADE_MIN_SATURATION) {
            // Decrease brightness using the matrix API - this preserves individual key colors
            rgb_matrix_decrease_val_noeeprom();
            rgb_matrix_decrease_sat_noeeprom();
            last_fade_out_time = current_time;
        }
    }
}

static void handle_fade_in_step(void) {
    uint32_t current_time = timer_read32();

    // Check if it's time for the next fade-in step
    if (current_time - last_fade_in_time >= RGB_FADE_IN_STEP_INTERVAL) {
        uint8_t current_val = rgb_matrix_get_val();
        uint8_t current_sat = rgb_matrix_get_sat();
        if (current_val < original_brightness || current_sat < original_saturation) {
            // Increase brightness gradually
            rgb_matrix_increase_val_noeeprom();
            rgb_matrix_increase_sat_noeeprom();
            last_fade_in_time = current_time;
        } else {
            // Fade-in complete
            current_state = RGB_ACTIVITY_NORMAL;
            state_saved = false;
        }
    }
}

bool rgb_matrix_indicators_rgb_activity(void) {
    uint32_t inactivity_time = last_input_activity_elapsed();

    // Update RGB activity management
    rgb_activity_update(inactivity_time);

    return true;
}
