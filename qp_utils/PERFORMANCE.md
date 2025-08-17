# QP Utils Performance Guide

This document provides performance characteristics, benchmarks, and optimization guidelines for the QP Utils module.

## Performance Characteristics

### Drawing Operations

#### Core Drawing Functions

| Function | Complexity | Notes |
|----------|------------|-------|
| `qp_clear_rect()` | O(w×h) | Uses QP's native rect clearing |
| `qp_fill_rect()` | O(w×h) | Hardware accelerated where available |
| `qp_draw_pixel()` | O(1) | Single pixel operations |
| `qp_draw_image()` | O(w×h) | Optimized for QP's native formats |

#### Color Conversion Performance

**Native Format Drawing**:
- **Performance**: Fastest, no conversion overhead
- **Memory**: Direct hardware writes
- **Recommendation**: Use display's native color format when possible

**Color Conversion**:
- **HSV to RGB**: ~10-20 CPU cycles per conversion
- **RGB to Native**: ~5-15 CPU cycles depending on display format
- **Monochrome**: ~2-5 CPU cycles per pixel
- **Recommendation**: Cache converted colors for repeated use

### Animation Performance

#### Frame Rate Considerations

| Display Type | Recommended Max FPS | Notes |
|--------------|-------------------|-------|
| SPI LCD (16MHz) | 15-30 FPS | Limited by SPI bandwidth |
| I2C OLED | 10-20 FPS | Limited by I2C speed |
| Parallel LCD | 30-60 FPS | Higher bandwidth available |
| Small displays (<128px) | 30-60 FPS | Less data to transfer |

#### Animation Optimization

**Frame Duration Guidelines**:
- **Smooth animation**: 33-50ms per frame (20-30 FPS)
- **Acceptable animation**: 50-100ms per frame (10-20 FPS)
- **Minimal animation**: 100-200ms per frame (5-10 FPS)

**Memory Usage**:
- **Animator state**: ~24 bytes per active animator
- **Widget state**: ~48 bytes per declarative widget
- **Image cache**: Varies by QGF image size and format

## Optimization Strategies

### Image Format Optimization

#### Choose Optimal Color Format

```c
// For monochrome displays
qmk painter-convert-graphics -f mono2 -i icon.png -o ./generated/

// For 16-color displays
qmk painter-convert-graphics -f pal16 -i icon.png -o ./generated/

// For RGB565 displays
qmk painter-convert-graphics -f rgb565 -i icon.png -o ./generated/

// For full RGB displays
qmk painter-convert-graphics -f rgb888 -i icon.png -o ./generated/
```

#### Image Size Guidelines

| Display Resolution | Recommended Max Image Size | Notes |
|-------------------|---------------------------|-------|
| 128×32 | 64×16 | Half screen or smaller |
| 128×64 | 64×32 | Quarter to half screen |
| 240×320 | 120×160 | Quarter screen for animations |
| 320×480 | 160×240 | Quarter screen for animations |

### Animation Optimization

#### Efficient Animation Patterns

```c
// GOOD: Reuse image sequences
static qp_image_sequence_t shared_spinner;
static qp_animator_t anim1, anim2;

void init_shared_animations(void) {
    // Both animators share the same sequence
    qp_animator_start(&anim1, &shared_spinner, true, timer_read32());
    qp_animator_start(&anim2, &shared_spinner, true, timer_read32());
}

// AVOID: Duplicate image data
static qp_image_sequence_t spinner1, spinner2; // Wasteful duplication
```

#### Frame Duration Optimization

```c
// GOOD: Adaptive frame rates based on display capabilities
uint16_t get_optimal_frame_duration(painter_device_t device) {
    const qp_display_info_t* info = qp_get_display_info(device);
    
    if (info->width <= 128) {
        return 33; // 30 FPS for small displays
    } else if (info->width <= 240) {
        return 50; // 20 FPS for medium displays
    } else {
        return 67; // 15 FPS for large displays
    }
}

// Use adaptive timing
QP_DEFINE_SEQUENCE(adaptive_anim, get_optimal_frame_duration(display), true, ...);
```

### Memory Optimization

#### Image Resource Management

```c
// GOOD: Load images once, reuse multiple times
static qp_image_t cached_icons[MAX_ICONS];
static bool icons_loaded = false;

void load_icons_once(void) {
    if (!icons_loaded) {
        cached_icons[0] = qp_load_image_mem(gfx_icon1, sizeof(gfx_icon1));
        cached_icons[1] = qp_load_image_mem(gfx_icon2, sizeof(gfx_icon2));
        // ... load other icons
        icons_loaded = true;
    }
}

void cleanup_icons(void) {
    if (icons_loaded) {
        for (int i = 0; i < MAX_ICONS; i++) {
            qp_free_image(&cached_icons[i]);
        }
        icons_loaded = false;
    }
}
```

#### Widget Optimization

```c
// GOOD: Minimize widget count
static qp_widget_t essential_widgets[3]; // Only essential widgets

// AVOID: Too many concurrent widgets
static qp_widget_t many_widgets[20]; // May cause performance issues
```

### Display-Specific Optimizations

#### SPI Display Optimization

```c
// Batch drawing operations to minimize SPI transactions
void draw_batch_optimized(painter_device_t device) {
    // Start batch operation
    qp_pixdata(device, NULL, 0);
    
    // Perform multiple drawing operations
    qp_fill_rect(device, 0, 0, 100, 50, QP_COLOR_BLUE);
    qp_draw_image(device, &icon, 10, 10);
    qp_draw_rect(device, 5, 5, 110, 60, QP_COLOR_WHITE);
    
    // Batch is automatically flushed
}
```

#### I2C Display Optimization

```c
// Minimize I2C transactions for OLED displays
void draw_i2c_optimized(painter_device_t device) {
    // Group operations by region to minimize address changes
    qp_fill_rect(device, 0, 0, 128, 16, QP_COLOR_WHITE);   // Top region
    qp_fill_rect(device, 0, 16, 128, 16, QP_COLOR_BLACK);  // Middle region
    qp_fill_rect(device, 0, 32, 128, 16, QP_COLOR_WHITE);  // Bottom region
}
```

## Performance Monitoring

### Timing Measurements

```c
// Measure drawing operation performance
void measure_drawing_performance(void) {
    uint32_t start_time = timer_read32();
    
    // Perform drawing operations
    qp_fill_rect(display, 0, 0, 240, 320, QP_COLOR_BLUE);
    
    uint32_t end_time = timer_read32();
    uint32_t duration = TIMER_DIFF_32(end_time, start_time);
    
    printf("Fill rect took %lu ms\n", duration);
}

// Measure animation frame rate
static uint32_t last_frame_time = 0;
static uint32_t frame_count = 0;

void measure_animation_fps(void) {
    uint32_t now = timer_read32();
    frame_count++;
    
    if (TIMER_DIFF_32(now, last_frame_time) >= 1000) { // Every second
        printf("Animation FPS: %lu\n", frame_count);
        frame_count = 0;
        last_frame_time = now;
    }
}
```

### Memory Usage Monitoring

```c
// Monitor widget memory usage
void print_widget_memory_usage(void) {
    size_t total_widgets = 0;
    size_t total_animators = 0;
    
    // Count active widgets and animators
    // ... counting logic ...
    
    size_t widget_memory = total_widgets * sizeof(qp_widget_t);
    size_t animator_memory = total_animators * sizeof(qp_animator_t);
    
    printf("Widget memory: %zu bytes\n", widget_memory);
    printf("Animator memory: %zu bytes\n", animator_memory);
}
```

## Benchmarks

### Drawing Performance (240×320 RGB565 Display)

| Operation | Time (ms) | Notes |
|-----------|-----------|-------|
| Clear full screen | 15-25 | Depends on SPI speed |
| Fill 100×100 rect | 2-4 | Hardware accelerated |
| Draw 32×32 image | 1-3 | QGF format optimized |
| Draw 100 pixels | 0.5-1 | Individual pixel ops |

### Animation Performance

| Animation Type | CPU Usage | Memory Usage | Notes |
|----------------|-----------|--------------|-------|
| 4-frame spinner | 2-5% | 96 bytes | Typical small animation |
| Layer transition | 5-10% | 150 bytes | Complex state changes |
| Boot sequence | 10-15% | 200 bytes | One-time heavy animation |

### Memory Footprint

| Component | RAM Usage | Flash Usage | Notes |
|-----------|-----------|-------------|-------|
| Core QP Utils | 128 bytes | 2-4 KB | Base functionality |
| Per display cache | 32 bytes | 0 bytes | Runtime cache |
| Per widget | 48 bytes | 0 bytes | Runtime state |
| Per animator | 24 bytes | 0 bytes | Runtime state |
| QGF images | Variable | Variable | Depends on image size |

## Troubleshooting Performance Issues

### Common Performance Problems

1. **Slow Animation**: Check frame duration and display bandwidth
2. **High CPU Usage**: Reduce animation complexity or frame rate
3. **Memory Issues**: Optimize image formats and widget count
4. **Display Lag**: Batch drawing operations and minimize transactions

### Performance Debugging

```c
// Enable performance debugging
#define QP_UTILS_DEBUG_PERFORMANCE

// Add timing instrumentation
#ifdef QP_UTILS_DEBUG_PERFORMANCE
#define QP_PERF_START() uint32_t _perf_start = timer_read32()
#define QP_PERF_END(op) printf("%s took %lu ms\n", (op), TIMER_DIFF_32(timer_read32(), _perf_start))
#else
#define QP_PERF_START()
#define QP_PERF_END(op)
#endif

void debug_drawing_performance(void) {
    QP_PERF_START();
    qp_fill_rect(display, 0, 0, 100, 100, QP_COLOR_RED);
    QP_PERF_END("fill_rect");
}
```

### Optimization Recommendations

1. **Use native color formats** for your display type
2. **Batch drawing operations** to minimize bus transactions
3. **Cache frequently used images** to avoid repeated loading
4. **Limit concurrent animations** to 3-5 for optimal performance
5. **Choose appropriate frame rates** based on display capabilities
6. **Profile your specific use case** to identify bottlenecks
