/**
 * @file oled_utils.h
 * @brief Core OLED utilities for rotation-safe drawing and PROGMEM bitmap handling
 *
 * This module provides:
 * - Rotation-safe drawing primitives with automatic clipping
 * - Fast blitting for page-aligned art with correct blending for unaligned Y
 * - PROGMEM-friendly bitmap abstraction without compiler grief
 * - Simple, reliable clearing of arbitrary rectangles and common spans
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include QMK_KEYBOARD_H

/**
 * @brief Bitmap slice structure for page-packed OLED data
 *
 * Represents a bitmap stored in PROGMEM with page-packed format.
 * The data pointer is NOT marked PROGMEM - the arrays themselves are.
 * Always access data via pgm_read_byte().
 */
typedef struct {
    const uint8_t *data;  ///< Pointer to page-packed bitmap data in PROGMEM
    uint8_t        width; ///< Width in pixels (bytes per page)
    uint8_t        pages; ///< Height in pages (height/8, e.g., 32px -> 4 pages)
} slice_t;

// ============================================================================
// Core Drawing Functions
// ============================================================================

/**
 * @brief Clear any rectangular area with rotation-safe pixel clearing
 *
 * Clears the specified rectangle by setting all pixels to off (black).
 * Automatically clips to screen bounds and handles rotation correctly.
 *
 * @param x_px X coordinate in pixels
 * @param y_px Y coordinate in pixels
 * @param w Width in pixels
 * @param h Height in pixels
 */
void clear_rect(uint8_t x_px, uint8_t y_px, uint8_t w, uint8_t h);

/**
 * @brief Clear a common 16x8 pixel span (convenience function)
 *
 * Equivalent to clear_rect(x_px, y_px, 16, 8). Useful for clearing
 * standard glyph cells or small UI elements.
 *
 * @param x_px X coordinate in pixels
 * @param y_px Y coordinate in pixels
 */
void clear_span16(uint8_t x_px, uint8_t y_px);

/**
 * @brief Draw a bitmap slice with rotation-safe blitting
 *
 * General blitter that handles any Y offset with automatic clipping.
 * - Fast path: page-aligned Y coordinates use direct column writes
 * - Unaligned path: uses read-modify-write with OR-blending across pages
 * - Horizontal clipping prevents buffer overruns
 *
 * @param s Pointer to slice_t structure containing bitmap data
 * @param x_px X coordinate in pixels
 * @param y_px Y coordinate in pixels
 */
void draw_slice_px(const slice_t *s, uint8_t x_px, uint8_t y_px);

// ============================================================================
// PROGMEM Bitmap Helpers
// ============================================================================

/**
 * @brief SLICE macro examples for PROGMEM bitmap data
 *
 * Helper macros to create slice_t structures without casting issues.
 * Use these patterns in your keymap to avoid compiler warnings about
 * PROGMEM pointers.
 *
 * Example usage in your keymap:
 * @code
 * #define SLICE16x8(p) ((slice_t){(p), 16, 1})
 * #define SLICE8x32(p) ((slice_t){(p), 8, 4})
 *
 * const uint8_t PROGMEM my_icon[] = { ... };
 * slice_t icon = SLICE16x8(my_icon);
 * draw_slice_px(&icon, 10, 8);
 * @endcode
 *
 * Common SLICE macro patterns:
 * - SLICE8x8(p)    → ((slice_t){(p), 8, 1})   // 8×8 bitmap (1 page high)
 * - SLICE16x8(p)   → ((slice_t){(p), 16, 1})  // 16×8 bitmap (1 page high)
 * - SLICE24x8(p)   → ((slice_t){(p), 24, 1})  // 24×8 bitmap (1 page high)
 * - SLICE32x8(p)   → ((slice_t){(p), 32, 1})  // 32×8 bitmap (1 page high)
 * - SLICE8x16(p)   → ((slice_t){(p), 8, 2})   // 8×16 bitmap (2 pages high)
 * - SLICE16x16(p)  → ((slice_t){(p), 16, 2})  // 16×16 bitmap (2 pages high)
 * - SLICE8x24(p)   → ((slice_t){(p), 8, 3})   // 8×24 bitmap (3 pages high)
 * - SLICE8x32(p)   → ((slice_t){(p), 8, 4})   // 8×32 bitmap (4 pages high)
 * - SLICE16x32(p)  → ((slice_t){(p), 16, 4})  // 16×32 bitmap (4 pages high)
 * - SLICE24x32(p)  → ((slice_t){(p), 24, 4})  // 24×32 bitmap (4 pages high)
 * - SLICE32x32(p)  → ((slice_t){(p), 32, 4})  // 32×32 bitmap (4 pages high)
 * - SLICE128x32(p) → ((slice_t){(p), 128, 4}) // 128×32 bitmap (4 pages high)
 */

/**
 * @brief Create slice_t for custom dimensions
 *
 * For non-standard bitmap sizes, use this macro with explicit dimensions.
 *
 * @param progmem_array Pointer to PROGMEM bitmap data
 * @param w Width in pixels
 * @param h Height in pages (height_pixels / 8)
 */
#define SLICE_CUSTOM(progmem_array, w, h) ((slice_t){(progmem_array), (w), (h)})

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Get the pixel width of a slice
 * @param s Pointer to slice_t
 * @return Width in pixels
 */
static inline uint8_t slice_width_px(const slice_t *s) {
    return s ? s->width : 0;
}

/**
 * @brief Get the pixel height of a slice
 * @param s Pointer to slice_t
 * @return Height in pixels
 */
static inline uint8_t slice_height_px(const slice_t *s) {
    return s ? (uint8_t)(s->pages * 8) : 0;
}

/**
 * @brief Check if a slice is valid (non-null with non-zero dimensions)
 * @param s Pointer to slice_t
 * @return true if slice is valid, false otherwise
 */
static inline bool slice_is_valid(const slice_t *s) {
    return s && s->data && s->width > 0 && s->pages > 0;
}
