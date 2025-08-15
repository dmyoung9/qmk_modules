#pragma once
#include <stdint.h>
#include <stdbool.h>
#include QMK_KEYBOARD_H

#include "oled_utils.h"   // slice_t, clear_rect, draw_slice_px
#include "oled_anim.h"    // slice_seq_t, animator_t, anim_result_t, TR_* enums

// Blend/clear policy (kept simple: OPAQUE clears the widget bbox every frame)
typedef enum { BLIT_OPAQUE = 0, BLIT_ADDITIVE = 1 } blit_mode_t;

// Direction for entering a given state.
// enter_dir = +1 → enter plays forward (0..N-1), steady is last frame
// enter_dir = -1 → enter plays backward (N-1..0), steady is first frame
typedef struct {
    const slice_seq_t *seq; // frames for this state (canonical order)
    int8_t             enter_dir;
} state_desc_t;

// Condition callback returns desired state index [0..state_count-1].
// Pass whatever you need via user_arg (e.g., a modifier mask, etc.)
typedef uint8_t (*state_query_fn_t)(uint32_t user_arg);

// Declarative widget config (works for both exclusive and binary/toggle widgets)
typedef struct {
    // Layout
    uint8_t x, y;
    uint8_t bbox_w, bbox_h;     // area to clear per frame if BLIT_OPAQUE

    // Rendering policy
    blit_mode_t blit;

    // States
    const state_desc_t *states; // array of length state_count
    uint8_t             state_count;

    // Condition
    state_query_fn_t query;
    uint32_t         user_arg;  // e.g., a MOD_MASK_* or unused (0)

    // Optional: initial state (if not set at init time)
    uint8_t initial_state;
} widget_config_t;

// Runtime instance (one per widget)
typedef struct {
    const widget_config_t *cfg;

    animator_t anim;
    tr_phase_t phase;

    uint8_t src;      // currently visible logical state
    uint8_t dst;      // target during an EXIT→ENTER chain
    uint8_t pending;  // queued desired (0xFF = none)

    bool     initialized;
} widget_t;

// ---- API ----
void widget_init(widget_t *w, const widget_config_t *cfg, uint8_t initial_state, uint32_t now);
void widget_tick(widget_t *w, uint32_t now);  // query→transition decision→render one frame

// Helpers to build declarative bits
#define ENTER_FWD  (+1)
#define ENTER_REV  (-1)

// Convenience macro: declare a frameset quickly (pairs nicely with your SLICE macros)
#define DEFINE_SLICE_SEQ(name, ...)                                \
    static const slice_t name##_frames[] = { __VA_ARGS__ };        \
    static const slice_seq_t name = {                               \
        name##_frames,                                              \
        (uint8_t)(sizeof(name##_frames)/sizeof(name##_frames[0]))   \
    }

// Convenience: get steady frame dims for a state (used internally too)
static inline uint8_t state_steady_w(const state_desc_t *s) {
    const slice_seq_t *q = s->seq;
    return (s->enter_dir > 0) ? q->frames[q->count - 1].width : q->frames[0].width;
}
static inline uint8_t state_steady_h(const state_desc_t *s) {
    const slice_seq_t *q = s->seq;
    const slice_t *fr = (s->enter_dir > 0) ? &q->frames[q->count - 1] : &q->frames[0];
    return (uint8_t)(fr->pages * 8);
}

