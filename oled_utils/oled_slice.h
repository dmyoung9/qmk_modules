/**
 * @file oled_slice.h
 * @brief PROGMEM bitmap slice utilities for OLED displays
 *
 * This module provides:
 * - slice_t structure for page-packed bitmap data
 * - Comprehensive SLICE macros for common bitmap sizes
 * - Utility functions for slice validation and dimension queries
 * - PROGMEM-friendly bitmap abstraction without compiler warnings
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// Core Slice Structure
// ============================================================================

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

/**
 * @brief Get the number of bytes required for slice data
 * @param s Pointer to slice_t
 * @return Total bytes in the bitmap data
 */
static inline uint16_t slice_data_size(const slice_t *s) {
    return s ? (uint16_t)s->width * s->pages : 0;
}

// ============================================================================
// Basic SLICE Macros
// ============================================================================

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
 * @brief Helper macro for width and height specification
 * @param progmem_array Pointer to PROGMEM bitmap data
 * @param w Width in pixels
 * @param h_px Height in pixels
 */
#define SLICE_W_H(progmem_array, w, h_px) SLICE_CUSTOM_PX(progmem_array, w, h_px)

// ============================================================================
// Standard Page-Aligned SLICE Macros
// ============================================================================

// 1 page high (8 pixels)
#define SLICE8x8(p)    ((slice_t){(p), 8, 1, 0})
#define SLICE16x8(p)   ((slice_t){(p), 16, 1, 0})
#define SLICE24x8(p)   ((slice_t){(p), 24, 1, 0})
#define SLICE32x8(p)   ((slice_t){(p), 32, 1, 0})
#define SLICE40x8(p)   ((slice_t){(p), 40, 1, 0})
#define SLICE48x8(p)   ((slice_t){(p), 48, 1, 0})
#define SLICE56x8(p)   ((slice_t){(p), 56, 1, 0})
#define SLICE64x8(p)   ((slice_t){(p), 64, 1, 0})
#define SLICE72x8(p)   ((slice_t){(p), 72, 1, 0})
#define SLICE80x8(p)   ((slice_t){(p), 80, 1, 0})
#define SLICE88x8(p)   ((slice_t){(p), 88, 1, 0})
#define SLICE96x8(p)   ((slice_t){(p), 96, 1, 0})
#define SLICE104x8(p)  ((slice_t){(p), 104, 1, 0})
#define SLICE112x8(p)  ((slice_t){(p), 112, 1, 0})
#define SLICE120x8(p)  ((slice_t){(p), 120, 1, 0})
#define SLICE128x8(p)  ((slice_t){(p), 128, 1, 0})

// 2 pages high (16 pixels)
#define SLICE8x16(p)   ((slice_t){(p), 8, 2, 0})
#define SLICE16x16(p)  ((slice_t){(p), 16, 2, 0})
#define SLICE24x16(p)  ((slice_t){(p), 24, 2, 0})
#define SLICE32x16(p)  ((slice_t){(p), 32, 2, 0})
#define SLICE40x16(p)  ((slice_t){(p), 40, 2, 0})
#define SLICE48x16(p)  ((slice_t){(p), 48, 2, 0})
#define SLICE56x16(p)  ((slice_t){(p), 56, 2, 0})
#define SLICE64x16(p)  ((slice_t){(p), 64, 2, 0})
#define SLICE72x16(p)  ((slice_t){(p), 72, 2, 0})
#define SLICE80x16(p)  ((slice_t){(p), 80, 2, 0})
#define SLICE88x16(p)  ((slice_t){(p), 88, 2, 0})
#define SLICE96x16(p)  ((slice_t){(p), 96, 2, 0})
#define SLICE104x16(p) ((slice_t){(p), 104, 2, 0})
#define SLICE112x16(p) ((slice_t){(p), 112, 2, 0})
#define SLICE120x16(p) ((slice_t){(p), 120, 2, 0})
#define SLICE128x16(p) ((slice_t){(p), 128, 2, 0})

// 3 pages high (24 pixels)
#define SLICE8x24(p)   ((slice_t){(p), 8, 3, 0})
#define SLICE16x24(p)  ((slice_t){(p), 16, 3, 0})
#define SLICE24x24(p)  ((slice_t){(p), 24, 3, 0})
#define SLICE32x24(p)  ((slice_t){(p), 32, 3, 0})
#define SLICE40x24(p)  ((slice_t){(p), 40, 3, 0})
#define SLICE48x24(p)  ((slice_t){(p), 48, 3, 0})
#define SLICE56x24(p)  ((slice_t){(p), 56, 3, 0})
#define SLICE64x24(p)  ((slice_t){(p), 64, 3, 0})
#define SLICE72x24(p)  ((slice_t){(p), 72, 3, 0})
#define SLICE80x24(p)  ((slice_t){(p), 80, 3, 0})
#define SLICE88x24(p)  ((slice_t){(p), 88, 3, 0})
#define SLICE96x24(p)  ((slice_t){(p), 96, 3, 0})
#define SLICE104x24(p) ((slice_t){(p), 104, 3, 0})
#define SLICE112x24(p) ((slice_t){(p), 112, 3, 0})
#define SLICE120x24(p) ((slice_t){(p), 120, 3, 0})
#define SLICE128x24(p) ((slice_t){(p), 128, 3, 0})

// 4 pages high (32 pixels) - full height for 128x32 displays
#define SLICE8x32(p)   ((slice_t){(p), 8, 4, 0})
#define SLICE16x32(p)  ((slice_t){(p), 16, 4, 0})
#define SLICE24x32(p)  ((slice_t){(p), 24, 4, 0})
#define SLICE32x32(p)  ((slice_t){(p), 32, 4, 0})
#define SLICE40x32(p)  ((slice_t){(p), 40, 4, 0})
#define SLICE48x32(p)  ((slice_t){(p), 48, 4, 0})
#define SLICE56x32(p)  ((slice_t){(p), 56, 4, 0})
#define SLICE64x32(p)  ((slice_t){(p), 64, 4, 0})
#define SLICE72x32(p)  ((slice_t){(p), 72, 4, 0})
#define SLICE80x32(p)  ((slice_t){(p), 80, 4, 0})
#define SLICE88x32(p)  ((slice_t){(p), 88, 4, 0})
#define SLICE96x32(p)  ((slice_t){(p), 96, 4, 0})
#define SLICE104x32(p) ((slice_t){(p), 104, 4, 0})
#define SLICE112x32(p) ((slice_t){(p), 112, 4, 0})
#define SLICE120x32(p) ((slice_t){(p), 120, 4, 0})
#define SLICE128x32(p) ((slice_t){(p), 128, 4, 0})

// ============================================================================
// Arbitrary Height SLICE Macros
// ============================================================================

// Common arbitrary heights (10, 12, 14 pixels)
#define SLICE8x10(p)   SLICE_CUSTOM_PX(p, 8, 10)
#define SLICE16x10(p)  SLICE_CUSTOM_PX(p, 16, 10)
#define SLICE24x10(p)  SLICE_CUSTOM_PX(p, 24, 10)
#define SLICE32x10(p)  SLICE_CUSTOM_PX(p, 32, 10)

#define SLICE8x12(p)   SLICE_CUSTOM_PX(p, 8, 12)
#define SLICE16x12(p)  SLICE_CUSTOM_PX(p, 16, 12)
#define SLICE24x12(p)  SLICE_CUSTOM_PX(p, 24, 12)
#define SLICE32x12(p)  SLICE_CUSTOM_PX(p, 32, 12)
#define SLICE40x12(p)  SLICE_CUSTOM_PX(p, 40, 12)
#define SLICE48x12(p)  SLICE_CUSTOM_PX(p, 48, 12)
#define SLICE56x12(p)  SLICE_CUSTOM_PX(p, 56, 12)
#define SLICE64x12(p)  SLICE_CUSTOM_PX(p, 64, 12)
#define SLICE72x12(p)  SLICE_CUSTOM_PX(p, 72, 12)

#define SLICE8x14(p)   SLICE_CUSTOM_PX(p, 8, 14)
#define SLICE16x14(p)  SLICE_CUSTOM_PX(p, 16, 14)
#define SLICE24x14(p)  SLICE_CUSTOM_PX(p, 24, 14)
#define SLICE32x14(p)  SLICE_CUSTOM_PX(p, 32, 14)

// Larger arbitrary heights (18, 20, 22, 26, 28, 30 pixels)
#define SLICE8x18(p)   SLICE_CUSTOM_PX(p, 8, 18)
#define SLICE16x18(p)  SLICE_CUSTOM_PX(p, 16, 18)
#define SLICE24x18(p)  SLICE_CUSTOM_PX(p, 24, 18)
#define SLICE32x18(p)  SLICE_CUSTOM_PX(p, 32, 18)

#define SLICE8x20(p)   SLICE_CUSTOM_PX(p, 8, 20)
#define SLICE16x20(p)  SLICE_CUSTOM_PX(p, 16, 20)
#define SLICE24x20(p)  SLICE_CUSTOM_PX(p, 24, 20)
#define SLICE32x20(p)  SLICE_CUSTOM_PX(p, 32, 20)

#define SLICE8x22(p)   SLICE_CUSTOM_PX(p, 8, 22)
#define SLICE16x22(p)  SLICE_CUSTOM_PX(p, 16, 22)
#define SLICE24x22(p)  SLICE_CUSTOM_PX(p, 24, 22)
#define SLICE32x22(p)  SLICE_CUSTOM_PX(p, 32, 22)

#define SLICE8x26(p)   SLICE_CUSTOM_PX(p, 8, 26)
#define SLICE16x26(p)  SLICE_CUSTOM_PX(p, 16, 26)
#define SLICE24x26(p)  SLICE_CUSTOM_PX(p, 24, 26)
#define SLICE32x26(p)  SLICE_CUSTOM_PX(p, 32, 26)

#define SLICE8x28(p)   SLICE_CUSTOM_PX(p, 8, 28)
#define SLICE16x28(p)  SLICE_CUSTOM_PX(p, 16, 28)
#define SLICE24x28(p)  SLICE_CUSTOM_PX(p, 24, 28)
#define SLICE32x28(p)  SLICE_CUSTOM_PX(p, 32, 28)

#define SLICE8x30(p)   SLICE_CUSTOM_PX(p, 8, 30)
#define SLICE16x30(p)  SLICE_CUSTOM_PX(p, 16, 30)
#define SLICE24x30(p)  SLICE_CUSTOM_PX(p, 24, 30)
#define SLICE32x30(p)  SLICE_CUSTOM_PX(p, 32, 30)

// ============================================================================
// Usage Examples and Documentation
// ============================================================================

/**
 * @brief Example usage for page-aligned bitmaps
 *
 * Page-aligned bitmaps have heights that are multiples of 8 pixels.
 * These are the most efficient and common case.
 *
 * @code
 * // 16x8 bitmap (1 page high)
 * const uint8_t PROGMEM my_icon_16x8[] = {
 *     0x3C, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3C,
 *     0x00, 0x00, 0x18, 0x24, 0x24, 0x18, 0x00, 0x00
 * };
 * slice_t icon = SLICE16x8(my_icon_16x8);
 *
 * // 32x16 bitmap (2 pages high)
 * const uint8_t PROGMEM my_logo_32x16[] = {
 *     // Page 0 (pixels 0-7): 32 bytes
 *     0xFF, 0x81, 0x81, 0x81, ..., 0xFF,
 *     // Page 1 (pixels 8-15): 32 bytes
 *     0xFF, 0x81, 0x81, 0x81, ..., 0xFF
 * };
 * slice_t logo = SLICE32x16(my_logo_32x16);
 * @endcode
 */

/**
 * @brief Example usage for arbitrary height bitmaps
 *
 * Arbitrary height bitmaps can have any pixel height. The bitmap data
 * must still be page-packed, but only the specified pixels are drawn.
 *
 * @code
 * // 16x12 bitmap (12 pixels high, needs 2 pages)
 * const uint8_t PROGMEM my_indicator_16x12[] = {
 *     // Page 0 (pixels 0-7): 16 bytes for 8 pixels
 *     0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFF,
 *     0x00, 0x7E, 0x42, 0x42, 0x42, 0x42, 0x7E, 0x00,
 *     // Page 1 (pixels 8-11): 16 bytes, but only first 4 pixels drawn
 *     0x0F, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x0F,
 *     0x00, 0x06, 0x02, 0x02, 0x02, 0x02, 0x06, 0x00
 * };
 * slice_t indicator = SLICE16x12(my_indicator_16x12);
 * // Only draws 12 pixels high, preserves existing pixels 12-15
 * @endcode
 */

/**
 * @brief Migration guide from old SLICE macros
 *
 * If you're migrating from the old oled_utils.h SLICE macros:
 *
 * Old usage:
 * @code
 * #define SLICE16x8(p) ((slice_t){(p), 16, 1, 0})
 * slice_t my_slice = SLICE16x8(my_data);
 * @endcode
 *
 * New usage (no changes needed):
 * @code
 * #include "oled_slice.h"
 * slice_t my_slice = SLICE16x8(my_data);  // Same syntax!
 * @endcode
 *
 * The new module provides many more predefined macros and better
 * organization, but maintains full backward compatibility.
 */
