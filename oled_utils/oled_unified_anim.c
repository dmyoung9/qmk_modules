/**
 * @file oled_unified_anim.c
 * @brief Implementation of unified animation controller
 */

#include QMK_KEYBOARD_H
#include "oled_unified_anim.h"

// ============================================================================
// Internal Helper Functions
// ============================================================================

/**
 * @brief Get the sequence for the current state (layer transitions only)
 */
static const slice_seq_t *get_current_sequence(const unified_anim_t *w) {
    if (w->cfg->behavior != ANIM_LAYER_TRANSITION || !w->cfg->seq_map) {
        return w->cfg->seq;
    }
    
    if (w->current_state < w->cfg->state_count) {
        return w->cfg->seq_map[w->current_state];
    }
    
    return NULL;
}

/**
 * @brief Get the steady frame for current configuration
 */
static const slice_t *get_steady_frame(const unified_anim_t *w) {
    const slice_seq_t *seq = get_current_sequence(w);
    if (!seq || !seq->count) return NULL;
    
    switch (w->cfg->behavior) {
        case ANIM_TOGGLE:
            // Toggle: off=first frame, on=last frame
            return w->visible_on ? &seq->frames[seq->count - 1] : &seq->frames[0];
            
        case ANIM_BOOTREV:
            // Boot-reverse: always last frame
            return &seq->frames[seq->count - 1];
            
        default:
            // Others: use configured steady frame
            return (w->cfg->steady == STEADY_LAST) 
                ? &seq->frames[seq->count - 1] 
                : &seq->frames[0];
    }
}

/**
 * @brief Draw the steady frame with appropriate blending
 */
static void draw_steady_frame(const unified_anim_t *w) {
    const slice_t *frame = get_steady_frame(w);
    if (!frame) return;
    
    if (w->cfg->blend == BLEND_OPAQUE) {
        clear_rect(w->cfg->x, w->cfg->y, frame->width, slice_height_px(frame));
    }
    draw_slice_px(frame, w->cfg->x, w->cfg->y);
}

/**
 * @brief Render current animation frame with appropriate blending
 */
static anim_result_t render_animation_frame(unified_anim_t *w, uint32_t now) {
    if (w->cfg->blend == BLEND_ADDITIVE) {
        return animator_step_and_draw_blend(&w->anim, w->cfg->x, w->cfg->y, now);
    } else {
        return animator_step_and_draw(&w->anim, w->cfg->x, w->cfg->y, now);
    }
}

// ============================================================================
// Behavior-Specific Logic
// ============================================================================

/**
 * @brief Handle oneshot animation behavior
 */
static bool handle_oneshot_behavior(unified_anim_t *w, uint32_t now) {
    switch (w->phase) {
        case PHASE_IDLE:
            draw_steady_frame(w);
            return false;
            
        case PHASE_BOOT:
        case PHASE_FORWARD: {
            anim_result_t r = render_animation_frame(w, now);
            if (r == ANIM_DONE_AT_END) {
                w->phase = PHASE_IDLE;
                if (w->phase == PHASE_BOOT) w->boot_done = true;
                draw_steady_frame(w);
                return true;
            }
            return false;
        }
        
        default:
            return false;
    }
}

/**
 * @brief Handle out-and-back animation behavior
 */
static bool handle_outback_behavior(unified_anim_t *w, uint32_t now) {
    switch (w->phase) {
        case PHASE_IDLE:
            draw_steady_frame(w);
            return false;
            
        case PHASE_BOOT: {
            anim_result_t r = render_animation_frame(w, now);
            if (r == ANIM_DONE_AT_END) {
                w->phase = PHASE_IDLE;
                w->boot_done = true;
                draw_steady_frame(w);
                return true;
            }
            return false;
        }
        
        case PHASE_FORWARD: {
            anim_result_t r = render_animation_frame(w, now);
            if (r == ANIM_DONE_AT_END) {
                // Start reverse phase
                animator_start(&w->anim, get_current_sequence(w), false, now);
                w->phase = PHASE_REVERSE;
            }
            return false;
        }
        
        case PHASE_REVERSE: {
            anim_result_t r = render_animation_frame(w, now);
            if (r == ANIM_DONE_AT_START) {
                w->phase = PHASE_IDLE;
                draw_steady_frame(w);
                return true;
            }
            return false;
        }
        
        default:
            return false;
    }
}

/**
 * @brief Handle toggle animation behavior
 */
static bool handle_toggle_behavior(unified_anim_t *w, uint32_t now) {
    switch (w->phase) {
        case PHASE_IDLE:
            draw_steady_frame(w);
            return false;
            
        case PHASE_FORWARD: {
            // Transitioning to ON state
            anim_result_t r = render_animation_frame(w, now);
            if (r == ANIM_DONE_AT_END) {
                w->phase = PHASE_IDLE;
                w->visible_on = true;
                draw_steady_frame(w);
                return true;
            }
            return false;
        }
        
        case PHASE_REVERSE: {
            // Transitioning to OFF state
            anim_result_t r = render_animation_frame(w, now);
            if (r == ANIM_DONE_AT_START) {
                w->phase = PHASE_IDLE;
                w->visible_on = false;
                draw_steady_frame(w);
                return true;
            }
            return false;
        }
        
        default:
            return false;
    }
}

/**
 * @brief Handle boot-reverse animation behavior
 */
static bool handle_bootrev_behavior(unified_anim_t *w, uint32_t now) {
    switch (w->phase) {
        case PHASE_IDLE:
            draw_steady_frame(w);
            return false;
            
        case PHASE_BOOT: {
            anim_result_t r = render_animation_frame(w, now);
            if (r == ANIM_DONE_AT_END) {
                w->phase = PHASE_IDLE;
                w->boot_done = true;
                draw_steady_frame(w);
                return true;
            }
            return false;
        }
        
        case PHASE_REVERSE: {
            // Reverse phase (end→start)
            anim_result_t r = render_animation_frame(w, now);
            if (r == ANIM_DONE_AT_START) {
                // Start forward phase (start→end)
                animator_start(&w->anim, get_current_sequence(w), true, now);
                w->phase = PHASE_FORWARD;
            }
            return false;
        }
        
        case PHASE_FORWARD: {
            // Forward phase (start→end)
            anim_result_t r = render_animation_frame(w, now);
            if (r == ANIM_DONE_AT_END) {
                w->phase = PHASE_IDLE;
                draw_steady_frame(w);
                return true;
            }
            return false;
        }
        
        default:
            return false;
    }
}

// ============================================================================
// Public API Implementation
// ============================================================================

void unified_anim_init(unified_anim_t *w, const unified_anim_config_t *cfg,
                       uint8_t initial_state, uint32_t now) {
    w->cfg = cfg;
    w->anim.active = false;
    w->phase = PHASE_IDLE;
    w->current_state = initial_state;
    w->target_state = initial_state;
    w->pending_state = 0xFF;
    w->boot_done = false;
    w->visible_on = (initial_state != 0);
    w->desired_on = w->visible_on;
    w->last_trigger = now;
    
    // Start boot animation if configured
    if (cfg->run_boot_anim) {
        const slice_seq_t *seq = get_current_sequence(w);
        if (seq && seq->count) {
            animator_start(&w->anim, seq, true, now);
            w->phase = PHASE_BOOT;
            return;
        }
    }
    
    // No boot animation, mark as done and draw steady frame
    w->boot_done = true;
    draw_steady_frame(w);
}

void unified_anim_trigger(unified_anim_t *w, uint8_t state_or_toggle, uint32_t now) {
    const slice_seq_t *seq = get_current_sequence(w);
    if (!seq || !seq->count) return;
    
    w->last_trigger = now;
    
    switch (w->cfg->behavior) {
        case ANIM_ONESHOT:
            if (w->boot_done && w->phase == PHASE_IDLE) {
                animator_start(&w->anim, seq, true, now);
                w->phase = PHASE_FORWARD;
            }
            break;
            
        case ANIM_OUTBACK:
            if (w->boot_done && w->phase == PHASE_IDLE) {
                animator_start(&w->anim, seq, true, now);
                w->phase = PHASE_FORWARD;
            }
            break;
            
        case ANIM_TOGGLE:
            w->desired_on = (state_or_toggle != 0);
            if (w->desired_on != w->visible_on && w->phase == PHASE_IDLE) {
                bool forward = w->desired_on;  // true=off→on, false=on→off
                animator_start(&w->anim, seq, forward, now);
                w->phase = forward ? PHASE_FORWARD : PHASE_REVERSE;
            }
            break;
            
        case ANIM_BOOTREV:
            if (w->boot_done && w->phase == PHASE_IDLE) {
                animator_start(&w->anim, seq, false, now);  // Start reverse
                w->phase = PHASE_REVERSE;
            }
            break;
            
        case ANIM_LAYER_TRANSITION:
            // TODO: Implement layer transition logic
            break;
    }
}

bool unified_anim_render(unified_anim_t *w, uint32_t now) {
    switch (w->cfg->behavior) {
        case ANIM_ONESHOT:
            return handle_oneshot_behavior(w, now);
        case ANIM_OUTBACK:
            return handle_outback_behavior(w, now);
        case ANIM_TOGGLE:
            return handle_toggle_behavior(w, now);
        case ANIM_BOOTREV:
            return handle_bootrev_behavior(w, now);
        case ANIM_LAYER_TRANSITION:
            // TODO: Implement layer transition behavior
            return false;
        default:
            return false;
    }
}
