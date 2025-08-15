# OLED Utils

A comprehensive OLED animation and widget system for QMK keyboards, providing rotation-safe drawing, declarative animations, and fluent APIs for creating sophisticated OLED displays.

## Features

### Core Drawing Utilities (`oled_utils`)
- **Rotation-safe drawing** on 128×32 OLEDs with page-packed memory
- **Simple, reliable clearing** of arbitrary rectangles and common spans
- **Fast blitting** for page-aligned art with correct blending for unaligned Y coordinates
- **PROGMEM-friendly bitmap abstraction** without compiler warnings
- **Comprehensive SLICE macros** for common bitmap sizes

### Animation Engine (`oled_anim`)
- **Low-level frame animator** with direction control and mid-flight reversal
- **State transition controllers** for exclusive widgets (layers) and binary widgets (modifiers)
- **Reusable animations** using the same frames for enter/exit (forward vs reverse)
- **Concurrent, independent widgets** with automatic timing management
- **Chained transitions** (exit old → enter new) with graceful cancellation

### Declarative Widget System (`oled_declarative`)
- **Fully declarative configuration** for widgets: states, conditions, positions, sizes
- **Automatic state management** with smooth transitions
- **Configurable blending modes** (opaque vs additive)
- **Query-driven state updates** with user-defined condition functions
- **Works for both exclusive and independent widgets**

## Quick Start

### Basic Setup

1. Add the module to your keymap's `keymap.json`:
```json
{
    "modules": ["dmyoung9/oled_utils"]
}
```

2. Include the headers in your code:
```c
#include "oled_utils.h"
#include "oled_anim.h"
#include "oled_declarative.h"
```

### Simple Drawing Example

```c
// Define bitmap data in PROGMEM
const uint8_t PROGMEM my_icon[] = {
    0x3C, 0x42, 0x81, 0x81, 0x81, 0x81, 0x42, 0x3C,
    0x00, 0x00, 0x18, 0x24, 0x24, 0x18, 0x00, 0x00
};

// Create a slice and draw it
slice_t icon = SLICE8x16(my_icon);
draw_slice_px(&icon, 10, 8);  // Draw at position (10, 8)
```

### Animation Example

```c
// Define animation frames
const uint8_t PROGMEM frame1[] = { /* frame data */ };
const uint8_t PROGMEM frame2[] = { /* frame data */ };
const uint8_t PROGMEM frame3[] = { /* frame data */ };

// Create animation sequence
DEFINE_SLICE_SEQ(my_animation,
    SLICE16x8(frame1),
    SLICE16x8(frame2),
    SLICE16x8(frame3)
);

// Use with animator
animator_t anim;
animator_start(&anim, &my_animation, true, timer_read32());

// In your OLED task:
if (anim.active) {
    animator_step_and_draw(&anim, 0, 0, timer_read32());
}
```

### Declarative Widget Example

```c
// Define states for a layer indicator
static const state_desc_t layer_states[] = {
    STATE_FWD(&layer0_anim),  // Layer 0: forward animation
    STATE_FWD(&layer1_anim),  // Layer 1: forward animation
    STATE_FWD(&layer2_anim),  // Layer 2: forward animation
    STATE_FWD(&layer3_anim),  // Layer 3: forward animation
};

// Query function to determine current layer
static uint8_t layer_query(uint32_t user_arg) {
    return get_highest_layer(layer_state);
}

// Widget configuration
static const widget_config_t layer_widget_config = {
    .x = 64, .y = 0,           // Position
    .bbox_w = 40, .bbox_h = 32, // Bounding box
    .blit = BLIT_OPAQUE,       // Clear background each frame
    .states = layer_states,
    .state_count = 4,
    .query = layer_query,
    .user_arg = 0,
    .initial_state = 0
};

// Runtime instance
static widget_t layer_widget;

// Initialize in keyboard_post_init_user()
widget_init(&layer_widget, &layer_widget_config, 0, timer_read32());

// Update in oled_task_user()
widget_tick(&layer_widget, timer_read32());
```

## API Reference

### Core Drawing (`oled_utils.h`)

#### Functions
- `void clear_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h)` - Clear rectangular area
- `void clear_span16(uint8_t x, uint8_t y)` - Clear 16×8 pixel span
- `void draw_slice_px(const slice_t *s, uint8_t x, uint8_t y)` - Draw bitmap slice

#### SLICE Macros
- `SLICE8x8(data)`, `SLICE16x8(data)`, `SLICE32x8(data)` - 1 page high
- `SLICE8x16(data)`, `SLICE16x16(data)` - 2 pages high  
- `SLICE8x32(data)`, `SLICE16x32(data)`, `SLICE128x32(data)` - 4 pages high
- `SLICE_CUSTOM(data, width, pages)` - Custom dimensions

#### Utility Functions
- `uint8_t slice_width_px(const slice_t *s)` - Get width in pixels
- `uint8_t slice_height_px(const slice_t *s)` - Get height in pixels
- `bool slice_is_valid(const slice_t *s)` - Check if slice is valid

### Animation Engine (`oled_anim.h`)

#### Low-Level Animator
- `void animator_start(animator_t *a, const slice_seq_t *seq, bool forward, uint32_t now)`
- `void animator_reverse(animator_t *a, uint32_t now)` - Reverse direction mid-flight
- `anim_result_t animator_step(animator_t *a, uint32_t now)` - Advance one frame
- `void animator_draw_current(const animator_t *a, uint8_t x, uint8_t y)` - Draw current frame

#### State Transition Controller
- `void layer_tr_init(layer_transition_t *t, ...)` - Initialize exclusive state controller
- `void layer_tr_request(layer_transition_t *t, uint8_t state, uint32_t now)` - Request state change
- `void layer_tr_render(layer_transition_t *t, uint32_t now)` - Update and render

#### Binary Toggle Controller  
- `void toggle_anim_init(toggle_anim_t *w, ...)` - Initialize binary toggle
- `void toggle_anim_set(toggle_anim_t *w, bool on, uint32_t now)` - Set desired state
- `void toggle_anim_render(toggle_anim_t *w, uint32_t now)` - Update and render

### Declarative Widgets (`oled_declarative.h`)

#### Core Functions
- `void widget_init(widget_t *w, const widget_config_t *cfg, uint8_t initial_state, uint32_t now)`
- `void widget_tick(widget_t *w, uint32_t now)` - Query state, transition, and render

#### Helper Macros
- `STATE_FWD(seq)` - Create forward-entering state description
- `STATE_REV(seq)` - Create reverse-entering state description  
- `WIDGET_CONFIG(x, y, w, h, states, count, query, user_data, init_state)` - Quick config

## Configuration

### Animation Timing
```c
// In your config.h
#define ANIM_FRAME_MS 60  // Faster animations (default: 80ms)
```

### Memory Usage
- Each `slice_t`: 4 bytes
- Each `animator_t`: ~12 bytes
- Each `widget_t`: ~20 bytes
- Frame data stored in PROGMEM (no RAM cost)

## Performance Characteristics

- **Page-aligned drawing**: Optimized fast path for Y coordinates divisible by 8
- **Unaligned drawing**: Automatic read-modify-write with OR-blending
- **Clipping**: All drawing functions clip to screen bounds automatically
- **Rotation-safe**: Works correctly with any OLED rotation setting
- **Concurrent widgets**: Run multiple independent animations simultaneously

## Integration Examples

See the `lcars` and `drinf58` keymaps for complete integration examples showing:
- Layer indicators with smooth transitions
- Modifier status with independent toggles  
- WPM displays with custom rendering
- Complex multi-widget layouts

## Troubleshooting

### Common Issues
1. **Blank display**: Check that `OLED_ENABLE = yes` in rules.mk
2. **Garbled graphics**: Ensure bitmap data is in PROGMEM and uses correct page format
3. **Animation not updating**: Call widget functions every OLED frame with current timestamp
4. **Compilation errors**: Verify all required source files are included in rules.mk

### Debug Tips
- Use `slice_is_valid()` to check bitmap data
- Verify state indices are within bounds
- Check that query functions return valid state numbers
- Ensure timing calls use `timer_read32()` consistently

## License

This module is part of the QMK userspace and follows the same licensing terms.
