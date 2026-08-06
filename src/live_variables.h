/**
 * @file
 * @brief Live variables screen: heap stats and ADC readings in a table,
 *        refreshed by timer.
 */

#ifndef APP_LIVE_VARIABLES_H_
#define APP_LIVE_VARIABLES_H_

#include <lvgl.h>

/** @brief Build the live variables screen. */
void live_variables_init(void);

/** @brief Get the live variables screen object. */
lv_obj_t *live_variables_screen_get(void);

#endif /* APP_LIVE_VARIABLES_H_ */
