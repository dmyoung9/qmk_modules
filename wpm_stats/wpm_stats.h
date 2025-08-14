#pragma once

#include QMK_KEYBOARD_H

/**
 * @brief Minimal WPM statistics tracking
 *
 * This is a lightweight alternative to typing_stats that only tracks:
 * - Current WPM
 * - Average WPM (exponential moving average)
 * - Maximum WPM
 *
 * No persistent storage, sessions, or complex statistics.
 */

// No configuration needed for this minimal implementation

/**
 * @brief WPM statistics structure
 */
typedef struct {
    uint16_t current_wpm;  /**< Current WPM reading */
    uint16_t average_wpm;      /**< Average WPM (exponential moving average) */
    uint16_t session_max_wpm;      /**< Maximum WPM ever recorded */
} wpm_stats_t;

extern wpm_stats_t g_slave_wpm_data;

/**
 * @brief Initialize WPM statistics tracking
 *
 * Call this once during keyboard initialization.
 */
void wpm_stats_init(void);

/**
 * @brief Periodic task function - call every 10ms
 *
 * Updates WPM tracking and statistics.
 * Call this from your keyboard's matrix_scan_user() or similar.
 */
void wpm_stats_task(void);

/**
 * @brief Record a key event
 *
 * Call this from process_record_user() to track key presses for WPM.
 *
 * @param record The QMK key record
 */
void wpm_stats_on_keyevent(keyrecord_t *record);

/**
 * @brief Get current WPM statistics
 *
 * @param stats Pointer to structure to fill with current statistics
 * @return true if successful, false if not initialized
 */
bool wpm_stats_get(wpm_stats_t *stats);

/**
 * @brief Get current WPM
 *
 * @return Current WPM reading
 */
uint16_t wpm_stats_get_current(void);

/**
 * @brief Get average WPM
 *
 * @return Average WPM (exponential moving average)
 */
uint16_t wpm_stats_get_avg(void);

/**
 * @brief Get maximum WPM
 *
 * @return Maximum WPM ever recorded
 */
uint16_t wpm_stats_get_max(void);

/**
 * @brief Reset all statistics
 *
 * Clears average and maximum WPM. Current WPM will update naturally.
 */
void wpm_stats_reset(void);

// Note: Split keyboard sync now uses shared variables automatically
