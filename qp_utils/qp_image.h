/**
 * @file qp_image.h
 * @brief QGF image and animation system for Quantum Painter
 *
 * This module provides:
 * - QGF image loading and management with PROGMEM support
 * - Animation sequence handling with frame timing
 * - Color format abstraction for different display types
 * - Fluent API for image creation and manipulation
 * - Integration with qp_utils drawing system
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include QMK_KEYBOARD_H
#include <qp.h>
#include "qp_utils.h"

// ============================================================================
// Core Image Types
// ============================================================================

/**
 * @brief QP image handle structure
 * 
 * Represents a loaded QGF image that can be drawn to any compatible display.
 * Images are loaded once and can be reused across multiple displays.
 */
typedef struct {
    painter_image_handle_t handle;  ///< QP native image handle
    uint16_t width, height;         ///< Image dimensions
    bool valid;                     ///< True if image loaded successfully
    bool owns_handle;               ///< True if we should free the handle
} qp_image_t;

/**
 * @brief Animation sequence structure
 * 
 * Represents a sequence of images for animation, similar to oled_utils slice_seq_t
 * but adapted for QGF images and QP's animation capabilities.
 */
typedef struct {
    const qp_image_t* frames;       ///< Array of image frames
    uint8_t count;                  ///< Number of frames
    uint16_t frame_duration_ms;     ///< Duration per frame in milliseconds
    bool loop;                      ///< True if animation should loop
} qp_image_sequence_t;

// ============================================================================
// Image Loading Functions
// ============================================================================

/**
 * @brief Load QGF image from PROGMEM data
 * @param qgf_data Pointer to QGF data in PROGMEM
 * @param qgf_size Size of QGF data in bytes
 * @return qp_image_t structure (check .valid field)
 */
qp_image_t qp_load_image_mem(const void* qgf_data, uint32_t qgf_size);

/**
 * @brief Load QGF image from external storage
 * @param qgf_data Pointer to QGF data
 * @param qgf_size Size of QGF data in bytes
 * @return qp_image_t structure (check .valid field)
 */
qp_image_t qp_load_image_flash(const void* qgf_data, uint32_t qgf_size);

/**
 * @brief Create image from existing QP handle
 * @param handle Existing QP image handle
 * @param take_ownership True if qp_image_t should manage the handle lifetime
 * @return qp_image_t structure
 */
qp_image_t qp_image_from_handle(painter_image_handle_t handle, bool take_ownership);

/**
 * @brief Free image resources
 * @param image Image to free
 */
void qp_free_image(qp_image_t* image);

// ============================================================================
// Image Drawing Functions
// ============================================================================

/**
 * @brief Draw image at specified position
 * @param device QP device handle
 * @param image Image to draw
 * @param x X coordinate
 * @param y Y coordinate
 * @return True if successful
 */
bool qp_draw_image(painter_device_t device, const qp_image_t* image, uint16_t x, uint16_t y);

/**
 * @brief Draw image with color tinting (for monochrome images)
 * @param device QP device handle
 * @param image Image to draw
 * @param x X coordinate
 * @param y Y coordinate
 * @param tint_color Color to tint the image
 * @return True if successful
 */
bool qp_draw_image_tinted(painter_device_t device, const qp_image_t* image, uint16_t x, uint16_t y, qp_color_t tint_color);

/**
 * @brief Draw image with clipping rectangle
 * @param device QP device handle
 * @param image Image to draw
 * @param x X coordinate
 * @param y Y coordinate
 * @param clip_rect Clipping rectangle (NULL for no clipping)
 * @return True if successful
 */
bool qp_draw_image_clipped(painter_device_t device, const qp_image_t* image, uint16_t x, uint16_t y, const qp_rect_t* clip_rect);

/**
 * @brief Draw portion of image (sub-image)
 * @param device QP device handle
 * @param image Source image
 * @param src_x Source X coordinate within image
 * @param src_y Source Y coordinate within image
 * @param src_w Source width
 * @param src_h Source height
 * @param dst_x Destination X coordinate
 * @param dst_y Destination Y coordinate
 * @return True if successful
 */
bool qp_draw_image_sub(painter_device_t device, const qp_image_t* image, 
                       uint16_t src_x, uint16_t src_y, uint16_t src_w, uint16_t src_h,
                       uint16_t dst_x, uint16_t dst_y);

// ============================================================================
// Animation Sequence Functions
// ============================================================================

/**
 * @brief Create animation sequence from image array
 * @param frames Array of images
 * @param count Number of frames
 * @param frame_duration_ms Duration per frame
 * @param loop True if animation should loop
 * @return qp_image_sequence_t structure
 */
qp_image_sequence_t qp_create_sequence(const qp_image_t* frames, uint8_t count, uint16_t frame_duration_ms, bool loop);

/**
 * @brief Get frame from sequence at specific index
 * @param seq Animation sequence
 * @param index Frame index
 * @return Pointer to image frame or NULL if invalid index
 */
const qp_image_t* qp_sequence_get_frame(const qp_image_sequence_t* seq, uint8_t index);

/**
 * @brief Get sequence frame count
 * @param seq Animation sequence
 * @return Number of frames
 */
static inline uint8_t qp_sequence_frame_count(const qp_image_sequence_t* seq) {
    return seq ? seq->count : 0;
}

/**
 * @brief Get sequence frame duration
 * @param seq Animation sequence
 * @return Frame duration in milliseconds
 */
static inline uint16_t qp_sequence_frame_duration(const qp_image_sequence_t* seq) {
    return seq ? seq->frame_duration_ms : 0;
}

// ============================================================================
// Convenience Macros
// ============================================================================

/**
 * @brief Create image from PROGMEM QGF data
 * @param qgf_var Variable name containing QGF data
 */
#define QP_IMAGE_PROGMEM(qgf_var) qp_load_image_mem(qgf_var, sizeof(qgf_var))

/**
 * @brief Create image from external QGF data
 * @param qgf_var Variable name containing QGF data
 */
#define QP_IMAGE_FLASH(qgf_var) qp_load_image_flash(qgf_var, sizeof(qgf_var))

/**
 * @brief Define animation sequence with frame duration
 * @param name Sequence variable name
 * @param duration_ms Frame duration in milliseconds
 * @param loop_flag True if should loop
 * @param ... Image variables
 */
#define QP_DEFINE_SEQUENCE(name, duration_ms, loop_flag, ...) \
    static const qp_image_t name##_frames[] = {__VA_ARGS__}; \
    static const qp_image_sequence_t name = { \
        .frames = name##_frames, \
        .count = sizeof(name##_frames) / sizeof(name##_frames[0]), \
        .frame_duration_ms = (duration_ms), \
        .loop = (loop_flag) \
    }

/**
 * @brief Define looping animation sequence
 * @param name Sequence variable name
 * @param duration_ms Frame duration in milliseconds
 * @param ... Image variables
 */
#define QP_DEFINE_LOOP_SEQUENCE(name, duration_ms, ...) \
    QP_DEFINE_SEQUENCE(name, duration_ms, true, __VA_ARGS__)

/**
 * @brief Define one-shot animation sequence
 * @param name Sequence variable name
 * @param duration_ms Frame duration in milliseconds
 * @param ... Image variables
 */
#define QP_DEFINE_ONESHOT_SEQUENCE(name, duration_ms, ...) \
    QP_DEFINE_SEQUENCE(name, duration_ms, false, __VA_ARGS__)

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Check if image is valid
 * @param image Image to check
 * @return True if image is valid and can be drawn
 */
static inline bool qp_image_is_valid(const qp_image_t* image) {
    return image && image->valid && image->handle != NULL;
}

/**
 * @brief Get image dimensions
 * @param image Image to query
 * @param width Pointer to store width (can be NULL)
 * @param height Pointer to store height (can be NULL)
 * @return True if image is valid
 */
static inline bool qp_image_get_size(const qp_image_t* image, uint16_t* width, uint16_t* height) {
    if (!qp_image_is_valid(image)) return false;
    if (width) *width = image->width;
    if (height) *height = image->height;
    return true;
}

// ============================================================================
// Integration Notes
// ============================================================================

/**
 * @brief Migration from oled_utils slices
 *
 * The QP image system replaces oled_utils slices with QGF images:
 *
 * Old oled_utils approach:
 * @code
 * const uint8_t PROGMEM my_icon[] = { ... };
 * slice_t icon = SLICE16x8(my_icon);
 * draw_slice_px(&icon, 10, 8);
 * @endcode
 *
 * New qp_utils approach:
 * @code
 * const uint8_t PROGMEM my_icon_qgf[] = { ... }; // QGF format
 * qp_image_t icon = QP_IMAGE_PROGMEM(my_icon_qgf);
 * qp_draw_image(device, &icon, 10, 8);
 * @endcode
 *
 * Use QMK's CLI tools to convert images to QGF format:
 * qmk painter-convert-graphics -f mono2 -i my_icon.png -o ./generated/
 */
