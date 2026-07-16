#include "launcher.h"
#include "app_calculator.h"
#include "app_alarm.h"
#include <lvgl_zephyr.h>

static lv_obj_t *launcher_screen;

static void open_calculator_cb(lv_event_t *e)
{
    (void)e;
    lv_screen_load(calculator_screen_get());
}

static void open_alarm_cb(lv_event_t *e)
{
    (void)e;
    lv_screen_load(alarm_screen_get());
}

lv_obj_t *launcher_screen_get(void)
{
    return launcher_screen;
}

void launcher_init(void)
{
    lvgl_lock();

    launcher_screen = lv_obj_create(NULL);

    lv_obj_t *title = lv_label_create(launcher_screen);
    lv_label_set_text(title, "Menu");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *calc_btn = lv_button_create(launcher_screen);
    lv_obj_set_size(calc_btn, 200, 60);
    lv_obj_align(calc_btn, LV_ALIGN_CENTER, 0, -40);
    lv_obj_add_event_cb(calc_btn, open_calculator_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *calc_label = lv_label_create(calc_btn);
    lv_label_set_text(calc_label, "Calculatrice");
    lv_obj_center(calc_label);

    lv_obj_t *alarm_btn = lv_button_create(launcher_screen);
    lv_obj_set_size(alarm_btn, 200, 60);
    lv_obj_align(alarm_btn, LV_ALIGN_CENTER, 0, 40);
    lv_obj_add_event_cb(alarm_btn, open_alarm_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *alarm_label = lv_label_create(alarm_btn);
    lv_label_set_text(alarm_label, "Alarme");
    lv_obj_center(alarm_label);

    lv_screen_load(launcher_screen);

    lvgl_unlock();
}
