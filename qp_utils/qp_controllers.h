/**
 * @file qp_controllers.h
 * @brief High-level animation controllers for Quantum Painter
 * 
 * This module provides:
 * - One-shot animation controller for boot animations and events
 * - Toggle animation controller for binary state indicators
 * - Out-and-back animation controller for layer transitions
 * - State management and automatic timing control
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
// One-Shot Animation Controller
// ============================================================================

/**
 * @brief One-shot animation controller
 *
 * Manages animations that run once and return to idle state.
 * Perfect for boot animations and event-triggered effects like layer changes.
 *
 * Features:
 * - Boot animation that runs once on initialization
 * - Triggered animations that can be started by external events
 * - Automatic return to idle state after completion
 * - Configurable steady frame (first or last frame of sequence)
 */
typedef struct {
    // Configuration
    const qp_image_sequence_t* sequence;   ///< Animation sequence
    uint16_t x, y;                         ///< Drawing position
    bool steady_at_end;                    ///< true=steady is last frame, false=steady is first frame
    bool auto_boot;                        ///< true=automatically start on init
    
    // Runtime state
    qp_animator_t animator;                ///< Low-level animator
    bool triggered;                        ///< true=animation was manually triggered
    bool completed;                        ///< true=animation has completed
    uint32_t last_render_time;             ///< Last render timestamp
} qp_oneshot_controller_t;

/**
 * @brief Initialize one-shot controller
 * @param controller Controller instance
 * @param sequence Animation sequence
 * @param x X drawing position
 * @param y Y drawing position
 * @param steady_at_end True if steady frame is last frame
 * @param auto_boot True to automatically start animation
 * @param now Current timestamp
 */
void qp_oneshot_init(qp_oneshot_controller_t* controller, const qp_image_sequence_t* sequence,
                     uint16_t x, uint16_t y, bool steady_at_end, bool auto_boot, uint32_t now);

/**
 * @brief Trigger one-shot animation
 * @param controller Controller instance
 * @param now Current timestamp
 */
void qp_oneshot_trigger(qp_oneshot_controller_t* controller, uint32_t now);

/**
 * @brief Render one-shot animation
 * @param controller Controller instance
 * @param device QP device handle
 * @param now Current timestamp
 */
void qp_oneshot_render(qp_oneshot_controller_t* controller, painter_device_t device, uint32_t now);

/**
 * @brief Check if one-shot animation is active
 * @param controller Controller instance
 * @return True if animation is running
 */
static inline bool qp_oneshot_is_active(const qp_oneshot_controller_t* controller) {
    return controller && qp_animator_is_active(&controller->animator);
}

// ============================================================================
// Toggle Animation Controller
// ============================================================================

/**
 * @brief Toggle animation controller
 *
 * Manages animations for binary state indicators (on/off, active/inactive).
 * Automatically transitions between states with smooth animations.
 *
 * Features:
 * - Binary state management (on/off)
 * - Smooth transitions between states
 * - Configurable enter/exit animations
 * - Steady state display when not transitioning
 */
typedef struct {
    // Configuration
    const qp_image_sequence_t* sequence;   ///< Animation sequence
    uint16_t x, y;                         ///< Drawing position
    bool steady_at_end;                    ///< true=steady is last frame, false=steady is first frame
    
    // Runtime state
    qp_animator_t animator;                ///< Low-level animator
    bool current_state;                    ///< Current logical state (on/off)
    bool target_state;                     ///< Target logical state
    bool in_transition;                    ///< true=currently animating transition
    uint32_t last_render_time;             ///< Last render timestamp
} qp_toggle_controller_t;

/**
 * @brief Initialize toggle controller
 * @param controller Controller instance
 * @param sequence Animation sequence
 * @param x X drawing position
 * @param y Y drawing position
 * @param steady_at_end True if steady frame is last frame
 * @param initial_state Initial state (true=on, false=off)
 * @param now Current timestamp
 */
void qp_toggle_init(qp_toggle_controller_t* controller, const qp_image_sequence_t* sequence,
                    uint16_t x, uint16_t y, bool steady_at_end, bool initial_state, uint32_t now);

/**
 * @brief Set toggle state
 * @param controller Controller instance
 * @param state New state (true=on, false=off)
 * @param now Current timestamp
 */
void qp_toggle_set(qp_toggle_controller_t* controller, bool state, uint32_t now);

/**
 * @brief Render toggle animation
 * @param controller Controller instance
 * @param device QP device handle
 * @param now Current timestamp
 */
void qp_toggle_render(qp_toggle_controller_t* controller, painter_device_t device, uint32_t now);

/**
 * @brief Get current toggle state
 * @param controller Controller instance
 * @return Current state (true=on, false=off)
 */
static inline bool qp_toggle_get_state(const qp_toggle_controller_t* controller) {
    return controller ? controller->current_state : false;
}

// ============================================================================
// Out-and-Back Animation Controller
// ============================================================================

/**
 * @brief Out-and-back animation controller
 *
 * Manages animations that run forward then reverse back to idle.
 * Perfect for boot animations and layer change effects.
 *
 * Features:
 * - Boot animation that runs once forward on initialization
 * - Triggered "out and back" animations (forward then reverse)
 * - Automatic return to idle state after completion
 * - Configurable steady frame (first or last frame of sequence)
 */
typedef struct {
    // Configuration
    const qp_image_sequence_t* sequence;   ///< Animation sequence
    uint16_t x, y;                         ///< Drawing position
    bool steady_at_end;                    ///< true=steady is last frame, false=steady is first frame
    bool auto_boot;                        ///< true=automatically start on init
    
    // Runtime state
    qp_animator_t animator;                ///< Low-level animator
    enum {
        QP_OAB_IDLE,                       ///< Not animating
        QP_OAB_FORWARD,                    ///< Animating forward
        QP_OAB_BACKWARD                    ///< Animating backward
    } phase;
    bool triggered;                        ///< true=animation was manually triggered
    bool completed;                        ///< true=full cycle completed
    uint32_t last_render_time;             ///< Last render timestamp
} qp_outback_controller_t;

/**
 * @brief Initialize out-and-back controller
 * @param controller Controller instance
 * @param sequence Animation sequence
 * @param x X drawing position
 * @param y Y drawing position
 * @param steady_at_end True if steady frame is last frame
 * @param auto_boot True to automatically start animation
 * @param now Current timestamp
 */
void qp_outback_init(qp_outback_controller_t* controller, const qp_image_sequence_t* sequence,
                     uint16_t x, uint16_t y, bool steady_at_end, bool auto_boot, uint32_t now);

/**
 * @brief Trigger out-and-back animation
 * @param controller Controller instance
 * @param now Current timestamp
 */
void qp_outback_trigger(qp_outback_controller_t* controller, uint32_t now);

/**
 * @brief Render out-and-back animation
 * @param controller Controller instance
 * @param device QP device handle
 * @param now Current timestamp
 */
void qp_outback_render(qp_outback_controller_t* controller, painter_device_t device, uint32_t now);

/**
 * @brief Check if out-and-back animation is active
 * @param controller Controller instance
 * @return True if animation is running
 */
static inline bool qp_outback_is_active(const qp_outback_controller_t* controller) {
    return controller && controller->phase != QP_OAB_IDLE;
}

// ============================================================================
// Convenience Macros
// ============================================================================

/**
 * @brief Initialize one-shot controller with auto-boot
 */
#define QP_ONESHOT_INIT_AUTOBOOT(ctrl, seq, x, y, steady_end, now) \
    qp_oneshot_init(&(ctrl), &(seq), (x), (y), (steady_end), true, (now))

/**
 * @brief Initialize toggle controller in off state
 */
#define QP_TOGGLE_INIT_OFF(ctrl, seq, x, y, steady_end, now) \
    qp_toggle_init(&(ctrl), &(seq), (x), (y), (steady_end), false, (now))

/**
 * @brief Initialize toggle controller in on state
 */
#define QP_TOGGLE_INIT_ON(ctrl, seq, x, y, steady_end, now) \
    qp_toggle_init(&(ctrl), &(seq), (x), (y), (steady_end), true, (now))

/**
 * @brief Initialize out-and-back controller with auto-boot
 */
#define QP_OUTBACK_INIT_AUTOBOOT(ctrl, seq, x, y, steady_end, now) \
    qp_outback_init(&(ctrl), &(seq), (x), (y), (steady_end), true, (now))

// ============================================================================
// Integration Notes
// ============================================================================

/**
 * @brief Migration from oled_utils controllers
 *
 * The QP controllers provide similar functionality to oled_utils controllers
 * but adapted for Quantum Painter's device model:
 *
 * Old oled_utils approach:
 * @code
 * oneshot_anim_t boot_anim;
 * oneshot_anim_init(&boot_anim, &boot_seq, 0, 0, true, true, now);
 * oneshot_anim_render(&boot_anim, now);
 * @endcode
 *
 * New qp_utils approach:
 * @code
 * qp_oneshot_controller_t boot_anim;
 * qp_oneshot_init(&boot_anim, &boot_seq, 0, 0, true, true, now);
 * qp_oneshot_render(&boot_anim, device, now);
 * @endcode
 */
