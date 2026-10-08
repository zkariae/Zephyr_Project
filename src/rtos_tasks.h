/**
 * @file
 * @brief RTOS thread monitor (DISABLED, not built - see CMakeLists.txt);
 *        replaced by task_management.c + pc_profiler.c, kept for reference.
 */

#ifndef APP_RTOS_TASKS_H_
#define APP_RTOS_TASKS_H_

#include <lvgl.h>

/** @brief Build the RTOS tasks screen. */
void rtos_tasks_init(void);

/** @brief Get the RTOS tasks screen object. */
lv_obj_t *rtos_tasks_screen_get(void);

#endif /* APP_RTOS_TASKS_H_ */
