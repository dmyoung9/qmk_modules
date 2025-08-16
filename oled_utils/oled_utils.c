/**
 * @file oled_utils.c
 * @brief Implementation of core OLED utilities
 */

#include QMK_KEYBOARD_H
#include "oled_utils.h"

// ============================================================================
// Internal Helpers
// ============================================================================

/**
 * @brief Calculate buffer offset for given pixel coordinates
 * @param x_px X coordinate in pixels
 * @param page Page number (y_px / 8)
 * @return Buffer offset
 */
static inline uint16_t oled_offset(uint8_t x_px, uint8_t page) {
    return (uint16_t)page * OLED_DISPLAY_WIDTH + x_px;
}

// ============================================================================
// Core Drawing Functions
// ============================================================================

void clear_rect(uint8_t x_px, uint8_t y_px, uint8_t w, uint8_t h) {
    // Early exit for invalid dimensions
    if (!w || !h) return;

    // Early exit if completely outside display bounds
    if (x_px >= OLED_DISPLAY_WIDTH || y_px >= OLED_DISPLAY_HEIGHT) return;

    // Calculate clipped bounds
    uint8_t x_end = x_px + w;
    uint8_t y_end = y_px + h;
    if (x_end > OLED_DISPLAY_WIDTH) x_end = OLED_DISPLAY_WIDTH;
    if (y_end > OLED_DISPLAY_HEIGHT) y_end = OLED_DISPLAY_HEIGHT;

    // Clear pixels using rotation-safe pixel writes
    for (uint8_t y = y_px; y < y_end; y++) {
        for (uint8_t x = x_px; x < x_end; x++) {
            oled_write_pixel(x, y, false); // rotation-safe
        }
    }
}

void clear_span16(uint8_t x_px, uint8_t y_px) {
    clear_rect(x_px, y_px, 16, 8);
}

void draw_slice_px(const slice_t *s, uint8_t x_px, uint8_t y_px) {
    // Validate input parameters
    if (!slice_is_valid(s)) return;
    if (x_px >= OLED_DISPLAY_WIDTH || y_px >= OLED_DISPLAY_HEIGHT) return;

    // Calculate clipped width to prevent buffer overruns
    uint8_t w = s->width;
    if ((uint16_t)x_px + w > OLED_DISPLAY_WIDTH) {
        w = OLED_DISPLAY_WIDTH - x_px;
    }

    // Get actual height in pixels (handles arbitrary heights)
    const uint8_t actual_height = slice_height_px(s);

    // Calculate page alignment parameters
    const uint8_t y_off    = (y_px & 7);        // Offset within page (0-7)
    const uint8_t start_pg = (y_px >> 3);       // Starting page number
    const uint8_t max_pg   = (OLED_DISPLAY_HEIGHT / 8); // Maximum page number

    // Fast path: page-aligned writes (y_px is multiple of 8)
    if (y_off == 0) {
        for (uint8_t p = 0; p < s->pages; p++) {
            const uint8_t dst_pg = (uint8_t)(start_pg + p);
            if (dst_pg >= max_pg) break; // Clip vertically

            // Calculate source and destination addresses
            uint16_t               dst_base = oled_offset(x_px, dst_pg);
            const uint8_t PROGMEM *src      = s->data + (uint16_t)p * s->width;

            // Check if this is the last page and we have arbitrary height
            const bool is_last_page = (p == s->pages - 1);
            const bool has_arbitrary_height = (s->height_px > 0);

            if (is_last_page && has_arbitrary_height) {
                // Calculate how many pixels to draw in the last page
                const uint8_t pixels_in_last_page = actual_height - (p * 8);
                const uint8_t src_mask = (uint8_t)((1 << pixels_in_last_page) - 1);
                const uint8_t dst_mask = (uint8_t)(~src_mask); // Preserve existing bits

                // Read-modify-write with proper masking
                oled_buffer_reader_t r = oled_read_raw(dst_base);
                uint8_t *dst = r.current_element;

                for (uint8_t i = 0; i < w; i++) {
                    uint8_t src_byte = pgm_read_byte(src + i);
                    uint8_t dst_byte = dst[i];
                    // Clear the bits we're going to write, then set the new bits
                    uint8_t new_val = (dst_byte & dst_mask) | (src_byte & src_mask);
                    oled_write_raw_byte(new_val, dst_base + i);
                }
            } else {
                // Copy entire row with PROGMEM reads
                for (uint8_t i = 0; i < w; i++) {
                    oled_write_raw_byte(pgm_read_byte(src + i), dst_base + i);
                }
            }
        }
        return;
    }

    // Unaligned path: split each source byte across two pages with proper masking
    const uint8_t carry_shift = (uint8_t)(8 - y_off);

    for (uint8_t p = 0; p < s->pages; p++) {
        const uint8_t dst_pg_lo = (uint8_t)(start_pg + p);     // Lower destination page
        const uint8_t dst_pg_hi = (uint8_t)(dst_pg_lo + 1);   // Upper destination page

        const uint8_t PROGMEM *src = s->data + (uint16_t)p * s->width;

        // Check if this is the last page and we have arbitrary height
        const bool is_last_page = (p == s->pages - 1);
        const bool has_arbitrary_height = (s->height_px > 0);
        uint8_t src_mask = 0xFF; // Default: use all bits

        if (is_last_page && has_arbitrary_height) {
            // Calculate how many pixels to draw in the last page
            const uint8_t pixels_in_last_page = actual_height - (p * 8);
            src_mask = (uint8_t)((1 << pixels_in_last_page) - 1);
        }

        // Calculate masks for destination pages
        const uint8_t lo_src_mask = (uint8_t)(src_mask << y_off);
        const uint8_t lo_dst_mask = (uint8_t)(~lo_src_mask);
        const uint8_t hi_src_mask = (uint8_t)(src_mask >> carry_shift);
        const uint8_t hi_dst_mask = (uint8_t)(~hi_src_mask);

        // Write to lower page (shift source bits up)
        if (dst_pg_lo < max_pg) {
            uint16_t             base = oled_offset(x_px, dst_pg_lo);
            oled_buffer_reader_t r    = oled_read_raw(base);
            uint8_t             *dst  = r.current_element;

            for (uint8_t i = 0; i < w; i++) {
                uint8_t src_byte = pgm_read_byte(src + i);
                uint8_t src_shifted = (uint8_t)(src_byte << y_off);
                uint8_t new_val = (dst[i] & lo_dst_mask) | (src_shifted & lo_src_mask);
                oled_write_raw_byte(new_val, base + i);
            }
        }

        // Write to upper page (carry bits from source)
        if (dst_pg_hi < max_pg) {
            uint16_t             base = oled_offset(x_px, dst_pg_hi);
            oled_buffer_reader_t r    = oled_read_raw(base);
            uint8_t             *dst  = r.current_element;

            for (uint8_t i = 0; i < w; i++) {
                uint8_t src_byte = pgm_read_byte(src + i);
                uint8_t src_shifted = (uint8_t)(src_byte >> carry_shift);
                uint8_t new_val = (dst[i] & hi_dst_mask) | (src_shifted & hi_src_mask);
                oled_write_raw_byte(new_val, base + i);
            }
        }
    }
}
