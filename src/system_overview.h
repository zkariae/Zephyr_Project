/**
 * @file
 * @brief System overview screen: board info, uptime, die temperature,
 *        and VREF+, refreshed by timer.
 */

#ifndef APP_SYSTEM_OVERVIEW_H_
#define APP_SYSTEM_OVERVIEW_H_

#include <lvgl.h>

/** @brief Build the system overview screen. */
void system_overview_init(void);

/** @brief Get the system overview screen object. */
lv_obj_t *system_overview_screen_get(void);

#endif /* APP_SYSTEM_OVERVIEW_H_ */