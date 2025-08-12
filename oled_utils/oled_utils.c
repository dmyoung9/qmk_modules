#include QMK_KEYBOARD_H
#include "oled_utils.h"

static inline uint16_t oled_offset(uint8_t x_px, uint8_t page) {
    return (uint16_t)page * OLED_DISPLAY_WIDTH + x_px;
}

// Rotation-safe, simple pixel clear for any rectangle.
void clear_rect(uint8_t x_px, uint8_t y_px, uint8_t w, uint8_t h) {
    if (!w || !h) return;
    if (x_px >= OLED_DISPLAY_WIDTH || y_px >= OLED_DISPLAY_HEIGHT) return;

    uint8_t x_end = x_px + w;
    uint8_t y_end = y_px + h;
    if (x_end > OLED_DISPLAY_WIDTH) x_end = OLED_DISPLAY_WIDTH;
    if (y_end > OLED_DISPLAY_HEIGHT) y_end = OLED_DISPLAY_HEIGHT;

    for (uint8_t y = y_px; y < y_end; y++) {
        for (uint8_t x = x_px; x < x_end; x++) {
            oled_write_pixel(x, y, false); // rotation-safe
        }
    }
}

// Keep the shim the same
void clear_span16(uint8_t x_px, uint8_t y_px) {
    clear_rect(x_px, y_px, 16, 8);
}

// General blitter: handles any y offset; keeps fast path for page-aligned
void draw_slice_px(const slice_t *s, uint8_t x_px, uint8_t y_px) {
    if (!s || !s->width || !s->pages) return;
    if (x_px >= OLED_DISPLAY_WIDTH || y_px >= OLED_DISPLAY_HEIGHT) return;

    uint8_t w = s->width;
    // Horizontal clip
    if ((uint16_t)x_px + w > OLED_DISPLAY_WIDTH) {
        w = OLED_DISPLAY_WIDTH - x_px;
    }

    const uint8_t y_off    = (y_px & 7);
    const uint8_t start_pg = (y_px >> 3);
    const uint8_t max_pg   = (OLED_DISPLAY_HEIGHT / 8);

    // Fast path: page-aligned writes
    if (y_off == 0) {
        for (uint8_t p = 0; p < s->pages; p++) {
            const uint8_t dst_pg = (uint8_t)(start_pg + p);
            if (dst_pg >= max_pg) break;

            uint16_t               dst_base = oled_offset(x_px, dst_pg);
            const uint8_t PROGMEM *src      = s->data + (uint16_t)p * s->width;
            for (uint8_t i = 0; i < w; i++) {
                oled_write_raw_byte(pgm_read_byte(src + i), dst_base + i);
            }
        }
        return;
    }

    // Unaligned: split each source byte across two pages (RMW OR-blend)
    const uint8_t carry_shift = (uint8_t)(8 - y_off);

    for (uint8_t p = 0; p < s->pages; p++) {
        const uint8_t dst_pg_lo = (uint8_t)(start_pg + p);
        const uint8_t dst_pg_hi = (uint8_t)(dst_pg_lo + 1);

        const uint8_t PROGMEM *src = s->data + (uint16_t)p * s->width;

        // Lower page (shift up)
        if (dst_pg_lo < max_pg) {
            uint16_t             base = oled_offset(x_px, dst_pg_lo);
            oled_buffer_reader_t r    = oled_read_raw(base);
            uint8_t             *dst  = r.current_element;

            for (uint8_t i = 0; i < w; i++) {
                uint8_t sb  = pgm_read_byte(src + i);
                uint8_t val = (uint8_t)(dst[i] | (uint8_t)(sb << y_off));
                oled_write_raw_byte(val, base + i);
            }
        }
        // Upper page (carry bits)
        if (dst_pg_hi < max_pg) {
            uint16_t             base = oled_offset(x_px, dst_pg_hi);
            oled_buffer_reader_t r    = oled_read_raw(base);
            uint8_t             *dst  = r.current_element;

            for (uint8_t i = 0; i < w; i++) {
                uint8_t sb  = pgm_read_byte(src + i);
                uint8_t val = (uint8_t)(dst[i] | (uint8_t)(sb >> carry_shift));
                oled_write_raw_byte(val, base + i);
            }
        }
    }
}
