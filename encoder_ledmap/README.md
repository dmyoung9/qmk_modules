# Encoder LED Map Module

The `encoder_ledmap` module provides visual LED feedback for rotary encoder interactions in QMK keyboards. When an encoder is rotated, the module illuminates designated LEDs with configurable colors based on the rotation direction, active layer, and encoder position.

## Features

- **Visual Feedback**: LEDs light up when encoders are rotated, providing immediate visual confirmation
- **Direction-Aware**: Different colors for clockwise and counter-clockwise rotation
- **Layer-Aware**: Different color schemes per layer
- **Multi-Encoder Support**: Supports multiple encoders with individual LED mappings
- **Timeout Control**: LEDs automatically turn off after a configurable timeout period
- **Split Keyboard Support**: Automatic synchronization between keyboard halves
- **Color Type Support**: RGB, HSV, and HUE color formats via the indicators module

## Requirements

### Mandatory Dependencies

- **RGB Matrix**: The keyboard must have `RGB_MATRIX_ENABLE = yes` in its configuration
- **Encoder Map**: The keyboard must have `ENCODER_MAP_ENABLE = yes` 
- **elpekenin/indicators Module**: This module is a mandatory dependency for color type support

### QMK Module Configuration

Add the following to your `qmk_module.json`:

```json
{
    "dependencies": [
        "elpekenin/indicators",
        "dmyoung9/encoder_ledmap"
    ]
}
```

**Important**: The `elpekenin/indicators` module must be enabled before the `encoder_ledmap` module to avoid overwriting indicator functionality.

## Configuration Options

### Timeout Configuration

```c
#define ENCODER_LED_TIMEOUT 500  // Default: 500ms
```

Controls how long LEDs remain illuminated after encoder activity. Set to the desired timeout in milliseconds.

## Setup Instructions

### 1. Enable Required Features

In your keyboard's `rules.mk`:

```make
RGB_MATRIX_ENABLE = yes
ENCODER_MAP_ENABLE = yes
```

### 2. Include the Header

In your `keymap.c`:

```c
#include "dmyoung9/encoder_ledmap.h"
```

### 3. Define LED Mappings

Create an array mapping each encoder to its corresponding LED index:

```c
const uint8_t encoder_leds[NUM_ENCODERS] = {
    0,  // Encoder 0 controls LED 0
    5,  // Encoder 1 controls LED 5
    // Add more mappings as needed
};
```

### 4. Define Color Mappings

Create the encoder ledmap array defining colors for each layer, encoder, and direction:

```c
const color_t PROGMEM encoder_ledmap[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    // Layer 0
    [0] = {
        {RGB_COLOR(RGB_RED), RGB_COLOR(RGB_GREEN)},    // Encoder 0: Red CCW, Green CW
        {RGB_COLOR(RGB_BLUE), RGB_COLOR(RGB_YELLOW)},  // Encoder 1: Blue CCW, Yellow CW
    },
    // Layer 1
    [1] = {
        {RGB_COLOR(RGB_PURPLE), RGB_COLOR(RGB_CYAN)},  // Encoder 0: Purple CCW, Cyan CW
        {RGB_COLOR(RGB_WHITE), RGB_COLOR(RGB_ORANGE)}, // Encoder 1: White CCW, Orange CW
    },
    // Add more layers as needed
};
```

## Usage Examples

### Basic Single Encoder Setup

```c
#include "dmyoung9/encoder_ledmap.h"

// Map encoder 0 to LED 10
const uint8_t encoder_leds[NUM_ENCODERS] = {10};

// Define colors for 2 layers
const color_t PROGMEM encoder_ledmap[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    [0] = {{RGB_COLOR(RGB_RED), RGB_COLOR(RGB_GREEN)}},     // Layer 0: Red CCW, Green CW
    [1] = {{RGB_COLOR(RGB_BLUE), RGB_COLOR(RGB_YELLOW)}},   // Layer 1: Blue CCW, Yellow CW
};
```

### Multi-Encoder Setup

```c
// Map 3 encoders to different LEDs
const uint8_t encoder_leds[NUM_ENCODERS] = {0, 15, 30};

const color_t PROGMEM encoder_ledmap[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    [0] = {
        {RGB_COLOR(RGB_RED), RGB_COLOR(RGB_GREEN)},      // Encoder 0
        {RGB_COLOR(RGB_BLUE), RGB_COLOR(RGB_YELLOW)},    // Encoder 1  
        {RGB_COLOR(RGB_PURPLE), RGB_COLOR(RGB_CYAN)},    // Encoder 2
    },
    [1] = {
        {RGB_COLOR(RGB_WHITE), RGB_COLOR(RGB_ORANGE)},   // Encoder 0 on layer 1
        {RGB_COLOR(RGB_PINK), RGB_COLOR(RGB_MAGENTA)},   // Encoder 1 on layer 1
        {RGB_COLOR(RGB_TEAL), RGB_COLOR(RGB_CORAL)},     // Encoder 2 on layer 1
    },
};
```

### Using Different Color Types

```c
const color_t PROGMEM encoder_ledmap[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    [0] = {
        // RGB colors
        {RGB_COLOR(RGB_RED), RGB_COLOR(RGB_GREEN)},
        // HSV colors  
        {HSV_COLOR(HSV_BLUE), HSV_COLOR(HSV_YELLOW)},
        // HUE-only colors (uses current saturation/value)
        {HUE(HUE_PURPLE), HUE(HUE_CYAN)},
    },
};
```

## Array Structure

The `encoder_ledmap` array has the following structure:

```c
encoder_ledmap[layer][encoder_index][direction]
```

Where:
- **layer**: Layer index (0 to NUM_KEYMAP_LAYERS-1)
- **encoder_index**: Encoder index (0 to NUM_ENCODERS-1)  
- **direction**: Rotation direction (0 = counter-clockwise, 1 = clockwise)

The array must have the same number of layers as your keymap layers. This is enforced by a compile-time assertion.

## Split Keyboard Support

The module automatically handles split keyboards:

- **Master Side**: Tracks encoder state and sends updates to slave
- **Slave Side**: Receives encoder state updates and displays LEDs accordingly
- **Synchronization**: Uses QMK's transaction system for reliable communication

No additional configuration is required for split keyboard support.

## Color Types

The module supports multiple color formats through the `elpekenin/indicators` module:

- **RGB**: Direct red, green, blue values
- **HSV**: Hue, saturation, value format
- **HUE**: Hue-only (uses current RGB matrix saturation/value settings)

## Limitations

1. **RGB Matrix Required**: The keyboard must support RGB matrix functionality
2. **Layer Count**: The encoder ledmap must have the same number of layers as the keymap
3. **Memory Usage**: Large encoder ledmaps consume PROGMEM space
4. **Dependency Order**: The `elpekenin/indicators` module must be loaded before this module
5. **No Transparency Support**: The `encoder_ledmap` array requires explicit color definitions for every position (layer, encoder, direction combination). Unlike QMK's keymap system which supports transparent keys that fall through to lower layers, the encoder ledmap does not support transparency or fallback behavior - each array position must contain a valid color definition. If a position is left undefined or contains an invalid color, the LED may not illuminate or may display unexpected colors.

## Troubleshooting

### LEDs Don't Light Up
- Verify RGB matrix is enabled and working
- Check that `encoder_leds` array maps to valid LED indices
- Ensure the `elpekenin/indicators` module is properly enabled

### Wrong Colors
- Verify the `encoder_ledmap` array structure matches your layer count
- Check that color definitions use the correct format (RGB, HSV, or HUE)
- Ensure layer indices in the array match your keymap layers

### Split Keyboard Issues
- Verify both halves have the module enabled
- Check that the master side is properly detected
- Ensure communication between halves is working

## License

This module is part of the QMK userspace and follows the same licensing terms.
