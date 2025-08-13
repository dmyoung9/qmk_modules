#pragma once

#include "typing_stats_core.h"

#if TS_ENABLE_BIGRAM_STATS

#define TS_MAX_BIGRAMS 64

// Bigram data structure
typedef struct {
    uint8_t  key1_pos;
    uint8_t  key2_pos;
    uint16_t count;
} ts_bigram_t;

// Bigram tracking API
void ts_bigram_init(void);
void ts_bigram_record_press(uint8_t row, uint8_t col);
void ts_bigram_reset(void);

// Bigram queries
bool ts_bigram_find_most_used(uint8_t *pos1_out, uint8_t *pos2_out, uint16_t *count_out);
void ts_bigram_get_top(ts_bigram_t *output, uint8_t max_count, uint8_t *actual_count);

#endif
