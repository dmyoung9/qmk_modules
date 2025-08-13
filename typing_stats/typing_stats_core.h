#pragma once

#include <stdbool.h>
#include <stdint.h>
#include QMK_KEYBOARD_H

// ---------- Configuration ----------
#ifndef TS_MAX_LAYERS
#    define TS_MAX_LAYERS 8
#endif
#ifndef TS_FLUSH_SECONDS
#    define TS_FLUSH_SECONDS 120
#endif
#ifndef TS_FLUSH_EVENTS
#    define TS_FLUSH_EVENTS 2000
#endif
#ifndef TS_WPM_EMA_ALPHA_NUM
#    define TS_WPM_EMA_ALPHA_NUM 1
#endif
#ifndef TS_WPM_EMA_ALPHA_DEN
#    define TS_WPM_EMA_ALPHA_DEN 8
#endif

// Feature flags
#ifndef TS_ENABLE_LAYER_TIME
#    define TS_ENABLE_LAYER_TIME 0
#endif
#ifndef TS_ENABLE_BIGRAM_STATS
#    define TS_ENABLE_BIGRAM_STATS 0
#endif
#ifndef TS_ENABLE_ADVANCED_ANALYSIS
#    define TS_ENABLE_ADVANCED_ANALYSIS 0
#endif

// Magic and version for EEPROM validation
#define TS_MAGIC 0x54535432u // "TST2"
#define TS_VERSION 0x0004

// Hand identification for split keyboards
typedef enum {
    TS_HAND_LEFT,
    TS_HAND_RIGHT,
    TS_HAND_UNKNOWN
} ts_hand_t;

// Per-position press counter
typedef struct {
    uint16_t presses;
} ts_pos_t;

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

// Forward declarations for internal data structure
typedef struct ts_counters ts_counters_t;
typedef struct ts_blob     ts_blob_t;

// Core system state access (implemented in typing_stats_core.c)
ts_counters_t *ts_core_get_counters(void);
void           ts_core_mark_dirty(void);
bool           ts_core_is_initialized(void);
uint32_t       ts_core_get_event_counter(void);
void           ts_core_increment_event_counter(void);

// Utility functions
uint16_t  ts_pos_to_index(uint8_t row, uint8_t col);
void      ts_index_to_pos(uint16_t index, uint8_t *row_out, uint8_t *col_out);
ts_hand_t ts_pos_to_hand(uint8_t row, uint8_t col);

uint16_t ts_get_current_wpm(void);
uint16_t ts_get_avg_wpm(void);
uint16_t ts_get_max_wpm(void);
uint16_t ts_get_session_max_wpm(void);
uint32_t ts_get_total_presses(void);
uint32_t ts_get_session_presses(void);
uint32_t ts_get_session_time_minutes(void);
float    ts_get_left_hand_ratio(void);
float    ts_get_right_hand_ratio(void);

uint32_t ts_core_get_pos_presses(uint16_t pos_index);        // 0..(MATRIX_ROWS*MATRIX_COLS-1)
uint32_t ts_core_get_consecutive_same_finger(void);
uint32_t ts_core_get_finger_rolls(void);

#if TS_ENABLE_BIGRAM_STATS
void    ts_core_bigram_clear(void);
uint8_t ts_core_bigram_count(void);
bool    ts_core_bigram_get(uint8_t idx, uint8_t *p1, uint8_t *p2, uint16_t *count);
void    ts_core_bigram_increment(uint8_t p1, uint8_t p2);
#endif

// ---- Layer counters (read/write) ----
uint32_t ts_core_layer_get_presses(uint8_t layer);
void     ts_core_layer_increment(uint8_t layer);

#if TS_ENABLE_LAYER_TIME
// ---- Layer time (read/accumulate) ----
uint32_t ts_core_layer_get_time_ms(uint8_t layer);
void     ts_core_layer_add_time(uint8_t layer, uint32_t delta_ms);
#endif

// ---- Modifier counters (read/write) ----
uint32_t ts_core_mod_get_presses(uint8_t mod_index);  // 0..7
void     ts_core_mod_increment(uint8_t mod_index);    // +1 and mark dirty
                                                      //
                                                      // // ---- Position counters (read/write by index) ----
uint32_t ts_core_pos_get_presses_by_index(uint16_t index);   // 0..(MATRIX_ROWS*MATRIX_COLS-1)
void     ts_core_pos_increment_by_index(uint16_t index);     // saturates at UINT16_MAX

// ---- Hand counters (read/write) ----
void     ts_core_hand_increment(ts_hand_t hand);
uint32_t ts_core_left_presses(void);
uint32_t ts_core_right_presses(void);

// Provide storage with access to the full blob address (for save/load)
ts_blob_t *ts_core_get_blob_ptr(void);

