/**
 * @file qp_utils.h
 * @brief Core Quantum Painter utilities for device-agnostic drawing
 *
 * This module provides:
 * - Device-agnostic drawing primitives that work across all QP display types
 * - Color abstraction layer supporting monochrome, palette, and RGB modes
 * - Coordinate system utilities and clipping helpers
 * - Performance-optimized drawing operations
 * - Clean integration with the qp_image animation system
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include QMK_KEYBOARD_H
#include <qp.h>

// ============================================================================
// Core Type Definitions
// ============================================================================

/**
 * @brief Color abstraction for device-agnostic drawing
 * 
 * Provides a unified color interface that automatically adapts to the
 * target display's native color format (monochrome, palette, RGB).
 */
typedef struct {
    union {
        struct {
            uint8_t h, s, v;    ///< HSV color (0-255 each)
        } hsv;
        struct {
            uint8_t r, g, b;    ///< RGB color (0-255 each)  
        } rgb;
        uint8_t palette_index;  ///< Palette index for palette displays
        bool    mono;           ///< Boolean for monochrome displays
    };
    enum {
        QP_COLOR_HSV,
        QP_COLOR_RGB, 
        QP_COLOR_PALETTE,
        QP_COLOR_MONO
    } type;
} qp_color_t;

/**
 * @brief Rectangle structure for clipping and area operations
 */
typedef struct {
    uint16_t x, y;      ///< Top-left corner
    uint16_t w, h;      ///< Width and height
} qp_rect_t;

/**
 * @brief Display information structure
 * 
 * Cached information about a display's capabilities to optimize
 * drawing operations and color conversions.
 */
typedef struct {
    uint16_t width, height;     ///< Display dimensions
    bool     has_color;         ///< True if display supports color
    uint8_t  color_depth;       ///< Bits per pixel
    bool     has_palette;       ///< True if display uses palette
    uint8_t  palette_size;      ///< Number of palette entries (if applicable)
} qp_display_info_t;

// ============================================================================
// Color Utilities
// ============================================================================

/**
 * @brief Create HSV color
 * @param h Hue (0-255)
 * @param s Saturation (0-255) 
 * @param v Value/brightness (0-255)
 * @return qp_color_t structure
 */
#define QP_HSV(h, s, v) ((qp_color_t){.hsv = {(h), (s), (v)}, .type = QP_COLOR_HSV})

/**
 * @brief Create RGB color
 * @param r Red (0-255)
 * @param g Green (0-255)
 * @param b Blue (0-255)
 * @return qp_color_t structure
 */
#define QP_RGB(r, g, b) ((qp_color_t){.rgb = {(r), (g), (b)}, .type = QP_COLOR_RGB})

/**
 * @brief Create palette color
 * @param idx Palette index
 * @return qp_color_t structure
 */
#define QP_PALETTE(idx) ((qp_color_t){.palette_index = (idx), .type = QP_COLOR_PALETTE})

/**
 * @brief Create monochrome color
 * @param on True for white/on, false for black/off
 * @return qp_color_t structure
 */
#define QP_MONO(on) ((qp_color_t){.mono = (on), .type = QP_COLOR_MONO})

// Common color constants
#define QP_COLOR_BLACK   QP_MONO(false)
#define QP_COLOR_WHITE   QP_MONO(true)
#define QP_COLOR_RED     QP_HSV(0, 255, 255)
#define QP_COLOR_GREEN   QP_HSV(85, 255, 255)
#define QP_COLOR_BLUE    QP_HSV(170, 255, 255)
#define QP_COLOR_YELLOW  QP_HSV(43, 255, 255)
#define QP_COLOR_CYAN    QP_HSV(128, 255, 255)
#define QP_COLOR_MAGENTA QP_HSV(213, 255, 255)

// ============================================================================
// Device Information and Setup
// ============================================================================

/**
 * @brief Get cached display information
 * @param device QP device handle
 * @return Pointer to display info structure (cached internally)
 */
const qp_display_info_t* qp_get_display_info(painter_device_t device);

/**
 * @brief Initialize QP utils for a device
 * @param device QP device handle
 * @return True if initialization successful
 */
bool qp_utils_init(painter_device_t device);

// ============================================================================
// Core Drawing Functions
// ============================================================================

/**
 * @brief Clear rectangular area
 * @param device QP device handle
 * @param x X coordinate
 * @param y Y coordinate  
 * @param w Width
 * @param h Height
 * @return True if successful
 */
bool qp_clear_rect(painter_device_t device, uint16_t x, uint16_t y, uint16_t w, uint16_t h);

/**
 * @brief Fill rectangular area with color
 * @param device QP device handle
 * @param x X coordinate
 * @param y Y coordinate
 * @param w Width
 * @param h Height
 * @param color Fill color
 * @return True if successful
 */
bool qp_fill_rect(painter_device_t device, uint16_t x, uint16_t y, uint16_t w, uint16_t h, qp_color_t color);

/**
 * @brief Draw single pixel
 * @param device QP device handle
 * @param x X coordinate
 * @param y Y coordinate
 * @param color Pixel color
 * @return True if successful
 */
bool qp_draw_pixel(painter_device_t device, uint16_t x, uint16_t y, qp_color_t color);

/**
 * @brief Draw horizontal line
 * @param device QP device handle
 * @param x1 Start X coordinate
 * @param x2 End X coordinate
 * @param y Y coordinate
 * @param color Line color
 * @return True if successful
 */
bool qp_draw_hline(painter_device_t device, uint16_t x1, uint16_t x2, uint16_t y, qp_color_t color);

/**
 * @brief Draw vertical line
 * @param device QP device handle
 * @param x X coordinate
 * @param y1 Start Y coordinate
 * @param y2 End Y coordinate
 * @param color Line color
 * @return True if successful
 */
bool qp_draw_vline(painter_device_t device, uint16_t x, uint16_t y1, uint16_t y2, qp_color_t color);

/**
 * @brief Draw rectangle outline
 * @param device QP device handle
 * @param x X coordinate
 * @param y Y coordinate
 * @param w Width
 * @param h Height
 * @param color Line color
 * @return True if successful
 */
bool qp_draw_rect(painter_device_t device, uint16_t x, uint16_t y, uint16_t w, uint16_t h, qp_color_t color);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Create rectangle structure
 * @param x X coordinate
 * @param y Y coordinate
 * @param w Width
 * @param h Height
 * @return qp_rect_t structure
 */
static inline qp_rect_t qp_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    return (qp_rect_t){x, y, w, h};
}

/**
 * @brief Check if point is within rectangle
 * @param rect Rectangle to test
 * @param x X coordinate
 * @param y Y coordinate
 * @return True if point is inside rectangle
 */
static inline bool qp_rect_contains(const qp_rect_t* rect, uint16_t x, uint16_t y) {
    return (x >= rect->x && x < rect->x + rect->w && 
            y >= rect->y && y < rect->y + rect->h);
}

/**
 * @brief Clip rectangle to display bounds
 * @param device QP device handle
 * @param rect Rectangle to clip (modified in place)
 * @return True if rectangle is still visible after clipping
 */
bool qp_clip_rect(painter_device_t device, qp_rect_t* rect);

// ============================================================================
// Integration Notes
// ============================================================================

/**
 * @brief Integration with qp_image module
 *
 * This module works seamlessly with the qp_image module for bitmap handling:
 *
 * @code
 * #include "qp_utils.h"    // Provides drawing functions
 * #include "qp_image.h"    // Provides image macros and loading
 *
 * qp_image_t icon = qp_load_image_mem(my_icon_qgf, sizeof(my_icon_qgf));
 * qp_draw_image(device, &icon, 10, 8);        // From qp_image.h
 * qp_clear_rect(device, 0, 0, 32, 16);        // From qp_utils.h
 * @endcode
 *
 * See qp_image.h for comprehensive image loading and animation documentation.
 */
