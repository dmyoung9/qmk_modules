/**
 * @file qp_anim.h
 * @brief Animation engine for Quantum Painter with direction control and mid-flight reversal
 * 
 * This module provides:
 * - Frame sequence management with QGF image support
 * - Low-level animator with direction control and reversal capabilities
 * - Timing control and frame advancement
 * - Integration with qp_image system
 * - Device-agnostic animation rendering
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include QMK_KEYBOARD_H
#include "qp_image.h"
#include "qp_utils.h"

// ============================================================================
// Animation Result Types
// ============================================================================

/**
 * @brief Animation step result
 * 
 * Indicates the state of an animation after a step operation.
 */
typedef enum {
    QP_ANIM_RUNNING,        ///< Animation is still running
    QP_ANIM_DONE_AT_START,  ///< Animation completed at first frame
    QP_ANIM_DONE_AT_END     ///< Animation completed at last frame
} qp_anim_result_t;

// ============================================================================
// Core Animation Types
// ============================================================================

/**
 * @brief Low-level frame animator with direction and reversal support
 * 
 * Manages stepping through an image sequence with timing control.
 * Supports forward/backward playback and mid-flight direction reversal.
 */
typedef struct {
    const qp_image_sequence_t* sequence;  ///< Image sequence to animate
    int8_t         dir;                   ///< Direction: +1 forward, -1 backward
    uint8_t        idx;                   ///< Current frame index
    bool           active;                ///< Whether animation is currently running
    uint32_t       next_ms;               ///< Timestamp for next frame advance
} qp_animator_t;

// ============================================================================
// Animator Control Functions
// ============================================================================

/**
 * @brief Start animation from beginning
 * @param anim Animator instance
 * @param sequence Image sequence to animate
 * @param forward True for forward, false for backward
 * @param now Current timestamp
 */
void qp_animator_start(qp_animator_t* anim, const qp_image_sequence_t* sequence, bool forward, uint32_t now);

/**
 * @brief Stop animation
 * @param anim Animator instance
 */
void qp_animator_stop(qp_animator_t* anim);

/**
 * @brief Reverse animation direction
 * @param anim Animator instance
 * @param now Current timestamp
 */
void qp_animator_reverse(qp_animator_t* anim, uint32_t now);

/**
 * @brief Check if animation is active
 * @param anim Animator instance
 * @return True if animation is running
 */
static inline bool qp_animator_is_active(const qp_animator_t* anim) {
    return anim && anim->active && anim->sequence;
}

/**
 * @brief Get current frame index
 * @param anim Animator instance
 * @return Current frame index
 */
static inline uint8_t qp_animator_current_frame(const qp_animator_t* anim) {
    return anim ? anim->idx : 0;
}

// ============================================================================
// Animation Step Functions
// ============================================================================

/**
 * @brief Step animation forward by one frame if time has elapsed
 * @param anim Animator instance
 * @param now Current timestamp
 * @return Animation result
 */
qp_anim_result_t qp_animator_step(qp_animator_t* anim, uint32_t now);

/**
 * @brief Step animation and draw current frame
 * @param anim Animator instance
 * @param device QP device handle
 * @param x X coordinate to draw at
 * @param y Y coordinate to draw at
 * @param now Current timestamp
 * @return Animation result
 */
qp_anim_result_t qp_animator_step_and_draw(qp_animator_t* anim, painter_device_t device, uint16_t x, uint16_t y, uint32_t now);

/**
 * @brief Step animation and draw with background clearing
 * @param anim Animator instance
 * @param device QP device handle
 * @param x X coordinate to draw at
 * @param y Y coordinate to draw at
 * @param clear_rect Rectangle to clear before drawing (NULL for no clearing)
 * @param now Current timestamp
 * @return Animation result
 */
qp_anim_result_t qp_animator_step_and_draw_cleared(qp_animator_t* anim, painter_device_t device, 
                                                   uint16_t x, uint16_t y, const qp_rect_t* clear_rect, uint32_t now);

// ============================================================================
// Frame Access Functions
// ============================================================================

/**
 * @brief Get current frame image
 * @param anim Animator instance
 * @return Pointer to current frame image or NULL
 */
const qp_image_t* qp_animator_current_image(const qp_animator_t* anim);

/**
 * @brief Get frame at specific index
 * @param anim Animator instance
 * @param index Frame index
 * @return Pointer to frame image or NULL if invalid
 */
const qp_image_t* qp_animator_get_frame(const qp_animator_t* anim, uint8_t index);

/**
 * @brief Get first frame of animation
 * @param anim Animator instance
 * @return Pointer to first frame or NULL
 */
static inline const qp_image_t* qp_animator_first_frame(const qp_animator_t* anim) {
    return qp_animator_get_frame(anim, 0);
}

/**
 * @brief Get last frame of animation
 * @param anim Animator instance
 * @return Pointer to last frame or NULL
 */
const qp_image_t* qp_animator_last_frame(const qp_animator_t* anim);

// ============================================================================
// Timing and Control Utilities
// ============================================================================

/**
 * @brief Set custom frame duration for current animation
 * @param anim Animator instance
 * @param duration_ms New frame duration in milliseconds
 * @param now Current timestamp
 */
void qp_animator_set_frame_duration(qp_animator_t* anim, uint16_t duration_ms, uint32_t now);

/**
 * @brief Jump to specific frame
 * @param anim Animator instance
 * @param index Frame index to jump to
 * @param now Current timestamp
 * @return True if successful
 */
bool qp_animator_jump_to_frame(qp_animator_t* anim, uint8_t index, uint32_t now);

/**
 * @brief Reset animation to beginning
 * @param anim Animator instance
 * @param now Current timestamp
 */
void qp_animator_reset(qp_animator_t* anim, uint32_t now);

// ============================================================================
// Drawing Utilities
// ============================================================================

/**
 * @brief Draw current frame without stepping
 * @param anim Animator instance
 * @param device QP device handle
 * @param x X coordinate
 * @param y Y coordinate
 * @return True if successful
 */
bool qp_animator_draw_current(const qp_animator_t* anim, painter_device_t device, uint16_t x, uint16_t y);

/**
 * @brief Draw specific frame by index
 * @param anim Animator instance
 * @param device QP device handle
 * @param x X coordinate
 * @param y Y coordinate
 * @param frame_index Frame index to draw
 * @return True if successful
 */
bool qp_animator_draw_frame(const qp_animator_t* anim, painter_device_t device, uint16_t x, uint16_t y, uint8_t frame_index);

/**
 * @brief Draw frame with color tinting
 * @param anim Animator instance
 * @param device QP device handle
 * @param x X coordinate
 * @param y Y coordinate
 * @param frame_index Frame index to draw
 * @param tint_color Color to tint the frame
 * @return True if successful
 */
bool qp_animator_draw_frame_tinted(const qp_animator_t* anim, painter_device_t device, uint16_t x, uint16_t y, 
                                   uint8_t frame_index, qp_color_t tint_color);

// ============================================================================
// Convenience Macros
// ============================================================================

/**
 * @brief Initialize animator with sequence
 * @param anim_var Animator variable name
 * @param seq_var Sequence variable name
 */
#define QP_ANIMATOR_INIT(anim_var, seq_var) \
    qp_animator_t anim_var = {.sequence = &(seq_var), .active = false}

/**
 * @brief Start forward animation
 * @param anim Animator instance
 * @param seq Sequence to animate
 * @param now Current timestamp
 */
#define QP_ANIMATOR_START_FORWARD(anim, seq, now) \
    qp_animator_start(&(anim), &(seq), true, (now))

/**
 * @brief Start backward animation
 * @param anim Animator instance
 * @param seq Sequence to animate
 * @param now Current timestamp
 */
#define QP_ANIMATOR_START_BACKWARD(anim, seq, now) \
    qp_animator_start(&(anim), &(seq), false, (now))

// ============================================================================
// Integration Notes
// ============================================================================

/**
 * @brief Migration from oled_anim
 *
 * The QP animation engine provides similar functionality to oled_anim
 * but adapted for Quantum Painter's image system:
 *
 * Old oled_anim approach:
 * @code
 * DEFINE_SLICE_SEQ(my_anim, SLICE16x8(frame1), SLICE16x8(frame2));
 * animator_t anim;
 * animator_start(&anim, &my_anim, true, timer_read32());
 * animator_step_and_draw(&anim, 0, 0, timer_read32());
 * @endcode
 *
 * New qp_anim approach:
 * @code
 * QP_DEFINE_SEQUENCE(my_anim, 100, true, frame1_img, frame2_img);
 * qp_animator_t anim;
 * qp_animator_start(&anim, &my_anim, true, timer_read32());
 * qp_animator_step_and_draw(&anim, device, 0, 0, timer_read32());
 * @endcode
 */
