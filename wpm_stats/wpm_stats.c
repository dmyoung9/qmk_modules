#include <stdbool.h>
#include <stdint.h>

#include QMK_KEYBOARD_H

#include "wpm_stats.h"
#ifdef OLED_ENABLE
#include "wpm_oled.h"
#endif

static bool     g_initialized = false;
static uint32_t g_last_update;
static uint16_t g_max_wpm   = 0;
static uint32_t g_wpm_sum   = 0;
static uint16_t g_wpm_count = 0;

void wpm_stats_init(void) {
    g_initialized = true;
    g_max_wpm     = 0;
    g_wpm_sum     = 0;
    g_wpm_count   = 0;
    g_last_update = timer_read();
}

void keyboard_post_init_wpm_stats(void) {
    wpm_stats_init();

#ifdef OLED_ENABLE
    wpm_stats_oled_init();
#endif
}

void housekeeping_task_wpm_stats(void) {
    if (!g_initialized) return;

    if (timer_elapsed(g_last_update) >= 1000) { // Update every second
        uint16_t current_wpm = wpm_stats_get_current();

        // Only track when actively typing (WPM > 0)
        if (current_wpm > 0) {
            // Update max WPM
            if (current_wpm > g_max_wpm) {
                g_max_wpm = current_wpm;
            }

            // Update running average (prevent overflow)
            if (g_wpm_count < 1000) {
                g_wpm_sum += current_wpm;
                g_wpm_count++;
            } else {
                // Reset to prevent overflow, keeping recent average
                g_wpm_sum   = (g_wpm_sum / g_wpm_count) * 100 + current_wpm;
                g_wpm_count = 101;
            }
        }

        g_last_update = timer_read();
    }
}

bool wpm_stats_get(wpm_stats_t *stats) {
    if (!stats || !g_initialized) {
        return false;
    }

    stats->current_wpm     = wpm_stats_get_current();
    stats->average_wpm     = wpm_stats_get_avg();
    stats->session_max_wpm = wpm_stats_get_max();

    return true;
}

uint16_t wpm_stats_get_current(void) {
    if (!g_initialized) return 0;

    return get_current_wpm();
}

uint16_t wpm_stats_get_avg(void) {
    if (!g_initialized) return 0;

    return (g_wpm_count > 0) ? (uint16_t)(g_wpm_sum / g_wpm_count) : 0;
}

uint16_t wpm_stats_get_max(void) {
    if (!g_initialized) return 0;

    return g_max_wpm;
}

