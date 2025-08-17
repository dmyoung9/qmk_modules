# QP Utils

A comprehensive, device-agnostic animation and widget system for Quantum Painter displays, providing color-aware drawing, declarative animations, and fluent APIs for creating sophisticated graphical interfaces.

## Features

### Core Drawing Utilities (`qp_utils`)
- **Device-agnostic drawing** that works across all QP-supported displays
- **Color abstraction layer** supporting monochrome, palette, and RGB modes
- **Coordinate system utilities** with automatic clipping and bounds checking
- **Performance-optimized primitives** for rectangles, lines, and pixels
- **Clean separation of concerns** with modular architecture

### QGF Image System (`qp_image`)
- **QGF image loading** from PROGMEM and external storage
- **Animation sequence management** with frame timing control
- **Color format abstraction** for different display technologies
- **Memory-efficient handling** with automatic resource management
- **Fluent API design** for easy image creation and manipulation

### Animation Engine (`qp_anim`)
- **Low-level frame animator** with direction control and mid-flight reversal
- **Timing management** with configurable frame durations
- **Device-agnostic rendering** that adapts to display capabilities
- **Sequence management** for complex animation patterns
- **Performance optimizations** for smooth playback

### Animation Controllers (`qp_controllers`)
- **One-shot animations** for boot sequences and event triggers
- **Toggle controllers** for binary state indicators (on/off, active/inactive)
- **Out-and-back animations** for layer transitions and effects
- **Automatic state management** with configurable timing
- **Concurrent operation** support for multiple independent animations

### Declarative Widget System (`qp_declarative`)
- **Fully declarative configuration** with states, conditions, and layout
- **Automatic state transitions** based on keyboard state queries
- **Configurable rendering policies** (opaque vs additive blending)
- **Fluent configuration APIs** for easy widget setup
- **Device-agnostic operation** across all QP display types

## Quick Start

1. Add to your `rules.mk`:
```make
QUANTUM_PAINTER_ENABLE = yes
QUANTUM_PAINTER_DRIVERS += your_display_driver
```

2. Include the headers you need:
```c
#include "qp_utils.h"        // Core drawing functions
#include "qp_image.h"        // QGF image handling
#include "qp_anim.h"         // Animation engine
#include "qp_controllers.h"  // Animation controllers
#include "qp_declarative.h"  // Declarative widgets
```

### Simple Drawing Example

```c
// Initialize QP device
painter_device_t display = qp_make_ili9341_spi(/* pins */);
qp_init(display, QP_ROTATION_0);
qp_utils_init(display);

// Draw basic shapes
qp_clear_rect(display, 0, 0, 100, 50);
qp_fill_rect(display, 10, 10, 80, 30, QP_COLOR_BLUE);
qp_draw_rect(display, 5, 5, 90, 40, QP_COLOR_WHITE);

// Draw with different color formats
qp_draw_pixel(display, 50, 25, QP_HSV(120, 255, 255));  // Green
qp_draw_pixel(display, 51, 25, QP_RGB(255, 0, 0));      // Red
qp_draw_pixel(display, 52, 25, QP_MONO(true));          // White
```

### QGF Image Example

```c
// Convert image to QGF format using QMK CLI:
// qmk painter-convert-graphics -f rgb565 -i my_icon.png -o ./generated/

// Load and display QGF image
const uint8_t PROGMEM my_icon_qgf[] = { /* QGF data */ };
qp_image_t icon = QP_IMAGE_PROGMEM(my_icon_qgf);

if (qp_image_is_valid(&icon)) {
    qp_draw_image(display, &icon, 10, 8);
}

// Clean up when done
qp_free_image(&icon);
```

### Animation Example

```c
// Create animation sequence
qp_image_t frame1 = QP_IMAGE_PROGMEM(frame1_qgf);
qp_image_t frame2 = QP_IMAGE_PROGMEM(frame2_qgf);
qp_image_t frame3 = QP_IMAGE_PROGMEM(frame3_qgf);

QP_DEFINE_LOOP_SEQUENCE(my_animation, 100, frame1, frame2, frame3);

// Use with animator
qp_animator_t anim;
qp_animator_start(&anim, &my_animation, true, timer_read32());

// In your main loop:
if (qp_animator_is_active(&anim)) {
    qp_animator_step_and_draw(&anim, display, 0, 0, timer_read32());
}
```

### Declarative Widget Example

```c
// Define layer indicator widget
QP_DEFINE_WIDGET_STATES(layer_states,
    QP_STATE_FWD(layer0_seq),
    QP_STATE_FWD(layer1_seq),
    QP_STATE_FWD(layer2_seq),
    QP_STATE_FWD(layer3_seq)
);

static const qp_widget_config_t layer_config = 
    QP_WIDGET_CONFIG_OPAQUE(64, 0, 40, 16, layer_states, 4, qp_query_layer, NULL);

static qp_widget_t layer_widget;

// Initialize in keyboard_post_init_user()
qp_widget_init(&layer_widget, &layer_config, 0, timer_read32());

// Update in your display task
qp_widget_tick(&layer_widget, display, timer_read32());
```

## API Reference

### Core Drawing (`qp_utils.h`)

#### Device Management
- `bool qp_utils_init(painter_device_t device)` - Initialize QP utils for device
- `const qp_display_info_t* qp_get_display_info(painter_device_t device)` - Get display capabilities

#### Drawing Functions
- `bool qp_clear_rect(painter_device_t device, uint16_t x, uint16_t y, uint16_t w, uint16_t h)` - Clear area
- `bool qp_fill_rect(painter_device_t device, uint16_t x, uint16_t y, uint16_t w, uint16_t h, qp_color_t color)` - Fill area
- `bool qp_draw_pixel(painter_device_t device, uint16_t x, uint16_t y, qp_color_t color)` - Draw pixel
- `bool qp_draw_rect(painter_device_t device, uint16_t x, uint16_t y, uint16_t w, uint16_t h, qp_color_t color)` - Draw outline

#### Color Utilities
- `QP_HSV(h, s, v)` - Create HSV color
- `QP_RGB(r, g, b)` - Create RGB color
- `QP_MONO(on)` - Create monochrome color
- `QP_PALETTE(idx)` - Create palette color

### Image System (`qp_image.h`)

#### Image Loading
- `qp_image_t qp_load_image_mem(const void* qgf_data, uint32_t qgf_size)` - Load from PROGMEM
- `qp_image_t qp_load_image_flash(const void* qgf_data, uint32_t qgf_size)` - Load from flash
- `void qp_free_image(qp_image_t* image)` - Free image resources

#### Image Drawing
- `bool qp_draw_image(painter_device_t device, const qp_image_t* image, uint16_t x, uint16_t y)` - Draw image
- `bool qp_draw_image_tinted(painter_device_t device, const qp_image_t* image, uint16_t x, uint16_t y, qp_color_t tint)` - Draw with tint

#### Animation Sequences
- `qp_image_sequence_t qp_create_sequence(const qp_image_t* frames, uint8_t count, uint16_t duration_ms, bool loop)` - Create sequence
- `QP_DEFINE_SEQUENCE(name, duration_ms, loop_flag, ...)` - Define sequence macro

### Animation Engine (`qp_anim.h`)

#### Animator Control
- `void qp_animator_start(qp_animator_t* anim, const qp_image_sequence_t* sequence, bool forward, uint32_t now)` - Start animation
- `void qp_animator_stop(qp_animator_t* anim)` - Stop animation
- `void qp_animator_reverse(qp_animator_t* anim, uint32_t now)` - Reverse direction

#### Animation Stepping
- `qp_anim_result_t qp_animator_step(qp_animator_t* anim, uint32_t now)` - Step animation
- `qp_anim_result_t qp_animator_step_and_draw(qp_animator_t* anim, painter_device_t device, uint16_t x, uint16_t y, uint32_t now)` - Step and draw

### Animation Controllers (`qp_controllers.h`)

#### One-Shot Controller
- `void qp_oneshot_init(qp_oneshot_controller_t* controller, ...)` - Initialize one-shot
- `void qp_oneshot_trigger(qp_oneshot_controller_t* controller, uint32_t now)` - Trigger animation
- `void qp_oneshot_render(qp_oneshot_controller_t* controller, painter_device_t device, uint32_t now)` - Render

#### Toggle Controller
- `void qp_toggle_init(qp_toggle_controller_t* controller, ...)` - Initialize toggle
- `void qp_toggle_set(qp_toggle_controller_t* controller, bool state, uint32_t now)` - Set state
- `void qp_toggle_render(qp_toggle_controller_t* controller, painter_device_t device, uint32_t now)` - Render

### Declarative Widgets (`qp_declarative.h`)

#### Core Functions
- `void qp_widget_init(qp_widget_t* widget, const qp_widget_config_t* config, uint8_t initial_state, uint32_t now)` - Initialize widget
- `void qp_widget_tick(qp_widget_t* widget, painter_device_t device, uint32_t now)` - Update and render widget

#### Helper Macros
- `QP_STATE_FWD(seq)` - Create forward-entering state
- `QP_STATE_REV(seq)` - Create reverse-entering state
- `QP_WIDGET_CONFIG_OPAQUE(x, y, w, h, states, count, query, user_data)` - Opaque widget config

## Migration from OLED Utils

QP Utils provides a smooth migration path from the existing oled_utils module:

### Key Differences
- **Device Context**: All functions now require a `painter_device_t` parameter
- **Color Support**: Enhanced color handling for RGB and palette displays
- **QGF Images**: Replace page-packed slices with QGF image format
- **Coordinate System**: Use pixel coordinates instead of page-based addressing

### Migration Steps
1. Convert bitmap data to QGF format using QMK CLI tools
2. Update function calls to include device parameter
3. Replace slice macros with QGF image loading
4. Update color specifications to use new color system
5. Test on target display hardware

## Performance Characteristics

### Drawing Operations
- **Rectangle operations**: O(w×h) with hardware acceleration where available
- **Image drawing**: Optimized for QP's native formats and display capabilities
- **Color conversion**: Cached and optimized for each display type
- **Animation stepping**: O(1) with efficient frame management

### Memory Usage
- **Display info caching**: ~32 bytes per display
- **Image handles**: Managed by QP's native system
- **Animation state**: ~24 bytes per animator
- **Widget state**: ~48 bytes per declarative widget

### Optimization Tips
- Use QGF images in the display's native color format
- Align animations to display refresh rates
- Cache frequently used images
- Use opaque blending only when necessary
- Batch drawing operations when possible

## Troubleshooting

### Common Issues
- **Images not displaying**: Check QGF format matches display capabilities
- **Poor animation performance**: Verify frame duration and display refresh rate
- **Color issues**: Ensure color format matches display type
- **Memory errors**: Check image loading and cleanup

### Debug Options
Add to your `config.h` for debugging:
```c
#define QUANTUM_PAINTER_DEBUG
```

## Integration Examples

See the example keymaps for complete integration examples showing:
- Multi-display setups with different technologies
- Complex animation sequences and state machines
- Performance optimization techniques
- Color management across display types
