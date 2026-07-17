#ifndef APP_RTOS_TASKS_H_
#define APP_RTOS_TASKS_H_

#include <lvgl.h>

/* STUB TEMPORAIRE : ecran vide, juste pour permettre a launcher.c de lier.
 * Sera remplace par la vraie appli rtos_tasks (RTC + thread dedie). */
void rtos_tasks_init(void);

lv_obj_t *rtos_tasks_screen_get(void);

#endif /* APP_RTOS_TASKS_H_ */