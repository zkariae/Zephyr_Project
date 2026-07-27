#include "launcher.h"
#include "system_overview.h"
#include "rtos_tasks.h"
#include "events_logs.h"
#include "live_variables.h"
#include "plot_display.h"
#include <lvgl_zephyr.h>

static lv_obj_t *launcher_screen;

static void open_system_overview(lv_event_t *e)
{
    (void)e;
    printk("[launcher]: Opening system overview\n");
    events_logs_add("[launcher] Ouverture System Overview");
    lv_screen_load(system_overview_screen_get());
}

static void open_rtos_info(lv_event_t *e)
{
    (void)e;
    printk("[launcher]: Opening RTOS info\n");
    events_logs_add("[launcher] Ouverture RTOS Tasks");
    lv_screen_load(rtos_tasks_screen_get());
}

static void open_event_info(lv_event_t *e)
{
    (void)e;
    printk("[launcher]: Opening event logs\n");
    events_logs_add("[launcher] Ouverture Event Logs");
    lv_screen_load(events_logs_screen_get());
}

static void open_live_variables(lv_event_t *e)
{
    (void)e;
    printk("[launcher]: Opening live variables\n");
    events_logs_add("[launcher] Ouverture Live Variables");
    lv_screen_load(live_variables_screen_get());
}

static void open_plot_display(lv_event_t *e)
{
    (void)e;
    printk("[launcher]: Opening plot display\n");
    events_logs_add("[launcher] Ouverture Plot Display");
    lv_screen_load(plot_display_screen_get());
}

lv_obj_t *launcher_screen_get(void)
{
    return launcher_screen;
}

void launcher_init(void)
{
    lvgl_lock();

    launcher_screen = lv_obj_create(NULL);

    lv_obj_t *title1 = lv_label_create(launcher_screen);
    lv_label_set_text(title1, "SMART DEBUGGER");
    lv_obj_align(title1, LV_ALIGN_TOP_LEFT, 10, 10);

    lv_obj_t *syst_btn = lv_button_create(launcher_screen);
    lv_obj_set_size(syst_btn, 180, 60);
    lv_obj_align(syst_btn, LV_ALIGN_CENTER, -100, -40);
    lv_obj_add_event_cb(syst_btn, open_system_overview, LV_EVENT_CLICKED, NULL);

    lv_obj_t *syst_label = lv_label_create(syst_btn);
    lv_label_set_text(syst_label, "SYSTEM OVERVIEW");
    lv_obj_center(syst_label);

    lv_obj_t *rtos_btn = lv_button_create(launcher_screen);
    lv_obj_set_size(rtos_btn, 180, 60);
    lv_obj_align(rtos_btn, LV_ALIGN_CENTER, 100, -40);
    lv_obj_add_event_cb(rtos_btn, open_rtos_info, LV_EVENT_CLICKED, NULL);

    lv_obj_t *rtos_label = lv_label_create(rtos_btn);
    lv_label_set_text(rtos_label, "RTOS TASKS");
    lv_obj_center(rtos_label);

    lv_obj_t *event_btn = lv_button_create(launcher_screen);
    lv_obj_set_size(event_btn, 180, 60);
    lv_obj_align(event_btn, LV_ALIGN_CENTER, -100, 40);
    lv_obj_add_event_cb(event_btn, open_event_info, LV_EVENT_CLICKED, NULL);

    lv_obj_t *event_label = lv_label_create(event_btn);
    lv_label_set_text(event_label, "EVENT LOGS");
    lv_obj_center(event_label);

    lv_obj_t *live_variable_btn = lv_button_create(launcher_screen);
    lv_obj_set_size(live_variable_btn, 180, 60);
    lv_obj_align(live_variable_btn, LV_ALIGN_CENTER, 100, 40);
    lv_obj_add_event_cb(live_variable_btn, open_live_variables, LV_EVENT_CLICKED, NULL);

    lv_obj_t *live_variable_label = lv_label_create(live_variable_btn);
    lv_label_set_text(live_variable_label, "LIVE VARIABLES");
    lv_obj_center(live_variable_label);

    lv_obj_t *plot_btn = lv_button_create(launcher_screen);
    lv_obj_set_size(plot_btn, 180, 60);
    lv_obj_align(plot_btn, LV_ALIGN_CENTER, 0, 120);
    lv_obj_add_event_cb(plot_btn, open_plot_display, LV_EVENT_CLICKED, NULL);

    lv_obj_t *plot_label = lv_label_create(plot_btn);
    lv_label_set_text(plot_label, "PLOT DISPLAY");
    lv_obj_center(plot_label);


    lv_screen_load(launcher_screen);

    lvgl_unlock();
}
