/**
 * @file qp_image.c
 * @brief Implementation of QGF image and animation system
 */

#include QMK_KEYBOARD_H
#include "qp_image.h"

// ============================================================================
// Internal Helper Functions
// ============================================================================

/**
 * @brief Initialize qp_image_t structure
 * @param image Image structure to initialize
 * @param handle QP image handle (can be NULL)
 * @param owns_handle True if image should manage handle lifetime
 */
static void init_qp_image(qp_image_t* image, painter_image_handle_t handle, bool owns_handle) {
    image->handle = handle;
    image->owns_handle = owns_handle;
    image->valid = (handle != NULL);
    image->width = 0;
    image->height = 0;
    
    if (handle) {
        // Get image dimensions from QP
        uint16_t w, h;
        if (qp_get_image_size(handle, &w, &h)) {
            image->width = w;
            image->height = h;
        } else {
            image->valid = false;
        }
    }
}

// ============================================================================
// Image Loading Functions
// ============================================================================

qp_image_t qp_load_image_mem(const void* qgf_data, uint32_t qgf_size) {
    qp_image_t image = {0};
    
    if (!qgf_data || !qgf_size) {
        return image;
    }
    
    // Load image using QP's memory loading function
    painter_image_handle_t handle = qp_load_image_mem(qgf_data);
    init_qp_image(&image, handle, true);
    
    return image;
}

qp_image_t qp_load_image_flash(const void* qgf_data, uint32_t qgf_size) {
    qp_image_t image = {0};
    
    if (!qgf_data || !qgf_size) {
        return image;
    }
    
    // For flash loading, we might need to copy to RAM first
    // This is a simplified implementation - actual flash loading
    // would depend on the specific storage mechanism
    painter_image_handle_t handle = qp_load_image_mem(qgf_data);
    init_qp_image(&image, handle, true);
    
    return image;
}

qp_image_t qp_image_from_handle(painter_image_handle_t handle, bool take_ownership) {
    qp_image_t image = {0};
    init_qp_image(&image, handle, take_ownership);
    return image;
}

void qp_free_image(qp_image_t* image) {
    if (!image) return;
    
    if (image->valid && image->owns_handle && image->handle) {
        qp_close_image(image->handle);
    }
    
    image->handle = NULL;
    image->valid = false;
    image->owns_handle = false;
    image->width = 0;
    image->height = 0;
}

// ============================================================================
// Image Drawing Functions
// ============================================================================

bool qp_draw_image(painter_device_t device, const qp_image_t* image, uint16_t x, uint16_t y) {
    if (!qp_image_is_valid(image)) {
        return false;
    }
    
    return qp_drawimage(device, x, y, image->handle);
}

bool qp_draw_image_tinted(painter_device_t device, const qp_image_t* image, uint16_t x, uint16_t y, qp_color_t tint_color) {
    if (!qp_image_is_valid(image)) {
        return false;
    }
    
    // For tinted drawing, we might need to use QP's palette manipulation
    // or draw with a specific foreground color for monochrome images
    // This is a simplified implementation
    
    // TODO: Implement proper color tinting based on image format
    // For now, fall back to regular drawing
    return qp_draw_image(device, image, x, y);
}

bool qp_draw_image_clipped(painter_device_t device, const qp_image_t* image, uint16_t x, uint16_t y, const qp_rect_t* clip_rect) {
    if (!qp_image_is_valid(image)) {
        return false;
    }
    
    if (!clip_rect) {
        return qp_draw_image(device, image, x, y);
    }
    
    // Calculate intersection of image bounds with clip rectangle
    qp_rect_t image_rect = {x, y, image->width, image->height};
    
    // Check if image intersects with clip rectangle
    if (x >= clip_rect->x + clip_rect->w || y >= clip_rect->y + clip_rect->h ||
        x + image->width <= clip_rect->x || y + image->height <= clip_rect->y) {
        return true; // Nothing to draw, but not an error
    }
    
    // For now, use QP's built-in clipping by setting viewport
    // A more sophisticated implementation would calculate the exact
    // sub-region to draw
    return qp_draw_image(device, image, x, y);
}

bool qp_draw_image_sub(painter_device_t device, const qp_image_t* image, 
                       uint16_t src_x, uint16_t src_y, uint16_t src_w, uint16_t src_h,
                       uint16_t dst_x, uint16_t dst_y) {
    if (!qp_image_is_valid(image)) {
        return false;
    }
    
    // Validate source rectangle
    if (src_x >= image->width || src_y >= image->height ||
        src_x + src_w > image->width || src_y + src_h > image->height) {
        return false;
    }
    
    // QP doesn't have built-in sub-image drawing, so this would require
    // more complex implementation, possibly involving:
    // 1. Creating a temporary image with the sub-region
    // 2. Using viewport/clipping to achieve the effect
    // 3. Manual pixel-by-pixel copying
    
    // For now, fall back to drawing the full image
    // TODO: Implement proper sub-image drawing
    return qp_draw_image(device, image, dst_x, dst_y);
}

// ============================================================================
// Animation Sequence Functions
// ============================================================================

qp_image_sequence_t qp_create_sequence(const qp_image_t* frames, uint8_t count, uint16_t frame_duration_ms, bool loop) {
    qp_image_sequence_t seq = {0};
    
    if (frames && count > 0) {
        seq.frames = frames;
        seq.count = count;
        seq.frame_duration_ms = frame_duration_ms;
        seq.loop = loop;
    }
    
    return seq;
}

const qp_image_t* qp_sequence_get_frame(const qp_image_sequence_t* seq, uint8_t index) {
    if (!seq || !seq->frames || index >= seq->count) {
        return NULL;
    }
    
    return &seq->frames[index];
}

// ============================================================================
// Utility Functions Implementation
// ============================================================================

// Note: Most utility functions are implemented as static inline in the header
// for performance reasons. Any complex utilities would be implemented here.
