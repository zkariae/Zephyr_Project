/**
 * @file
 * @brief Boot splash screens.
 */

#ifndef SPLASH_H_
#define SPLASH_H_

#include <lvgl.h>

/** @brief Build and load splash screen 1. */
void splash_show_image1(void);

/** @brief Build and load splash screen 2, then free splash screen 1. */
void splash_show_image2(void);

/** @brief Free splash screen 2. Call just before loading the launcher. */
void splash_cleanup(void);

#endif /* SPLASH_H_ */
