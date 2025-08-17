/**
 * @file qp_declarative.h
 * @brief Fully declarative widget system for Quantum Painter animations
 *
 * This module provides:
 * - Declarative widget configuration with states, conditions, and layout
 * - Automatic state management and transition handling
 * - Support for both exclusive (layer-like) and independent (modifier-like) widgets
 * - Configurable rendering policies (opaque vs additive blending)
 * - Fluent APIs for easy widget creation and management
 * - Device-agnostic rendering with QP integration
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include QMK_KEYBOARD_H
#include "qp_anim.h"
#include "qp_image.h"
#include "qp_utils.h"

// ============================================================================
// Core Widget Types
// ============================================================================

/**
 * @brief Widget state description
 * 
 * Describes how to enter a particular widget state, including the animation
 * sequence and direction.
 */
typedef struct {
    const qp_image_sequence_t* sequence;   ///< Animation sequence for this state
    bool enter_forward;                    ///< True=enter forward, false=enter backward
} qp_widget_state_t;

/**
 * @brief Widget query function type
 * 
 * Function that determines the current desired state of the widget.
 * Called periodically to check if the widget should transition.
 * 
 * @param user_data User-provided data pointer
 * @return Desired state index (0-based)
 */
typedef uint8_t (*qp_widget_query_fn_t)(void* user_data);

/**
 * @brief Widget configuration structure
 * 
 * Declarative configuration for a widget, including layout, states,
 * and behavior settings.
 */
typedef struct {
    // Layout
    uint16_t x, y;                         ///< Widget position
    uint16_t w, h;                         ///< Widget bounding box (for clearing)
    
    // States
    const qp_widget_state_t* states;      ///< Array of state descriptions
    uint8_t state_count;                   ///< Number of states
    
    // Behavior
    qp_widget_query_fn_t query_fn;        ///< Function to query desired state
    void* user_data;                       ///< User data for query function
    
    // Rendering
    bool opaque_blending;                  ///< True=clear background, false=additive
    qp_color_t clear_color;                ///< Color to use for clearing (if opaque)
} qp_widget_config_t;

/**
 * @brief Widget transition phase
 */
typedef enum {
    QP_WIDGET_IDLE,                        ///< Not transitioning
    QP_WIDGET_EXITING,                     ///< Exiting current state
    QP_WIDGET_ENTERING                     ///< Entering new state
} qp_widget_phase_t;

/**
 * @brief Widget runtime instance
 * 
 * Runtime state for a declarative widget instance.
 */
typedef struct {
    // Configuration (reference)
    const qp_widget_config_t* config;     ///< Widget configuration
    
    // Runtime state
    qp_animator_t animator;                ///< Low-level animator
    qp_widget_phase_t phase;               ///< Current transition phase
    uint8_t current_state;                 ///< Current state index
    uint8_t target_state;                  ///< Target state index
    uint8_t pending_state;                 ///< Pending state (for chained transitions)
    
    // Timing and control
    uint32_t last_query_time;              ///< Last time query function was called
    uint32_t last_state_change;            ///< Last state change timestamp
    uint32_t stuck_timeout;                ///< Timeout for stuck transitions
    uint8_t last_query_result;             ///< Last query result for change detection
    
    // Status
    bool initialized;                      ///< True if widget has been initialized
} qp_widget_t;

// ============================================================================
// Widget Management Functions
// ============================================================================

/**
 * @brief Initialize widget
 * @param widget Widget instance
 * @param config Widget configuration
 * @param initial_state Initial state index
 * @param now Current timestamp
 */
void qp_widget_init(qp_widget_t* widget, const qp_widget_config_t* config, uint8_t initial_state, uint32_t now);

/**
 * @brief Update widget state and render
 * @param widget Widget instance
 * @param device QP device handle
 * @param now Current timestamp
 */
void qp_widget_tick(qp_widget_t* widget, painter_device_t device, uint32_t now);

/**
 * @brief Force widget to specific state
 * @param widget Widget instance
 * @param state State index to transition to
 * @param now Current timestamp
 */
void qp_widget_force_state(qp_widget_t* widget, uint8_t state, uint32_t now);

/**
 * @brief Get current widget state
 * @param widget Widget instance
 * @return Current state index
 */
static inline uint8_t qp_widget_get_state(const qp_widget_t* widget) {
    return widget ? widget->current_state : 0;
}

/**
 * @brief Check if widget is transitioning
 * @param widget Widget instance
 * @return True if widget is currently animating a transition
 */
static inline bool qp_widget_is_transitioning(const qp_widget_t* widget) {
    return widget && widget->phase != QP_WIDGET_IDLE;
}

// ============================================================================
// State Description Helpers
// ============================================================================

/**
 * @brief Create forward-entering state description
 * @param seq Animation sequence
 * @return State description
 */
static inline qp_widget_state_t qp_state_forward(const qp_image_sequence_t* seq) {
    return (qp_widget_state_t){seq, true};
}

/**
 * @brief Create reverse-entering state description
 * @param seq Animation sequence
 * @return State description
 */
static inline qp_widget_state_t qp_state_reverse(const qp_image_sequence_t* seq) {
    return (qp_widget_state_t){seq, false};
}

// ============================================================================
// Configuration Helpers
// ============================================================================

/**
 * @brief Create widget configuration
 * @param x X position
 * @param y Y position
 * @param w Width
 * @param h Height
 * @param states Array of state descriptions
 * @param count Number of states
 * @param query Query function
 * @param user_data User data for query function
 * @param opaque True for opaque blending
 * @param clear_color Clear color (if opaque)
 * @return Widget configuration
 */
static inline qp_widget_config_t qp_widget_config(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                                   const qp_widget_state_t* states, uint8_t count,
                                                   qp_widget_query_fn_t query, void* user_data,
                                                   bool opaque, qp_color_t clear_color) {
    return (qp_widget_config_t){
        .x = x, .y = y, .w = w, .h = h,
        .states = states, .state_count = count,
        .query_fn = query, .user_data = user_data,
        .opaque_blending = opaque, .clear_color = clear_color
    };
}

// ============================================================================
// Convenience Macros
// ============================================================================

/**
 * @brief Define widget states array
 * @param name Variable name
 * @param ... State descriptions
 */
#define QP_DEFINE_WIDGET_STATES(name, ...) \
    static const qp_widget_state_t name[] = {__VA_ARGS__}

/**
 * @brief Create widget configuration with opaque blending
 */
#define QP_WIDGET_CONFIG_OPAQUE(x, y, w, h, states, count, query, user_data) \
    qp_widget_config((x), (y), (w), (h), (states), (count), (query), (user_data), true, QP_COLOR_BLACK)

/**
 * @brief Create widget configuration with additive blending
 */
#define QP_WIDGET_CONFIG_ADDITIVE(x, y, w, h, states, count, query, user_data) \
    qp_widget_config((x), (y), (w), (h), (states), (count), (query), (user_data), false, QP_COLOR_BLACK)

/**
 * @brief Forward state helper macro
 */
#define QP_STATE_FWD(seq) qp_state_forward(&(seq))

/**
 * @brief Reverse state helper macro
 */
#define QP_STATE_REV(seq) qp_state_reverse(&(seq))

// ============================================================================
// Common Query Functions
// ============================================================================

/**
 * @brief Layer query function
 * @param user_data Unused
 * @return Current highest layer
 */
uint8_t qp_query_layer(void* user_data);

/**
 * @brief Modifier query function
 * @param user_data Pointer to modifier mask (uint8_t*)
 * @return 1 if any specified modifiers active, 0 otherwise
 */
uint8_t qp_query_modifiers(void* user_data);

/**
 * @brief Caps lock query function
 * @param user_data Unused
 * @return 1 if caps lock active, 0 otherwise
 */
uint8_t qp_query_caps_lock(void* user_data);

// ============================================================================
// Integration Notes
// ============================================================================

/**
 * @brief Migration from oled_declarative
 *
 * The QP declarative system provides similar functionality to oled_declarative
 * but adapted for Quantum Painter's device model:
 *
 * Old oled_declarative approach:
 * @code
 * static const widget_state_t layer_states[] = {
 *     STATE_FWD(layer0_seq), STATE_FWD(layer1_seq)
 * };
 * static const widget_config_t layer_config = 
 *     WIDGET_CONFIG(64, 0, 40, 16, layer_states, 2, layer_query, NULL, 0);
 * widget_t layer_widget;
 * widget_init(&layer_widget, &layer_config, 0, timer_read32());
 * widget_tick(&layer_widget, timer_read32());
 * @endcode
 *
 * New qp_declarative approach:
 * @code
 * QP_DEFINE_WIDGET_STATES(layer_states,
 *     QP_STATE_FWD(layer0_seq), QP_STATE_FWD(layer1_seq)
 * );
 * static const qp_widget_config_t layer_config = 
 *     QP_WIDGET_CONFIG_OPAQUE(64, 0, 40, 16, layer_states, 2, qp_query_layer, NULL);
 * qp_widget_t layer_widget;
 * qp_widget_init(&layer_widget, &layer_config, 0, timer_read32());
 * qp_widget_tick(&layer_widget, device, timer_read32());
 * @endcode
 */
