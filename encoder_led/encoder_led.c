#include <stdbool.h>
#include <stdint.h>

#include QMK_KEYBOARD_H
#include "transactions.h"

static bool g_encoder_clockwise                  = false;
static bool g_encoder_led_sync_split_initialized = false;

static void encoder_led_sync_slave_handler(uint8_t in_buflen, const void *in_data, uint8_t out_buflen, void *out_data) {
    const bool clockwise = *(const bool *)in_data;
    g_encoder_clockwise  = clockwise;
}

void keyboard_post_init_encoder_led(void) {
    if (!g_encoder_led_sync_split_initialized) {
        transaction_register_rpc(ENCODER_LED_SYNC, encoder_led_sync_slave_handler);
        g_encoder_led_sync_split_initialized = true;
    }
}

bool process_record_encoder_led(uint16_t keycode, keyrecord_t *record) {
    if (!g_encoder_led_sync_split_initialized) return true;

    if (is_keyboard_master()) {
        const bool previous = g_encoder_clockwise;
        if (record->event.type == ENCODER_CW_EVENT) {
            g_encoder_clockwise = true;
        } else if (record->event.type == ENCODER_CCW_EVENT) {
            g_encoder_clockwise = false;
        }

        if (previous != g_encoder_clockwise) {
            transaction_rpc_send(ENCODER_LED_SYNC, sizeof(bool), &g_encoder_clockwise);
        }
    }

    return true;
}

bool rgb_matrix_indicators_encoder_led(void) {
    if (!g_encoder_led_sync_split_initialized) return true;

    if (!is_keyboard_master()) {
        if (last_encoder_activity_elapsed() < 500) {
            if (g_encoder_clockwise) {
                rgb_matrix_set_color(ENCODER_LED_INDEX, 0, 0xff, 0);
            } else {
                rgb_matrix_set_color(ENCODER_LED_INDEX, 0xff, 0, 0);
            }
        }
    }

    return true;
}
