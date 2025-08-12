#pragma once

#include <stdint.h>
#include QMK_KEYBOARD_H

typedef struct {
    const uint8_t PROGMEM *data;  // pointer to page-packed bitmap
    uint8_t                width; // bytes per page (== pixels wide)
    uint8_t                pages; // height/8 (32px -> 4)
} slice_t;
