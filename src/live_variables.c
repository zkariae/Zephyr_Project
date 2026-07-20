#include "live_variables.h"
#include "launcher.h"
#include <lvgl_zephyr.h>
#include <lvgl_mem.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/mem_stats.h>

#define VARS_REFRESH_PERIOD_MS 1000
#define VARS_ROW_CNT 4

static lv_obj_t *live_variables_screen;
static lv_obj_t *vars_table;

static void back_to_menu_cb(lv_event_t *e)
{
    (void)e;
    printk("[live_variables]: Back to menu\n");
    lv_screen_load(launcher_screen_get());
}

static void vars_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    struct sys_memory_stats heap_stats;
    size_t stack_unused = 0;

    lvgl_heap_stats(&heap_stats);
    k_thread_stack_space_get(k_current_get(), &stack_unused);
    printk("[live_variables]: Refreshing variables\n");
    lv_table_set_cell_value_fmt(vars_table, 1, 1, "%u", (unsigned int)heap_stats.allocated_bytes);
    lv_table_set_cell_value_fmt(vars_table, 2, 1, "%u", (unsigned int)heap_stats.free_bytes);
    lv_table_set_cell_value_fmt(vars_table, 3, 1, "%u", (unsigned int)heap_stats.max_allocated_bytes);
    lv_table_set_cell_value_fmt(vars_table, 4, 1, "%u", (unsigned int)stack_unused);
}

lv_obj_t *live_variables_screen_get(void)
{
    return live_variables_screen;
}

void live_variables_init(void)
{
    lvgl_lock();

    live_variables_screen = lv_obj_create(NULL);

    lv_obj_t *title = lv_label_create(live_variables_screen);
    lv_label_set_text(title, "LIVE VARIABLES");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *back_btn = lv_button_create(live_variables_screen);
    lv_obj_set_size(back_btn, 80, 30);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_event_cb(back_btn, back_to_menu_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Menu");
    lv_obj_center(back_label);

    vars_table = lv_table_create(live_variables_screen);
    lv_table_set_column_count(vars_table, 2);
    lv_table_set_row_count(vars_table, VARS_ROW_CNT + 1);
    lv_table_set_column_width(vars_table, 0, 180);
    lv_table_set_column_width(vars_table, 1, 100);
    lv_obj_set_style_pad_top(vars_table, 4, LV_PART_ITEMS);
    lv_obj_set_style_pad_bottom(vars_table, 4, LV_PART_ITEMS);
    lv_obj_align(vars_table, LV_ALIGN_TOP_LEFT, 20, 60);

    lv_table_set_cell_value(vars_table, 0, 0, "Variable");
    lv_table_set_cell_value(vars_table, 0, 1, "Valeur");
    lv_table_set_cell_value(vars_table, 1, 0, "Heap LVGL utilise (o)");
    lv_table_set_cell_value(vars_table, 2, 0, "Heap LVGL libre (o)");
    lv_table_set_cell_value(vars_table, 3, 0, "Heap LVGL pic max (o)");
    lv_table_set_cell_value(vars_table, 4, 0, "Stack libre thread (o)");

    lv_timer_create(vars_timer_cb, VARS_REFRESH_PERIOD_MS, NULL);
    vars_timer_cb(NULL);

    lvgl_unlock();
}
