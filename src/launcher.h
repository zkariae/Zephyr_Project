#ifndef LAUNCHER_H_
#define LAUNCHER_H_

#include <lvgl.h>

/* Construit l'ecran d'accueil et l'affiche (lv_screen_load). A appeler une
 * seule fois au demarrage, apres l'init de l'ecran calculatrice et alarme
 * (leurs boutons referencent calculator_screen_get() / alarm_screen_get()). */
void launcher_init(void);

/* Ecran d'accueil, utilise par les autres ecrans pour le bouton "Menu". */
lv_obj_t *launcher_screen_get(void);

#endif /* LAUNCHER_H_ */
