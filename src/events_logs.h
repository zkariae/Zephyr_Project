#ifndef APP_EVENTS_LOGS_H_
#define APP_EVENTS_LOGS_H_

#include <lvgl.h>

void events_logs_init(void);

lv_obj_t *events_logs_screen_get(void);

/* Empile une ligne horodatee dans le buffer d'evenements (format style
 * printf, sans '\n' final). Appelable avant events_logs_init() : les
 * evenements de boot sont geres dans un buffer statique et apparaissent
 * a l'ecran des que celui-ci est cree. */
void events_logs_add(const char *fmt, ...);

#endif /* APP_EVENTS_LOGS_H_ */
