#pragma once

#include <stdbool.h>
#include <stdint.h>

#include QMK_KEYBOARD_H

// ---------- Config ----------
#ifndef TS_MAX_LAYERS
#    define TS_MAX_LAYERS 8 // how many layers to count
#endif
#ifndef TS_FLUSH_SECONDS
#    define TS_FLUSH_SECONDS 120 // snapshot interval
#endif
#ifndef TS_FLUSH_EVENTS
#    define TS_FLUSH_EVENTS 2000 // snapshot by event count
#endif
#ifndef TS_WPM_EMA_ALPHA_NUM
#    define TS_WPM_EMA_ALPHA_NUM 1 // EMA alpha numerator
#endif
#ifndef TS_WPM_EMA_ALPHA_DEN
#    define TS_WPM_EMA_ALPHA_DEN 8 // EMA alpha denominator (1/8 smoothing)
#endif
#ifndef TS_ENABLE_LAYER_TIME
#    define TS_ENABLE_LAYER_TIME 0 // enable layer time tracking
#endif
#ifndef TS_ENABLE_BIGRAM_STATS
#    define TS_ENABLE_BIGRAM_STATS 0 // enable bigram (key pair) tracking
#endif

// Per-position counters: rows*cols*uint16_t -> small & universal
typedef struct {
    uint16_t presses; // wraps after 65535; bump to u32 if needed
} ts_pos_t;

#if TS_ENABLE_BIGRAM_STATS
// Bigram tracking (limited set to avoid memory explosion)
#define TS_MAX_BIGRAMS 64
typedef struct {
    uint8_t  key1_pos; // position index of first key
    uint8_t  key2_pos; // position index of second key
    uint16_t count;    // frequency of this bigram
} ts_bigram_t;
#endif

typedef struct {
    uint32_t total_presses;
    uint16_t max_wpm;
    uint16_t avg_wpm_ema;   // fixed-point-ish (integer EMA of WPM)
    uint32_t mod_counts[8]; // bit order: LCTL, LSFT, LALT, LGUI, RCTL, RSFT, RALT, RGUI
    uint32_t layer_counts[TS_MAX_LAYERS];

#if TS_ENABLE_LAYER_TIME
    uint32_t layer_time_ms[TS_MAX_LAYERS]; // time spent on each layer
#endif

#if TS_ENABLE_BIGRAM_STATS
    ts_bigram_t bigrams[TS_MAX_BIGRAMS];
    uint8_t     bigram_count; // number of active bigram entries
#endif

    // Session statistics
    uint32_t session_presses;
    uint32_t session_start_time;
    uint16_t session_max_wpm;

    // Hand balance (assuming split keyboard layout)
    uint32_t left_hand_presses;  // positions 0 to MATRIX_COLS/2 - 1
    uint32_t right_hand_presses; // positions MATRIX_COLS/2 to MATRIX_COLS - 1

    // Typing patterns
    uint32_t consecutive_same_finger; // rough estimate
    uint32_t finger_rolls;            // adjacent finger sequences

    // Per-position
    ts_pos_t pos[MATRIX_ROWS * MATRIX_COLS];
} ts_counters_t;

// EEPROM layout: header + counters + CRC
typedef struct {
    uint32_t      magic;
    uint16_t      version;
    uint16_t      reserved;
    ts_counters_t c;
    uint32_t      crc32;
} ts_blob_t;

// Hand identification for split keyboards
typedef enum {
    TS_HAND_LEFT,
    TS_HAND_RIGHT,
    TS_HAND_UNKNOWN
} ts_hand_t;

// Statistics summary structure
typedef struct {
    uint32_t total_lifetime_presses;
    uint32_t session_presses;
    uint16_t current_wpm;
    uint16_t avg_wpm;
    uint16_t max_wpm;
    uint16_t session_max_wpm;
    float    left_hand_ratio;
    uint8_t  most_used_layer;
    uint8_t  most_used_mod;
    uint16_t most_used_pos_index;
} ts_summary_t;

// API - Core functions
void          ts_init(void);
void          ts_task_10ms(void); // call periodically (e.g., from matrix_scan_user)
void          ts_on_keyevent(keyrecord_t *record, uint16_t keycode);
layer_state_t ts_on_layer_change(layer_state_t new_state);
void          ts_eeconfig_init_user(void); // call this from eeconfig_init_user()
void          ts_reset_defaults(void);     // optional: resets in-RAM to defaults (no save)

// API - Basic getters
uint16_t ts_get_current_wpm(void);
uint16_t ts_get_avg_wpm(void);
uint16_t ts_get_max_wpm(void);
uint32_t ts_get_total_presses(void);
uint32_t ts_get_session_presses(void);
uint16_t ts_get_session_max_wpm(void);
uint32_t ts_get_session_time_minutes(void);

// API - Hand balance
float    ts_get_left_hand_ratio(void);
float    ts_get_right_hand_ratio(void);
ts_hand_t ts_pos_to_hand(uint8_t row, uint8_t col);

// API - Position queries (computed on demand, O(n) over arrays)
uint16_t ts_pos_to_index(uint8_t row, uint8_t col);
void     ts_index_to_pos(uint16_t index, uint8_t *row_out, uint8_t *col_out);
bool     ts_find_most_used_pos(uint16_t *index_out, uint32_t *count_out);
bool     ts_find_least_used_pos(uint16_t *index_out, uint32_t *count_out, bool nonzero_only);
uint32_t ts_get_pos_presses(uint8_t row, uint8_t col);

// API - Layer queries
int8_t   ts_find_most_used_layer(uint8_t *layer_out, uint32_t *count_out);
int8_t   ts_find_least_used_layer(uint8_t *layer_out, uint32_t *count_out, bool nonzero_only);
uint32_t ts_get_layer_presses(uint8_t layer);
#if TS_ENABLE_LAYER_TIME
uint32_t ts_get_layer_time_ms(uint8_t layer);
float    ts_get_layer_time_ratio(uint8_t layer);
#endif

// API - Modifier queries
int8_t   ts_find_most_used_mod(uint8_t *modbit_out, uint32_t *count_out);
int8_t   ts_find_least_used_mod(uint8_t *modbit_out, uint32_t *count_out, bool nonzero_only);
uint32_t ts_get_mod_presses(uint8_t modbit_index);
const char* ts_modbit_to_string(uint8_t modbit_index);

#if TS_ENABLE_BIGRAM_STATS
// API - Bigram queries
bool     ts_find_most_used_bigram(uint8_t *pos1_out, uint8_t *pos2_out, uint16_t *count_out);
void     ts_get_top_bigrams(ts_bigram_t *output, uint8_t max_count, uint8_t *actual_count);
#endif

// API - Summary and reporting
void     ts_get_summary(ts_summary_t *summary);
void     ts_start_new_session(void);

// API - Advanced analysis
uint32_t ts_estimate_consecutive_same_finger(void);
uint32_t ts_estimate_finger_rolls(void);
float    ts_calculate_key_distribution_entropy(void);

// Debug and maintenance
void ts_mark_dirty(void);
void ts_force_flush(void);
void ts_debug_print(void);
void ts_debug_print_detailed(void);
void ts_debug_print_heatmap(void);

// Optional display integration
#ifdef OLED_ENABLE
void ts_render_oled_stats(void);
void ts_render_oled_heatmap(void);
#endif
