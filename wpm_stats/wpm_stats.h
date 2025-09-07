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

/**
 * @brief Configuration structure for WPM bar graph
 */
typedef struct {
    uint16_t x;                    /**< X position of the bar */
    uint16_t y;                    /**< Y position of the bar */
    uint16_t width;                /**< Width of the bar */
    uint16_t height;               /**< Height of the bar */
} wpm_bar_config_t;

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
void matrix_scan_wpm_stats(void);

void keyboard_post_init_wpm_stats(void);

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

/**
 * @brief Initialize split keyboard synchronization
 *
 * Call this once during keyboard initialization to set up master-slave sync.
 * This registers the transaction handler for WPM data synchronization.
 */
void wpm_stats_init_split_sync(void);

/**
 * @brief Housekeeping task for split keyboard sync
 *
 * Call this from housekeeping_task_user() to handle periodic sync between
 * master and slave sides of a split keyboard.
 */
void housekeeping_task_wpm_stats(void);

#ifdef OLED_ENABLE
/**
 * @brief Initialize OLED rendering with default bar graph configuration
 *
 * Call this once during keyboard initialization to set up WPM bar graph rendering.
 * Uses default configuration values defined in the module.
 *
 * @return true if initialization successful, false otherwise
 */
bool wpm_stats_oled_init(void);

/**
 * @brief Initialize OLED rendering with custom bar graph configuration
 *
 * Call this once during keyboard initialization to set up WPM bar graph rendering
 * with custom positioning and sizing.
 *
 * @param config Custom configuration structure for bar graph
 * @return true if initialization successful, false otherwise
 */
bool wpm_stats_oled_init_config(const wpm_bar_config_t *config);

/**
 * @brief Render the WPM bar graph to OLED display
 *
 * Call this from oled_task_user() to render the current WPM statistics
 * as a bar graph on the OLED display.
 *
 * @return true if rendering successful, false otherwise
 */
bool wpm_stats_oled_render(void);
#endif // OLED_ENABLE
