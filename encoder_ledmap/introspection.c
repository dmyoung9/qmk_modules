#include "compiler_support.h"
#include "progmem.h"

#include "dmyoung9/encoder_ledmap.h"

#define NUM_ENCODER_LEDMAP_LAYERS_RAW ARRAY_SIZE(encoder_ledmap)

STATIC_ASSERT(NUM_KEYMAP_LAYERS_RAW == NUM_ENCODER_LEDMAP_LAYERS_RAW, "Number of encoder ledmap layers doesn't match the number of keymap layers");

size_t encoder_ledmap_layer_count(void) {
    return NUM_ENCODER_LEDMAP_LAYERS_RAW;
}

color_t color_at_encoder_ledmap_location(uint8_t layer, uint8_t index, bool clockwise) {
    color_t value;
    memcpy_P(&value, &encoder_ledmap[layer][index][clockwise], sizeof(color_t));
    return value;
}
