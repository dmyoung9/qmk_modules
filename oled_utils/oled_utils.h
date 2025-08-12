#pragma once

#include <stdint.h>
#include QMK_KEYBOARD_H

typedef struct {
    const uint8_t PROGMEM *data;  // pointer to page-packed bitmap
    uint8_t                width; // bytes per page (== pixels wide)
    uint8_t                pages; // height/8 (32px -> 4)
} slice_t;

void clear_rect(uint8_t x_px, uint8_t y_px, uint8_t w, uint8_t h);
void clear_span16(uint8_t x_px, uint8_t y_px);
void draw_slice_px(const slice_t *s, uint8_t x_px, uint8_t y_px);
