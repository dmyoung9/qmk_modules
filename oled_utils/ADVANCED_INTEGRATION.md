# Advanced Integration Examples

This document shows how to create sophisticated OLED displays by combining multiple modules and advanced features.

## Table of Contents

1. [Multi-Widget Dashboard](#multi-widget-dashboard)
2. [Animated Status Bar](#animated-status-bar)
3. [Context-Aware Display](#context-aware-display)
4. [Split Keyboard Integration](#split-keyboard-integration)
5. [Error Recovery System](#error-recovery-system)
6. [Performance-Optimized Display](#performance-optimized-display)

## Multi-Widget Dashboard

### Complete Gaming Keyboard Display

```c
#include "oled_utils.h"
#include "oled_unified_anim.h"
#include "oled_declarative.h"

// ============================================================================
// Animation Sequences
// ============================================================================

// Boot sequence
DEFINE_SLICE_SEQ(boot_seq,
    SLICE128x32(boot_0), SLICE128x32(boot_1), SLICE128x32(boot_2),
    SLICE128x32(boot_3), SLICE128x32(boot_4), SLICE128x32(boot_5)
);

// Layer indicators
DEFINE_SLICE_SEQ(layer_base, SLICE40x8(layer_base_0), SLICE40x8(layer_base_1), SLICE40x8(layer_base_2));
DEFINE_SLICE_SEQ(layer_game, SLICE40x8(layer_game_0), SLICE40x8(layer_game_1), SLICE40x8(layer_game_2));
DEFINE_SLICE_SEQ(layer_func, SLICE40x8(layer_func_0), SLICE40x8(layer_func_1), SLICE40x8(layer_func_2));

// Modifier animations
DEFINE_SLICE_SEQ(caps_seq, SLICE20x8(caps_off), SLICE20x8(caps_on));
DEFINE_SLICE_SEQ(shift_seq, SLICE20x8(shift_off), SLICE20x8(shift_on));
DEFINE_SLICE_SEQ(ctrl_seq, SLICE20x8(ctrl_off), SLICE20x8(ctrl_on));

// WPM indicator
DEFINE_SLICE_SEQ(wpm_seq, SLICE32x8(wpm_0), SLICE32x8(wpm_1), SLICE32x8(wpm_2), SLICE32x8(wpm_3));

// ============================================================================
// Widget Configurations
// ============================================================================

// Layer widget states
static const state_desc_t layer_states[] = {
    STATE_FWD(&layer_base),  // Base layer
    STATE_FWD(&layer_game),  // Gaming layer
    STATE_FWD(&layer_func),  // Function layer
};

// Layer query with debouncing
static uint8_t layer_query(uint32_t user_arg, uint8_t current_state, uint32_t now) {
    static uint32_t last_change = 0;
    static uint8_t last_layer = 0;
    
    uint8_t current_layer = get_highest_layer(layer_state);
    
    // Debounce rapid layer changes
    if (current_layer != last_layer) {
        if (now - last_change < 50) {  // 50ms debounce
            return current_state;
        }
        last_change = now;
        last_layer = current_layer;
    }
    
    return (current_layer < 3) ? current_layer : 0;
}

// WPM states based on typing speed
static const state_desc_t wpm_states[] = {
    STATE_FWD(&wpm_seq),  // All WPM ranges use same animation, different triggers
};

static uint8_t wpm_query(uint32_t user_arg, uint8_t current_state, uint32_t now) {
    uint8_t wpm = get_current_wpm();
    
    // Animate based on WPM ranges
    if (wpm > 60) return 3;      // Fast typing
    if (wpm > 30) return 2;      // Medium typing  
    if (wpm > 10) return 1;      // Slow typing
    return 0;                    // No typing
}

// ============================================================================
// Runtime Instances
// ============================================================================

// Boot animation
static const unified_anim_config_t boot_config = 
    UNIFIED_ONESHOT_CONFIG(&boot_seq, 0, 0, STEADY_LAST, true);
static unified_anim_t boot_anim;

// Layer widget
static const widget_config_t layer_config = 
    WIDGET_CONFIG(64, 0, 40, 8, layer_states, 3, layer_query, 0, 0);
static widget_t layer_widget;

// Modifier toggles
static const unified_anim_config_t caps_config = 
    UNIFIED_TOGGLE_CONFIG(&caps_seq, 10, 8, BLEND_OPAQUE);
static const unified_anim_config_t shift_config = 
    UNIFIED_TOGGLE_CONFIG(&shift_seq, 35, 8, BLEND_OPAQUE);
static const unified_anim_config_t ctrl_config = 
    UNIFIED_TOGGLE_CONFIG(&ctrl_seq, 60, 8, BLEND_OPAQUE);

static unified_anim_t caps_anim, shift_anim, ctrl_anim;

// WPM indicator
static const widget_config_t wpm_config = WIDGET_CONFIG_ADVANCED(
    88, 8, 32, 8,           // Position and size
    BLIT_ADDITIVE,          // Additive blending for overlay effect
    wpm_states, 1,          // Single state with animation
    wpm_query, 0, 0,        // Query function
    NULL, NULL,             // No validation/error handling
    100,                    // Update every 100ms
    false, 0                // No auto-recovery needed
);
static widget_t wpm_widget;

// ============================================================================
// Display Management
// ============================================================================

void init_gaming_dashboard(void) {
    uint32_t now = timer_read32();
    
    // Initialize boot animation
    unified_anim_init(&boot_anim, &boot_config, 0, now);
    
    // Initialize widgets (will be shown after boot)
    widget_init(&layer_widget, &layer_config, get_highest_layer(layer_state), now);
    widget_init(&wpm_widget, &wpm_config, 0, now);
    
    // Initialize modifier animations
    unified_anim_init(&caps_anim, &caps_config, 0, now);
    unified_anim_init(&shift_anim, &shift_config, 0, now);
    unified_anim_init(&ctrl_anim, &ctrl_config, 0, now);
}

void update_gaming_dashboard(void) {
    uint32_t now = timer_read32();
    
    // Show boot animation first
    if (!unified_anim_boot_done(&boot_anim)) {
        unified_anim_render(&boot_anim, now);
        return;
    }
    
    // Update main widgets
    widget_tick(&layer_widget, now);
    widget_tick(&wpm_widget, now);
    
    // Update modifier states
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
    draw_slice_px(&logo_slice, 0, 24);
    draw_slice_px(&separator_slice, 0, 16);
}
```

## Animated Status Bar

### Dynamic Status Indicators

```c
// ============================================================================
// Status Bar with Multiple Animated Elements
// ============================================================================

// Battery level animation (for wireless keyboards)
DEFINE_SLICE_SEQ(battery_seq,
    SLICE16x8(battery_empty), SLICE16x8(battery_low),
    SLICE16x8(battery_med), SLICE16x8(battery_full)
);

// Connection status
DEFINE_SLICE_SEQ(connection_seq,
    SLICE12x8(conn_disconnected), SLICE12x8(conn_connecting),
    SLICE12x8(conn_connected), SLICE12x8(conn_strong)
);

// Time-based background animation
DEFINE_SLICE_SEQ(background_seq,
    SLICE128x8(bg_morning), SLICE128x8(bg_day),
    SLICE128x8(bg_evening), SLICE128x8(bg_night)
);

// Status bar query functions
static uint8_t battery_query(uint32_t user_arg, uint8_t current_state, uint32_t now) {
    // Simulate battery level (replace with actual battery reading)
    static uint32_t last_update = 0;
    static uint8_t battery_level = 100;
    
    if (now - last_update > 30000) {  // Update every 30 seconds
        // battery_level = read_battery_level();  // Your battery reading function
        last_update = now;
    }
    
    if (battery_level > 75) return 3;      // Full
    if (battery_level > 50) return 2;      // Medium
    if (battery_level > 25) return 1;      // Low
    return 0;                              // Empty
}

static uint8_t connection_query(uint32_t user_arg, uint8_t current_state, uint32_t now) {
    // Simulate connection strength
    if (is_keyboard_connected()) {
        return 3;  // Strong connection
    }
    return 0;  // Disconnected
}

static uint8_t background_query(uint32_t user_arg, uint8_t current_state, uint32_t now) {
    // Time-based background (simplified)
    uint32_t hour = (now / 3600000) % 24;  // Rough hour calculation
    
    if (hour >= 6 && hour < 12) return 0;   // Morning
    if (hour >= 12 && hour < 18) return 1;  // Day
    if (hour >= 18 && hour < 22) return 2;  // Evening
    return 3;                               // Night
}

// Status bar configurations
static const state_desc_t battery_states[] = { STATE_FWD(&battery_seq) };
static const state_desc_t connection_states[] = { STATE_FWD(&connection_seq) };
static const state_desc_t background_states[] = { STATE_FWD(&background_seq) };

static const widget_config_t battery_config = WIDGET_CONFIG_ADVANCED(
    100, 0, 16, 8, BLIT_ADDITIVE, battery_states, 1, battery_query, 0, 0,
    NULL, NULL, 5000, true, 3  // Update every 5 seconds
);

static const widget_config_t connection_config = WIDGET_CONFIG_ADVANCED(
    112, 0, 12, 8, BLIT_ADDITIVE, connection_states, 1, connection_query, 0, 0,
    NULL, NULL, 1000, true, 3  // Update every second
);

static const widget_config_t background_config = WIDGET_CONFIG_ADVANCED(
    0, 24, 128, 8, BLIT_OPAQUE, background_states, 1, background_query, 0, 0,
    NULL, NULL, 60000, false, 0  // Update every minute
);

static widget_t battery_widget, connection_widget, background_widget;

void init_status_bar(void) {
    uint32_t now = timer_read32();
    
    widget_init(&battery_widget, &battery_config, 0, now);
    widget_init(&connection_widget, &connection_config, 0, now);
    widget_init(&background_widget, &background_config, 0, now);
}

void update_status_bar(void) {
    uint32_t now = timer_read32();
    
    // Update background first (opaque)
    widget_tick(&background_widget, now);
    
    // Update status indicators (additive)
    widget_tick(&battery_widget, now);
    widget_tick(&connection_widget, now);
}
```

## Context-Aware Display

### Adaptive Interface Based on Usage

```c
// ============================================================================
// Context-Aware Display System
// ============================================================================

typedef enum {
    CONTEXT_TYPING,
    CONTEXT_GAMING,
    CONTEXT_CODING,
    CONTEXT_IDLE
} display_context_t;

static display_context_t current_context = CONTEXT_IDLE;
static uint32_t context_change_time = 0;

// Context detection
static display_context_t detect_context(void) {
    uint8_t current_layer = get_highest_layer(layer_state);
    uint8_t wpm = get_current_wpm();
    uint32_t now = timer_read32();
    static uint32_t last_keypress = 0;
    
    // Update last keypress time (call this from process_record_user)
    // last_keypress = now;
    
    // Gaming layer detection
    if (current_layer == 1) {  // Gaming layer
        return CONTEXT_GAMING;
    }
    
    // High WPM = typing
    if (wpm > 40) {
        return CONTEXT_TYPING;
    }
    
    // Coding detection (simplified - could check for specific key patterns)
    if (current_layer == 2 && wpm > 20) {  // Function layer with moderate WPM
        return CONTEXT_CODING;
    }
    
    // Idle detection
    if (now - last_keypress > 30000) {  // 30 seconds of inactivity
        return CONTEXT_IDLE;
    }
    
    return current_context;  // No change
}

// Context-specific widget configurations
static void switch_to_context(display_context_t new_context) {
    if (new_context == current_context) return;
    
    uint32_t now = timer_read32();
    current_context = new_context;
    context_change_time = now;
    
    // Trigger context transition animation
    unified_anim_trigger(&context_transition_anim, new_context, now);
    
    // Reconfigure widgets based on context
    switch (new_context) {
        case CONTEXT_TYPING:
            // Show WPM, hide gaming stats
            widget_force_state(&wpm_widget, 1, now);  // Show WPM
            widget_force_state(&gaming_widget, 0, now);  // Hide gaming
            break;
            
        case CONTEXT_GAMING:
            // Show gaming stats, hide WPM
            widget_force_state(&wpm_widget, 0, now);  // Hide WPM
            widget_force_state(&gaming_widget, 1, now);  // Show gaming
            break;
            
        case CONTEXT_CODING:
            // Show layer info prominently
            widget_force_state(&layer_widget, 1, now);  // Highlight layers
            break;
            
        case CONTEXT_IDLE:
            // Show clock or screensaver
            widget_force_state(&clock_widget, 1, now);
            break;
    }
}

void update_context_aware_display(void) {
    uint32_t now = timer_read32();
    
    // Detect context changes
    display_context_t detected = detect_context();
    if (detected != current_context) {
        // Add hysteresis to prevent rapid switching
        static uint32_t context_stable_time = 0;
        static display_context_t pending_context = CONTEXT_IDLE;
        
        if (detected != pending_context) {
            pending_context = detected;
            context_stable_time = now;
        } else if (now - context_stable_time > 2000) {  // 2 second delay
            switch_to_context(detected);
        }
    }
    
    // Update all widgets
    widget_tick(&layer_widget, now);
    widget_tick(&wpm_widget, now);
    widget_tick(&gaming_widget, now);
    widget_tick(&clock_widget, now);
    
    // Update context transition animation
    unified_anim_render(&context_transition_anim, now);
}
```

## Split Keyboard Integration

### Master-Slave Coordination

```c
// ============================================================================
// Split Keyboard OLED Coordination
// ============================================================================

// Shared data structure for split sync
typedef struct {
    uint8_t current_layer;
    uint8_t wpm;
    uint8_t modifier_state;
    bool caps_active;
    uint32_t last_keypress;
} split_oled_data_t;

static split_oled_data_t oled_data = {0};

// Master side: collect and send data
void master_oled_update(void) {
    uint32_t now = timer_read32();

    // Collect data
    oled_data.current_layer = get_highest_layer(layer_state);
    oled_data.wpm = get_current_wpm();
    oled_data.modifier_state = get_mods() | get_oneshot_mods();
    oled_data.caps_active = host_keyboard_led_state().caps_lock || is_caps_word_on();

    // Send to slave (implement your split sync mechanism)
    // split_sync_send(&oled_data, sizeof(oled_data));

    // Update master display
    update_master_widgets(now);
}

// Slave side: receive and display data
void slave_oled_update(void) {
    uint32_t now = timer_read32();

    // Receive from master
    // split_sync_receive(&oled_data, sizeof(oled_data));

    // Update slave display with received data
    update_slave_widgets(now);
}

// Master widgets: full interface
void update_master_widgets(uint32_t now) {
    // Layer indicator
    widget_tick(&layer_widget, now);

    // Modifier indicators
    unified_anim_trigger(&caps_anim, oled_data.caps_active ? 1 : 0, now);
    unified_anim_trigger(&shift_anim, (oled_data.modifier_state & MOD_MASK_SHIFT) ? 1 : 0, now);
    unified_anim_render(&caps_anim, now);
    unified_anim_render(&shift_anim, now);

    // Static elements
    draw_slice_px(&master_logo, 0, 24);
}

// Slave widgets: complementary display
void update_slave_widgets(uint32_t now) {
    // WPM display
    widget_tick(&wpm_widget, now);

    // Connection status
    widget_tick(&connection_widget, now);

    // Decorative elements
    draw_slice_px(&slave_decoration, 0, 0);

    // Mirror some master state
    if (oled_data.caps_active) {
        draw_slice_px(&caps_indicator, 100, 24);
    }
}

bool oled_task_user(void) {
    if (is_keyboard_master()) {
        master_oled_update();
    } else {
        slave_oled_update();
    }
    return false;
}
```

## Error Recovery System

### Robust Error Handling

```c
// ============================================================================
// Comprehensive Error Recovery System
// ============================================================================

// Error tracking
typedef struct {
    uint32_t widget_errors;
    uint32_t animation_errors;
    uint32_t last_recovery;
    bool recovery_mode;
} error_tracker_t;

static error_tracker_t error_tracker = {0};

// Widget error handler
static void widget_error_handler(const widget_config_t *cfg, uint8_t error_code, uint32_t context) {
    error_tracker.widget_errors++;

    switch (error_code) {
        case WIDGET_ERROR_STUCK_ANIMATION:
            // Force widget reset
            widget_reset((widget_t*)context, timer_read32());
            break;

        case WIDGET_ERROR_INVALID_STATE:
            // Reset to safe state
            widget_force_state((widget_t*)context, 0, timer_read32());
            break;

        case WIDGET_ERROR_QUERY_FAILED:
            // Disable widget temporarily
            error_tracker.recovery_mode = true;
            error_tracker.last_recovery = timer_read32();
            break;
    }
}

// System-wide error recovery
void perform_error_recovery(void) {
    uint32_t now = timer_read32();

    // Check if recovery is needed
    if (error_tracker.widget_errors > 5 || error_tracker.animation_errors > 3) {
        // Full system reset
        init_all_widgets();
        error_tracker.widget_errors = 0;
        error_tracker.animation_errors = 0;
        error_tracker.recovery_mode = true;
        error_tracker.last_recovery = now;
    }

    // Exit recovery mode after delay
    if (error_tracker.recovery_mode && (now - error_tracker.last_recovery > 5000)) {
        error_tracker.recovery_mode = false;
    }
}

// Safe widget update with error handling
void safe_widget_update(widget_t *widget, uint32_t now) {
    if (error_tracker.recovery_mode) {
        // Minimal updates during recovery
        if (widget_has_error(widget)) {
            widget_reset(widget, now);
        }
        return;
    }

    // Normal update with error checking
    widget_tick(widget, now);

    // Check for new errors
    if (widget_has_error(widget)) {
        widget_error_handler(widget->cfg, widget_get_error(widget), (uint32_t)widget);
    }
}

// Watchdog system
void oled_watchdog_task(void) {
    static uint32_t last_update = 0;
    uint32_t now = timer_read32();

    // Check for system freeze
    if (now - last_update > 10000) {  // 10 second timeout
        // System appears frozen, force recovery
        perform_error_recovery();
    }

    last_update = now;
}
```

## Performance-Optimized Display

### High-Performance Multi-Widget System

```c
// ============================================================================
// Performance-Optimized Display System
// ============================================================================

// Priority-based update system
typedef enum {
    PRIORITY_CRITICAL = 0,  // Every frame
    PRIORITY_HIGH = 1,      // Every 2-3 frames
    PRIORITY_MEDIUM = 2,    // Every 5-10 frames
    PRIORITY_LOW = 3        // Every 20+ frames
} update_priority_t;

typedef struct {
    widget_t *widget;
    unified_anim_t *anim;
    update_priority_t priority;
    uint32_t last_update;
    uint16_t interval_ms;
} managed_widget_t;

// Widget registry
static managed_widget_t widget_registry[] = {
    {&layer_widget, NULL, PRIORITY_CRITICAL, 0, 16},      // 60 FPS
    {&caps_widget, &caps_anim, PRIORITY_HIGH, 0, 33},     // 30 FPS
    {&wpm_widget, NULL, PRIORITY_MEDIUM, 0, 100},         // 10 FPS
    {&status_widget, NULL, PRIORITY_LOW, 0, 200},         // 5 FPS
};

#define WIDGET_COUNT (sizeof(widget_registry) / sizeof(managed_widget_t))

// Optimized update loop
void optimized_oled_update(void) {
    uint32_t now = timer_read32();
    static uint8_t frame_counter = 0;

    frame_counter++;

    for (uint8_t i = 0; i < WIDGET_COUNT; i++) {
        managed_widget_t *managed = &widget_registry[i];

        // Check if it's time to update this widget
        if (now - managed->last_update >= managed->interval_ms) {
            // Update widget
            if (managed->widget) {
                widget_tick(managed->widget, now);
            }

            // Update animation
            if (managed->anim) {
                unified_anim_render(managed->anim, now);
            }

            managed->last_update = now;
        }
    }

    // Update critical elements every frame
    if (frame_counter % 1 == 0) {
        // Critical updates
        widget_tick(&layer_widget, now);
    }

    // Update high priority elements every few frames
    if (frame_counter % 2 == 0) {
        unified_anim_render(&caps_anim, now);
    }

    // Update medium priority elements less frequently
    if (frame_counter % 5 == 0) {
        widget_tick(&wpm_widget, now);
    }

    // Update low priority elements rarely
    if (frame_counter % 20 == 0) {
        widget_tick(&background_widget, now);
    }
}

// Dynamic priority adjustment
void adjust_widget_priority(uint8_t widget_index, update_priority_t new_priority) {
    if (widget_index >= WIDGET_COUNT) return;

    managed_widget_t *managed = &widget_registry[widget_index];
    managed->priority = new_priority;

    // Adjust interval based on priority
    switch (new_priority) {
        case PRIORITY_CRITICAL: managed->interval_ms = 16; break;   // 60 FPS
        case PRIORITY_HIGH:     managed->interval_ms = 33; break;   // 30 FPS
        case PRIORITY_MEDIUM:   managed->interval_ms = 100; break;  // 10 FPS
        case PRIORITY_LOW:      managed->interval_ms = 200; break;  // 5 FPS
    }
}

// Adaptive performance scaling
void adaptive_performance_scaling(void) {
    static uint32_t last_performance_check = 0;
    static uint32_t frame_start_time = 0;
    uint32_t now = timer_read32();

    // Measure frame time
    if (frame_start_time > 0) {
        uint32_t frame_time = now - frame_start_time;

        // If frame time is too high, reduce update frequencies
        if (frame_time > 20) {  // >20ms per frame (< 50 FPS)
            // Reduce non-critical widget priorities
            for (uint8_t i = 0; i < WIDGET_COUNT; i++) {
                if (widget_registry[i].priority > PRIORITY_CRITICAL) {
                    widget_registry[i].interval_ms += 10;  // Slow down updates
                }
            }
        }
        // If frame time is good, we can increase frequencies
        else if (frame_time < 10) {  // <10ms per frame (> 100 FPS)
            for (uint8_t i = 0; i < WIDGET_COUNT; i++) {
                if (widget_registry[i].interval_ms > 16) {
                    widget_registry[i].interval_ms -= 5;  // Speed up updates
                }
            }
        }
    }

    frame_start_time = now;
}

// Main optimized OLED task
bool optimized_oled_task_user(void) {
    adaptive_performance_scaling();
    optimized_oled_update();
    oled_watchdog_task();
    perform_error_recovery();

    return false;
}
```

## Integration Best Practices

### Key Principles

1. **Modular Design**: Keep widgets independent and reusable
2. **Performance Awareness**: Use appropriate update frequencies
3. **Error Resilience**: Implement comprehensive error handling
4. **Context Sensitivity**: Adapt display based on usage patterns
5. **Resource Management**: Balance features with memory constraints

### Common Patterns

- **Boot → Dashboard**: Start with boot animation, transition to functional display
- **Priority-Based Updates**: Update critical widgets more frequently
- **Context Switching**: Change display based on keyboard state
- **Error Recovery**: Graceful degradation and automatic recovery
- **Split Coordination**: Synchronize displays across keyboard halves

These examples demonstrate how to create sophisticated, responsive OLED displays that enhance the user experience while maintaining good performance and reliability.
```
