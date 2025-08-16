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
 *
 * For arbitrary heights (not multiples of 8), use height_px to specify
 * the exact pixel height. The pages field should be set to the number
 * of pages needed to contain the height (ceil(height_px/8)).
 */
typedef struct {
    const uint8_t *data;      ///< Pointer to page-packed bitmap data in PROGMEM
    uint8_t        width;     ///< Width in pixels (bytes per page)
    uint8_t        pages;     ///< Height in pages (ceil(height_px/8))
    uint8_t        height_px; ///< Actual height in pixels (0 = use pages*8)
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
 * // Page-aligned heights (multiples of 8 pixels)
 * #define SLICE16x8(p) ((slice_t){(p), 16, 1, 0})
 * #define SLICE8x32(p) ((slice_t){(p), 8, 4, 0})
 *
 * // Arbitrary heights (any pixel height)
 * #define SLICE16x14(p) ((slice_t){(p), 16, 2, 14})  // 14px high, needs 2 pages
 * #define SLICE8x26(p) ((slice_t){(p), 8, 4, 26})    // 26px high, needs 4 pages
 *
 * const uint8_t PROGMEM my_icon[] = { ... };
 * slice_t icon = SLICE16x14(my_icon);
 * draw_slice_px(&icon, 10, 8);
 * @endcode
 *
 * Common SLICE macro patterns (page-aligned):
 * - SLICE8x8(p)    → ((slice_t){(p), 8, 1, 0})   // 8×8 bitmap (1 page high)
 * - SLICE16x8(p)   → ((slice_t){(p), 16, 1, 0})  // 16×8 bitmap (1 page high)
 * - SLICE24x8(p)   → ((slice_t){(p), 24, 1, 0})  // 24×8 bitmap (1 page high)
 * - SLICE32x8(p)   → ((slice_t){(p), 32, 1, 0})  // 32×8 bitmap (1 page high)
 * - SLICE8x16(p)   → ((slice_t){(p), 8, 2, 0})   // 8×16 bitmap (2 pages high)
 * - SLICE16x16(p)  → ((slice_t){(p), 16, 2, 0})  // 16×16 bitmap (2 pages high)
 * - SLICE8x24(p)   → ((slice_t){(p), 8, 3, 0})   // 8×24 bitmap (3 pages high)
 * - SLICE8x32(p)   → ((slice_t){(p), 8, 4, 0})   // 8×32 bitmap (4 pages high)
 * - SLICE16x32(p)  → ((slice_t){(p), 16, 4, 0})  // 16×32 bitmap (4 pages high)
 * - SLICE24x32(p)  → ((slice_t){(p), 24, 4, 0})  // 24×32 bitmap (4 pages high)
 * - SLICE32x32(p)  → ((slice_t){(p), 32, 4, 0})  // 32×32 bitmap (4 pages high)
 * - SLICE128x32(p) → ((slice_t){(p), 128, 4, 0}) // 128×32 bitmap (4 pages high)
 *
 * For arbitrary heights, use SLICE_CUSTOM_PX() or the provided helper macros.
 */

/**
 * @brief Create slice_t for custom dimensions (page-aligned heights)
 *
 * For non-standard bitmap sizes with heights that are multiples of 8 pixels.
 *
 * @param progmem_array Pointer to PROGMEM bitmap data
 * @param w Width in pixels
 * @param h Height in pages (height_pixels / 8)
 */
#define SLICE_CUSTOM(progmem_array, w, h) ((slice_t){(progmem_array), (w), (h), 0})

/**
 * @brief Create slice_t for arbitrary pixel heights
 *
 * For bitmap sizes with heights that are NOT multiples of 8 pixels.
 * The bitmap data should still be page-packed, but only the specified
 * number of pixels will be drawn from the last page.
 *
 * @param progmem_array Pointer to PROGMEM bitmap data
 * @param w Width in pixels
 * @param h_px Height in pixels (can be any value)
 */
#define SLICE_CUSTOM_PX(progmem_array, w, h_px) \
    ((slice_t){(progmem_array), (w), (uint8_t)(((h_px) + 7) / 8), (h_px)})

/**
 * @brief Helper macros for common arbitrary heights
 *
 * These macros create slice_t structures for common non-page-aligned heights.
 * The bitmap data should be page-packed with the appropriate number of pages.
 *
 * Examples:
 * - 14px high bitmap needs 2 pages (ceil(14/8) = 2)
 * - 26px high bitmap needs 4 pages (ceil(26/8) = 4)
 */
#define SLICE_W_H(progmem_array, w, h_px) SLICE_CUSTOM_PX(progmem_array, w, h_px)

// Common arbitrary height patterns
#define SLICE8x12(p)   SLICE_CUSTOM_PX(p, 8, 12)    // 8×12 bitmap
#define SLICE16x12(p)  SLICE_CUSTOM_PX(p, 16, 12)   // 16×12 bitmap
#define SLICE24x12(p)  SLICE_CUSTOM_PX(p, 24, 12)   // 24×12 bitmap
#define SLICE32x12(p)  SLICE_CUSTOM_PX(p, 32, 12)   // 32×12 bitmap

#define SLICE8x14(p)   SLICE_CUSTOM_PX(p, 8, 14)    // 8×14 bitmap
#define SLICE16x14(p)  SLICE_CUSTOM_PX(p, 16, 14)   // 16×14 bitmap
#define SLICE24x14(p)  SLICE_CUSTOM_PX(p, 24, 14)   // 24×14 bitmap
#define SLICE32x14(p)  SLICE_CUSTOM_PX(p, 32, 14)   // 32×14 bitmap

#define SLICE8x26(p)   SLICE_CUSTOM_PX(p, 8, 26)    // 8×26 bitmap
#define SLICE16x26(p)  SLICE_CUSTOM_PX(p, 16, 26)   // 16×26 bitmap
#define SLICE24x26(p)  SLICE_CUSTOM_PX(p, 24, 26)   // 24×26 bitmap
#define SLICE32x26(p)  SLICE_CUSTOM_PX(p, 32, 26)   // 32×26 bitmap

/**
 * @brief Example usage for arbitrary height bitmaps
 *
 * When creating bitmaps with arbitrary heights, remember:
 * 1. The bitmap data must still be page-packed (stored in 8-pixel vertical chunks)
 * 2. You need enough pages to contain your height: ceil(height_px / 8)
 * 3. Only the specified number of pixels will be drawn from the last page
 *
 * Example for a 12-pixel high layer indicator at y=0:
 * @code
 * // 12px high bitmap needs 2 pages: ceil(12/8) = 2
 * const uint8_t PROGMEM layer_indicator_12px[] = {
 *     // Page 0 (pixels 0-7): 8 bytes for 8 pixels
 *     0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFF,
 *     // Page 1 (pixels 8-11): 8 bytes, but only first 4 pixels (12-8=4) will be drawn
 *     // Pixels 12-15 in this page will be preserved from existing display content
 *     0x0F, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x0F,
 * };
 *
 * slice_t indicator = SLICE8x12(layer_indicator_12px);
 * draw_slice_px(&indicator, 0, 0);  // Draws exactly 12 pixels high, preserves pixels 12-15
 * @endcode
 *
 * Example for a 14-pixel high bitmap:
 * @code
 * // 14px high bitmap needs 2 pages: ceil(14/8) = 2
 * const uint8_t PROGMEM my_14px_icon[] = {
 *     // Page 0 (pixels 0-7): 8 bytes for 8 pixels
 *     0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFF,
 *     // Page 1 (pixels 8-13): 8 bytes, but only first 6 pixels (14-8=6) will be drawn
 *     0x3F, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x3F,
 * };
 *
 * slice_t my_icon = SLICE8x14(my_14px_icon);
 * draw_slice_px(&my_icon, 10, 5);  // Draws exactly 14 pixels high
 * @endcode
 */

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
    if (!s) return 0;
    // Use explicit height_px if set, otherwise fall back to pages*8
    return s->height_px ? s->height_px : (uint8_t)(s->pages * 8);
}

/**
 * @brief Check if a slice is valid (non-null with non-zero dimensions)
 * @param s Pointer to slice_t
 * @return true if slice is valid, false otherwise
 */
static inline bool slice_is_valid(const slice_t *s) {
    return s && s->data && s->width > 0 && s->pages > 0;
}
