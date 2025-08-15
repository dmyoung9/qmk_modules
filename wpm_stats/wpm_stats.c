#include "wpm_stats.h"
#include "wpm.h"
#include "timer.h"
#include "transactions.h"

#ifdef OLED_ENABLE
#include "oled_driver.h"

// Default bar graph configuration
#ifndef WPM_BAR_X
#define WPM_BAR_X 1
#endif
#ifndef WPM_BAR_Y
#define WPM_BAR_Y 1
#endif
#ifndef WPM_BAR_WIDTH
#define WPM_BAR_WIDTH 124
#endif
#ifndef WPM_BAR_HEIGHT
#define WPM_BAR_HEIGHT 28
#endif

// Include the bar graph functionality
// Note: We'll need to copy the bar graph implementation into this module
// or make it a separate reusable component
#endif

// Internal state
static bool g_initialized = false;
static uint16_t g_max_wpm = 0;
static uint32_t g_wpm_sum = 0;
static uint16_t g_wpm_count = 0;

// Split keyboard sync state
wpm_stats_t g_slave_wpm_data = {0, 0, 0};
static bool g_split_sync_initialized = false;

#ifdef OLED_ENABLE
// OLED rendering state
static bool g_oled_initialized = false;
static wpm_bar_config_t g_bar_config = {
    .x = WPM_BAR_X,
    .y = WPM_BAR_Y,
    .width = WPM_BAR_WIDTH,
    .height = WPM_BAR_HEIGHT
};
#endif

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

// =============================================================================
// SPLIT KEYBOARD SYNC FUNCTIONALITY
// =============================================================================

// Slave-side handler for WPM data sync
static void wpm_stats_sync_slave_handler(uint8_t in_buflen, const void* in_data, uint8_t out_buflen, void* out_data) {
    const wpm_stats_t *master_data = (const wpm_stats_t*)in_data;
    g_slave_wpm_data = *master_data;
}

void wpm_stats_init_split_sync(void) {
    if (g_split_sync_initialized) return;

    // Register the slave-side handler for WPM sync
    transaction_register_rpc(WPM_STATS_SYNC, wpm_stats_sync_slave_handler);
    g_split_sync_initialized = true;
}

void wpm_stats_housekeeping_task(void) {
    if (!g_initialized) return;

    if (is_keyboard_master()) {
        // Sync with slave every 500ms
        static uint32_t last_sync = 0;
        if (timer_elapsed32(last_sync) > 500) {
            wpm_stats_t m2s;
            if (wpm_stats_get(&m2s)) {
                if (transaction_rpc_send(WPM_STATS_SYNC, sizeof(m2s), &m2s)) {
                    last_sync = timer_read32();
                } else {
                    // Sync failed, will retry next time
                }
            }
        }
    }
}

// =============================================================================
// OLED RENDERING INTEGRATION
// =============================================================================

#ifdef OLED_ENABLE

// =============================================================================
// BAR GRAPH IMPLEMENTATION
// =============================================================================

/**
 * @brief Calculate the X position for a WPM value within the bar
 * @param wpm WPM value to position
 * @param max_wpm Maximum WPM value for scaling
 * @return X position within the bar (relative to bar start)
 */
static uint16_t calculate_wpm_position(uint16_t wpm, uint16_t max_wpm) {
    if (max_wpm == 0) {
        return 0;
    }

    // Calculate position within the inner area (excluding border)
    uint16_t inner_width = g_bar_config.width - 2;  // Subtract 2 for left and right borders

    // Position markers proportionally between 0 and max_wpm
    uint32_t position = ((uint32_t)wpm * inner_width) / max_wpm;

    // Ensure we don't exceed the inner width
    if (position > inner_width) {
        position = inner_width;
    }

    return (uint16_t)position;
}

/**
 * @brief Set a pixel on the OLED display
 * @param x X coordinate
 * @param y Y coordinate
 * @param on true for white pixel, false for black
 */
static void set_pixel(uint16_t x, uint16_t y, bool on) {
    if (x >= OLED_DISPLAY_WIDTH || y >= OLED_DISPLAY_HEIGHT) {
        return;
    }
    oled_write_pixel(x, y, on);
}

/**
 * @brief Draw a horizontal line
 */
static void draw_hline(uint16_t x1, uint16_t x2, uint16_t y, bool on) {
    for (uint16_t x = x1; x <= x2; x++) {
        set_pixel(x, y, on);
    }
}

/**
 * @brief Draw a vertical line
 */
static void draw_vline(uint16_t x, uint16_t y1, uint16_t y2, bool on) {
    for (uint16_t y = y1; y <= y2; y++) {
        set_pixel(x, y, on);
    }
}

/**
 * @brief Draw the bar border
 */
static bool draw_bar_border(void) {
    uint16_t x1 = g_bar_config.x;
    uint16_t y1 = g_bar_config.y;
    uint16_t x2 = g_bar_config.x + g_bar_config.width - 1;
    uint16_t y2 = g_bar_config.y + g_bar_config.height - 1;

    // Top and bottom lines
    draw_hline(x1, x2, y1, true);
    draw_hline(x1, x2, y2, true);

    // Left and right lines
    draw_vline(x1, y1, y2, true);
    draw_vline(x2, y1, y2, true);

    return true;
}

/**
 * @brief Clear the inner area of the bar
 */
static bool clear_bar_inner(void) {
    for (uint16_t y = g_bar_config.y + 1; y < g_bar_config.y + g_bar_config.height - 1; y++) {
        for (uint16_t x = g_bar_config.x + 1; x < g_bar_config.x + g_bar_config.width - 1; x++) {
            set_pixel(x, y, false);
        }
    }
    return true;
}

/**
 * @brief Draw a vertical line for WPM indicator
 */
static bool draw_wpm_line(uint16_t wpm_position, uint8_t line_width) {
    uint16_t line_x = g_bar_config.x + 1 + wpm_position;
    uint16_t line_y_start = g_bar_config.y + 1;
    uint16_t line_y_end = g_bar_config.y + g_bar_config.height - 2;

    for (uint8_t i = 0; i < line_width; i++) {
        uint16_t current_x = line_x + i;
        if (current_x >= g_bar_config.x + g_bar_config.width - 1) {
            break;
        }
        draw_vline(current_x, line_y_start, line_y_end, true);
    }
    return true;
}

bool wpm_stats_oled_init(void) {
    if (g_oled_initialized) return true;

    // Use default configuration
    g_oled_initialized = true;
    return true;
}

bool wpm_stats_oled_init_config(const wpm_bar_config_t *config) {
    if (g_oled_initialized) return true;

    if (!config) return false;

    // Store the configuration
    g_bar_config = *config;

    g_oled_initialized = true;
    return true;
}

bool wpm_stats_oled_render(void) {
    if (!g_oled_initialized || !g_initialized) {
        return false;
    }

    // Get current WPM statistics
    wpm_stats_t wpm_data;
    if (!wpm_stats_get(&wpm_data)) {
        return false;
    }

    // Use the user's session max WPM as the scale
    uint16_t max_wpm = wpm_data.session_max_wpm;

    // Set a reasonable minimum scale
    uint16_t min_scale = 60;
    if (max_wpm < min_scale) {
        max_wpm = min_scale;
    }

    // Clear the area above the bar for text
    for (uint16_t y = 0; y < g_bar_config.y; y++) {
        for (uint16_t x = 0; x < OLED_DISPLAY_WIDTH; x++) {
            set_pixel(x, y, false);
        }
    }

    // Clear the inner area and draw border
    if (!clear_bar_inner() || !draw_bar_border()) {
        return false;
    }

    // Calculate positions for current and average WPM
    uint16_t current_pos, average_pos;

    if (wpm_data.session_max_wpm == 0) {
        current_pos = 0;
        average_pos = 0;
    } else {
        current_pos = calculate_wpm_position(wpm_data.current_wpm, max_wpm);
        average_pos = calculate_wpm_position(wpm_data.average_wpm, max_wpm);
    }

    // Draw average WPM line first (3px wide)
    if (!draw_wpm_line(average_pos, 3)) {
        return false;
    }

    // Draw current WPM line (1px wide, offset if overlapping)
    uint16_t current_draw_pos = current_pos;
    if (current_pos >= average_pos && current_pos <= average_pos + 2) {
        current_draw_pos = (average_pos > 2) ? average_pos - 2 : average_pos + 4;
        uint16_t inner_width = g_bar_config.width - 2;
        if (current_draw_pos >= inner_width) {
            current_draw_pos = inner_width - 1;
        }
    }

    return draw_wpm_line(current_draw_pos, 1);
}

#endif // OLED_ENABLE
