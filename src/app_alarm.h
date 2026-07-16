#ifndef APP_ALARM_H_
#define APP_ALARM_H_

#include <lvgl.h>

/* STUB TEMPORAIRE : ecran vide, juste pour permettre a launcher.c de lier.
 * Sera remplace par la vraie appli alarme (RTC + thread dedie). */
void alarm_init(void);

lv_obj_t *alarm_screen_get(void);

#endif /* APP_ALARM_H_ */
