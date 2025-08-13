#pragma once

#include "typing_stats_core.h"

// Modifier tracking API
void ts_mod_record_press(uint8_t mods);
uint32_t ts_mod_get_presses(uint8_t modbit_index);

// Modifier queries
int8_t ts_mod_find_most_used(uint8_t *modbit_out, uint32_t *count_out);
int8_t ts_mod_find_least_used(uint8_t *modbit_out, uint32_t *count_out, bool nonzero_only);

// Modifier utilities
const char* ts_mod_bit_to_string(uint8_t modbit_index);
