/**
 * @file qp_controllers.c
 * @brief Implementation of high-level animation controllers
 */

#include QMK_KEYBOARD_H
#include "qp_controllers.h"

// ============================================================================
// One-Shot Animation Controller
// ============================================================================

void qp_oneshot_init(qp_oneshot_controller_t* controller, const qp_image_sequence_t* sequence,
                     uint16_t x, uint16_t y, bool steady_at_end, bool auto_boot, uint32_t now) {
    if (!controller || !sequence) return;
    
    controller->sequence = sequence;
    controller->x = x;
    controller->y = y;
    controller->steady_at_end = steady_at_end;
    controller->auto_boot = auto_boot;
    controller->triggered = false;
    controller->completed = false;
    controller->last_render_time = now;
    
    // Initialize animator but don't start yet
    controller->animator.sequence = NULL;
    controller->animator.active = false;
    
    if (auto_boot) {
        qp_oneshot_trigger(controller, now);
    }
}

void qp_oneshot_trigger(qp_oneshot_controller_t* controller, uint32_t now) {
    if (!controller || !controller->sequence) return;
    
    controller->triggered = true;
    controller->completed = false;
    
    qp_animator_start(&controller->animator, controller->sequence, true, now);
}

void qp_oneshot_render(qp_oneshot_controller_t* controller, painter_device_t device, uint32_t now) {
    if (!controller || !controller->sequence) return;
    
    controller->last_render_time = now;
    
    if (qp_animator_is_active(&controller->animator)) {
        // Animation is running
        qp_anim_result_t result = qp_animator_step_and_draw(&controller->animator, device, controller->x, controller->y, now);
        
        if (result != QP_ANIM_RUNNING) {
            controller->completed = true;
        }
    } else if (controller->completed || !controller->triggered) {
        // Show steady frame
        const qp_image_t* steady_frame;
        if (controller->steady_at_end) {
            steady_frame = qp_animator_last_frame(&controller->animator);
        } else {
            steady_frame = qp_animator_first_frame(&controller->animator);
        }
        
        if (steady_frame) {
            qp_draw_image(device, steady_frame, controller->x, controller->y);
        }
    }
}

// ============================================================================
// Toggle Animation Controller
// ============================================================================

void qp_toggle_init(qp_toggle_controller_t* controller, const qp_image_sequence_t* sequence,
                    uint16_t x, uint16_t y, bool steady_at_end, bool initial_state, uint32_t now) {
    if (!controller || !sequence) return;
    
    controller->sequence = sequence;
    controller->x = x;
    controller->y = y;
    controller->steady_at_end = steady_at_end;
    controller->current_state = initial_state;
    controller->target_state = initial_state;
    controller->in_transition = false;
    controller->last_render_time = now;
    
    // Initialize animator but don't start yet
    controller->animator.sequence = NULL;
    controller->animator.active = false;
}

void qp_toggle_set(qp_toggle_controller_t* controller, bool state, uint32_t now) {
    if (!controller || !controller->sequence) return;
    
    if (controller->target_state == state) {
        return; // No change needed
    }
    
    controller->target_state = state;
    controller->in_transition = true;
    
    // Start animation in appropriate direction
    bool forward = (state == controller->steady_at_end);
    qp_animator_start(&controller->animator, controller->sequence, forward, now);
}

void qp_toggle_render(qp_toggle_controller_t* controller, painter_device_t device, uint32_t now) {
    if (!controller || !controller->sequence) return;
    
    controller->last_render_time = now;
    
    if (controller->in_transition && qp_animator_is_active(&controller->animator)) {
        // Animation is running
        qp_anim_result_t result = qp_animator_step_and_draw(&controller->animator, device, controller->x, controller->y, now);
        
        if (result != QP_ANIM_RUNNING) {
            // Transition completed
            controller->current_state = controller->target_state;
            controller->in_transition = false;
        }
    } else {
        // Show steady frame based on current state
        const qp_image_t* steady_frame;
        if (controller->current_state == controller->steady_at_end) {
            steady_frame = qp_animator_last_frame(&controller->animator);
        } else {
            steady_frame = qp_animator_first_frame(&controller->animator);
        }
        
        if (steady_frame) {
            qp_draw_image(device, steady_frame, controller->x, controller->y);
        }
    }
}

// ============================================================================
// Out-and-Back Animation Controller
// ============================================================================

void qp_outback_init(qp_outback_controller_t* controller, const qp_image_sequence_t* sequence,
                     uint16_t x, uint16_t y, bool steady_at_end, bool auto_boot, uint32_t now) {
    if (!controller || !sequence) return;
    
    controller->sequence = sequence;
    controller->x = x;
    controller->y = y;
    controller->steady_at_end = steady_at_end;
    controller->auto_boot = auto_boot;
    controller->phase = QP_OAB_IDLE;
    controller->triggered = false;
    controller->completed = false;
    controller->last_render_time = now;
    
    // Initialize animator but don't start yet
    controller->animator.sequence = NULL;
    controller->animator.active = false;
    
    if (auto_boot) {
        qp_outback_trigger(controller, now);
    }
}

void qp_outback_trigger(qp_outback_controller_t* controller, uint32_t now) {
    if (!controller || !controller->sequence) return;
    
    controller->triggered = true;
    controller->completed = false;
    controller->phase = QP_OAB_FORWARD;
    
    qp_animator_start(&controller->animator, controller->sequence, true, now);
}

void qp_outback_render(qp_outback_controller_t* controller, painter_device_t device, uint32_t now) {
    if (!controller || !controller->sequence) return;
    
    controller->last_render_time = now;
    
    switch (controller->phase) {
        case QP_OAB_FORWARD:
            if (qp_animator_is_active(&controller->animator)) {
                qp_anim_result_t result = qp_animator_step_and_draw(&controller->animator, device, controller->x, controller->y, now);
                
                if (result == QP_ANIM_DONE_AT_END) {
                    // Forward animation completed, start backward
                    controller->phase = QP_OAB_BACKWARD;
                    qp_animator_start(&controller->animator, controller->sequence, false, now);
                }
            }
            break;
            
        case QP_OAB_BACKWARD:
            if (qp_animator_is_active(&controller->animator)) {
                qp_anim_result_t result = qp_animator_step_and_draw(&controller->animator, device, controller->x, controller->y, now);
                
                if (result == QP_ANIM_DONE_AT_START) {
                    // Backward animation completed
                    controller->phase = QP_OAB_IDLE;
                    controller->completed = true;
                }
            }
            break;
            
        case QP_OAB_IDLE:
        default:
            // Show steady frame
            const qp_image_t* steady_frame;
            if (controller->steady_at_end) {
                steady_frame = qp_animator_last_frame(&controller->animator);
            } else {
                steady_frame = qp_animator_first_frame(&controller->animator);
            }
            
            if (steady_frame) {
                qp_draw_image(device, steady_frame, controller->x, controller->y);
            }
            break;
    }
}
