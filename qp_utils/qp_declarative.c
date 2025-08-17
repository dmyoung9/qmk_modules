/**
 * @file qp_declarative.c
 * @brief Implementation of declarative widget system
 */

#include QMK_KEYBOARD_H
#include "qp_declarative.h"

// ============================================================================
// Configuration Constants
// ============================================================================

#ifndef QP_WIDGET_QUERY_INTERVAL_MS
#define QP_WIDGET_QUERY_INTERVAL_MS 50  ///< How often to query widget state
#endif

#ifndef QP_WIDGET_STUCK_TIMEOUT_MS
#define QP_WIDGET_STUCK_TIMEOUT_MS 5000  ///< Timeout for stuck transitions
#endif

// ============================================================================
// Internal Helper Functions
// ============================================================================

/**
 * @brief Clear widget background if using opaque blending
 * @param widget Widget instance
 * @param device QP device handle
 */
static void clear_widget_background(const qp_widget_t* widget, painter_device_t device) {
    if (!widget || !widget->config || !widget->config->opaque_blending) return;
    
    qp_fill_rect(device, 
                 widget->config->x, widget->config->y,
                 widget->config->w, widget->config->h,
                 widget->config->clear_color);
}

/**
 * @brief Draw steady frame for current state
 * @param widget Widget instance
 * @param device QP device handle
 */
static void draw_steady_frame(const qp_widget_t* widget, painter_device_t device) {
    if (!widget || !widget->config || widget->current_state >= widget->config->state_count) return;
    
    const qp_widget_state_t* state = &widget->config->states[widget->current_state];
    if (!state->sequence || !state->sequence->frames || state->sequence->count == 0) return;
    
    // Determine which frame to show as steady
    const qp_image_t* steady_frame;
    if (state->enter_forward) {
        // Forward entry: steady is last frame
        steady_frame = &state->sequence->frames[state->sequence->count - 1];
    } else {
        // Reverse entry: steady is first frame
        steady_frame = &state->sequence->frames[0];
    }
    
    if (qp_image_is_valid(steady_frame)) {
        qp_draw_image(device, steady_frame, widget->config->x, widget->config->y);
    }
}

/**
 * @brief Start transition to new state
 * @param widget Widget instance
 * @param new_state Target state index
 * @param now Current timestamp
 */
static void start_transition(qp_widget_t* widget, uint8_t new_state, uint32_t now) {
    if (!widget || !widget->config || new_state >= widget->config->state_count) return;
    
    const qp_widget_state_t* state = &widget->config->states[new_state];
    if (!state->sequence) return;
    
    widget->target_state = new_state;
    widget->phase = QP_WIDGET_ENTERING;
    widget->last_state_change = now;
    widget->stuck_timeout = now + QP_WIDGET_STUCK_TIMEOUT_MS;
    
    // Start animation in appropriate direction
    qp_animator_start(&widget->animator, state->sequence, state->enter_forward, now);
}

// ============================================================================
// Public API Implementation
// ============================================================================

void qp_widget_init(qp_widget_t* widget, const qp_widget_config_t* config, uint8_t initial_state, uint32_t now) {
    if (!widget || !config) return;
    
    widget->config = config;
    widget->phase = QP_WIDGET_IDLE;
    widget->current_state = initial_state;
    widget->target_state = initial_state;
    widget->pending_state = 0xFF;  // Invalid state
    widget->last_query_time = now;
    widget->last_state_change = now;
    widget->stuck_timeout = 0;
    widget->last_query_result = initial_state;
    widget->initialized = true;
    
    // Initialize animator
    widget->animator.sequence = NULL;
    widget->animator.active = false;
}

void qp_widget_tick(qp_widget_t* widget, painter_device_t device, uint32_t now) {
    if (!widget || !widget->initialized || !widget->config) return;
    
    // Query current desired state periodically
    bool should_query = (TIMER_DIFF_32(now, widget->last_query_time) >= QP_WIDGET_QUERY_INTERVAL_MS);
    uint8_t desired_state = widget->last_query_result;
    
    if (should_query && widget->config->query_fn) {
        desired_state = widget->config->query_fn(widget->config->user_data);
        widget->last_query_time = now;
        
        // Clamp to valid range
        if (desired_state >= widget->config->state_count) {
            desired_state = 0;
        }
        
        widget->last_query_result = desired_state;
    }
    
    // Handle state machine
    switch (widget->phase) {
        case QP_WIDGET_IDLE:
            if (desired_state != widget->current_state) {
                start_transition(widget, desired_state, now);
            } else {
                // Draw steady frame
                clear_widget_background(widget, device);
                draw_steady_frame(widget, device);
            }
            break;
            
        case QP_WIDGET_ENTERING:
            if (qp_animator_is_active(&widget->animator)) {
                // Clear background and step animation
                clear_widget_background(widget, device);
                qp_anim_result_t result = qp_animator_step_and_draw(&widget->animator, device, 
                                                                   widget->config->x, widget->config->y, now);
                
                if (result != QP_ANIM_RUNNING) {
                    // Transition completed
                    widget->current_state = widget->target_state;
                    widget->phase = QP_WIDGET_IDLE;
                    
                    // Check for pending state change
                    if (desired_state != widget->current_state) {
                        start_transition(widget, desired_state, now);
                    }
                }
            } else {
                // Animation not active, transition completed
                widget->current_state = widget->target_state;
                widget->phase = QP_WIDGET_IDLE;
            }
            
            // Check for stuck transition
            if (TIMER_DIFF_32(now, widget->stuck_timeout) >= 0) {
                widget->current_state = widget->target_state;
                widget->phase = QP_WIDGET_IDLE;
                qp_animator_stop(&widget->animator);
            }
            break;
            
        default:
            // Invalid phase, reset
            widget->phase = QP_WIDGET_IDLE;
            break;
    }
}

void qp_widget_force_state(qp_widget_t* widget, uint8_t state, uint32_t now) {
    if (!widget || !widget->config || state >= widget->config->state_count) return;
    
    // Stop any current animation
    qp_animator_stop(&widget->animator);
    
    // Set state immediately
    widget->current_state = state;
    widget->target_state = state;
    widget->phase = QP_WIDGET_IDLE;
    widget->last_query_result = state;
    widget->last_state_change = now;
}

// ============================================================================
// Common Query Functions
// ============================================================================

uint8_t qp_query_layer(void* user_data) {
    (void)user_data;  // Unused
    return get_highest_layer(layer_state);
}

uint8_t qp_query_modifiers(void* user_data) {
    if (!user_data) return 0;

    uint8_t* modifier_mask = (uint8_t*)user_data;
    uint8_t current_mods = get_mods() | get_oneshot_mods();

    return (current_mods & *modifier_mask) ? 1 : 0;
}

uint8_t qp_query_caps_lock(void* user_data) {
    (void)user_data;  // Unused
    return host_keyboard_led_state().caps_lock ? 1 : 0;
}
