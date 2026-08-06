/**
 * @file
 * @brief Home screen: grid of tiles to open each app screen.
 */

#ifndef LAUNCHER_H_
#define LAUNCHER_H_

#include <lvgl.h>

/**
 * @brief Build and load the launcher screen. Call once at startup, after
 *        all other screen modules have been initialized.
 */
void launcher_init(void);

/** @brief Get the launcher screen, used by other screens for the "Menu" button. */
lv_obj_t *launcher_screen_get(void);

#endif /* LAUNCHER_H_ */
