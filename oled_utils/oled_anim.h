/**
 * @file oled_anim.h
 * @brief Low-level animation engine with direction control and mid-flight reversal
 * 
 * This module provides:
 * - Frame sequence management with PROGMEM support
 * - Low-level animator with direction control and reversal capabilities
 * - State transition controllers for exclusive and binary widgets
 * - Reusable animations with enter/exit using the same frames
 * - Mid-flight reversal and cancellation support
 * - Chained transitions (exit old → enter new)
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include QMK_KEYBOARD_H
#include "oled_utils.h"  // for slice_t, clear_rect, draw_slice_px

/**
 * @brief Default frame duration in milliseconds
 * 
 * Override this in your config.h to change animation speed globally.
 * Lower values = faster animations, higher values = slower animations.
 */
#ifndef ANIM_FRAME_MS
#    define ANIM_FRAME_MS 80
#endif

// ============================================================================
// Frame Sequence Management
// ============================================================================

/**
 * @brief Frame sequence structure for animation data
 * 
 * Contains a contiguous array of slice_t frames that make up an animation.
 * Used by animators to step through frames in forward or reverse order.
 */
typedef struct {
    const slice_t *frames;  ///< Array of animation frames
    uint8_t        count;   ///< Number of frames in the sequence
} slice_seq_t;

/**
 * @brief Define a frame sequence from slice_t structures
 * 
 * Creates both the frame array and slice_seq_t structure in one declaration.
 * Use this macro to easily create animation sequences from your bitmap slices.
 * 
 * @param name Base name for the sequence (creates name##_frames array and name sequence)
 * @param ... Comma-separated list of slice_t structures
 * 
 * Example:
 * @code
 * DEFINE_SLICE_SEQ(my_anim, 
 *     SLICE16x8(frame1_data),
 *     SLICE16x8(frame2_data),
 *     SLICE16x8(frame3_data)
 * );
 * @endcode
 */
#define DEFINE_SLICE_SEQ(name, ...)                                \
    static const slice_t name##_frames[] = { __VA_ARGS__ };        \
    static const slice_seq_t name = {                               \
        name##_frames,                                              \
        (uint8_t)(sizeof(name##_frames) / sizeof(name##_frames[0])) \
    }

// ============================================================================
// Low-Level Frame Animator
// ============================================================================

/**
 * @brief Low-level frame animator with direction and reversal support
 * 
 * Manages stepping through a frame sequence with timing control.
 * Supports forward/backward playback and mid-flight direction reversal.
 */
typedef struct {
    const slice_t *frames;  ///< Frame array (not owned, points to slice_seq_t data)
    uint8_t        count;   ///< Number of frames in sequence
    int8_t         dir;     ///< Direction: +1 forward, -1 backward
    uint8_t        idx;     ///< Current frame index
    bool           active;  ///< Whether animation is currently running
    uint32_t       next_ms; ///< Timestamp for next frame advance
} animator_t;

/**
 * @brief Animation completion states
 */
typedef enum {
    ANIM_RUNNING = 0,       ///< Animation is still running
    ANIM_DONE_AT_START,     ///< Animation finished at frame 0
    ANIM_DONE_AT_END        ///< Animation finished at last frame
} anim_result_t;

/**
 * @brief Start an animation sequence
 * 
 * Initializes the animator to play the given sequence in the specified direction.
 * Sets up timing and positions the frame index appropriately.
 * 
 * @param a Animator instance to initialize
 * @param seq Frame sequence to animate
 * @param forward true for forward playback (0→N-1), false for reverse (N-1→0)
 * @param now Current timestamp from timer_read32()
 */
void animator_start(animator_t *a,
                    const slice_seq_t *seq,
                    bool forward,
                    uint32_t now);

/**
 * @brief Reverse animation direction mid-flight
 * 
 * Immediately flips the direction and continues from current frame.
 * Useful for canceling transitions or implementing reversible animations.
 * 
 * @param a Animator instance to reverse
 * @param now Current timestamp from timer_read32()
 */
void animator_reverse(animator_t *a, uint32_t now);

/**
 * @brief Draw the current frame with opaque clearing
 * 
 * Clears the frame area and draws the current frame. Uses opaque rendering
 * (clears background first) to prevent trails between frames of different sizes.
 * 
 * @param a Animator instance
 * @param x_px X coordinate for drawing
 * @param y_px Y coordinate for drawing
 */
void animator_draw_current(const animator_t *a, uint8_t x_px, uint8_t y_px);

/**
 * @brief Advance animation by one frame if timing allows
 * 
 * Checks if enough time has passed and advances to the next frame.
 * Returns completion status for transition management.
 * 
 * @param a Animator instance to advance
 * @param now Current timestamp from timer_read32()
 * @return Animation status (running, done at start, or done at end)
 */
anim_result_t animator_step(animator_t *a, uint32_t now);

/**
 * @brief Combined step and draw operation
 * 
 * Convenience function that draws current frame then advances animation.
 * Equivalent to calling animator_draw_current() followed by animator_step().
 * 
 * @param a Animator instance
 * @param x X coordinate for drawing
 * @param y Y coordinate for drawing  
 * @param now Current timestamp from timer_read32()
 * @return Animation status (running, done at start, or done at end)
 */
anim_result_t animator_step_and_draw(animator_t *a, uint8_t x, uint8_t y, uint32_t now);

/**
 * @brief Step animation and draw current frame with OR blending
 *
 * Same as animator_step_and_draw but uses OR blending (no background clear).
 * This allows multiple animations to be layered without overwriting each other.
 *
 * @param a Animator instance
 * @param x X coordinate for drawing
 * @param y Y coordinate for drawing
 * @param now Current timestamp from timer_read32()
 * @return Animation result (ANIM_RUNNING, ANIM_DONE_AT_START, ANIM_DONE_AT_END)
 */
anim_result_t animator_step_and_draw_blend(animator_t *a, uint8_t x, uint8_t y, uint32_t now);

// ============================================================================
// State Transition Controllers
// ============================================================================

/**
 * @brief Transition phases for exclusive state widgets
 * 
 * Manages the three-phase transition cycle: IDLE → EXIT → ENTER → IDLE
 */
typedef enum {
    TR_IDLE = 0,    ///< Steady state, showing current state's final frame
    TR_EXIT,        ///< Exiting current state (playing animation backward)
    TR_ENTER        ///< Entering new state (playing animation forward)
} tr_phase_t;

/**
 * @brief Exclusive state transition controller
 * 
 * Manages widgets that show exactly one state at a time (e.g., current layer).
 * Each state has its own animation sequence. Transitions use the same frames
 * for enter (forward) and exit (backward), ensuring visual consistency.
 * 
 * Features:
 * - Graceful mid-transition reversal and cancellation
 * - Queued target changes during transitions
 * - Chained exit→enter transitions for smooth state changes
 */
typedef struct {
    // Configuration
    const slice_seq_t * const *seq_map; ///< Array of sequences, one per state
    uint8_t state_count;                 ///< Number of valid states
    uint8_t x, y;                        ///< Drawing position

    // Runtime state
    animator_t anim;        ///< Low-level animator instance
    tr_phase_t phase;       ///< Current transition phase
    uint8_t    src;         ///< Currently visible/committed logical state
    uint8_t    dst;         ///< Target state for current transition chain
    uint8_t    pending;     ///< Queued desired state (0xFF = none)
    bool       initialized; ///< Whether controller has been initialized
} layer_transition_t;

/**
 * @brief Initialize exclusive state transition controller
 * 
 * Sets up the controller with state sequences and initial state.
 * Draws the steady frame for the initial state.
 * 
 * @param t Controller instance to initialize
 * @param seq_map Array of frame sequences, one per state
 * @param state_count Number of valid states
 * @param x X coordinate for drawing
 * @param y Y coordinate for drawing
 * @param initial_state Starting state index
 * @param now Current timestamp from timer_read32()
 */
void layer_tr_init(layer_transition_t *t,
                   const slice_seq_t * const *seq_map,
                   uint8_t state_count,
                   uint8_t x, uint8_t y,
                   uint8_t initial_state,
                   uint32_t now);

/**
 * @brief Request transition to a new state
 * 
 * Handles all transition logic including mid-flight reversal and queueing.
 * Can be called multiple times during a transition to change targets.
 * 
 * @param t Controller instance
 * @param desired_state Target state index
 * @param now Current timestamp from timer_read32()
 */
void layer_tr_request(layer_transition_t *t, uint8_t desired_state, uint32_t now);

/**
 * @brief Advance and render the transition controller
 * 
 * Call this every OLED tick to update animations and draw current frame.
 * Handles all transition phases and state management automatically.
 * 
 * @param t Controller instance
 * @param now Current timestamp from timer_read32()
 */
void layer_tr_render(layer_transition_t *t, uint32_t now);

/**
 * @brief Draw the steady frame for a specific state
 * 
 * Utility function to draw the final frame of a state's animation.
 * Useful for initialization or manual state display.
 * 
 * @param t Controller instance
 * @param state State index to draw
 */
void layer_tr_draw_steady(const layer_transition_t *t, uint8_t state);

// ============================================================================
// Binary Toggle Controllers
// ============================================================================

/**
 * @brief Toggle animation phases for binary widgets
 *
 * Manages independent on/off state with smooth transitions.
 */
typedef enum {
    TOG_IDLE_OFF = 0,   ///< Steady off state (showing first frame)
    TOG_ENTERING,       ///< Transitioning from off to on (forward animation)
    TOG_IDLE_ON,        ///< Steady on state (showing last frame)
    TOG_EXITING         ///< Transitioning from on to off (reverse animation)
} tog_phase_t;

/**
 * @brief Binary toggle animation controller
 *
 * Manages independent binary widgets like modifier indicators.
 * Uses a single animation sequence: off=first frame, on=last frame.
 * Supports mid-flight reversal for responsive state changes.
 */
typedef struct {
    // Configuration
    const slice_seq_t *seq; ///< Animation frames (off→on transition)
    uint8_t x, y;           ///< Drawing position

    // Runtime state
    animator_t  anim;       ///< Low-level animator instance
    tog_phase_t phase;      ///< Current toggle phase
    bool visible_on;        ///< Current steady state when idle
    bool desired_on;        ///< Target state
} toggle_anim_t;

/**
 * @brief Initialize binary toggle controller
 *
 * Sets up the toggle with animation sequence and initial state.
 * Draws the appropriate steady frame for the initial state.
 *
 * @param w Toggle controller instance
 * @param seq Animation sequence (off→on transition)
 * @param x X coordinate for drawing
 * @param y Y coordinate for drawing
 * @param initial_on Starting state (true=on, false=off)
 * @param now Current timestamp from timer_read32()
 */
void toggle_anim_init(toggle_anim_t *w, const slice_seq_t *seq,
                      uint8_t x, uint8_t y, bool initial_on, uint32_t now);

/**
 * @brief Set desired toggle state
 *
 * Requests transition to on or off state. Handles mid-flight reversal
 * if called while animation is in progress.
 *
 * @param w Toggle controller instance
 * @param want_on Desired state (true=on, false=off)
 * @param now Current timestamp from timer_read32()
 */
void toggle_anim_set(toggle_anim_t *w, bool want_on, uint32_t now);

/**
 * @brief Advance and render the toggle controller
 *
 * Call this every OLED tick to update animations and draw current frame.
 * Handles all toggle phases and state management automatically.
 *
 * @param w Toggle controller instance
 * @param now Current timestamp from timer_read32()
 */
void toggle_anim_render(toggle_anim_t *w, uint32_t now);

/**
 * @brief Advance and render the toggle controller with blend mode
 *
 * Same as toggle_anim_render but with optional OR blending support.
 * When use_or_blend is true, frames are drawn without clearing background.
 *
 * @param w Toggle controller instance
 * @param now Current timestamp from timer_read32()
 * @param use_or_blend true for OR blending (no clear), false for opaque (clear first)
 */
void toggle_anim_render_blend(toggle_anim_t *w, uint32_t now, bool use_or_blend);

// ============================================================================
// One-Shot Animation Controllers
// ============================================================================

/**
 * @brief One-shot animation phases
 *
 * Manages animations that run once and return to idle state.
 * Supports boot animations and triggered one-shots.
 */
typedef enum {
    ONESHOT_IDLE = 0,       ///< Idle state (showing steady frame)
    ONESHOT_BOOT,           ///< Boot animation in progress
    ONESHOT_TRIGGERED       ///< Triggered animation in progress
} oneshot_phase_t;

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
    const slice_seq_t *seq;     ///< Animation sequence
    uint8_t x, y;               ///< Drawing position
    bool steady_at_end;         ///< true=steady is last frame, false=steady is first frame

    // Runtime state
    animator_t      anim;       ///< Low-level animator instance
    oneshot_phase_t phase;      ///< Current animation phase
    bool            boot_done;  ///< Whether boot animation has completed
} oneshot_anim_t;

/**
 * @brief Initialize one-shot animation controller
 *
 * Sets up the controller and optionally starts the boot animation.
 * If run_boot_anim is true, the animation will start immediately.
 *
 * @param w One-shot controller instance
 * @param seq Animation sequence
 * @param x X coordinate for drawing
 * @param y Y coordinate for drawing
 * @param steady_at_end true=steady frame is last frame, false=steady frame is first frame
 * @param run_boot_anim true to start boot animation immediately, false to start idle
 * @param now Current timestamp from timer_read32()
 */
void oneshot_anim_init(oneshot_anim_t *w, const slice_seq_t *seq,
                       uint8_t x, uint8_t y, bool steady_at_end,
                       bool run_boot_anim, uint32_t now);

/**
 * @brief Trigger a one-shot animation
 *
 * Starts the animation sequence if not already running. If a boot animation
 * is in progress, this will be ignored. If already triggered, this resets
 * the animation to the beginning.
 *
 * @param w One-shot controller instance
 * @param now Current timestamp from timer_read32()
 */
void oneshot_anim_trigger(oneshot_anim_t *w, uint32_t now);

/**
 * @brief Advance and render the one-shot controller
 *
 * Call this every OLED tick to update animations and draw current frame.
 * Handles all phases and automatically returns to idle when complete.
 *
 * @param w One-shot controller instance
 * @param now Current timestamp from timer_read32()
 * @return true if animation just completed this frame, false otherwise
 */
bool oneshot_anim_render(oneshot_anim_t *w, uint32_t now);

/**
 * @brief Advance and render the one-shot controller with blend mode
 *
 * Same as oneshot_anim_render but with optional OR blending support.
 * When use_or_blend is true, frames are drawn without clearing background.
 *
 * @param w One-shot controller instance
 * @param now Current timestamp from timer_read32()
 * @param use_or_blend true for OR blending (no clear), false for opaque (clear first)
 * @return true if animation just completed this frame, false otherwise
 */
bool oneshot_anim_render_blend(oneshot_anim_t *w, uint32_t now, bool use_or_blend);

/**
 * @brief Check if one-shot animation is currently running
 *
 * @param w One-shot controller instance
 * @return true if animation is running, false if idle
 */
static inline bool oneshot_anim_is_running(const oneshot_anim_t *w) {
    return w->phase != ONESHOT_IDLE;
}

/**
 * @brief Check if boot animation has completed
 *
 * @param w One-shot controller instance
 * @return true if boot animation has finished, false otherwise
 */
static inline bool oneshot_anim_boot_done(const oneshot_anim_t *w) {
    return w->boot_done;
}

// ============================================================================
// Out-and-Back Animation Controllers
// ============================================================================

/**
 * @brief Out-and-back animation phases
 *
 * Manages animations that go "out" (forward) and then "back" (reverse) to idle.
 * Perfect for layer change indicators and other event-triggered effects.
 */
typedef enum {
    OUTBACK_IDLE = 0,       ///< Idle state (showing steady frame)
    OUTBACK_BOOT,           ///< Boot animation in progress (forward only)
    OUTBACK_OUT,            ///< Going "out" (forward animation)
    OUTBACK_BACK            ///< Coming "back" (reverse animation)
} outback_phase_t;

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
    const slice_seq_t *seq;     ///< Animation sequence
    uint8_t x, y;               ///< Drawing position
    bool steady_at_end;         ///< true=steady is last frame, false=steady is first frame

    // Runtime state
    animator_t       anim;      ///< Low-level animator instance
    outback_phase_t  phase;     ///< Current animation phase
    bool             boot_done; ///< Whether boot animation has completed
} outback_anim_t;

/**
 * @brief Initialize out-and-back animation controller
 *
 * Sets up the controller and optionally starts the boot animation.
 * If run_boot_anim is true, the animation will start immediately.
 *
 * @param w Out-and-back controller instance
 * @param seq Animation sequence
 * @param x X coordinate for drawing
 * @param y Y coordinate for drawing
 * @param steady_at_end true=steady frame is last frame, false=steady frame is first frame
 * @param run_boot_anim true to start boot animation immediately, false to start idle
 * @param now Current timestamp from timer_read32()
 */
void outback_anim_init(outback_anim_t *w, const slice_seq_t *seq,
                       uint8_t x, uint8_t y, bool steady_at_end,
                       bool run_boot_anim, uint32_t now);

/**
 * @brief Trigger an out-and-back animation
 *
 * Starts the "out and back" sequence if not already running. If a boot animation
 * is in progress, this will be ignored. If already running, this resets to the beginning.
 *
 * @param w Out-and-back controller instance
 * @param now Current timestamp from timer_read32()
 */
void outback_anim_trigger(outback_anim_t *w, uint32_t now);

/**
 * @brief Advance and render the out-and-back controller
 *
 * Call this every OLED tick to update animations and draw current frame.
 * Handles all phases and automatically returns to idle when complete.
 *
 * @param w Out-and-back controller instance
 * @param now Current timestamp from timer_read32()
 * @return true if animation just completed this frame, false otherwise
 */
bool outback_anim_render(outback_anim_t *w, uint32_t now);

/**
 * @brief Check if out-and-back animation is currently running
 *
 * @param w Out-and-back controller instance
 * @return true if animation is running, false if idle
 */
static inline bool outback_anim_is_running(const outback_anim_t *w) {
    return w->phase != OUTBACK_IDLE;
}

/**
 * @brief Check if boot animation has completed
 *
 * @param w Out-and-back controller instance
 * @return true if boot animation has finished, false otherwise
 */
static inline bool outback_anim_boot_done(const outback_anim_t *w) {
    return w->boot_done;
}

// ============================================================================
// Boot-Then-Reverse-Out-Back Animation Controllers
// ============================================================================

/**
 * @brief Boot-then-reverse-out-back animation phases
 *
 * Specialized controller for animations that:
 * 1. Boot: 0→end, stay at end
 * 2. Triggered: end→start→end (reverse out, then forward back)
 */
typedef enum {
    BOOTREV_IDLE = 0,       ///< Idle state (showing steady frame at end)
    BOOTREV_BOOT,           ///< Boot animation in progress (0→end)
    BOOTREV_OUT,            ///< Going "out" (end→start, reverse animation)
    BOOTREV_BACK            ///< Coming "back" (start→end, forward animation)
} bootrev_phase_t;

/**
 * @brief Boot-then-reverse-out-back animation controller
 *
 * Perfect for layer frame animations where:
 * - Boot runs 0→4 and stays at frame 4
 * - Layer changes trigger 4→0→4 animation
 *
 * Features:
 * - Boot animation runs forward once (0→end)
 * - Triggered animations go reverse then forward (end→start→end)
 * - Steady state is always the last frame
 */
typedef struct {
    // Configuration
    const slice_seq_t *seq;     ///< Animation sequence
    uint8_t x, y;               ///< Drawing position

    // Runtime state
    animator_t       anim;      ///< Low-level animator instance
    bootrev_phase_t  phase;     ///< Current animation phase
    bool             boot_done; ///< Whether boot animation has completed
} bootrev_anim_t;

/**
 * @brief Initialize boot-then-reverse-out-back animation controller
 *
 * Sets up the controller and optionally starts the boot animation.
 * Steady state is always the last frame of the sequence.
 *
 * @param w Boot-reverse controller instance
 * @param seq Animation sequence
 * @param x X coordinate for drawing
 * @param y Y coordinate for drawing
 * @param run_boot_anim true to start boot animation immediately, false to start idle
 * @param now Current timestamp from timer_read32()
 */
void bootrev_anim_init(bootrev_anim_t *w, const slice_seq_t *seq,
                       uint8_t x, uint8_t y, bool run_boot_anim, uint32_t now);

/**
 * @brief Trigger a reverse-out-back animation
 *
 * Starts the end→start→end sequence if not already running. If a boot animation
 * is in progress, this will be ignored. If already running, this resets to the beginning.
 *
 * @param w Boot-reverse controller instance
 * @param now Current timestamp from timer_read32()
 */
void bootrev_anim_trigger(bootrev_anim_t *w, uint32_t now);

/**
 * @brief Advance and render the boot-reverse controller
 *
 * Call this every OLED tick to update animations and draw current frame.
 * Handles all phases and automatically returns to idle when complete.
 *
 * @param w Boot-reverse controller instance
 * @param now Current timestamp from timer_read32()
 * @return true if animation just completed this frame, false otherwise
 */
bool bootrev_anim_render(bootrev_anim_t *w, uint32_t now);

/**
 * @brief Advance and render the boot-reverse controller with blend mode
 *
 * Same as bootrev_anim_render but with optional OR blending support.
 * When use_or_blend is true, frames are drawn without clearing background.
 *
 * @param w Boot-reverse controller instance
 * @param now Current timestamp from timer_read32()
 * @param use_or_blend true for OR blending (no clear), false for opaque (clear first)
 * @return true if animation just completed this frame, false otherwise
 */
bool bootrev_anim_render_blend(bootrev_anim_t *w, uint32_t now, bool use_or_blend);

/**
 * @brief Check if boot-reverse animation is currently running
 *
 * @param w Boot-reverse controller instance
 * @return true if animation is running, false if idle
 */
static inline bool bootrev_anim_is_running(const bootrev_anim_t *w) {
    return w->phase != BOOTREV_IDLE;
}

/**
 * @brief Check if boot animation has completed
 *
 * @param w Boot-reverse controller instance
 * @return true if boot animation has finished, false otherwise
 */
static inline bool bootrev_anim_boot_done(const bootrev_anim_t *w) {
    return w->boot_done;
}
