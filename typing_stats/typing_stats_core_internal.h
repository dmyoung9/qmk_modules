#pragma once

#include "typing_stats_core.h"
#if TS_ENABLE_BIGRAM_STATS
#    include "typing_stats_bigram.h"   // for TS_MAX_BIGRAMS
#endif

// ---- Full, private definitions ----

// Core counters structure definition
struct ts_counters {
    uint32_t total_presses;
    uint16_t max_wpm;
    uint16_t avg_wpm_ema;
    uint32_t session_presses;
    uint32_t session_start_time;
    uint16_t session_max_wpm;
    uint32_t left_hand_presses;
    uint32_t right_hand_presses;
    uint32_t consecutive_same_finger;
    uint32_t finger_rolls;

    ts_pos_t  pos[MATRIX_ROWS * MATRIX_COLS];
    uint32_t  mod_counts[8];
    uint32_t  layer_counts[TS_MAX_LAYERS];

#if TS_ENABLE_LAYER_TIME
    uint32_t layer_time_ms[TS_MAX_LAYERS];
#endif

#if TS_ENABLE_BIGRAM_STATS
    struct { uint8_t key1_pos, key2_pos; uint16_t count; } bigrams[TS_MAX_BIGRAMS];
    uint8_t bigram_count;
#endif
};

// EEPROM blob structure
struct ts_blob {
    uint32_t      magic;
    uint16_t      version;
    uint16_t      reserved;
    ts_counters_t c;       // full, now-complete type
    uint32_t      crc32;
};

