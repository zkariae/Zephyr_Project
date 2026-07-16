/* STUB TEMPORAIRE : ecran vide, juste pour permettre a launcher.c de lier
 * pendant qu'on valide la chaine QSPI XIP. Sera remplace par la vraie
 * appli alarme (RTC + thread dedie) a l'etape dediee du plan. */

#include "app_alarm.h"
#include "launcher.h"
#include <lvgl_zephyr.h>

static lv_obj_t *alarm_screen;

static void back_to_menu_cb(lv_event_t *e)
{
    (void)e;
    lv_screen_load(launcher_screen_get());
}

lv_obj_t *alarm_screen_get(void)
{
    return alarm_screen;
}

void alarm_init(void)
{
    lvgl_lock();

    alarm_screen = lv_obj_create(NULL);

    lv_obj_t *label = lv_label_create(alarm_screen);
    lv_label_set_text(label, "Alarme (bientot disponible)");
    lv_obj_center(label);

    lv_obj_t *back_btn = lv_button_create(alarm_screen);
    lv_obj_set_size(back_btn, 80, 30);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_event_cb(back_btn, back_to_menu_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Menu");
    lv_obj_center(back_label);

    lvgl_unlock();
}
