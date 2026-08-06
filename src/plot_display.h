/**
 * @file
 * @brief Live line chart of the 4 ADC channels.
 */

#ifndef APP_PLOT_DISPLAY_H_
#define APP_PLOT_DISPLAY_H_

#include <lvgl.h>

/** @brief Build the plot screen. Call once at startup, after adc_input_init(). */
void plot_display_init(void);

/** @brief Get the plot screen object. */
lv_obj_t *plot_display_screen_get(void);

#endif /* APP_PLOT_DISPLAY_H_ */
