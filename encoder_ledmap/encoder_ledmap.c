#include <stdbool.h>
#include <stdint.h>

#include "quantum.h"

#include "dmyoung9/encoder_ledmap.h"

static encoder_state_t g_encoder_state[NUM_ENCODERS];

#ifdef SPLIT_KEYBOARD
#include "transactions.h"

static bool g_encoder_ledmap_sync_initialized = false;

static void encoder_ledmap_sync_slave_handler(uint8_t in_buflen, const void *in_data, uint8_t out_buflen, void *out_data) {
    if (in_buflen >= sizeof(g_encoder_state)) {
        memcpy(g_encoder_state, in_data, sizeof(g_encoder_state));
    }
}

void keyboard_post_init_encoder_ledmap(void) {
    if (g_encoder_ledmap_sync_initialized) return;

    transaction_register_rpc(ENCODER_LEDMAP_SYNC, encoder_ledmap_sync_slave_handler);
    g_encoder_ledmap_sync_initialized = true;
}
#endif

bool process_record_encoder_ledmap(uint16_t keycode, keyrecord_t *record) {
    if (record->event.type == ENCODER_CCW_EVENT || record->event.type == ENCODER_CW_EVENT) {
        const uint8_t encoder_index = record->event.key.col;
        if (encoder_index < NUM_ENCODERS) {
            g_encoder_state[encoder_index].clockwise = (record->event.type == ENCODER_CW_EVENT);
            g_encoder_state[encoder_index].layer = get_highest_layer(layer_state);
        }
    }

    return true;
}

// Helper function to convert color_t to rgb_t (based on indicators module implementation)
static int encoder_ledmap_get_rgb(color_t color, rgb_t *rgb) {
    switch (color.type) {
        case COLOR_TYPE_RGB:
            *rgb = color.rgb;
            break;

        case COLOR_TYPE_HSV:
            *rgb = hsv_to_rgb(color.hsv);
            break;

        case COLOR_TYPE_HUE:
            *rgb = hsv_to_rgb((hsv_t){
                .h = color.hsv.h,
                .s = rgb_matrix_get_sat(),
                .v = rgb_matrix_get_val(),
            });
            break;

        default:
            return -1;
    }

    return 0;
}

bool rgb_matrix_indicators_encoder_ledmap(void) {
#ifdef SPLIT_KEYBOARD
    if (!g_encoder_ledmap_sync_initialized) return true;
#endif

    if (!is_keyboard_master()) {
        if (last_encoder_activity_elapsed() < ENCODER_LED_TIMEOUT) {
            for (uint8_t encoder_index = 0; encoder_index < NUM_ENCODERS; encoder_index++) {
                const uint8_t layer = g_encoder_state[encoder_index].layer;
                const bool clockwise = g_encoder_state[encoder_index].clockwise;

                if (layer >= encoder_ledmap_layer_count()) {
                    continue;
                }

                color_t color = color_at_encoder_ledmap_location(layer, encoder_index, clockwise);
                rgb_t rgb;

                if (encoder_ledmap_get_rgb(color, &rgb) == 0) {
                    rgb_matrix_set_color(encoder_leds[encoder_index], rgb.r, rgb.g, rgb.b);
                }
            }
        }
    }

    return true;
}

#ifdef SPLIT_KEYBOARD
void housekeeping_task_encoder_ledmap(void) {
    if (!g_encoder_ledmap_sync_initialized) return;

    if (is_keyboard_master()) {
        transaction_rpc_send(ENCODER_LEDMAP_SYNC, sizeof(g_encoder_state), &g_encoder_state);
    }
}
#endif
