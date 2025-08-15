/**
 * @file oled_anim.c
 * @brief Implementation of low-level animation engine
 */

#include QMK_KEYBOARD_H
#include "oled_anim.h"

// ============================================================================
// Internal Helpers
// ============================================================================

/**
 * @brief Get width of a frame in a sequence
 * @param seq Frame sequence
 * @param idx Frame index
 * @return Width in pixels
 */
static inline uint8_t seq_w(const slice_seq_t *seq, uint8_t idx) {
    return seq->frames[idx].width;
}

/**
 * @brief Get height of a frame in a sequence
 * @param seq Frame sequence
 * @param idx Frame index
 * @return Height in pixels
 */
static inline uint8_t seq_h(const slice_seq_t *seq, uint8_t idx) {
    return (uint8_t)(seq->frames[idx].pages * 8);
}

// ============================================================================
// Low-Level Animator Implementation
// ============================================================================

void animator_start(animator_t *a, const slice_seq_t *seq, bool forward, uint32_t now) {
    // Validate input parameters
    if (!seq || !seq->count) {
        a->active = false;
        return;
    }

    // Initialize animator state
    a->frames  = seq->frames;
    a->count   = seq->count;
    a->dir     = forward ? +1 : -1;
    a->idx     = forward ? 0 : (uint8_t)(seq->count - 1);
    a->active  = true;
    a->next_ms = now + ANIM_FRAME_MS;
}

void animator_reverse(animator_t *a, uint32_t now) {
    // Only reverse if animation is active and valid
    if (!a->active || !a->count) return;

    // Flip direction and reset timing
    a->dir = (int8_t)-a->dir;
    a->next_ms = now + ANIM_FRAME_MS;
}

void animator_draw_current(const animator_t *a, uint8_t x, uint8_t y) {
    // Only draw if animation is active and valid
    if (!a->active || !a->count) return;

    // Get current frame and draw with opaque clearing
    const slice_t *s = &a->frames[a->idx];
    clear_rect(x, y, s->width, (uint8_t)(s->pages * 8));  // Clear background first
    draw_slice_px(s, x, y);                               // Draw current frame
}

anim_result_t animator_step(animator_t *a, uint32_t now) {
    // Early exit if animation is not active or invalid
    if (!a->active || !a->count) return ANIM_RUNNING;

    // Check if enough time has passed for next frame
    if ((int32_t)(now - a->next_ms) < 0) return ANIM_RUNNING;

    // Schedule next frame
    a->next_ms += ANIM_FRAME_MS;

    // Calculate next frame index
    int16_t next_idx = (int16_t)a->idx + (int16_t)a->dir;

    // Check for animation completion
    if (next_idx < 0) {
        // Reached start of sequence
        a->idx = 0;
        a->active = false;
        return ANIM_DONE_AT_START;
    }
    if (next_idx >= a->count) {
        // Reached end of sequence
        a->idx = (uint8_t)(a->count - 1);
        a->active = false;
        return ANIM_DONE_AT_END;
    }

    // Continue animation
    a->idx = (uint8_t)next_idx;
    return ANIM_RUNNING;
}

anim_result_t animator_step_and_draw(animator_t *a, uint8_t x, uint8_t y, uint32_t now) {
    // Draw current frame first, then advance
    animator_draw_current(a, x, y);
    return animator_step(a, now);
}

// ============================================================================
// Exclusive State Transition Controller Implementation
// ============================================================================

// ---------------- layer_transition_t ----------------

static inline void start_exit(layer_transition_t *t, uint32_t now) {
    const slice_seq_t *seq = t->seq_map[t->src];
    animator_start(&t->anim, seq, /*forward=*/false, now); // exit = backward
    t->phase = TR_EXIT;
}

static inline void start_enter(layer_transition_t *t, uint32_t now) {
    const slice_seq_t *seq = t->seq_map[t->src];
    animator_start(&t->anim, seq, /*forward=*/true, now); // enter = forward
    t->phase = TR_ENTER;
}

void layer_tr_init(layer_transition_t *t,
                   const slice_seq_t * const *seq_map,
                   uint8_t state_count,
                   uint8_t x, uint8_t y,
                   uint8_t initial_state,
                   uint32_t now) {
    t->seq_map     = seq_map;
    t->state_count = state_count;
    t->x = x; t->y = y;
    t->anim.active = false;
    t->anim.count  = 0;
    t->phase = TR_IDLE;
    t->src   = initial_state;
    t->dst   = initial_state;
    t->pending = 0xFF;
    t->initialized = true;

    // Draw steady frame for initial state
    layer_tr_draw_steady(t, initial_state);
    (void)now;
}

void layer_tr_request(layer_transition_t *t, uint8_t desired, uint32_t now) {
    if (!t->initialized || desired >= t->state_count) return;

    // If already idle and same state, nothing to do
    if (t->phase == TR_IDLE && desired == t->src) return;

    if (t->phase == TR_IDLE) {
        // Begin a new transition: src -> desired
        if (desired != t->src) {
            t->dst = desired;
            start_exit(t, now); // will run src backward
        }
        return;
    }

    if (t->phase == TR_EXIT) {
        // During exit of src
        if (desired == t->src) {
            // Cancel → reverse exit back to full src
            animator_reverse(&t->anim, now);
            t->dst = t->src;       // we’re “transitioning to src” again
            return;
        }
        if (desired != t->dst) {
            // Changing target mid-exit: update the intended destination.
            // We'll still finish the exit, then enter the *latest* desired.
            t->dst = desired;
            return;
        }
        // Else desired == current dst → nothing to change
        return;
    }

    if (t->phase == TR_ENTER) {
        // During enter of current src (which equals the destination we just adopted)
        // If we changed our mind:
        if (desired == t->src) {
            // Already entering the same src → keep going
            return;
        }
        // Reverse the enter (back to start), then we’ll exit src toward new desired
        animator_reverse(&t->anim, now);
        t->pending = desired; // remember where we *really* want to go after cancel
        return;
    }
}

void layer_tr_draw_steady(const layer_transition_t *t, uint8_t state) {
    // By convention: "steady" = last frame of that state's sequence
    const slice_seq_t *seq = t->seq_map[state];
    if (!seq || !seq->count) return;
    const slice_t *steady = &seq->frames[seq->count - 1];
    clear_rect(t->x, t->y, steady->width, (uint8_t)(steady->pages * 8));
    draw_slice_px(steady, t->x, t->y);
}

void layer_tr_render(layer_transition_t *t, uint32_t now) {
    switch (t->phase) {
        case TR_IDLE: {
            // Keep steady art visible (cheap + safe)
            layer_tr_draw_steady(t, t->src);
            return;
        }
        case TR_EXIT: {
            anim_result_t r = animator_step_and_draw(&t->anim, t->x, t->y, now);
            if (r == ANIM_RUNNING) return;

            if (r == ANIM_DONE_AT_START) {
                // Completed exit of old src. Adopt destination as new src, then ENTER.
                t->src = t->dst;
                start_enter(t, now);
                return;
            }

            // r == ANIM_DONE_AT_END: we reversed and returned to full src -> idle
            t->phase = TR_IDLE;
            // If someone queued a different target during reversal, kick it now
            if (t->pending != 0xFF && t->pending != t->src) {
                t->dst = t->pending;
                t->pending = 0xFF;
                start_exit(t, now);
                return;
            }
            // Otherwise just draw steady
            layer_tr_draw_steady(t, t->src);
            return;
        }
        case TR_ENTER: {
            anim_result_t r = animator_step_and_draw(&t->anim, t->x, t->y, now);
            if (r == ANIM_RUNNING) return;

            if (r == ANIM_DONE_AT_END) {
                // Finished entering new src
                t->phase = TR_IDLE;
                layer_tr_draw_steady(t, t->src);
                // If a new desire arrived exactly as we finished, request will handle next tick
                return;
            }

            // r == ANIM_DONE_AT_START: we reversed/canceled the enter
            t->phase = TR_IDLE;
            // If we have a pending different target, begin fresh exit toward it
            if (t->pending != 0xFF && t->pending != t->src) {
                t->dst = t->pending;
                t->pending = 0xFF;
                start_exit(t, now);
                return;
            }
            // Otherwise just show steady of current src (we're back to start)
            layer_tr_draw_steady(t, t->src);
            return;
        }
    }
}

static inline void toggle_draw_steady(const toggle_anim_t *w, bool on) {
    const slice_t *s = on ? &w->seq->frames[w->seq->count - 1]
                          : &w->seq->frames[0];
    clear_rect(w->x, w->y, s->width, (uint8_t)(s->pages * 8));
    draw_slice_px(s, w->x, w->y);
}

void toggle_anim_init(toggle_anim_t *w, const slice_seq_t *seq,
                      uint8_t x, uint8_t y, bool initial_on, uint32_t now) {
    (void)now;
    w->seq = seq;
    w->x = x; w->y = y;
    w->phase = initial_on ? TOG_IDLE_ON : TOG_IDLE_OFF;
    w->visible_on = initial_on;
    w->desired_on = initial_on;
    w->anim.active = false;
    toggle_draw_steady(w, initial_on);
}

void toggle_anim_set(toggle_anim_t *w, bool want_on, uint32_t now) {
    w->desired_on = want_on;

    switch (w->phase) {
    case TOG_IDLE_OFF:
        if (want_on) {
            animator_start(&w->anim, w->seq, /*forward=*/true, now);
            w->phase = TOG_ENTERING;
        }
        break;

    case TOG_IDLE_ON:
        if (!want_on) {
            animator_start(&w->anim, w->seq, /*forward=*/false, now);
            w->phase = TOG_EXITING;
        }
        break;

    case TOG_ENTERING:
        if (!want_on) { // cancel mid-enter → reverse to exit
            animator_reverse(&w->anim, now);
            w->phase = TOG_EXITING;
        }
        break;

    case TOG_EXITING:
        if (want_on) { // cancel mid-exit → reverse to enter
            animator_reverse(&w->anim, now);
            w->phase = TOG_ENTERING;
        }
        break;
    }
}

void toggle_anim_render(toggle_anim_t *w, uint32_t now) {
    switch (w->phase) {
    case TOG_IDLE_OFF:
        toggle_draw_steady(w, /*on=*/false);
        return;

    case TOG_IDLE_ON:
        toggle_draw_steady(w, /*on=*/true);
        return;

    case TOG_ENTERING: {
        anim_result_t r = animator_step_and_draw(&w->anim, w->x, w->y, now);
        if (r == ANIM_DONE_AT_END) {
            w->phase = TOG_IDLE_ON;
            w->visible_on = true;
            // If user flipped back to off right as we finished, `toggle_anim_set`
            // next tick will start an exit.
        }
        return;
    }

    case TOG_EXITING: {
        anim_result_t r = animator_step_and_draw(&w->anim, w->x, w->y, now);
        if (r == ANIM_DONE_AT_START) {
            w->phase = TOG_IDLE_OFF;
            w->visible_on = false;
        }
        return;
    }
    }
}

