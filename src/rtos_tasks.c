/* STUB TEMPORAIRE : ecran vide, juste pour permettre a launcher.c de lier
 * pendant qu'on valide la chaine QSPI XIP. Sera remplace par la vraie
 * appli rtos_tasks (RTC + thread dedie) a l'etape dediee du plan. */

#include "rtos_tasks.h"
#include "launcher.h"
#include <lvgl_zephyr.h>

static lv_obj_t *rtos_tasks_screen;

static void back_to_menu_cb(lv_event_t *e)
{
    (void)e;
    lv_screen_load(launcher_screen_get());
}

lv_obj_t *rtos_tasks_screen_get(void)
{
    return rtos_tasks_screen;
}

void rtos_tasks_init(void)
{
    lvgl_lock();

    rtos_tasks_screen = lv_obj_create(NULL);

    lv_obj_t *label = lv_label_create(rtos_tasks_screen);
    lv_label_set_text(label, "RTOS Tasks (bientot disponible)");
    lv_obj_center(label);

    lv_obj_t *back_btn = lv_button_create(rtos_tasks_screen);
    lv_obj_set_size(back_btn, 80, 30);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_event_cb(back_btn, back_to_menu_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Menu");
    lv_obj_center(back_label);

    lvgl_unlock();
}
