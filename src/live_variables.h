#ifndef APP_LIVE_VARIABLES_H_
#define APP_LIVE_VARIABLES_H_

#include <lvgl.h>

/* STUB TEMPORAIRE : ecran vide, juste pour permettre a launcher.c de lier.
 * Sera remplace par la vraie appli live_variables (table rafraichie par
 * timer). */
void live_variables_init(void);

lv_obj_t *live_variables_screen_get(void);

#endif /* APP_LIVE_VARIABLES_H_ */
