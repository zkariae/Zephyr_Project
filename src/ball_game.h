/**
 * @file
 * @brief Tilt-controlled bouncing ball screen, driven by the MPU6050.
 */

#ifndef APP_BALL_GAME_H_
#define APP_BALL_GAME_H_

#include <lvgl.h>

/** @brief Build the ball game screen. */
void ball_game_init(void);

/** @brief Get the ball game screen object. */
lv_obj_t *ball_game_screen_get(void);

#endif /* APP_BALL_GAME_H_ */
