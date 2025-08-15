/**
 * @file oled_declarative.h
 * @brief Fully declarative widget system for OLED animations
 *
 * This module provides:
 * - Declarative widget configuration with states, conditions, and layout
 * - Automatic state management and transition handling
 * - Support for both exclusive (layer-like) and independent (modifier-like) widgets
 * - Configurable rendering policies (opaque vs additive blending)
 * - Fluent APIs for easy widget creation and management
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include QMK_KEYBOARD_H

#include "oled_utils.h"   // slice_t, clear_rect, draw_slice_px
#include "oled_anim.h"    // slice_seq_t, animator_t, anim_result_t, TR_* enums

// ============================================================================
// Watchdog Configuration
// ============================================================================

/**
 * @brief Animation watchdog timeout in milliseconds
 *
 * If an animation is running for longer than this duration after a state change,
 * it will be considered stuck and automatically reset. Set to 0 to disable watchdog.
 */
#ifndef WIDGET_WATCHDOG_TIMEOUT_MS
#    define WIDGET_WATCHDOG_TIMEOUT_MS 1000
#endif

/**
 * @brief Additional grace period before forcing reset
 *
 * After detecting a stuck animation, wait this additional time before forcing
 * a reset to allow for natural completion.
 */
#ifndef WIDGET_WATCHDOG_GRACE_MS
#    define WIDGET_WATCHDOG_GRACE_MS 500
#endif

// ============================================================================
// Rendering Policies
// ============================================================================

/**
 * @brief Blending modes for widget rendering
 *
 * Controls how widgets handle background clearing and frame composition.
 */
typedef enum {
    BLIT_OPAQUE = 0,    ///< Clear widget bbox before drawing each frame (no trails)
    BLIT_ADDITIVE = 1   ///< OR-blend frames without clearing (allows overlays)
} blit_mode_t;

// ============================================================================
// State Configuration
// ============================================================================

/**
 * @brief Enter direction constants for state descriptions
 *
 * Controls which direction the animation plays when entering a state.
 */
#define ENTER_FWD  (+1)  ///< Enter plays forward (0→N-1), steady is last frame
#define ENTER_REV  (-1)  ///< Enter plays backward (N-1→0), steady is first frame

/**
 * @brief State description for declarative widgets
 *
 * Defines how a widget state should be animated. Each state has its own
 * animation sequence and enter direction. The same frames are used for
 * both enter and exit, with direction determining the playback order.
 */
typedef struct {
    const slice_seq_t *seq;     ///< Animation frames for this state
    int8_t             enter_dir; ///< Direction for entering: ENTER_FWD or ENTER_REV
} state_desc_t;

/**
 * @brief State query callback function
 *
 * Called by widgets to determine the desired state. Should return a state
 * index in the range [0, state_count-1]. Use user_arg to pass context data.
 *
 * @param user_arg Context data passed from widget configuration
 * @return Desired state index
 */
typedef uint8_t (*state_query_fn_t)(uint32_t user_arg);

// ============================================================================
// Widget Configuration
// ============================================================================

/**
 * @brief Declarative widget configuration
 *
 * Defines all aspects of a widget's behavior, appearance, and state management.
 * Works for both exclusive widgets (like layer indicators) and independent
 * widgets (like modifier indicators).
 *
 * The widget system automatically handles:
 * - State transitions with smooth animations
 * - Mid-flight reversal and cancellation
 * - Queued state changes during transitions
 * - Rendering with configurable blending modes
 */
typedef struct {
    // Layout configuration
    uint8_t x, y;               ///< Drawing position in pixels
    uint8_t bbox_w, bbox_h;     ///< Bounding box for clearing (BLIT_OPAQUE mode)

    // Rendering configuration
    blit_mode_t blit;           ///< Blending mode (opaque or additive)

    // State configuration
    const state_desc_t *states; ///< Array of state descriptions
    uint8_t             state_count; ///< Number of valid states

    // Condition configuration
    state_query_fn_t query;     ///< Function to determine desired state
    uint32_t         user_arg;  ///< Context data for query function

    // Initialization
    uint8_t initial_state;      ///< Starting state index
} widget_config_t;

// ============================================================================
// Widget Runtime
// ============================================================================

/**
 * @brief Widget runtime instance
 *
 * Contains all runtime state for a declarative widget. Initialize with
 * widget_init() and update with widget_tick() every OLED frame.
 */
typedef struct {
    const widget_config_t *cfg; ///< Configuration (not owned)

    // Animation state
    animator_t anim;            ///< Low-level animator instance
    tr_phase_t phase;           ///< Current transition phase

    // Logical state
    uint8_t src;                ///< Currently visible/committed state
    uint8_t dst;                ///< Target state for current transition
    uint8_t pending;            ///< Queued desired state (0xFF = none)

    // Watchdog state
    uint8_t last_query_result;  ///< Last result from query function
    uint32_t last_state_change; ///< Timestamp of last state change
    uint32_t stuck_timeout;     ///< Timestamp when stuck condition was detected (0 = not stuck)

    // Status
    bool initialized;           ///< Whether widget has been initialized
} widget_t;

// ============================================================================
// Widget API
// ============================================================================

/**
 * @brief Initialize a declarative widget
 *
 * Sets up the widget with the given configuration and draws the initial state.
 * The widget will be ready to respond to widget_tick() calls.
 *
 * @param w Widget instance to initialize
 * @param cfg Widget configuration (must remain valid for widget lifetime)
 * @param initial_state Starting state index (overrides cfg->initial_state)
 * @param now Current timestamp from timer_read32()
 */
void widget_init(widget_t *w, const widget_config_t *cfg, uint8_t initial_state, uint32_t now);

/**
 * @brief Update and render a declarative widget
 *
 * Call this every OLED frame to:
 * 1. Query the desired state using the configured query function
 * 2. Make transition decisions (start, continue, reverse, or queue changes)
 * 3. Render the current frame with appropriate clearing/blending
 *
 * This function handles all state management automatically based on the
 * widget configuration.
 *
 * @param w Widget instance to update
 * @param now Current timestamp from timer_read32()
 */
void widget_tick(widget_t *w, uint32_t now);

// ============================================================================
// Convenience Macros
// ============================================================================

/**
 * @brief Create a state description with forward enter direction
 *
 * Convenience macro for creating state_desc_t with ENTER_FWD direction.
 * The steady state will be the last frame of the sequence.
 *
 * @param seq_ptr Pointer to slice_seq_t
 */
#define STATE_FWD(seq_ptr) { (seq_ptr), ENTER_FWD }

/**
 * @brief Create a state description with reverse enter direction
 *
 * Convenience macro for creating state_desc_t with ENTER_REV direction.
 * The steady state will be the first frame of the sequence.
 *
 * @param seq_ptr Pointer to slice_seq_t
 */
#define STATE_REV(seq_ptr) { (seq_ptr), ENTER_REV }

/**
 * @brief Create a simple widget configuration
 *
 * Convenience macro for creating widget_config_t with common defaults.
 * Uses BLIT_OPAQUE mode and requires manual state array definition.
 *
 * @param x_pos X coordinate
 * @param y_pos Y coordinate
 * @param width Bounding box width
 * @param height Bounding box height
 * @param states_array Array of state_desc_t
 * @param count Number of states
 * @param query_fn State query function
 * @param user_data User argument for query function
 * @param init_state Initial state index
 */
#define WIDGET_CONFIG(x_pos, y_pos, width, height, states_array, count, query_fn, user_data, init_state) \
    { (x_pos), (y_pos), (width), (height), BLIT_OPAQUE, (states_array), (count), (query_fn), (user_data), (init_state) }

// Convenience: get steady frame dims for a state (used internally too)
static inline uint8_t state_steady_w(const state_desc_t *s) {
    const slice_seq_t *q = s->seq;
    return (s->enter_dir > 0) ? q->frames[q->count - 1].width : q->frames[0].width;
}
static inline uint8_t state_steady_h(const state_desc_t *s) {
    const slice_seq_t *q = s->seq;
    const slice_t *fr = (s->enter_dir > 0) ? &q->frames[q->count - 1] : &q->frames[0];
    return (uint8_t)(fr->pages * 8);
}

