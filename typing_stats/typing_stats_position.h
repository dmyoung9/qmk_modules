#pragma once

#include "typing_stats_core.h"

// Position tracking API
void ts_pos_record_press(uint8_t row, uint8_t col);
uint32_t ts_pos_get_presses(uint8_t row, uint8_t col);

// Position queries
bool ts_pos_find_most_used(uint16_t *index_out, uint32_t *count_out);
bool ts_pos_find_least_used(uint16_t *index_out, uint32_t *count_out, bool nonzero_only);

// Hand balance functions
void ts_pos_record_hand_press(ts_hand_t hand);
uint32_t ts_pos_get_left_hand_presses(void);
uint32_t ts_pos_get_right_hand_presses(void);
