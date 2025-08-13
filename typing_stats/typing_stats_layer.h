#pragma once

#include "typing_stats_core.h"

// Layer tracking API
void ts_layer_record_press(uint8_t layer);
uint32_t ts_layer_get_presses(uint8_t layer);

// Layer queries
int8_t ts_layer_find_most_used(uint8_t *layer_out, uint32_t *count_out);
int8_t ts_layer_find_least_used(uint8_t *layer_out, uint32_t *count_out, bool nonzero_only);

#if TS_ENABLE_LAYER_TIME
// Layer time tracking
void ts_layer_init(void);
void ts_layer_on_change(layer_state_t new_state);
uint32_t ts_layer_get_time_ms(uint8_t layer);
float ts_layer_get_time_ratio(uint8_t layer);
#endif
