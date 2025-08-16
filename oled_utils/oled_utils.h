/**
 * @file oled_utils.h
 * @brief Core OLED drawing utilities for rotation-safe rendering
 *
 * This module provides:
 * - Rotation-safe drawing primitives with automatic clipping
 * - Fast blitting for page-aligned art with correct blending for unaligned Y
 * - Simple, reliable clearing of arbitrary rectangles and common spans
 * - Integration with the oled_slice module for bitmap handling
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include QMK_KEYBOARD_H
#include "oled_slice.h"  // For slice_t and bitmap utilities

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
// Performance and Implementation Notes
// ============================================================================

/**
 * @brief Performance characteristics of drawing functions
 *
 * - **Page-aligned drawing**: Optimized fast path for Y coordinates divisible by 8
 * - **Unaligned drawing**: Automatic read-modify-write with OR-blending
 * - **Clipping**: All drawing functions clip to screen bounds automatically
 * - **Rotation-safe**: Works correctly with any OLED rotation setting
 * - **Memory safety**: Bounds checking prevents buffer overruns
 *
 * For best performance, align your UI elements to 8-pixel boundaries when possible.
 * The drawing functions handle unaligned coordinates correctly but with some overhead.
 */

/**
 * @brief Integration with oled_slice module
 *
 * This module works seamlessly with the oled_slice module for bitmap handling:
 *
 * @code
 * #include "oled_utils.h"    // Provides drawing functions
 * #include "oled_slice.h"    // Provides SLICE macros (auto-included)
 *
 * const uint8_t PROGMEM my_icon[] = { ... };
 * slice_t icon = SLICE16x8(my_icon);  // From oled_slice.h
 * draw_slice_px(&icon, 10, 8);        // From oled_utils.h
 * @endcode
 *
 * See oled_slice.h for comprehensive SLICE macro documentation and examples.
 */
