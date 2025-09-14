#include <stdbool.h>
#include <stdint.h>

#include QMK_KEYBOARD_H
#ifdef SPLIT_KEYBOARD
#include "transactions.h"
#endif

#include "dmyoung9/encoder_ledmap.h"

static bool g_encoder_clockwise                  = false;

#ifdef SPLIT_KEYBOARD
static bool g_encoder_led_sync_split_initialized = false;

static void encoder_led_sync_slave_handler(uint8_t in_buflen, const void *in_data, uint8_t out_buflen, void *out_data) {
    const bool clockwise = *(const bool *)in_data;
    g_encoder_clockwise  = clockwise;
}

void keyboard_post_init_encoder_ledmap(void) {
    if (!g_encoder_led_sync_split_initialized) {
        transaction_register_rpc(ENCODER_LED_SYNC, encoder_led_sync_slave_handler);
        g_encoder_led_sync_split_initialized = true;
    }
}
#endif

bool process_record_encoder_ledmap(uint16_t keycode, keyrecord_t *record) {
#ifdef SPLIT_KEYBOARD
    if (!g_encoder_led_sync_split_initialized) return true;
#endif

    if (is_keyboard_master()) {
        if (record->event.type == ENCODER_CW_EVENT) {
            g_encoder_clockwise = true;
        } else if (record->event.type == ENCODER_CCW_EVENT) {
            g_encoder_clockwise = false;
        }
    }

    return true;
}

bool rgb_matrix_indicators_encoder_ledmap(void) {
#ifdef SPLIT_KEYBOARD
    if (!g_encoder_led_sync_split_initialized) return true;
#endif

    if (!is_keyboard_master()) {
        if (last_encoder_activity_elapsed() < ENCODER_LED_TIMEOUT) {
            if (g_encoder_clockwise) {
                rgb_matrix_set_color(encoder_leds[0], ENCODER_LED_CW_RGB);
            } else {
                rgb_matrix_set_color(encoder_leds[0], ENCODER_LED_CCW_RGB);
            }
        }
    }

    return true;
}

#ifdef SPLIT_KEYBOARD
void housekeeping_task_encoder_ledmap(void) {
    if (!g_encoder_led_sync_split_initialized) return;

    if (is_keyboard_master()) {
        transaction_rpc_send(ENCODER_LED_SYNC, sizeof(bool), &g_encoder_clockwise);
    }
}
#endif
