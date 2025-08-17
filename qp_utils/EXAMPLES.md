# QP Utils Examples

This document provides comprehensive examples for using the QP Utils module with Quantum Painter displays.

## Table of Contents

1. [Basic Setup and Drawing](#basic-setup-and-drawing)
2. [Color Management](#color-management)
3. [QGF Image Handling](#qgf-image-handling)
4. [Animation Examples](#animation-examples)
5. [Controller Examples](#controller-examples)
6. [Declarative Widget Examples](#declarative-widget-examples)
7. [Multi-Display Examples](#multi-display-examples)
8. [Migration Examples](#migration-examples)

## Basic Setup and Drawing

### Display Initialization

```c
#include "qp_utils.h"

// Example for ILI9341 SPI display
painter_device_t display;

void keyboard_post_init_user(void) {
    // Initialize display
    display = qp_ili9341_make_spi_device(240, 320, 
                                         DISPLAY_CS_PIN, 
                                         DISPLAY_DC_PIN, 
                                         DISPLAY_RST_PIN, 
                                         16, 0);
    
    if (qp_init(display, QP_ROTATION_0)) {
        qp_utils_init(display);
        qp_power(display, true);
        
        // Clear display to black
        qp_clear_rect(display, 0, 0, 240, 320);
    }
}
```

### Basic Drawing Operations

```c
void draw_basic_shapes(void) {
    // Clear a rectangular area
    qp_clear_rect(display, 10, 10, 100, 50);
    
    // Fill rectangles with different colors
    qp_fill_rect(display, 20, 20, 80, 30, QP_COLOR_BLUE);
    qp_fill_rect(display, 120, 20, 80, 30, QP_HSV(120, 255, 255)); // Green
    
    // Draw rectangle outlines
    qp_draw_rect(display, 15, 15, 90, 40, QP_COLOR_WHITE);
    qp_draw_rect(display, 115, 15, 90, 40, QP_RGB(255, 255, 0)); // Yellow
    
    // Draw lines
    qp_draw_hline(display, 10, 210, 70, QP_COLOR_RED);
    qp_draw_vline(display, 50, 80, 150, QP_COLOR_CYAN);
    
    // Draw individual pixels
    for (int i = 0; i < 10; i++) {
        qp_draw_pixel(display, 100 + i, 100, QP_HSV(i * 25, 255, 255));
    }
}
```

## Color Management

### Working with Different Color Formats

```c
void demonstrate_color_formats(void) {
    // HSV colors (hue, saturation, value)
    qp_color_t red = QP_HSV(0, 255, 255);
    qp_color_t green = QP_HSV(85, 255, 255);
    qp_color_t blue = QP_HSV(170, 255, 255);
    
    // RGB colors (red, green, blue)
    qp_color_t purple = QP_RGB(128, 0, 128);
    qp_color_t orange = QP_RGB(255, 165, 0);
    
    // Monochrome colors (for monochrome displays)
    qp_color_t white = QP_MONO(true);
    qp_color_t black = QP_MONO(false);
    
    // Palette colors (for palette-based displays)
    qp_color_t palette_color = QP_PALETTE(5);
    
    // Use colors in drawing
    qp_fill_rect(display, 0, 0, 50, 50, red);
    qp_fill_rect(display, 50, 0, 50, 50, green);
    qp_fill_rect(display, 100, 0, 50, 50, blue);
    qp_fill_rect(display, 150, 0, 50, 50, purple);
}
```

### Adaptive Color Functions

```c
qp_color_t get_adaptive_color(painter_device_t device, bool is_active) {
    const qp_display_info_t* info = qp_get_display_info(device);
    
    if (!info->has_color) {
        // Monochrome display
        return is_active ? QP_COLOR_WHITE : QP_COLOR_BLACK;
    } else {
        // Color display
        return is_active ? QP_COLOR_GREEN : QP_COLOR_RED;
    }
}

void draw_adaptive_indicator(bool active) {
    qp_color_t color = get_adaptive_color(display, active);
    qp_fill_rect(display, 200, 10, 20, 20, color);
}
```

## QGF Image Handling

### Converting and Loading Images

```bash
# Convert PNG to QGF using QMK CLI
qmk painter-convert-graphics -f rgb565 -i my_icon.png -o ./generated/
qmk painter-convert-graphics -f mono2 -i my_mono_icon.png -o ./generated/
```

```c
// Include generated QGF data
#include "generated/my_icon.qgf.h"
#include "generated/my_mono_icon.qgf.h"

void load_and_display_images(void) {
    // Load color image
    qp_image_t color_icon = qp_load_image_mem(gfx_my_icon, sizeof(gfx_my_icon));
    
    // Load monochrome image
    qp_image_t mono_icon = qp_load_image_mem(gfx_my_mono_icon, sizeof(gfx_my_mono_icon));
    
    if (qp_image_is_valid(&color_icon)) {
        qp_draw_image(display, &color_icon, 10, 10);
        
        // Get image dimensions
        uint16_t width, height;
        qp_image_get_size(&color_icon, &width, &height);
        printf("Color icon size: %dx%d\n", width, height);
    }
    
    if (qp_image_is_valid(&mono_icon)) {
        // Draw with tinting
        qp_draw_image_tinted(display, &mono_icon, 50, 10, QP_COLOR_BLUE);
    }
    
    // Clean up
    qp_free_image(&color_icon);
    qp_free_image(&mono_icon);
}
```

### Image Sequences for Animation

```c
// Load animation frames
qp_image_t spinner_frame1 = qp_load_image_mem(gfx_spinner1, sizeof(gfx_spinner1));
qp_image_t spinner_frame2 = qp_load_image_mem(gfx_spinner2, sizeof(gfx_spinner2));
qp_image_t spinner_frame3 = qp_load_image_mem(gfx_spinner3, sizeof(gfx_spinner3));
qp_image_t spinner_frame4 = qp_load_image_mem(gfx_spinner4, sizeof(gfx_spinner4));

// Create animation sequence
QP_DEFINE_LOOP_SEQUENCE(spinner_animation, 150, 
                        spinner_frame1, spinner_frame2, 
                        spinner_frame3, spinner_frame4);

void cleanup_spinner_images(void) {
    qp_free_image(&spinner_frame1);
    qp_free_image(&spinner_frame2);
    qp_free_image(&spinner_frame3);
    qp_free_image(&spinner_frame4);
}
```

## Animation Examples

### Basic Animation

```c
static qp_animator_t spinner_anim;
static bool spinner_initialized = false;

void update_spinner_animation(void) {
    uint32_t now = timer_read32();
    
    if (!spinner_initialized) {
        qp_animator_start(&spinner_anim, &spinner_animation, true, now);
        spinner_initialized = true;
    }
    
    if (qp_animator_is_active(&spinner_anim)) {
        // Clear background and draw current frame
        qp_clear_rect(display, 100, 100, 32, 32);
        qp_animator_step_and_draw(&spinner_anim, display, 100, 100, now);
    }
}
```

### Reversible Animation

```c
static qp_animator_t bounce_anim;
static bool bounce_forward = true;

void update_bounce_animation(void) {
    uint32_t now = timer_read32();
    
    if (qp_animator_is_active(&bounce_anim)) {
        qp_anim_result_t result = qp_animator_step_and_draw(&bounce_anim, display, 50, 50, now);
        
        if (result != QP_ANIM_RUNNING) {
            // Animation completed, reverse direction
            bounce_forward = !bounce_forward;
            qp_animator_start(&bounce_anim, &bounce_animation, bounce_forward, now);
        }
    } else {
        // Start initial animation
        qp_animator_start(&bounce_anim, &bounce_animation, bounce_forward, now);
    }
}
```

## Controller Examples

### One-Shot Boot Animation

```c
static qp_oneshot_controller_t boot_controller;

void init_boot_animation(void) {
    qp_oneshot_init(&boot_controller, &boot_sequence, 
                    10, 10,     // Position
                    true,       // Steady at end
                    true,       // Auto-boot
                    timer_read32());
}

void update_boot_animation(void) {
    qp_oneshot_render(&boot_controller, display, timer_read32());
}
```

### Toggle Controller for Caps Lock

```c
static qp_toggle_controller_t caps_controller;

void init_caps_indicator(void) {
    qp_toggle_init(&caps_controller, &caps_sequence,
                   200, 0,     // Position
                   true,       // Steady at end
                   false,      // Initial state (off)
                   timer_read32());
}

void update_caps_indicator(void) {
    bool caps_active = host_keyboard_led_state().caps_lock;
    qp_toggle_set(&caps_controller, caps_active, timer_read32());
    qp_toggle_render(&caps_controller, display, timer_read32());
}
```

## Declarative Widget Examples

### Layer Indicator Widget

```c
// Define layer animation sequences (loaded elsewhere)
extern const qp_image_sequence_t layer0_seq, layer1_seq, layer2_seq, layer3_seq;

// Define widget states
QP_DEFINE_WIDGET_STATES(layer_states,
    QP_STATE_FWD(layer0_seq),  // Default layer
    QP_STATE_FWD(layer1_seq),  // Function layer
    QP_STATE_FWD(layer2_seq),  // Navigation layer
    QP_STATE_FWD(layer3_seq)   // Media layer
);

// Widget configuration
static const qp_widget_config_t layer_config =
    QP_WIDGET_CONFIG_OPAQUE(64, 0, 40, 16, layer_states, 4, qp_query_layer, NULL);

static qp_widget_t layer_widget;

void init_layer_widget(void) {
    qp_widget_init(&layer_widget, &layer_config, 0, timer_read32());
}

void update_layer_widget(void) {
    qp_widget_tick(&layer_widget, display, timer_read32());
}
```

### Modifier Status Widget

```c
// Modifier mask for Ctrl+Shift
static uint8_t ctrl_shift_mask = MOD_MASK_CTRL | MOD_MASK_SHIFT;

QP_DEFINE_WIDGET_STATES(modifier_states,
    QP_STATE_REV(modifier_off_seq),  // No modifiers
    QP_STATE_FWD(modifier_on_seq)    // Modifiers active
);

static const qp_widget_config_t modifier_config =
    QP_WIDGET_CONFIG_ADDITIVE(100, 20, 24, 8, modifier_states, 2,
                              qp_query_modifiers, &ctrl_shift_mask);

static qp_widget_t modifier_widget;

void init_modifier_widget(void) {
    qp_widget_init(&modifier_widget, &modifier_config, 0, timer_read32());
}

void update_modifier_widget(void) {
    qp_widget_tick(&modifier_widget, display, timer_read32());
}
```

### Custom Query Function

```c
// Custom query for WPM ranges
uint8_t query_wpm_range(void* user_data) {
    (void)user_data;
    uint8_t wpm = get_current_wpm();

    if (wpm < 20) return 0;      // Slow
    if (wpm < 40) return 1;      // Medium
    if (wpm < 60) return 2;      // Fast
    return 3;                    // Very fast
}

QP_DEFINE_WIDGET_STATES(wpm_states,
    QP_STATE_FWD(wpm_slow_seq),
    QP_STATE_FWD(wpm_medium_seq),
    QP_STATE_FWD(wpm_fast_seq),
    QP_STATE_FWD(wpm_vfast_seq)
);

static const qp_widget_config_t wpm_config =
    QP_WIDGET_CONFIG_OPAQUE(0, 20, 60, 12, wpm_states, 4, query_wpm_range, NULL);

static qp_widget_t wpm_widget;
```

## Multi-Display Examples

### Dual Display Setup

```c
painter_device_t primary_display;
painter_device_t secondary_display;

void init_dual_displays(void) {
    // Primary display (color LCD)
    primary_display = qp_ili9341_make_spi_device(240, 320,
                                                 PRIMARY_CS_PIN,
                                                 PRIMARY_DC_PIN,
                                                 PRIMARY_RST_PIN,
                                                 16, 0);

    // Secondary display (monochrome OLED)
    secondary_display = qp_sh1106_make_i2c_device(128, 64, 0x3C);

    if (qp_init(primary_display, QP_ROTATION_0)) {
        qp_utils_init(primary_display);
        qp_power(primary_display, true);
    }

    if (qp_init(secondary_display, QP_ROTATION_0)) {
        qp_utils_init(secondary_display);
        qp_power(secondary_display, true);
    }
}

void update_dual_displays(void) {
    uint32_t now = timer_read32();

    // Update primary display with full interface
    qp_widget_tick(&layer_widget, primary_display, now);
    qp_widget_tick(&modifier_widget, primary_display, now);
    qp_widget_tick(&wpm_widget, primary_display, now);

    // Update secondary display with simplified info
    qp_oneshot_render(&boot_controller, secondary_display, now);
    qp_toggle_render(&caps_controller, secondary_display, now);
}
```

### Display-Specific Adaptations

```c
void draw_adaptive_content(painter_device_t device) {
    const qp_display_info_t* info = qp_get_display_info(device);

    if (info->width >= 240) {
        // Large display - show detailed interface
        draw_full_interface(device);
    } else if (info->width >= 128) {
        // Medium display - show simplified interface
        draw_compact_interface(device);
    } else {
        // Small display - show minimal interface
        draw_minimal_interface(device);
    }
}

void draw_full_interface(painter_device_t device) {
    // Full color interface with animations
    qp_fill_rect(device, 0, 0, 240, 20, QP_HSV(200, 100, 50)); // Header
    // ... more complex drawing
}

void draw_compact_interface(painter_device_t device) {
    // Simplified interface for medium displays
    qp_draw_rect(device, 0, 0, 128, 64, QP_COLOR_WHITE); // Border
    // ... compact layout
}

void draw_minimal_interface(painter_device_t device) {
    // Minimal interface for small displays
    qp_draw_pixel(device, 0, 0, QP_COLOR_WHITE); // Status dot
    // ... minimal indicators
}
```

## Migration Examples

### From OLED Utils to QP Utils

#### Old OLED Utils Code

```c
// OLD: oled_utils approach
#include "oled_utils.h"

const uint8_t PROGMEM my_icon[] = {
    0x3C, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3C,
    0x00, 0x00, 0x18, 0x24, 0x24, 0x18, 0x00, 0x00
};

slice_t icon = SLICE16x8(my_icon);
draw_slice_px(&icon, 10, 8);
clear_rect(0, 0, 32, 16);

DEFINE_SLICE_SEQ(my_anim, SLICE16x8(frame1), SLICE16x8(frame2));
animator_t anim;
animator_start(&anim, &my_anim, true, timer_read32());
```

#### New QP Utils Code

```c
// NEW: qp_utils approach
#include "qp_utils.h"
#include "qp_image.h"
#include "qp_anim.h"

// Convert bitmap to QGF format first:
// qmk painter-convert-graphics -f mono2 -i my_icon.png -o ./generated/

#include "generated/my_icon.qgf.h"
#include "generated/frame1.qgf.h"
#include "generated/frame2.qgf.h"

qp_image_t icon = qp_load_image_mem(gfx_my_icon, sizeof(gfx_my_icon));
qp_draw_image(display, &icon, 10, 8);
qp_clear_rect(display, 0, 0, 32, 16);

qp_image_t frame1_img = qp_load_image_mem(gfx_frame1, sizeof(gfx_frame1));
qp_image_t frame2_img = qp_load_image_mem(gfx_frame2, sizeof(gfx_frame2));
QP_DEFINE_SEQUENCE(my_anim, 100, true, frame1_img, frame2_img);

qp_animator_t anim;
qp_animator_start(&anim, &my_anim, true, timer_read32());
```

### Migration Checklist

1. **Convert Images**: Use QMK CLI to convert bitmap data to QGF format
2. **Add Device Parameter**: All drawing functions now require `painter_device_t`
3. **Update Color Handling**: Use new color abstraction system
4. **Replace Slice Macros**: Use QGF image loading functions
5. **Update Animation Setup**: Use new sequence definition macros
6. **Test on Hardware**: Verify functionality on target display

### Automated Migration Script

```bash
#!/bin/bash
# migrate_to_qp_utils.sh - Helper script for migration

echo "Converting images to QGF format..."
for img in images/*.png; do
    basename=$(basename "$img" .png)
    qmk painter-convert-graphics -f mono2 -i "$img" -o "./generated/"
done

echo "Migration complete. Update your code to use the new QP Utils API."
echo "See EXAMPLES.md for detailed migration examples."
```
```
