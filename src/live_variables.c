/* STUB TEMPORAIRE : ecran vide, juste pour permettre a launcher.c de lier
 * pendant qu'on valide la chaine QSPI XIP. Sera remplace par la vraie
 * appli live_variables (table rafraichie par timer) a l'etape dediee du
 * plan. */

#include "live_variables.h"
#include "launcher.h"
#include <lvgl_zephyr.h>

static lv_obj_t *live_variables_screen;

static void back_to_menu_cb(lv_event_t *e)
{
    (void)e;
    lv_screen_load(launcher_screen_get());
}

lv_obj_t *live_variables_screen_get(void)
{
    return live_variables_screen;
}

void live_variables_init(void)
{
    lvgl_lock();

    live_variables_screen = lv_obj_create(NULL);

    lv_obj_t *label = lv_label_create(live_variables_screen);
    lv_label_set_text(label, "Live Variables (bientot disponible)");
    lv_obj_center(label);

    lv_obj_t *back_btn = lv_button_create(live_variables_screen);
    lv_obj_set_size(back_btn, 80, 30);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_event_cb(back_btn, back_to_menu_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Menu");
    lv_obj_center(back_label);

    lvgl_unlock();
}
