/**
 * @file
 * @brief Scrollable on-screen event log (timestamped, ring-buffered).
 */

#ifndef APP_EVENTS_LOGS_H_
#define APP_EVENTS_LOGS_H_

#include <lvgl.h>

/** @brief Build the event log screen. */
void events_logs_init(void);

/** @brief Get the event log screen object. */
lv_obj_t *events_logs_screen_get(void);

/**
 * @brief Push a timestamped, printf-style line to the event log.
 *
 * Safe to call before events_logs_init(): boot-time events are held in a
 * static buffer and appear as soon as the screen is created.
 *
 * @param fmt printf-style format string (no trailing '\n').
 */
void events_logs_add(const char *fmt, ...);

#endif /* APP_EVENTS_LOGS_H_ */
