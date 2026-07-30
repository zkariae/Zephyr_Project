#include "live_variables.h"
#include "launcher.h"
#include "events_logs.h"
#include "adc_input.h"
#include <lvgl_zephyr.h>
#include <lvgl_mem.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/mem_stats.h>

#define VARS_REFRESH_PERIOD_MS 1000
#define VARS_ROW_CNT (4 + ADC_CHANNEL_COUNT)

static lv_obj_t *live_variables_screen;
static lv_obj_t *vars_table;
static lv_timer_t *vars_timer;

static void back_to_menu_cb(lv_event_t *e)
{
    (void)e;
    printk("[live_variables]: Back to menu\n");
    events_logs_add("[live_variables] Retour menu");
    lv_screen_load(launcher_screen_get());
}

static void vars_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    struct sys_memory_stats heap_stats;
    size_t stack_unused = 0;
    int32_t adc_mv[ADC_CHANNEL_COUNT];

    lvgl_heap_stats(&heap_stats);
    k_thread_stack_space_get(k_current_get(), &stack_unused);

    printk("[live_variables]: Refreshing variables\n");
    for (int i = 0; i < ADC_CHANNEL_COUNT; i++) {
        adc_mv[i] = adc_input_get_mv(i);
        printk("[live_variables]: adc_mv[%d] = %d mV\n", i, adc_mv[i]);
    }

    lv_table_set_cell_value_fmt(vars_table, 1, 1, "%u", (unsigned int)heap_stats.allocated_bytes);
    lv_table_set_cell_value_fmt(vars_table, 2, 1, "%u", (unsigned int)heap_stats.free_bytes);
    lv_table_set_cell_value_fmt(vars_table, 3, 1, "%u", (unsigned int)heap_stats.max_allocated_bytes);
    lv_table_set_cell_value_fmt(vars_table, 4, 1, "%u", (unsigned int)stack_unused);
    for (int i = 0; i < ADC_CHANNEL_COUNT; i++) {
        lv_table_set_cell_value_fmt(vars_table, 5 + i, 1, "%d", (int)adc_mv[i]);
    }
}

/* Le timer ne tourne que lorsque cet ecran est reellement affiche, pour
 * ne pas charger le thread LVGL en permanence pour un ecran invisible. */
static void live_variables_visibility_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_SCREEN_LOADED) {
        lv_timer_resume(vars_timer);
        lv_timer_ready(vars_timer);
    } else {
        lv_timer_pause(vars_timer);
    }
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
    /* Colonnes = toute la largeur de l'ecran (480px) */
    lv_table_set_column_width(vars_table, 0, 340);
    lv_table_set_column_width(vars_table, 1, 140);
    lv_obj_set_style_pad_top(vars_table, 5, LV_PART_ITEMS);
    lv_obj_set_style_pad_bottom(vars_table, 5, LV_PART_ITEMS);
    /* Hauteur fixe : garde un espace visible entre le bas du tableau et le
     * bas de l'ecran (272px de haut), au lieu de laisser le tableau
     * s'etendre jusqu'au contenu. */
    lv_obj_set_height(vars_table, 197);
    lv_obj_align(vars_table, LV_ALIGN_TOP_LEFT, 0, 60);
    lv_obj_clear_flag(vars_table, LV_OBJ_FLAG_SCROLL_ELASTIC);


    lv_table_set_cell_value(vars_table, 0, 0, "Variable");
    lv_table_set_cell_value(vars_table, 0, 1, "Valeur");
    lv_table_set_cell_value(vars_table, 1, 0, "Heap LVGL utilise (o)");
    lv_table_set_cell_value(vars_table, 2, 0, "Heap LVGL libre (o)");
    lv_table_set_cell_value(vars_table, 3, 0, "Heap LVGL pic max (o)");
    lv_table_set_cell_value(vars_table, 4, 0, "Stack libre thread (o)");
    lv_table_set_cell_value(vars_table, 5, 0, "Valeur ADC pot0 (mV)");
    lv_table_set_cell_value(vars_table, 6, 0, "Valeur ADC pot1 (mV)");
    lv_table_set_cell_value(vars_table, 7, 0, "Valeur ADC pot2 (mV)");
    lv_table_set_cell_value(vars_table, 8, 0, "Valeur ADC pot3 (mV)");


    lv_obj_clear_flag(live_variables_screen, LV_OBJ_FLAG_SCROLL_ELASTIC);

    vars_timer = lv_timer_create(vars_timer_cb, VARS_REFRESH_PERIOD_MS, NULL);
    lv_timer_pause(vars_timer);
    vars_timer_cb(NULL);

    lv_obj_add_event_cb(live_variables_screen, live_variables_visibility_cb,
                         LV_EVENT_SCREEN_LOADED, NULL);
    lv_obj_add_event_cb(live_variables_screen, live_variables_visibility_cb,
                         LV_EVENT_SCREEN_UNLOADED, NULL);

    lvgl_unlock();
}
