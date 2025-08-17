/**
 * @file qp_utils.c
 * @brief Implementation of core Quantum Painter utilities
 */

#include QMK_KEYBOARD_H
#include "qp_utils.h"

// ============================================================================
// Internal State and Caching
// ============================================================================

/**
 * @brief Maximum number of displays we can cache info for
 */
#ifndef QP_UTILS_MAX_DISPLAYS
#define QP_UTILS_MAX_DISPLAYS 4
#endif

/**
 * @brief Display info cache entry
 */
typedef struct {
    painter_device_t device;
    qp_display_info_t info;
    bool valid;
} display_cache_entry_t;

/**
 * @brief Display info cache
 */
static display_cache_entry_t display_cache[QP_UTILS_MAX_DISPLAYS];

// ============================================================================
// Internal Helper Functions
// ============================================================================

/**
 * @brief Convert qp_color_t to native QP color value
 * @param device QP device handle
 * @param color Color to convert
 * @return Native QP color value
 */
static uint32_t color_to_native(painter_device_t device, qp_color_t color) {
    const qp_display_info_t* info = qp_get_display_info(device);
    if (!info) return 0;

    switch (color.type) {
        case QP_COLOR_MONO:
            return color.mono ? 1 : 0;
            
        case QP_COLOR_PALETTE:
            return color.palette_index;
            
        case QP_COLOR_HSV:
            // Convert HSV to RGB then to native format
            // QMK's HSV uses 0-255 range, QP expects 0-360 for hue
            return qp_hsv888_to_native(device, 
                (color.hsv.h * 360) / 255, 
                color.hsv.s, 
                color.hsv.v);
                
        case QP_COLOR_RGB:
            return qp_rgb888_to_native(device, color.rgb.r, color.rgb.g, color.rgb.b);
            
        default:
            return 0;
    }
}

/**
 * @brief Find or create cache entry for device
 * @param device QP device handle
 * @return Pointer to cache entry or NULL if cache full
 */
static display_cache_entry_t* get_cache_entry(painter_device_t device) {
    // First, look for existing entry
    for (int i = 0; i < QP_UTILS_MAX_DISPLAYS; i++) {
        if (display_cache[i].valid && display_cache[i].device == device) {
            return &display_cache[i];
        }
    }
    
    // Find empty slot
    for (int i = 0; i < QP_UTILS_MAX_DISPLAYS; i++) {
        if (!display_cache[i].valid) {
            display_cache[i].device = device;
            display_cache[i].valid = true;
            return &display_cache[i];
        }
    }
    
    return NULL; // Cache full
}

// ============================================================================
// Public API Implementation
// ============================================================================

const qp_display_info_t* qp_get_display_info(painter_device_t device) {
    display_cache_entry_t* entry = get_cache_entry(device);
    if (!entry) return NULL;
    
    return &entry->info;
}

bool qp_utils_init(painter_device_t device) {
    display_cache_entry_t* entry = get_cache_entry(device);
    if (!entry) return false;
    
    // Get display geometry
    uint16_t width, height;
    if (!qp_get_geometry(device, &width, &height, NULL, NULL, NULL)) {
        return false;
    }
    
    // Fill in display info
    entry->info.width = width;
    entry->info.height = height;
    
    // Determine color capabilities based on device type
    // This is a simplified heuristic - in practice you might need
    // device-specific detection
    entry->info.has_color = true;  // Assume color unless proven otherwise
    entry->info.color_depth = 16; // Common RGB565
    entry->info.has_palette = false;
    entry->info.palette_size = 0;
    
    // TODO: Add device-specific capability detection
    // For now, assume RGB565 color displays
    
    return true;
}

bool qp_clear_rect(painter_device_t device, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (!w || !h) return true; // Nothing to clear
    
    // Use QP's native rect fill with black
    return qp_rect(device, x, y, x + w - 1, y + h - 1, 0, 0, 0, true);
}

bool qp_fill_rect(painter_device_t device, uint16_t x, uint16_t y, uint16_t w, uint16_t h, qp_color_t color) {
    if (!w || !h) return true; // Nothing to fill
    
    uint32_t native_color = color_to_native(device, color);
    
    // For monochrome, use the mono color directly
    if (color.type == QP_COLOR_MONO) {
        return qp_rect(device, x, y, x + w - 1, y + h - 1, 0, 0, 0, color.mono);
    }
    
    // For color displays, we need to extract RGB components
    // This is a simplified approach - QP's rect function signature varies
    switch (color.type) {
        case QP_COLOR_RGB:
            return qp_rect(device, x, y, x + w - 1, y + h - 1, 
                          color.rgb.r, color.rgb.g, color.rgb.b, true);
                          
        case QP_COLOR_HSV: {
            // Convert HSV to RGB for QP
            uint8_t r, g, b;
            // Use QMK's HSV to RGB conversion
            hsv_to_rgb_nocie((HSV){color.hsv.h, color.hsv.s, color.hsv.v}, &r, &g, &b);
            return qp_rect(device, x, y, x + w - 1, y + h - 1, r, g, b, true);
        }
        
        default:
            // Fallback to black
            return qp_rect(device, x, y, x + w - 1, y + h - 1, 0, 0, 0, true);
    }
}

bool qp_draw_pixel(painter_device_t device, uint16_t x, uint16_t y, qp_color_t color) {
    uint32_t native_color = color_to_native(device, color);
    
    // For monochrome
    if (color.type == QP_COLOR_MONO) {
        return qp_pixdata(device, NULL, 1) && qp_pixel(device, x, y, color.mono ? 1 : 0);
    }
    
    // For color displays
    switch (color.type) {
        case QP_COLOR_RGB:
            return qp_pixdata(device, NULL, 1) && 
                   qp_pixel(device, x, y, qp_rgb888_to_native(device, color.rgb.r, color.rgb.g, color.rgb.b));
                   
        case QP_COLOR_HSV: {
            uint8_t r, g, b;
            hsv_to_rgb_nocie((HSV){color.hsv.h, color.hsv.s, color.hsv.v}, &r, &g, &b);
            return qp_pixdata(device, NULL, 1) && 
                   qp_pixel(device, x, y, qp_rgb888_to_native(device, r, g, b));
        }
        
        default:
            return qp_pixdata(device, NULL, 1) && qp_pixel(device, x, y, 0);
    }
}

bool qp_draw_hline(painter_device_t device, uint16_t x1, uint16_t x2, uint16_t y, qp_color_t color) {
    if (x1 > x2) {
        uint16_t temp = x1;
        x1 = x2;
        x2 = temp;
    }
    
    return qp_fill_rect(device, x1, y, x2 - x1 + 1, 1, color);
}

bool qp_draw_vline(painter_device_t device, uint16_t x, uint16_t y1, uint16_t y2, qp_color_t color) {
    if (y1 > y2) {
        uint16_t temp = y1;
        y1 = y2;
        y2 = temp;
    }
    
    return qp_fill_rect(device, x, y1, 1, y2 - y1 + 1, color);
}

bool qp_draw_rect(painter_device_t device, uint16_t x, uint16_t y, uint16_t w, uint16_t h, qp_color_t color) {
    if (!w || !h) return true;
    
    // Draw four lines to form rectangle outline
    bool success = true;
    success &= qp_draw_hline(device, x, x + w - 1, y, color);              // Top
    success &= qp_draw_hline(device, x, x + w - 1, y + h - 1, color);      // Bottom
    success &= qp_draw_vline(device, x, y, y + h - 1, color);              // Left
    success &= qp_draw_vline(device, x + w - 1, y, y + h - 1, color);      // Right
    
    return success;
}

bool qp_clip_rect(painter_device_t device, qp_rect_t* rect) {
    const qp_display_info_t* info = qp_get_display_info(device);
    if (!info) return false;
    
    // Check if completely outside display
    if (rect->x >= info->width || rect->y >= info->height) {
        rect->w = rect->h = 0;
        return false;
    }
    
    // Clip to display bounds
    if (rect->x + rect->w > info->width) {
        rect->w = info->width - rect->x;
    }
    
    if (rect->y + rect->h > info->height) {
        rect->h = info->height - rect->y;
    }
    
    return rect->w > 0 && rect->h > 0;
}
