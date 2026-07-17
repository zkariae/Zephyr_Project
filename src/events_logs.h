#ifndef APP_EVENTS_LOGS_H_
#define APP_EVENTS_LOGS_H_

#include <lvgl.h>

/* STUB TEMPORAIRE : ecran vide, juste pour permettre a launcher.c de lier.
 * Sera remplace par la vraie appli events_logs (buffer d'evenements). */
void events_logs_init(void);

lv_obj_t *events_logs_screen_get(void);

#endif /* APP_EVENTS_LOGS_H_ */
