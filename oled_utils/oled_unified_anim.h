/**
 * @file oled_unified_anim.h
 * @brief Unified animation controller that consolidates all animation patterns
 *
 * This module provides a single, flexible animation controller that can handle:
 * - One-shot animations (boot, triggered)
 * - Out-and-back animations (forward then reverse)
 * - Boot-then-reverse-out-back animations
 * - Binary toggle animations (on/off states)
 * - Layer transition animations (exclusive state management)
 *
 * This replaces the multiple specialized controllers with a single, configurable one.
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include QMK_KEYBOARD_H

#include "oled_slice.h"   // slice_t
#include "oled_utils.h"   // clear_rect, draw_slice_px
#include "oled_anim.h"    // slice_seq_t, animator_t, anim_result_t

// ============================================================================
// Animation Behavior Configuration
// ============================================================================

/**
 * @brief Animation behavior patterns
 *
 * Defines the high-level behavior pattern for the animation controller.
 * Each pattern determines how the controller responds to triggers and
 * what sequence of animations it performs.
 */
typedef enum {
    ANIM_ONESHOT = 0,       ///< Run once forward, return to steady state
    ANIM_OUTBACK,           ///< Run forward, then reverse back to start
    ANIM_TOGGLE,            ///< Binary on/off state with smooth transitions
    ANIM_BOOTREV,           ///< Boot forward, triggered reverse-out-back
    ANIM_LAYER_TRANSITION   ///< Exclusive state transitions with exit/enter
} anim_behavior_t;

/**
 * @brief Steady state configuration
 *
 * Determines which frame is shown when the animation is idle.
 */
typedef enum {
    STEADY_FIRST = 0,       ///< Show first frame when idle
    STEADY_LAST = 1         ///< Show last frame when idle
} steady_frame_t;

/**
 * @brief Blending mode for rendering
 */
typedef enum {
    BLEND_OPAQUE = 0,       ///< Clear background before drawing (default)
    BLEND_ADDITIVE = 1      ///< OR-blend without clearing background
} blend_mode_t;

/**
 * @brief Unified animation configuration
 *
 * Configures the behavior, appearance, and timing of the animation controller.
 * This single structure replaces the multiple specialized controller configs.
 */
typedef struct {
    // Animation data
    const slice_seq_t *seq;         ///< Animation frames
    
    // Behavior configuration
    anim_behavior_t    behavior;    ///< Animation behavior pattern
    steady_frame_t     steady;      ///< Which frame to show when idle
    blend_mode_t       blend;       ///< Rendering blend mode
    
    // Layout
    uint8_t x, y;                   ///< Drawing position
    
    // Boot animation
    bool run_boot_anim;             ///< Whether to run boot animation on init
    
    // Layer transition specific (only used for ANIM_LAYER_TRANSITION)
    const slice_seq_t * const *seq_map;  ///< Array of sequences for each state
    uint8_t state_count;                  ///< Number of states
} unified_anim_config_t;

// ============================================================================
// Unified Animation Controller
// ============================================================================

/**
 * @brief Animation phases for unified controller
 *
 * Covers all possible animation states across all behavior patterns.
 */
typedef enum {
    PHASE_IDLE = 0,         ///< Idle state (showing steady frame)
    PHASE_BOOT,             ///< Boot animation in progress
    PHASE_FORWARD,          ///< Forward animation (enter, out, on)
    PHASE_REVERSE,          ///< Reverse animation (exit, back, off)
    PHASE_TRANSITION        ///< Layer transition in progress
} anim_phase_t;

/**
 * @brief Unified animation controller runtime state
 *
 * Single controller that can handle all animation patterns through configuration.
 * Replaces oneshot_anim_t, outback_anim_t, bootrev_anim_t, toggle_anim_t, etc.
 */
typedef struct {
    const unified_anim_config_t *cfg;   ///< Configuration (not owned)
    
    // Core animation state
    animator_t    anim;                 ///< Low-level animator
    anim_phase_t  phase;                ///< Current animation phase
    
    // State management
    uint8_t current_state;              ///< Current visible state (for layer transitions)
    uint8_t target_state;               ///< Target state (for layer transitions)
    uint8_t pending_state;              ///< Queued state change (0xFF = none)
    
    // Status flags
    bool boot_done;                     ///< Whether boot animation completed
    bool visible_on;                    ///< Current on/off state (for toggle behavior)
    bool desired_on;                    ///< Target on/off state (for toggle behavior)
    
    // Timing
    uint32_t last_trigger;              ///< Timestamp of last trigger
} unified_anim_t;

// ============================================================================
// Unified Animation API
// ============================================================================

/**
 * @brief Initialize unified animation controller
 *
 * Sets up the controller with the given configuration. The behavior is
 * determined by the config->behavior field.
 *
 * @param w Controller instance to initialize
 * @param cfg Configuration (must remain valid for controller lifetime)
 * @param initial_state Initial state (for layer transitions) or on/off (for toggles)
 * @param now Current timestamp from timer_read32()
 */
void unified_anim_init(unified_anim_t *w, const unified_anim_config_t *cfg,
                       uint8_t initial_state, uint32_t now);

/**
 * @brief Trigger animation or state change
 *
 * Behavior depends on the configured animation pattern:
 * - ONESHOT: Triggers one-shot animation
 * - OUTBACK: Triggers out-and-back sequence
 * - TOGGLE: Toggles on/off state
 * - BOOTREV: Triggers reverse-out-back sequence
 * - LAYER_TRANSITION: Changes to specified state
 *
 * @param w Controller instance
 * @param state_or_toggle For layer transitions: target state. For toggles: 1=on, 0=off. Ignored for others.
 * @param now Current timestamp from timer_read32()
 */
void unified_anim_trigger(unified_anim_t *w, uint8_t state_or_toggle, uint32_t now);

/**
 * @brief Update and render unified animation controller
 *
 * Call this every OLED frame to update animations and draw current frame.
 * Handles all animation phases and state management automatically.
 *
 * @param w Controller instance
 * @param now Current timestamp from timer_read32()
 * @return true if animation just completed this frame, false otherwise
 */
bool unified_anim_render(unified_anim_t *w, uint32_t now);

/**
 * @brief Check if animation is currently running
 *
 * @param w Controller instance
 * @return true if any animation is in progress, false if idle
 */
static inline bool unified_anim_is_running(const unified_anim_t *w) {
    return w->phase != PHASE_IDLE;
}

/**
 * @brief Check if boot animation has completed
 *
 * @param w Controller instance
 * @return true if boot animation finished or was not configured, false otherwise
 */
static inline bool unified_anim_boot_done(const unified_anim_t *w) {
    return w->boot_done;
}

// ============================================================================
// Configuration Helper Macros
// ============================================================================

/**
 * @brief Create oneshot animation configuration
 */
#define UNIFIED_ONESHOT_CONFIG(seq_ptr, x_pos, y_pos, steady_frame, run_boot) \
    { (seq_ptr), ANIM_ONESHOT, (steady_frame), BLEND_OPAQUE, (x_pos), (y_pos), (run_boot), NULL, 0 }

/**
 * @brief Create out-and-back animation configuration
 */
#define UNIFIED_OUTBACK_CONFIG(seq_ptr, x_pos, y_pos, steady_frame, run_boot) \
    { (seq_ptr), ANIM_OUTBACK, (steady_frame), BLEND_OPAQUE, (x_pos), (y_pos), (run_boot), NULL, 0 }

/**
 * @brief Create toggle animation configuration
 */
#define UNIFIED_TOGGLE_CONFIG(seq_ptr, x_pos, y_pos, blend_mode) \
    { (seq_ptr), ANIM_TOGGLE, STEADY_FIRST, (blend_mode), (x_pos), (y_pos), false, NULL, 0 }

/**
 * @brief Create boot-reverse animation configuration
 */
#define UNIFIED_BOOTREV_CONFIG(seq_ptr, x_pos, y_pos, run_boot) \
    { (seq_ptr), ANIM_BOOTREV, STEADY_LAST, BLEND_OPAQUE, (x_pos), (y_pos), (run_boot), NULL, 0 }

/**
 * @brief Create layer transition animation configuration
 */
#define UNIFIED_LAYER_CONFIG(seq_map_ptr, state_cnt, x_pos, y_pos) \
    { NULL, ANIM_LAYER_TRANSITION, STEADY_LAST, BLEND_OPAQUE, (x_pos), (y_pos), false, (seq_map_ptr), (state_cnt) }
