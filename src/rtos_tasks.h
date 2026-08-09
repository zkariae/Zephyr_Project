/**
 * @file
 * @brief RTOS thread monitor: table of live threads, state, priority, and
 *        free stack.
 *
 * DISABLED: not built (see CMakeLists.txt). Replaced by the Task
 * Management screen (src/task_management.c + src/pc_profiler.c), which
 * shows per-function CPU usage instead of per-thread state. Kept on disk
 * for reference / possible reuse, not deleted.
 */

#ifndef APP_RTOS_TASKS_H_
#define APP_RTOS_TASKS_H_

#include <lvgl.h>

/** @brief Build the RTOS tasks screen. */
void rtos_tasks_init(void);

/** @brief Get the RTOS tasks screen object. */
lv_obj_t *rtos_tasks_screen_get(void);

#endif /* APP_RTOS_TASKS_H_ */
