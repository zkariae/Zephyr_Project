#ifndef APP_CALCULATOR_H_
#define APP_CALCULATOR_H_

#include <lvgl.h>

/* Construit l'ecran calculatrice et demarre son thread de calcul.
 * A appeler une seule fois au demarrage, avant launcher_init(). */
void calculator_init(void);

/* Ecran calculatrice, utilise par launcher.c pour lv_screen_load(). */
lv_obj_t *calculator_screen_get(void);

#endif /* APP_CALCULATOR_H_ */
