/**
 * @file qp_anim.c
 * @brief Implementation of animation engine for Quantum Painter
 */

#include QMK_KEYBOARD_H
#include "qp_anim.h"

// ============================================================================
// Internal Helper Functions
// ============================================================================

/**
 * @brief Calculate next frame timestamp
 * @param anim Animator instance
 * @param now Current timestamp
 */
static void update_next_timestamp(qp_animator_t* anim, uint32_t now) {
    if (!anim || !anim->sequence) return;
    
    anim->next_ms = now + anim->sequence->frame_duration_ms;
}

/**
 * @brief Validate animator state
 * @param anim Animator instance
 * @return True if animator is in valid state
 */
static bool is_animator_valid(const qp_animator_t* anim) {
    return anim && anim->sequence && anim->sequence->frames && anim->sequence->count > 0;
}

// ============================================================================
// Animator Control Functions
// ============================================================================

void qp_animator_start(qp_animator_t* anim, const qp_image_sequence_t* sequence, bool forward, uint32_t now) {
    if (!anim || !sequence || !sequence->frames || sequence->count == 0) {
        return;
    }
    
    anim->sequence = sequence;
    anim->dir = forward ? 1 : -1;
    anim->idx = forward ? 0 : (sequence->count - 1);
    anim->active = true;
    
    update_next_timestamp(anim, now);
}

void qp_animator_stop(qp_animator_t* anim) {
    if (!anim) return;
    
    anim->active = false;
    anim->next_ms = 0;
}

void qp_animator_reverse(qp_animator_t* anim, uint32_t now) {
    if (!is_animator_valid(anim) || !anim->active) return;
    
    anim->dir = -anim->dir;
    update_next_timestamp(anim, now);
}

// ============================================================================
// Animation Step Functions
// ============================================================================

qp_anim_result_t qp_animator_step(qp_animator_t* anim, uint32_t now) {
    if (!is_animator_valid(anim) || !anim->active) {
        return QP_ANIM_DONE_AT_START;
    }
    
    // Check if it's time to advance
    if (TIMER_DIFF_32(now, anim->next_ms) < 0) {
        return QP_ANIM_RUNNING;
    }
    
    // Advance frame
    int16_t new_idx = anim->idx + anim->dir;
    
    // Handle boundary conditions
    if (new_idx < 0) {
        if (anim->sequence->loop) {
            anim->idx = anim->sequence->count - 1;
        } else {
            anim->idx = 0;
            anim->active = false;
            return QP_ANIM_DONE_AT_START;
        }
    } else if (new_idx >= anim->sequence->count) {
        if (anim->sequence->loop) {
            anim->idx = 0;
        } else {
            anim->idx = anim->sequence->count - 1;
            anim->active = false;
            return QP_ANIM_DONE_AT_END;
        }
    } else {
        anim->idx = new_idx;
    }
    
    update_next_timestamp(anim, now);
    return QP_ANIM_RUNNING;
}

qp_anim_result_t qp_animator_step_and_draw(qp_animator_t* anim, painter_device_t device, uint16_t x, uint16_t y, uint32_t now) {
    qp_anim_result_t result = qp_animator_step(anim, now);
    
    if (qp_animator_is_active(anim)) {
        qp_animator_draw_current(anim, device, x, y);
    }
    
    return result;
}

qp_anim_result_t qp_animator_step_and_draw_cleared(qp_animator_t* anim, painter_device_t device, 
                                                   uint16_t x, uint16_t y, const qp_rect_t* clear_rect, uint32_t now) {
    qp_anim_result_t result = qp_animator_step(anim, now);
    
    if (qp_animator_is_active(anim)) {
        // Clear background if requested
        if (clear_rect) {
            qp_clear_rect(device, clear_rect->x, clear_rect->y, clear_rect->w, clear_rect->h);
        }
        
        qp_animator_draw_current(anim, device, x, y);
    }
    
    return result;
}

// ============================================================================
// Frame Access Functions
// ============================================================================

const qp_image_t* qp_animator_current_image(const qp_animator_t* anim) {
    if (!is_animator_valid(anim)) return NULL;

    return qp_sequence_get_frame(anim->sequence, anim->idx);
}

const qp_image_t* qp_animator_get_frame(const qp_animator_t* anim, uint8_t index) {
    if (!is_animator_valid(anim)) return NULL;

    return qp_sequence_get_frame(anim->sequence, index);
}

const qp_image_t* qp_animator_last_frame(const qp_animator_t* anim) {
    if (!is_animator_valid(anim)) return NULL;

    return qp_sequence_get_frame(anim->sequence, anim->sequence->count - 1);
}

// ============================================================================
// Timing and Control Utilities
// ============================================================================

void qp_animator_set_frame_duration(qp_animator_t* anim, uint16_t duration_ms, uint32_t now) {
    if (!is_animator_valid(anim)) return;

    // Note: This modifies the sequence duration, affecting all animators using this sequence
    // In a more sophisticated implementation, we might want per-animator timing overrides
    ((qp_image_sequence_t*)anim->sequence)->frame_duration_ms = duration_ms;

    if (anim->active) {
        update_next_timestamp(anim, now);
    }
}

bool qp_animator_jump_to_frame(qp_animator_t* anim, uint8_t index, uint32_t now) {
    if (!is_animator_valid(anim) || index >= anim->sequence->count) {
        return false;
    }

    anim->idx = index;

    if (anim->active) {
        update_next_timestamp(anim, now);
    }

    return true;
}

void qp_animator_reset(qp_animator_t* anim, uint32_t now) {
    if (!is_animator_valid(anim)) return;

    anim->idx = (anim->dir > 0) ? 0 : (anim->sequence->count - 1);

    if (anim->active) {
        update_next_timestamp(anim, now);
    }
}

// ============================================================================
// Drawing Utilities
// ============================================================================

bool qp_animator_draw_current(const qp_animator_t* anim, painter_device_t device, uint16_t x, uint16_t y) {
    const qp_image_t* image = qp_animator_current_image(anim);
    if (!image) return false;

    return qp_draw_image(device, image, x, y);
}

bool qp_animator_draw_frame(const qp_animator_t* anim, painter_device_t device, uint16_t x, uint16_t y, uint8_t frame_index) {
    const qp_image_t* image = qp_animator_get_frame(anim, frame_index);
    if (!image) return false;

    return qp_draw_image(device, image, x, y);
}

bool qp_animator_draw_frame_tinted(const qp_animator_t* anim, painter_device_t device, uint16_t x, uint16_t y,
                                   uint8_t frame_index, qp_color_t tint_color) {
    const qp_image_t* image = qp_animator_get_frame(anim, frame_index);
    if (!image) return false;

    return qp_draw_image_tinted(device, image, x, y, tint_color);
}
