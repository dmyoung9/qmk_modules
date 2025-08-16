# OLED Utils Examples

This document provides comprehensive examples for using the refactored oled_utils module.

## Table of Contents

1. [Basic Drawing Examples](#basic-drawing-examples)
2. [Slice Macro Usage](#slice-macro-usage)
3. [Unified Animation Controller](#unified-animation-controller)
4. [Enhanced Declarative Widgets](#enhanced-declarative-widgets)
5. [Migration Examples](#migration-examples)
6. [Advanced Integration](#advanced-integration)

## Basic Drawing Examples

### Simple Bitmap Drawing

```c
#include "oled_utils.h"

// Define bitmap data in PROGMEM
const uint8_t PROGMEM my_icon_16x8[] = {
    0x3C, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3C,
    0x00, 0x00, 0x18, 0x24, 0x24, 0x18, 0x00, 0x00
};

void draw_my_icon(void) {
    // Create slice using macro from oled_slice.h
    slice_t icon = SLICE16x8(my_icon_16x8);
    
    // Draw at position (10, 8)
    draw_slice_px(&icon, 10, 8);
}
```

### Clearing Areas

```c
void clear_display_areas(void) {
    // Clear rectangular area
    clear_rect(0, 0, 64, 16);
    
    // Clear common 16x8 spans
    clear_span16(64, 0);   // Clear at (64, 0)
    clear_span16(80, 8);   // Clear at (80, 8)
}
```

## Slice Macro Usage

### Standard Page-Aligned Bitmaps

```c
// 1 page high (8 pixels)
const uint8_t PROGMEM small_icon[] = { /* 8x8 data */ };
slice_t small = SLICE8x8(small_icon);

// 2 pages high (16 pixels)  
const uint8_t PROGMEM medium_icon[] = { /* 16x16 data */ };
slice_t medium = SLICE16x16(medium_icon);

// 4 pages high (32 pixels) - full height
const uint8_t PROGMEM large_icon[] = { /* 32x32 data */ };
slice_t large = SLICE32x32(large_icon);

// Wide bitmaps
const uint8_t PROGMEM banner[] = { /* 128x8 data */ };
slice_t banner_slice = SLICE128x8(banner);
```

### Arbitrary Height Bitmaps

```c
// 12 pixels high (needs 2 pages, but only draws 12 pixels)
const uint8_t PROGMEM indicator_12px[] = {
    // Page 0 (pixels 0-7): 16 bytes
    0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFF,
    0x00, 0x7E, 0x42, 0x42, 0x42, 0x42, 0x7E, 0x00,
    // Page 1 (pixels 8-11): 16 bytes, only first 4 pixels drawn
    0x0F, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x0F,
    0x00, 0x06, 0x02, 0x02, 0x02, 0x02, 0x06, 0x00
};

void draw_arbitrary_height(void) {
    slice_t indicator = SLICE16x12(indicator_12px);
    draw_slice_px(&indicator, 0, 0);  // Only draws 12 pixels high
}
```

### Custom Slice Creation

```c
// For non-standard sizes
const uint8_t PROGMEM custom_bitmap[] = { /* your data */ };

// Custom page-aligned
slice_t custom1 = SLICE_CUSTOM(custom_bitmap, 48, 3);  // 48x24

// Custom arbitrary height
slice_t custom2 = SLICE_CUSTOM_PX(custom_bitmap, 40, 18);  // 40x18

// Using the generic helper
slice_t custom3 = SLICE_W_H(custom_bitmap, 56, 22);  // 56x22
```

## Unified Animation Controller

### Oneshot Animation (Boot/Trigger)

```c
#include "oled_unified_anim.h"

// Animation sequence
DEFINE_SLICE_SEQ(boot_anim,
    SLICE32x16(boot_frame_0),
    SLICE32x16(boot_frame_1),
    SLICE32x16(boot_frame_2),
    SLICE32x16(boot_frame_3)
);

// Configuration
static const unified_anim_config_t boot_config = 
    UNIFIED_ONESHOT_CONFIG(&boot_anim, 48, 8, STEADY_LAST, true);

// Runtime instance
static unified_anim_t boot_widget;

void init_boot_animation(void) {
    uint32_t now = timer_read32();
    unified_anim_init(&boot_widget, &boot_config, 0, now);
}

void update_boot_animation(void) {
    uint32_t now = timer_read32();
    unified_anim_render(&boot_widget, now);
}
```

### Toggle Animation (On/Off States)

```c
// Caps lock indicator
DEFINE_SLICE_SEQ(caps_anim,
    SLICE24x8(caps_off),    // Frame 0: off state
    SLICE24x8(caps_fade1),  // Frame 1: transition
    SLICE24x8(caps_fade2),  // Frame 2: transition
    SLICE24x8(caps_on)      // Frame 3: on state
);

static const unified_anim_config_t caps_config = 
    UNIFIED_TOGGLE_CONFIG(&caps_anim, 10, 2, BLEND_OPAQUE);

static unified_anim_t caps_widget;

void init_caps_indicator(void) {
    uint32_t now = timer_read32();
    bool initial_state = host_keyboard_led_state().caps_lock;
    unified_anim_init(&caps_widget, &caps_config, initial_state ? 1 : 0, now);
}

void update_caps_indicator(void) {
    uint32_t now = timer_read32();
    
    // Check caps lock state and trigger animation if changed
    bool caps_active = host_keyboard_led_state().caps_lock || is_caps_word_on();
    unified_anim_trigger(&caps_widget, caps_active ? 1 : 0, now);
    
    // Render current frame
    unified_anim_render(&caps_widget, now);
}
```

### Out-and-Back Animation

```c
// Button press feedback animation
DEFINE_SLICE_SEQ(button_anim,
    SLICE16x16(button_normal),
    SLICE16x16(button_press1),
    SLICE16x16(button_press2),
    SLICE16x16(button_pressed)
);

static const unified_anim_config_t button_config = 
    UNIFIED_OUTBACK_CONFIG(&button_anim, 64, 16, STEADY_FIRST, false);

static unified_anim_t button_widget;

void trigger_button_feedback(void) {
    uint32_t now = timer_read32();
    if (unified_anim_boot_done(&button_widget)) {
        unified_anim_trigger(&button_widget, 0, now);  // State ignored for outback
    }
}
```

## Enhanced Declarative Widgets

### Layer Indicator with Error Handling

```c
#include "oled_declarative.h"

// Layer animation sequences
DEFINE_SLICE_SEQ(layer0_seq, SLICE32x12(layer0_0), SLICE32x12(layer0_1), SLICE32x12(layer0_2));
DEFINE_SLICE_SEQ(layer1_seq, SLICE32x12(layer1_0), SLICE32x12(layer1_1), SLICE32x12(layer1_2));
DEFINE_SLICE_SEQ(layer2_seq, SLICE32x12(layer2_0), SLICE32x12(layer2_1), SLICE32x12(layer2_2));

// State descriptions
static const state_desc_t layer_states[] = {
    STATE_FWD(&layer0_seq),  // Layer 0: forward animation
    STATE_FWD(&layer1_seq),  // Layer 1: forward animation  
    STATE_FWD(&layer2_seq),  // Layer 2: forward animation
};

// Query function (new signature)
static uint8_t layer_query(uint32_t user_arg, uint8_t current_state, uint32_t now) {
    uint8_t layer = get_highest_layer(layer_state);
    return (layer < 3) ? layer : current_state;  // Safety check
}

// Error handler
static void layer_error_handler(const widget_config_t *cfg, uint8_t error_code, uint32_t context) {
    // Log error or take recovery action
    if (error_code == WIDGET_ERROR_STUCK_ANIMATION) {
        // Force reset on stuck animation
        // Implementation depends on your logging system
    }
}

// Widget configuration with error handling
static const widget_config_t layer_widget_config = 
    WIDGET_CONFIG_WITH_ERROR_HANDLER(64, 0, 32, 12, layer_states, 3, layer_query, 0, 0, layer_error_handler);

static widget_t layer_widget;

void init_layer_widget(void) {
    uint32_t now = timer_read32();
    widget_init(&layer_widget, &layer_widget_config, get_highest_layer(layer_state), now);
}

void update_layer_widget(void) {
    uint32_t now = timer_read32();
    widget_tick(&layer_widget, now);
}
```

### Advanced Widget with Validation

```c
// Validation function
static bool modifier_widget_validate(const widget_config_t *cfg, const widget_t *runtime_state) {
    // Check configuration validity
    if (!cfg || !cfg->states || cfg->state_count == 0) {
        return false;
    }
    
    // Check runtime state if provided
    if (runtime_state && runtime_state->initialized) {
        if (runtime_state->src >= cfg->state_count) {
            return false;
        }
    }
    
    return true;
}

// Advanced configuration
static const widget_config_t modifier_config = WIDGET_CONFIG_ADVANCED(
    10, 20,                    // Position
    80, 8,                     // Bounding box
    BLIT_ADDITIVE,            // Additive blending
    modifier_states,           // State array
    4,                        // State count
    modifier_query,           // Query function
    0,                        // User arg
    0,                        // Initial state
    modifier_widget_validate, // Validation function
    NULL,                     // Error handler (use default)
    50,                       // Query every 50ms
    true,                     // Auto-recover
    5                         // Max 5 retries
);
```

## Migration Examples

### From Old Animation Controllers to Unified

#### Old Oneshot Controller → Unified

```c
// OLD WAY
oneshot_anim_t old_anim;
oneshot_anim_init(&old_anim, &my_seq, 10, 8, true, true, now);
oneshot_anim_render(&old_anim, now);

// NEW WAY (unified)
static const unified_anim_config_t config =
    UNIFIED_ONESHOT_CONFIG(&my_seq, 10, 8, STEADY_LAST, true);
unified_anim_t new_anim;
unified_anim_init(&new_anim, &config, 0, now);
unified_anim_render(&new_anim, now);
```

#### Old Toggle Controller → Unified

```c
// OLD WAY
toggle_anim_t old_toggle;
toggle_anim_init(&old_toggle, &caps_seq, 10, 2, false, now);
toggle_anim_set(&old_toggle, caps_active, now);
toggle_anim_render(&old_toggle, now);

// NEW WAY (unified)
static const unified_anim_config_t config =
    UNIFIED_TOGGLE_CONFIG(&caps_seq, 10, 2, BLEND_OPAQUE);
unified_anim_t new_toggle;
unified_anim_init(&new_toggle, &config, caps_active ? 1 : 0, now);
unified_anim_trigger(&new_toggle, caps_active ? 1 : 0, now);
unified_anim_render(&new_toggle, now);
```

#### Old Bootrev Controller → Unified

```c
// OLD WAY
bootrev_anim_t old_bootrev;
bootrev_anim_init(&old_bootrev, &layer_seq, 42, 0, true, now);
bootrev_anim_trigger(&old_bootrev, now);
bootrev_anim_render(&old_bootrev, now);

// NEW WAY (unified)
static const unified_anim_config_t config =
    UNIFIED_BOOTREV_CONFIG(&layer_seq, 42, 0, true);
unified_anim_t new_bootrev;
unified_anim_init(&new_bootrev, &config, 0, now);
unified_anim_trigger(&new_bootrev, 0, now);
unified_anim_render(&new_bootrev, now);
```

### Legacy Query Function Migration

```c
// OLD SIGNATURE (still supported)
static uint8_t old_layer_query(uint32_t user_arg) {
    return get_highest_layer(layer_state);
}

// Use with legacy config macro
static const widget_config_t old_config =
    WIDGET_CONFIG_LEGACY(64, 0, 32, 12, layer_states, 3, old_layer_query, 0, 0);

// NEW SIGNATURE (recommended)
static uint8_t new_layer_query(uint32_t user_arg, uint8_t current_state, uint32_t now) {
    uint8_t layer = get_highest_layer(layer_state);

    // Can use current_state for context-aware decisions
    if (layer == current_state) {
        return 0xFF;  // No change needed
    }

    // Can use timestamp for time-based logic
    static uint32_t last_change = 0;
    if (now - last_change < 100) {
        return current_state;  // Debounce rapid changes
    }
    last_change = now;

    return layer;
}

// Use with standard config macro
static const widget_config_t new_config =
    WIDGET_CONFIG(64, 0, 32, 12, layer_states, 3, new_layer_query, 0, 0);
```

## Advanced Integration

### Complete OLED Display with Multiple Widgets

```c
#include "oled_utils.h"
#include "oled_unified_anim.h"
#include "oled_declarative.h"

// Boot animation
static const unified_anim_config_t boot_config =
    UNIFIED_ONESHOT_CONFIG(&boot_seq, 0, 0, STEADY_LAST, true);
static unified_anim_t boot_anim;

// Layer indicator widget
static const widget_config_t layer_config =
    WIDGET_CONFIG(64, 0, 40, 16, layer_states, 4, layer_query, 0, 0);
static widget_t layer_widget;

// Modifier indicators (multiple unified controllers)
static const unified_anim_config_t caps_config =
    UNIFIED_TOGGLE_CONFIG(&caps_seq, 10, 20, BLEND_OPAQUE);
static const unified_anim_config_t shift_config =
    UNIFIED_TOGGLE_CONFIG(&shift_seq, 35, 20, BLEND_OPAQUE);
static const unified_anim_config_t ctrl_config =
    UNIFIED_TOGGLE_CONFIG(&ctrl_seq, 60, 20, BLEND_OPAQUE);

static unified_anim_t caps_anim, shift_anim, ctrl_anim;

void init_complete_display(void) {
    uint32_t now = timer_read32();

    // Initialize boot animation
    unified_anim_init(&boot_anim, &boot_config, 0, now);

    // Initialize layer widget
    widget_init(&layer_widget, &layer_config, get_highest_layer(layer_state), now);

    // Initialize modifier animations
    unified_anim_init(&caps_anim, &caps_config, 0, now);
    unified_anim_init(&shift_anim, &shift_config, 0, now);
    unified_anim_init(&ctrl_anim, &ctrl_config, 0, now);
}

void update_complete_display(void) {
    uint32_t now = timer_read32();

    // Update boot animation (runs once)
    if (!unified_anim_boot_done(&boot_anim)) {
        unified_anim_render(&boot_anim, now);
        return;  // Don't show other widgets during boot
    }

    // Update layer widget
    widget_tick(&layer_widget, now);

    // Update modifier states and animations
    uint8_t mods = get_mods() | get_oneshot_mods();
    bool caps_active = host_keyboard_led_state().caps_lock || is_caps_word_on();
    bool shift_active = (mods & MOD_MASK_SHIFT) != 0;
    bool ctrl_active = (mods & MOD_MASK_CTRL) != 0;

    unified_anim_trigger(&caps_anim, caps_active ? 1 : 0, now);
    unified_anim_trigger(&shift_anim, shift_active ? 1 : 0, now);
    unified_anim_trigger(&ctrl_anim, ctrl_active ? 1 : 0, now);

    unified_anim_render(&caps_anim, now);
    unified_anim_render(&shift_anim, now);
    unified_anim_render(&ctrl_anim, now);

    // Draw static elements
    draw_slice_px(&logo_slice, 104, 0);
}

// Integration with QMK
bool oled_task_user(void) {
    if (is_keyboard_master()) {
        update_complete_display();
    } else {
        // Slave side display
        render_slave_content();
    }
    return false;
}

void keyboard_post_init_user(void) {
    init_complete_display();
}
```

### Performance-Optimized Display

```c
// Use additive blending for overlapping elements
static const unified_anim_config_t overlay_config =
    UNIFIED_TOGGLE_CONFIG(&overlay_seq, 50, 10, BLEND_ADDITIVE);

// Optimize query intervals for less critical widgets
static const widget_config_t background_config = WIDGET_CONFIG_ADVANCED(
    0, 24, 128, 8,           // Full width background
    BLIT_OPAQUE,             // Clear background
    bg_states, 2,            // Simple 2-state background
    bg_query, 0, 0,          // Query function
    NULL, NULL,              // No validation/error handling
    200,                     // Query only every 200ms
    false, 0                 // No auto-recovery needed
);

void optimized_display_update(void) {
    uint32_t now = timer_read32();

    // Update critical widgets every frame
    widget_tick(&layer_widget, now);
    unified_anim_render(&caps_anim, now);

    // Update background widget less frequently (handled by query_interval_ms)
    widget_tick(&background_widget, now);

    // Render overlay with additive blending
    unified_anim_render(&overlay_anim, now);
}
```
