#ifndef APP_PLOT_DISPLAY_H_
#define APP_PLOT_DISPLAY_H_

#include <lvgl.h>

/* Construit l'ecran de trace graphique des 4 canaux ADC. A appeler une seule
 * fois au demarrage, apres adc_input_init(). */
void plot_display_init(void);

lv_obj_t *plot_display_screen_get(void);

#endif /* APP_PLOT_DISPLAY_H_ */
