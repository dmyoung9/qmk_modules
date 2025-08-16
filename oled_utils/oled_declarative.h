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

#include "oled_slice.h"   // slice_t
#include "oled_utils.h"   // clear_rect, draw_slice_px
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
 * @brief State query callback function (new signature)
 *
 * Called by widgets to determine the desired state. Should return a state
 * index in the range [0, state_count-1]. Use user_arg to pass context data.
 *
 * @param user_arg Context data passed from widget configuration
 * @param current_state Current visible state (for context-aware queries)
 * @param now Current timestamp (for time-based state logic)
 * @return Desired state index, or 0xFF to indicate no change
 */
typedef uint8_t (*state_query_fn_t)(uint32_t user_arg, uint8_t current_state, uint32_t now);

/**
 * @brief Legacy state query callback function (backward compatibility)
 *
 * Legacy signature for backward compatibility with existing code.
 * New code should use state_query_fn_t instead.
 *
 * @param user_arg Context data passed from widget configuration
 * @return Desired state index
 */
typedef uint8_t (*state_query_legacy_fn_t)(uint32_t user_arg);

// Forward declarations
typedef struct widget_config widget_config_t;
typedef struct widget_runtime widget_t;

/**
 * @brief Widget validation callback function
 *
 * Optional callback to validate widget configuration and state.
 * Called during initialization and optionally during runtime.
 *
 * @param cfg Widget configuration to validate
 * @param runtime_state Current runtime state (NULL during init)
 * @return true if valid, false if invalid
 */
typedef bool (*widget_validate_fn_t)(const widget_config_t *cfg, const widget_t *runtime_state);

/**
 * @brief Widget error callback function
 *
 * Optional callback to handle widget errors (stuck animations, invalid states, etc.)
 * Allows custom error recovery or logging.
 *
 * @param cfg Widget configuration
 * @param error_code Error type (see widget_error_t)
 * @param context Additional context data
 */
typedef void (*widget_error_fn_t)(const widget_config_t *cfg, uint8_t error_code, uint32_t context);

/**
 * @brief Widget error codes
 */
typedef enum {
    WIDGET_ERROR_NONE = 0,          ///< No error
    WIDGET_ERROR_INVALID_CONFIG,    ///< Invalid configuration detected
    WIDGET_ERROR_INVALID_STATE,     ///< Query returned invalid state index
    WIDGET_ERROR_STUCK_ANIMATION,   ///< Animation stuck (watchdog timeout)
    WIDGET_ERROR_NULL_SEQUENCE,     ///< Null animation sequence
    WIDGET_ERROR_EMPTY_SEQUENCE,    ///< Empty animation sequence
    WIDGET_ERROR_QUERY_FAILED       ///< State query function failed
} widget_error_t;

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
 * - Error detection and recovery
 */
struct widget_config {
    // Layout configuration
    uint8_t x, y;               ///< Drawing position in pixels
    uint8_t bbox_w, bbox_h;     ///< Bounding box for clearing (BLIT_OPAQUE mode)

    // Rendering configuration
    blit_mode_t blit;           ///< Blending mode (opaque or additive)

    // State configuration
    const state_desc_t *states; ///< Array of state descriptions
    uint8_t             state_count; ///< Number of valid states

    // Condition configuration
    state_query_fn_t query;     ///< Function to determine desired state (new signature)
    state_query_legacy_fn_t legacy_query; ///< Legacy query function (backward compatibility)
    uint32_t         user_arg;  ///< Context data for query function

    // Initialization
    uint8_t initial_state;      ///< Starting state index

    // Error handling and validation (optional)
    widget_validate_fn_t validate; ///< Validation callback (NULL = no validation)
    widget_error_fn_t    on_error;  ///< Error handler callback (NULL = ignore errors)

    // Advanced configuration
    uint16_t query_interval_ms; ///< Minimum interval between state queries (0 = every frame)
    bool     auto_recover;      ///< Automatically recover from stuck animations
    uint8_t  max_retries;       ///< Maximum retry attempts for failed operations
};

// ============================================================================
// Widget Runtime
// ============================================================================

/**
 * @brief Widget runtime instance
 *
 * Contains all runtime state for a declarative widget. Initialize with
 * widget_init() and update with widget_tick() every OLED frame.
 */
struct widget_runtime {
    const widget_config_t *cfg; ///< Configuration (not owned)

    // Animation state
    animator_t anim;            ///< Low-level animator instance
    tr_phase_t phase;           ///< Current transition phase

    // Logical state
    uint8_t src;                ///< Currently visible/committed state
    uint8_t dst;                ///< Target state for current transition
    uint8_t pending;            ///< Queued desired state (0xFF = none)

    // Watchdog and error state
    uint8_t  last_query_result; ///< Last result from query function
    uint32_t last_state_change; ///< Timestamp of last state change
    uint32_t last_query_time;   ///< Timestamp of last query execution
    uint32_t stuck_timeout;     ///< Timestamp when stuck condition was detected (0 = not stuck)

    // Error tracking
    widget_error_t last_error;  ///< Last error that occurred
    uint8_t        error_count; ///< Number of consecutive errors
    uint8_t        retry_count; ///< Current retry attempt count
    uint32_t       last_error_time; ///< Timestamp of last error

    // Status flags
    bool initialized;           ///< Whether widget has been initialized
    bool error_state;           ///< Whether widget is in error state
    bool recovery_mode;         ///< Whether widget is attempting recovery
};

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
 * 4. Handle error detection and recovery
 *
 * This function handles all state management automatically based on the
 * widget configuration.
 *
 * @param w Widget instance to update
 * @param now Current timestamp from timer_read32()
 */
void widget_tick(widget_t *w, uint32_t now);

/**
 * @brief Validate widget configuration
 *
 * Checks widget configuration for common errors and inconsistencies.
 * Can be called before initialization to catch configuration issues early.
 *
 * @param cfg Widget configuration to validate
 * @return true if configuration is valid, false otherwise
 */
bool widget_validate_config(const widget_config_t *cfg);

/**
 * @brief Force widget state change
 *
 * Immediately changes widget to the specified state, bypassing normal
 * query-driven state management. Useful for manual control or error recovery.
 *
 * @param w Widget instance
 * @param new_state Target state index
 * @param now Current timestamp from timer_read32()
 * @return true if state change was accepted, false if invalid
 */
bool widget_force_state(widget_t *w, uint8_t new_state, uint32_t now);

/**
 * @brief Reset widget to initial state
 *
 * Clears all error conditions and resets widget to its initial state.
 * Useful for error recovery or manual reset.
 *
 * @param w Widget instance
 * @param now Current timestamp from timer_read32()
 */
void widget_reset(widget_t *w, uint32_t now);

/**
 * @brief Get widget error status
 *
 * Returns information about the widget's current error state.
 *
 * @param w Widget instance
 * @return Current error code (WIDGET_ERROR_NONE if no error)
 */
widget_error_t widget_get_error(const widget_t *w);

/**
 * @brief Check if widget is in error state
 *
 * @param w Widget instance
 * @return true if widget has unrecovered errors, false otherwise
 */
static inline bool widget_has_error(const widget_t *w) {
    return w && w->error_state;
}

/**
 * @brief Check if widget is currently animating
 *
 * @param w Widget instance
 * @return true if animation is in progress, false if idle
 */
static inline bool widget_is_animating(const widget_t *w) {
    return w && w->anim.active;
}

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
 * Uses BLIT_OPAQUE mode and basic error handling.
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
    { (x_pos), (y_pos), (width), (height), BLIT_OPAQUE, (states_array), (count), (query_fn), NULL, (user_data), (init_state), \
      NULL, NULL, 0, true, 3 }

/**
 * @brief Create a widget configuration with legacy query function
 *
 * For backward compatibility with existing code that uses the old query signature.
 *
 * @param x_pos X coordinate
 * @param y_pos Y coordinate
 * @param width Bounding box width
 * @param height Bounding box height
 * @param states_array Array of state_desc_t
 * @param count Number of states
 * @param legacy_query_fn Legacy query function (old signature)
 * @param user_data User argument for query function
 * @param init_state Initial state index
 */
#define WIDGET_CONFIG_LEGACY(x_pos, y_pos, width, height, states_array, count, legacy_query_fn, user_data, init_state) \
    { (x_pos), (y_pos), (width), (height), BLIT_OPAQUE, (states_array), (count), NULL, (legacy_query_fn), (user_data), (init_state), \
      NULL, NULL, 0, true, 3 }

/**
 * @brief Create an advanced widget configuration
 *
 * Full configuration macro with all options exposed.
 *
 * @param x_pos X coordinate
 * @param y_pos Y coordinate
 * @param width Bounding box width
 * @param height Bounding box height
 * @param blit_mode Blending mode (BLIT_OPAQUE or BLIT_ADDITIVE)
 * @param states_array Array of state_desc_t
 * @param count Number of states
 * @param query_fn State query function
 * @param user_data User argument for query function
 * @param init_state Initial state index
 * @param validate_fn Validation callback (NULL for none)
 * @param error_fn Error handler callback (NULL for none)
 * @param query_interval_ms Minimum query interval in milliseconds
 * @param auto_recover Enable automatic error recovery
 * @param max_retries Maximum retry attempts
 */
#define WIDGET_CONFIG_ADVANCED(x_pos, y_pos, width, height, blit_mode, states_array, count, query_fn, user_data, init_state, \
                              validate_fn, error_fn, query_interval_ms, auto_recover, max_retries) \
    { (x_pos), (y_pos), (width), (height), (blit_mode), (states_array), (count), (query_fn), NULL, (user_data), (init_state), \
      (validate_fn), (error_fn), (query_interval_ms), (auto_recover), (max_retries) }

/**
 * @brief Create a widget configuration with error handling
 *
 * Convenience macro for widgets that need custom error handling.
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
 * @param error_fn Error handler callback
 */
#define WIDGET_CONFIG_WITH_ERROR_HANDLER(x_pos, y_pos, width, height, states_array, count, query_fn, user_data, init_state, error_fn) \
    { (x_pos), (y_pos), (width), (height), BLIT_OPAQUE, (states_array), (count), (query_fn), NULL, (user_data), (init_state), \
      NULL, (error_fn), 0, true, 3 }

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

