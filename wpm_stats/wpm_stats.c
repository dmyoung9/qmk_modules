#include "wpm_stats.h"
#include "wpm.h"
#include "timer.h"

// Internal state
static bool g_initialized = false;
static uint16_t g_max_wpm = 0;
static uint32_t g_wpm_sum = 0;
static uint16_t g_wpm_count = 0;

void wpm_stats_init(void) {
    g_initialized = true;
    g_max_wpm = 0;
    g_wpm_sum = 0;
    g_wpm_count = 0;
}

void wpm_stats_task(void) {
    if (!g_initialized) return;

    // Update WPM stats periodically
    static uint32_t last = 0;
    uint32_t now = timer_read32();
    if (now - last >= 1000) {  // Update every second
        last = now;
        uint16_t current_wpm = get_current_wpm();

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
                g_wpm_sum = (g_wpm_sum / g_wpm_count) * 100 + current_wpm;
                g_wpm_count = 101;
            }
        }
    }
}

void wpm_stats_on_keyevent(keyrecord_t *record) {
    if (!g_initialized || !record->event.pressed) return;

    // QMK's built-in WPM system handles the keypress automatically
    // We just need to make sure it's enabled
}

bool wpm_stats_get(wpm_stats_t *stats) {
    if (!stats || !g_initialized) {
        return false;
    }

    stats->current_wpm = get_current_wpm();
    stats->average_wpm = wpm_stats_get_avg();
    stats->session_max_wpm = wpm_stats_get_max();

    return true;
}

uint16_t wpm_stats_get_current(void) {
    if (!g_initialized) return 0;
    return get_current_wpm();
}

uint16_t wpm_stats_get_avg(void) {
    if (!g_initialized) return 0;

    // On slave side, use synced data from master
    extern bool is_keyboard_master(void);
    if (!is_keyboard_master()) {
        return g_slave_wpm_data.average_wpm;
    }

    return (g_wpm_count > 0) ? (uint16_t)(g_wpm_sum / g_wpm_count) : 0;
}

uint16_t wpm_stats_get_max(void) {
    if (!g_initialized) return 0;

    // On slave side, use synced data from master
    extern bool is_keyboard_master(void);
    if (!is_keyboard_master()) {
        return g_slave_wpm_data.session_max_wpm;
    }

    return g_max_wpm;
}

void wpm_stats_reset(void) {
    if (!g_initialized) return;

    g_max_wpm = 0;
    g_wpm_sum = 0;
    g_wpm_count = 0;
    // Current WPM will naturally reset when typing stops
}

// Note: wpm_stats_set_sync_data function removed as we're using shared variables instead
