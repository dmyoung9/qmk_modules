#include QMK_KEYBOARD_H
#include "oled_declarative.h"

// --- Internals ---

static inline void draw_frame_raw(const slice_t *s, uint8_t x, uint8_t y) {
    draw_slice_px(s, x, y); // uses your rotation-safe blitter
}

static void draw_state_steady(const widget_t *w, uint8_t state) {
    const widget_config_t *cfg = w->cfg;
    const state_desc_t    *sd  = &cfg->states[state];
    const slice_seq_t     *seq = sd->seq;

    const slice_t *steady = (sd->enter_dir > 0)
        ? &seq->frames[seq->count - 1]
        : &seq->frames[0];

    if (cfg->blit == BLIT_OPAQUE) {
        clear_rect(cfg->x, cfg->y, cfg->bbox_w, cfg->bbox_h);
    }
    draw_frame_raw(steady, cfg->x, cfg->y);
}

// Map: for a given state's canonical sequence + enter_dir, what do we use for ENTER/EXIT?
typedef struct {
    const slice_seq_t *seq;
    bool forward; // true = 0→end, false = end→0
} mapped_anim_t;

static mapped_anim_t map_enter_of(const state_desc_t *sd) {
    mapped_anim_t m = { sd->seq, sd->enter_dir > 0 };
    return m;
}
static mapped_anim_t map_exit_of(const state_desc_t *sd) {
    // Exit direction is the opposite of enter
    mapped_anim_t m = { sd->seq, sd->enter_dir <= 0 }; // if enter_rev, exit is forward
    return m;
}

static inline void start_exit(widget_t *w, uint32_t now) {
    const widget_config_t *cfg = w->cfg;
    const state_desc_t    *sd  = &cfg->states[w->src];
    mapped_anim_t m = map_exit_of(sd);

    animator_start(&w->anim, sd->seq, /*forward=*/m.forward, now);
    w->phase = TR_EXIT;
}
static inline void start_enter(widget_t *w, uint32_t now) {
    const widget_config_t *cfg = w->cfg;
    const state_desc_t    *sd  = &cfg->states[w->src];
    mapped_anim_t m = map_enter_of(sd);

    animator_start(&w->anim, sd->seq, /*forward=*/m.forward, now);
    w->phase = TR_ENTER;
}

static inline void pre_clear_bbox_if_opaque(const widget_t *w) {
    if (w->cfg->blit == BLIT_OPAQUE) {
        clear_rect(w->cfg->x, w->cfg->y, w->cfg->bbox_w, w->cfg->bbox_h);
    }
}

// We want to draw current frame without leaving trails even if frames vary in size.
// Easiest: clear the declared bbox, then let animator draw current frame (which
// itself clears only its own frame box if your animator_draw_current does; to avoid
// double clear, we call animator_step first then draw).
static anim_result_t step_then_draw(widget_t *w, uint32_t now) {
    anim_result_t r = animator_step(&w->anim, now);
    pre_clear_bbox_if_opaque(w);

    // Draw current frame
    if (w->anim.active && w->anim.count) {
        const slice_t *s = &w->anim.frames[w->anim.idx];
        draw_frame_raw(s, w->cfg->x, w->cfg->y);
    }
    return r;
}

// --- Public API ---

void widget_init(widget_t *w, const widget_config_t *cfg, uint8_t initial_state, uint32_t now) {
    (void)now;
    w->cfg = cfg;
    w->anim.active = false;
    w->anim.count  = 0;
    w->phase = TR_IDLE;
    w->src   = initial_state;
    w->dst   = initial_state;
    w->pending = 0xFF;
    w->initialized = true;

    // If bbox not provided, you *can* compute a safe default from the steady frame,
    // but we’ll trust the declarative bbox to avoid surprises.
    draw_state_steady(w, w->src);
}

void widget_tick(widget_t *w, uint32_t now) {
    if (!w->initialized) return;

    // 1) Ask "what do you want to be?"
    uint8_t desired = w->cfg->query ? w->cfg->query(w->cfg->user_arg) : w->src;
    if (desired >= w->cfg->state_count) desired = w->src; // safety

    // 2) Transition logic (reversible, identical to our earlier controller but generic)
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

