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

anim_result_t animator_step_and_draw_blend(animator_t *a, uint8_t x, uint8_t y, uint32_t now) {
    // Draw current frame with OR blending (no clear), then advance
    if (a->active && a->count) {
        const slice_t *s = &a->frames[a->idx];
        draw_slice_px(s, x, y);  // No clear_rect() call - OR blend
    }
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

static inline void toggle_draw_steady(const toggle_anim_t *w, bool on, bool use_or_blend) {
    const slice_t *s = on ? &w->seq->frames[w->seq->count - 1]
                          : &w->seq->frames[0];
    if (!use_or_blend) {
        clear_rect(w->x, w->y, s->width, slice_height_px(s));
    }
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
    toggle_draw_steady(w, initial_on, false);
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
    toggle_anim_render_blend(w, now, false);
}

void toggle_anim_render_blend(toggle_anim_t *w, uint32_t now, bool use_or_blend) {
    switch (w->phase) {
    case TOG_IDLE_OFF:
        toggle_draw_steady(w, /*on=*/false, use_or_blend);
        return;

    case TOG_IDLE_ON:
        toggle_draw_steady(w, /*on=*/true, use_or_blend);
        return;

    case TOG_ENTERING: {
        anim_result_t r = use_or_blend
            ? animator_step_and_draw_blend(&w->anim, w->x, w->y, now)
            : animator_step_and_draw(&w->anim, w->x, w->y, now);
        if (r == ANIM_DONE_AT_END) {
            w->phase = TOG_IDLE_ON;
            w->visible_on = true;
            // If user flipped back to off right as we finished, `toggle_anim_set`
            // next tick will start an exit.
        }
        return;
    }

    case TOG_EXITING: {
        anim_result_t r = use_or_blend
            ? animator_step_and_draw_blend(&w->anim, w->x, w->y, now)
            : animator_step_and_draw(&w->anim, w->x, w->y, now);
        if (r == ANIM_DONE_AT_START) {
            w->phase = TOG_IDLE_OFF;
            w->visible_on = false;
        }
        return;
    }
    }
}

// ============================================================================
// One-Shot Animation Controller Implementation
// ============================================================================

/**
 * @brief Draw the steady frame for one-shot animation with blend mode
 * @param w One-shot controller instance
 * @param use_or_blend true for OR blending (no clear), false for opaque (clear first)
 */
static inline void oneshot_draw_steady_blend(const oneshot_anim_t *w, bool use_or_blend) {
    if (!w->seq || !w->seq->count) return;

    const slice_t *steady = w->steady_at_end
        ? &w->seq->frames[w->seq->count - 1]  // Last frame
        : &w->seq->frames[0];                 // First frame

    if (!use_or_blend) {
        clear_rect(w->x, w->y, steady->width, (uint8_t)(steady->pages * 8));
    }
    draw_slice_px(steady, w->x, w->y);
}

/**
 * @brief Draw the steady frame for one-shot animation
 * @param w One-shot controller instance
 */
static inline void oneshot_draw_steady(const oneshot_anim_t *w) {
    oneshot_draw_steady_blend(w, false);
}

void oneshot_anim_init(oneshot_anim_t *w, const slice_seq_t *seq,
                       uint8_t x, uint8_t y, bool steady_at_end,
                       bool run_boot_anim, uint32_t now) {
    w->seq = seq;
    w->x = x;
    w->y = y;
    w->steady_at_end = steady_at_end;
    w->boot_done = false;
    w->anim.active = false;

    if (run_boot_anim && seq && seq->count) {
        // Start boot animation
        animator_start(&w->anim, seq, /*forward=*/true, now);
        w->phase = ONESHOT_BOOT;
    } else {
        // Start idle
        w->phase = ONESHOT_IDLE;
        w->boot_done = true;  // Mark boot as done if we're not running it
        oneshot_draw_steady(w);
    }
}

void oneshot_anim_trigger(oneshot_anim_t *w, uint32_t now) {
    // Only trigger if boot is done and we're not already running
    if (!w->boot_done || !w->seq || !w->seq->count) return;

    // Start triggered animation
    animator_start(&w->anim, w->seq, /*forward=*/true, now);
    w->phase = ONESHOT_TRIGGERED;
}

bool oneshot_anim_render(oneshot_anim_t *w, uint32_t now) {
    return oneshot_anim_render_blend(w, now, false);
}

bool oneshot_anim_render_blend(oneshot_anim_t *w, uint32_t now, bool use_or_blend) {
    switch (w->phase) {
        case ONESHOT_IDLE:
            // Draw steady frame and stay idle
            oneshot_draw_steady_blend(w, use_or_blend);
            return false;

        case ONESHOT_BOOT: {
            anim_result_t r = use_or_blend
                ? animator_step_and_draw_blend(&w->anim, w->x, w->y, now)
                : animator_step_and_draw(&w->anim, w->x, w->y, now);
            if (r == ANIM_DONE_AT_END) {
                // Boot animation completed
                w->phase = ONESHOT_IDLE;
                w->boot_done = true;
                oneshot_draw_steady_blend(w, use_or_blend);
                return true;  // Animation just completed
            }
            return false;
        }

        case ONESHOT_TRIGGERED: {
            anim_result_t r = use_or_blend
                ? animator_step_and_draw_blend(&w->anim, w->x, w->y, now)
                : animator_step_and_draw(&w->anim, w->x, w->y, now);
            if (r == ANIM_DONE_AT_END) {
                // Triggered animation completed
                w->phase = ONESHOT_IDLE;
                oneshot_draw_steady_blend(w, use_or_blend);
                return true;  // Animation just completed
            }
            return false;
        }
    }

    return false;
}

// ============================================================================
// Out-and-Back Animation Controller Implementation
// ============================================================================

/**
 * @brief Draw the steady frame for out-and-back animation
 * @param w Out-and-back controller instance
 */
static inline void outback_draw_steady(const outback_anim_t *w) {
    if (!w->seq || !w->seq->count) return;

    const slice_t *steady = w->steady_at_end
        ? &w->seq->frames[w->seq->count - 1]  // Last frame
        : &w->seq->frames[0];                 // First frame

    clear_rect(w->x, w->y, steady->width, (uint8_t)(steady->pages * 8));
    draw_slice_px(steady, w->x, w->y);
}

void outback_anim_init(outback_anim_t *w, const slice_seq_t *seq,
                       uint8_t x, uint8_t y, bool steady_at_end,
                       bool run_boot_anim, uint32_t now) {
    w->seq = seq;
    w->x = x;
    w->y = y;
    w->steady_at_end = steady_at_end;
    w->boot_done = false;
    w->anim.active = false;

    if (run_boot_anim && seq && seq->count) {
        // Start boot animation (forward only)
        animator_start(&w->anim, seq, /*forward=*/true, now);
        w->phase = OUTBACK_BOOT;
    } else {
        // Start idle
        w->phase = OUTBACK_IDLE;
        w->boot_done = true;  // Mark boot as done if we're not running it
        outback_draw_steady(w);
    }
}

void outback_anim_trigger(outback_anim_t *w, uint32_t now) {
    // Only trigger if boot is done and we're not already running
    if (!w->boot_done || !w->seq || !w->seq->count) return;

    // Start "out" phase (forward animation)
    animator_start(&w->anim, w->seq, /*forward=*/true, now);
    w->phase = OUTBACK_OUT;
}

bool outback_anim_render(outback_anim_t *w, uint32_t now) {
    switch (w->phase) {
        case OUTBACK_IDLE:
            // Draw steady frame and stay idle
            outback_draw_steady(w);
            return false;

        case OUTBACK_BOOT: {
            anim_result_t r = animator_step_and_draw(&w->anim, w->x, w->y, now);
            if (r == ANIM_DONE_AT_END) {
                // Boot animation completed
                w->phase = OUTBACK_IDLE;
                w->boot_done = true;
                outback_draw_steady(w);
                return true;  // Animation just completed
            }
            return false;
        }

        case OUTBACK_OUT: {
            anim_result_t r = animator_step_and_draw(&w->anim, w->x, w->y, now);
            if (r == ANIM_DONE_AT_END) {
                // "Out" phase completed, start "back" phase (reverse)
                animator_start(&w->anim, w->seq, /*forward=*/false, now);
                w->phase = OUTBACK_BACK;
            }
            return false;
        }

        case OUTBACK_BACK: {
            anim_result_t r = animator_step_and_draw(&w->anim, w->x, w->y, now);
            if (r == ANIM_DONE_AT_START) {
                // "Back" phase completed, return to idle
                w->phase = OUTBACK_IDLE;
                outback_draw_steady(w);
                return true;  // Animation just completed
            }
            return false;
        }
    }

    return false;
}

// ============================================================================
// Boot-Then-Reverse-Out-Back Animation Controller Implementation
// ============================================================================

/**
 * @brief Draw the steady frame for boot-reverse animation (always last frame)
 * @param w Boot-reverse controller instance
 * @param use_or_blend If true, use OR blending (no clear), if false use opaque (clear first)
 */
static inline void bootrev_draw_steady(const bootrev_anim_t *w, bool use_or_blend) {
    if (!w->seq || !w->seq->count) return;

    // Steady frame is always the last frame
    const slice_t *steady = &w->seq->frames[w->seq->count - 1];

    if (!use_or_blend) {
        clear_rect(w->x, w->y, steady->width, slice_height_px(steady));
    }
    draw_slice_px(steady, w->x, w->y);
}

void bootrev_anim_init(bootrev_anim_t *w, const slice_seq_t *seq,
                       uint8_t x, uint8_t y, bool run_boot_anim, uint32_t now) {
    w->seq = seq;
    w->x = x;
    w->y = y;
    w->boot_done = false;
    w->anim.active = false;

    if (run_boot_anim && seq && seq->count) {
        // Start boot animation (0→end)
        animator_start(&w->anim, seq, /*forward=*/true, now);
        w->phase = BOOTREV_BOOT;
    } else {
        // Start idle at last frame
        w->phase = BOOTREV_IDLE;
        w->boot_done = true;  // Mark boot as done if we're not running it
        bootrev_draw_steady(w, false);  // false = opaque (clear first)
    }
}

void bootrev_anim_trigger(bootrev_anim_t *w, uint32_t now) {
    // Only trigger if boot is done and we're not already running
    if (!w->boot_done || !w->seq || !w->seq->count) return;

    // Start "out" phase (end→start, reverse animation)
    animator_start(&w->anim, w->seq, /*forward=*/false, now);
    w->phase = BOOTREV_OUT;
}

bool bootrev_anim_render(bootrev_anim_t *w, uint32_t now) {
    return bootrev_anim_render_blend(w, now, false);
}

bool bootrev_anim_render_blend(bootrev_anim_t *w, uint32_t now, bool use_or_blend) {
    switch (w->phase) {
        case BOOTREV_IDLE:
            // Draw steady frame (last frame) and stay idle
            bootrev_draw_steady(w, use_or_blend);
            return false;

        case BOOTREV_BOOT: {
            anim_result_t r = use_or_blend
                ? animator_step_and_draw_blend(&w->anim, w->x, w->y, now)
                : animator_step_and_draw(&w->anim, w->x, w->y, now);
            if (r == ANIM_DONE_AT_END) {
                // Boot animation completed (reached last frame)
                w->phase = BOOTREV_IDLE;
                w->boot_done = true;
                bootrev_draw_steady(w, use_or_blend);
                return true;  // Animation just completed
            }
            return false;
        }

        case BOOTREV_OUT: {
            anim_result_t r = use_or_blend
                ? animator_step_and_draw_blend(&w->anim, w->x, w->y, now)
                : animator_step_and_draw(&w->anim, w->x, w->y, now);
            if (r == ANIM_DONE_AT_START) {
                // "Out" phase completed (reached first frame), start "back" phase (forward)
                animator_start(&w->anim, w->seq, /*forward=*/true, now);
                w->phase = BOOTREV_BACK;
            }
            return false;
        }

        case BOOTREV_BACK: {
            anim_result_t r = use_or_blend
                ? animator_step_and_draw_blend(&w->anim, w->x, w->y, now)
                : animator_step_and_draw(&w->anim, w->x, w->y, now);
            if (r == ANIM_DONE_AT_END) {
                // "Back" phase completed (reached last frame), return to idle
                w->phase = BOOTREV_IDLE;
                bootrev_draw_steady(w, use_or_blend);
                return true;  // Animation just completed
            }
            return false;
        }
    }

    return false;
}

