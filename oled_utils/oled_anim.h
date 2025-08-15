#pragma once
#include <stdint.h>
#include <stdbool.h>
#include QMK_KEYBOARD_H
#include "oled_utils.h"  // for slice_t, clear_rect, draw_slice_px

#ifndef ANIM_FRAME_MS
#    define ANIM_FRAME_MS 80
#endif

// A simple frameset: contiguous array of slice_t frames
typedef struct {
    const slice_t *frames;
    uint8_t        count;
} slice_seq_t;

// Handy macro to define a frameset from slices
#define DEFINE_SLICE_SEQ(name, ...)                                \
    static const slice_t name##_frames[] = { __VA_ARGS__ };        \
    static const slice_seq_t name = {                               \
        name##_frames,                                              \
        (uint8_t)(sizeof(name##_frames) / sizeof(name##_frames[0])) \
    }

// Low-level frame animator with direction + reversal
typedef struct {
    const slice_t *frames;  // not owned
    uint8_t        count;
    int8_t         dir;     // +1 forward, -1 backward
    uint8_t        idx;     // current frame index
    bool           active;
    uint32_t       next_ms;
} animator_t;

typedef enum {
    ANIM_RUNNING = 0,
    ANIM_DONE_AT_START,   // finished at idx==0
    ANIM_DONE_AT_END      // finished at idx==count-1
} anim_result_t;

void animator_start(animator_t *a,
                    const slice_seq_t *seq,
                    bool forward,        // true = enter, false = exit
                    uint32_t now);

void animator_reverse(animator_t *a, uint32_t now);
void animator_draw_current(const animator_t *a, uint8_t x_px, uint8_t y_px);
anim_result_t animator_step(animator_t *a, uint32_t now);
anim_result_t animator_step_and_draw(animator_t *a, uint8_t x, uint8_t y, uint32_t now);

// ---------------- State transition controller ----------------
//
// “One widget that shows exactly one state at a time” (e.g., current layer).
// For each state S you provide a sequence (its enter/exit frames).
// Enter = play S forward; Exit = play S backward (same frames).
//
typedef enum {
    TR_IDLE = 0,
    TR_EXIT,
    TR_ENTER
} tr_phase_t;

typedef struct {
    // Config
    const slice_seq_t * const *seq_map; // [state_count]; per-state frames
    uint8_t state_count;
    uint8_t x, y;

    // Live state
    animator_t anim;
    tr_phase_t phase;
    uint8_t    src;        // committed/visible logical state
    uint8_t    dst;        // desired state for the current transition chain
    uint8_t    pending;    // queued desired state (0xFF = none)
    bool       initialized;
} layer_transition_t;

// Initialize and set the initial visible state
void layer_tr_init(layer_transition_t *t,
                   const slice_seq_t * const *seq_map,
                   uint8_t state_count,
                   uint8_t x, uint8_t y,
                   uint8_t initial_state,
                   uint32_t now);

// Request a new desired state; handles reversal/cancel/queueing
void layer_tr_request(layer_transition_t *t, uint8_t desired_state, uint32_t now);

// Advance/draw; call every OLED tick
void layer_tr_render(layer_transition_t *t, uint32_t now);

// Utility: draw the steady frame (last frame) for a given state
void layer_tr_draw_steady(const layer_transition_t *t, uint8_t state);

// ---- Binary (non-exclusive) toggle animator for things like modifiers ----
typedef enum { TOG_IDLE_OFF=0, TOG_ENTERING, TOG_IDLE_ON, TOG_EXITING } tog_phase_t;

typedef struct {
    // Config
    const slice_seq_t *seq; // frames for this widget (enter uses forward; exit uses reverse)
    uint8_t x, y;

    // Live
    animator_t anim;
    tog_phase_t phase;
    bool visible_on;  // current steady side when idle (false=off @ frame0, true=on @ last frame)
    bool desired_on;  // what we want right now
} toggle_anim_t;

void toggle_anim_init(toggle_anim_t *w, const slice_seq_t *seq,
                      uint8_t x, uint8_t y, bool initial_on, uint32_t now);

void toggle_anim_set(toggle_anim_t *w, bool want_on, uint32_t now); // handles reversal
void toggle_anim_render(toggle_anim_t *w, uint32_t now);            // call every OLED tick
