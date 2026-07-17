#ifndef APP_SYSTEM_OVERVIEW_H_
#define APP_SYSTEM_OVERVIEW_H_

#include <lvgl.h>

/* STUB TEMPORAIRE : ecran vide, juste pour permettre a launcher.c de lier.
 * Sera remplace par la vraie appli system_overview (RTC + thread dedie). */
void system_overview_init(void);

lv_obj_t *system_overview_screen_get(void);

#endif /* APP_SYSTEM_OVERVIEW_H_ */