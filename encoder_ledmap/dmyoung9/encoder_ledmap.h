#pragma once

#include <errno.h>
#include <stdint.h>

#include "compiler_support.h"
#include "quantum.h"

#ifndef ENCODER_LED_TIMEOUT
#define ENCODER_LED_TIMEOUT 500
#endif

#ifndef ENCODER_LED_CCW_RGB
#define ENCODER_LED_CCW_RGB 0x0ff, 0, 0
#endif

#ifndef ENCODER_LED_CW_RGB
#define ENCODER_LED_CW_RGB 0, 0xff, 0
#endif

#if !defined(RGB_MATRIX_ENABLE)
#    error RGB matrix must be enabled to use ledmap
#endif

#if defined(COMMUNITY_MODULE_INDICATORS_ENABLE)
#    include "elpekenin/indicators.h"
#    pragma message "Enable indicators after ledmap, otherwise you will overwrite them."
#else
#    error Must enable 'elpekenin/indicators'
#endif

/**
 * @brief Encoder state structure to track direction, layer, and activity information
 */
typedef struct {
    bool clockwise;         ///< True if last rotation was clockwise, false for counterclockwise
    uint8_t layer;          ///< Active layer when the encoder event was processed
} encoder_state_t;

extern const uint8_t encoder_leds[NUM_ENCODERS];
extern const color_t PROGMEM encoder_ledmap[][NUM_ENCODERS][NUM_DIRECTIONS];

size_t encoder_ledmap_layer_count(void);
color_t color_at_encoder_ledmap_location(uint8_t layer_num, uint8_t encoder_idx, bool clockwise);
