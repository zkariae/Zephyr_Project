/**
 * @file
 * @brief Task Management screen: PC profiler table; replaces rtos_tasks.c.
 */

#ifndef APP_TASK_MANAGEMENT_H_
#define APP_TASK_MANAGEMENT_H_

#include <lvgl.h>

/** @brief Build the Task Management screen. */
void task_management_init(void);

/** @brief Get the Task Management screen object. */
lv_obj_t *task_management_screen_get(void);

#endif /* APP_TASK_MANAGEMENT_H_ */
