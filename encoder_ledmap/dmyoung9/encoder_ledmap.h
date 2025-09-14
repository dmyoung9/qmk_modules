#pragma once

#include <stdint.h>
#include QMK_KEYBOARD_H

#ifndef ENCODER_LED_TIMEOUT
#define ENCODER_LED_TIMEOUT 500
#endif

#ifndef ENCODER_LED_CCW_RGB
#define ENCODER_LED_CCW_RGB 0x0ff, 0, 0
#endif

#ifndef ENCODER_LED_CW_RGB
#define ENCODER_LED_CW_RGB 0, 0xff, 0
#endif

extern const uint8_t encoder_leds[NUM_ENCODERS];
