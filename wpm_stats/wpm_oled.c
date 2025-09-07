#include <stdbool.h>
#include <stdint.h>

#include QMK_KEYBOARD_H

#include "wpm_stats.h"

#ifdef OLED_ENABLE
static bool             g_oled_initialized = false;
static wpm_bar_config_t g_bar_config       = {.x = WPM_BAR_X, .y = WPM_BAR_Y, .width = WPM_BAR_WIDTH, .height = WPM_BAR_HEIGHT};

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
    uint16_t inner_width = g_bar_config.width - 2; // Subtract 2 for left and right borders

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
    uint16_t line_x       = g_bar_config.x + 1 + wpm_position;
    uint16_t line_y_start = g_bar_config.y + 1;
    uint16_t line_y_end   = g_bar_config.y + g_bar_config.height - 2;

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

bool wpm_stats_oled_render(void) {
    if (!g_oled_initialized) {
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
        current_draw_pos     = (average_pos > 2) ? average_pos - 2 : average_pos + 4;
        uint16_t inner_width = g_bar_config.width - 2;
        if (current_draw_pos >= inner_width) {
            current_draw_pos = inner_width - 1;
        }
    }

    return draw_wpm_line(current_draw_pos, 1);
}

#endif
