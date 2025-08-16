/**
 * @file oled_declarative.c
 * @brief Implementation of declarative widget system
 */

#include QMK_KEYBOARD_H
#include "oled_declarative.h"

// ============================================================================
// Internal Helper Functions
// ============================================================================

/**
 * @brief Draw a frame without any clearing
 * @param s Slice to draw
 * @param x X coordinate
 * @param y Y coordinate
 */
static inline void draw_frame_raw(const slice_t *s, uint8_t x, uint8_t y) {
    draw_slice_px(s, x, y);
}

/**
 * @brief Draw the steady frame for a specific state
 *
 * Determines the appropriate steady frame based on enter direction and
 * renders it with the configured blending mode.
 *
 * @param w Widget instance
 * @param state State index to draw
 */
static void draw_state_steady(const widget_t *w, uint8_t state) {
    const widget_config_t *cfg = w->cfg;
    const state_desc_t    *sd  = &cfg->states[state];
    const slice_seq_t     *seq = sd->seq;

    // Determine steady frame based on enter direction
    const slice_t *steady = (sd->enter_dir > 0)
        ? &seq->frames[seq->count - 1]  // Forward: steady is last frame
        : &seq->frames[0];              // Reverse: steady is first frame

    // Clear background if using opaque blending
    if (cfg->blit == BLIT_OPAQUE) {
        clear_rect(cfg->x, cfg->y, cfg->bbox_w, cfg->bbox_h);
    }

    // Draw the steady frame
    draw_frame_raw(steady, cfg->x, cfg->y);
}

/**
 * @brief Animation direction mapping for enter/exit transitions
 *
 * Maps a state's configuration to the actual animation parameters needed
 * for enter and exit transitions.
 */
typedef struct {
    const slice_seq_t *seq;     ///< Animation sequence to use
    bool forward;               ///< Direction: true = 0→end, false = end→0
} mapped_anim_t;

/**
 * @brief Get animation mapping for entering a state
 * @param sd State description
 * @return Animation mapping for enter transition
 */
static mapped_anim_t map_enter_of(const state_desc_t *sd) {
    mapped_anim_t m = { sd->seq, sd->enter_dir > 0 };
    return m;
}

/**
 * @brief Get animation mapping for exiting a state
 * @param sd State description
 * @return Animation mapping for exit transition (opposite of enter)
 */
static mapped_anim_t map_exit_of(const state_desc_t *sd) {
    // Exit direction is the opposite of enter direction
    mapped_anim_t m = { sd->seq, sd->enter_dir <= 0 };
    return m;
}

/**
 * @brief Start exit animation for current state
 * @param w Widget instance
 * @param now Current timestamp
 */
static inline void start_exit(widget_t *w, uint32_t now) {
    const widget_config_t *cfg = w->cfg;
    const state_desc_t    *sd  = &cfg->states[w->src];
    mapped_anim_t m = map_exit_of(sd);

    animator_start(&w->anim, sd->seq, m.forward, now);
    w->phase = TR_EXIT;
}

/**
 * @brief Start enter animation for current state
 * @param w Widget instance
 * @param now Current timestamp
 */
static inline void start_enter(widget_t *w, uint32_t now) {
    const widget_config_t *cfg = w->cfg;
    const state_desc_t    *sd  = &cfg->states[w->src];
    mapped_anim_t m = map_enter_of(sd);

    animator_start(&w->anim, sd->seq, m.forward, now);
    w->phase = TR_ENTER;
}

/**
 * @brief Clear widget bounding box if using opaque blending
 * @param w Widget instance
 */
static inline void pre_clear_bbox_if_opaque(const widget_t *w) {
    if (w->cfg->blit == BLIT_OPAQUE) {
        clear_rect(w->cfg->x, w->cfg->y, w->cfg->bbox_w, w->cfg->bbox_h);
    }
}

/**
 * @brief Step animation and draw current frame
 *
 * Advances the animation by one frame and draws the result. Handles
 * background clearing based on the widget's blending mode to prevent
 * trails between frames of different sizes.
 *
 * @param w Widget instance
 * @param now Current timestamp
 * @return Animation result (running, done at start, or done at end)
 */
static anim_result_t step_then_draw(widget_t *w, uint32_t now) {
    // Step animation first
    anim_result_t r = animator_step(&w->anim, now);

    // Clear background if using opaque blending
    pre_clear_bbox_if_opaque(w);

    // Draw current frame if animation is active
    if (w->anim.active && w->anim.count) {
        const slice_t *s = &w->anim.frames[w->anim.idx];
        draw_frame_raw(s, w->cfg->x, w->cfg->y);
    }

    return r;
}

// ============================================================================
// Public API Implementation
// ============================================================================

// --- Public API ---

void widget_init(widget_t *w, const widget_config_t *cfg, uint8_t initial_state, uint32_t now) {
    w->cfg = cfg;
    w->anim.active = false;
    w->anim.count  = 0;
    w->phase = TR_IDLE;
    w->src   = initial_state;
    w->dst   = initial_state;
    w->pending = 0xFF;
    w->last_query_result = initial_state;
    w->last_state_change = now;
    w->stuck_timeout = 0;
    w->initialized = true;

    // If bbox not provided, you *can* compute a safe default from the steady frame,
    // but we’ll trust the declarative bbox to avoid surprises.
    draw_state_steady(w, w->src);
}

void widget_tick(widget_t *w, uint32_t now) {
    if (!w->initialized) return;

    // 1) Ask "what do you want to be?"
    uint8_t desired = w->src; // default to current state
    if (w->cfg->query) {
        // Use new signature
        desired = w->cfg->query(w->cfg->user_arg, w->src, now);
    } else if (w->cfg->legacy_query) {
        // Use legacy signature for backward compatibility
        desired = w->cfg->legacy_query(w->cfg->user_arg);
    }
    if (desired >= w->cfg->state_count) desired = w->src; // safety

    // 2) Watchdog: detect state changes and stuck animations
#if WIDGET_WATCHDOG_TIMEOUT_MS > 0
    // Reset watchdog on state changes
    if (desired != w->last_query_result) {
        w->last_query_result = desired;
        w->last_state_change = now;
        w->stuck_timeout = 0;
    }

    // Detect stuck animations
    if (w->phase != TR_IDLE && w->stuck_timeout == 0) {
        uint32_t animation_duration = now - w->last_state_change;
        if (animation_duration > WIDGET_WATCHDOG_TIMEOUT_MS) {
            w->stuck_timeout = now;
        }
    }

    // Force reset if stuck for too long
    if (w->stuck_timeout != 0) {
        uint32_t stuck_duration = now - w->stuck_timeout;
        if (stuck_duration > WIDGET_WATCHDOG_GRACE_MS) {
            // Force widget to idle state and redraw current state
            w->phase = TR_IDLE;
            w->src = desired;
            w->dst = desired;
            w->pending = 0xFF;
            w->anim.active = false;
            w->stuck_timeout = 0;
            draw_state_steady(w, w->src);
            return;
        }
    }
#endif

    // 3) Transition logic (reversible, identical to our earlier controller but generic)
    if (w->phase == TR_IDLE) {
        if (desired != w->src) {
            w->dst = desired;
            start_exit(w, now);
        } else {
            // stay steady
            draw_state_steady(w, w->src);
        }
        return;
    }

    if (w->phase == TR_EXIT) {
        // Mid-exit changes
        if (desired == w->src && w->dst != w->src) {
            // Cancel → reverse back to steady of src
            animator_reverse(&w->anim, now);
            w->dst = w->src;
        } else if (desired != w->dst) {
            // Update the target; we'll adopt it after exit completes
            w->dst = desired;
        }

        anim_result_t r = step_then_draw(w, now);
        if (r == ANIM_RUNNING) return;

        if (r == ANIM_DONE_AT_START) {
            // Finished exiting the old src → adopt destination and ENTER it
            w->src = w->dst;
            start_enter(w, now);
            return;
        }

        // Reversed back to full src
        w->phase = TR_IDLE;
        draw_state_steady(w, w->src);
        return;
    }

    if (w->phase == TR_ENTER) {
        if (desired != w->src) {
            // Changed mind mid-enter → reverse back to start, then we’ll exit towards desired
            animator_reverse(&w->anim, now);
            w->pending = desired;
        }

        anim_result_t r = step_then_draw(w, now);
        if (r == ANIM_RUNNING) return;

        if (r == ANIM_DONE_AT_END) {
            // Finished entering
            w->phase = TR_IDLE;
            draw_state_steady(w, w->src);
            return;
        }

        // r == ANIM_DONE_AT_START (we reversed back to start)
        w->phase = TR_IDLE;
        draw_state_steady(w, w->src);
        if (w->pending != 0xFF && w->pending != w->src) {
            w->dst = w->pending;
            w->pending = 0xFF;
            start_exit(w, now);
        }
        return;
    }
}

