#pragma once

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
 * @brief Render the WPM bar graph to OLED display
 *
 * Call this from oled_task_user() to render the current WPM statistics
 * as a bar graph on the OLED display.
 *
 * @return true if rendering successful, false otherwise
 */
bool wpm_stats_oled_render(void);
#endif
