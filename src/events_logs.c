#include "events_logs.h"
#include "launcher.h"
#include <lvgl_zephyr.h>

static lv_obj_t *events_logs_screen;

static void back_to_menu_cb(lv_event_t *e)
{
    (void)e;
    printk("[events_logs]: Back to menu\n");
    lv_screen_load(launcher_screen_get());
}

lv_obj_t *events_logs_screen_get(void)
{
    return events_logs_screen;
}

void events_logs_init(void)
{
    lvgl_lock();

    events_logs_screen = lv_obj_create(NULL);

    lv_obj_t *label = lv_label_create(events_logs_screen);
    lv_label_set_text(label, "Event Logs (bientot disponible)");
    lv_obj_center(label);

    lv_obj_t *back_btn = lv_button_create(events_logs_screen);
    lv_obj_set_size(back_btn, 80, 30);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_event_cb(back_btn, back_to_menu_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Menu");
    lv_obj_center(back_label);

    lvgl_unlock();
}
