#ifndef SPLASH_H_
#define SPLASH_H_

#include <lvgl.h>

/* Construit et affiche l'ecran splash 1 (lv_screen_load). */
void splash_show_image1(void);

/* Construit et affiche l'ecran splash 2 (lv_screen_load), puis libere
 * l'ecran splash 1. */
void splash_show_image2(void);

/* Libere l'ecran splash 2. A appeler juste avant de charger le launcher. */
void splash_cleanup(void);

#endif /* SPLASH_H_ */
