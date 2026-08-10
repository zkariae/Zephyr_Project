/**
 * @file
 * @brief Live variables screen: heap stats and ADC readings in a table,
 *        refreshed by timer.
 */

#include "live_variables.h"
#include "launcher.h"
#include "events_logs.h"
#include "adc_input.h"
#include "watchdog.h"
#include <lvgl_zephyr.h>
#include <lvgl_mem.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/mem_stats.h>

#define VARS_REFRESH_PERIOD_MS 1000
#define VARS_ROW_CNT (5 + ADC_CHANNEL_COUNT)

static lv_obj_t *live_variables_screen;
static lv_obj_t *vars_table;
static lv_timer_t *vars_timer;

static void back_to_menu_cb(lv_event_t *e)
{
    (void)e;
    printk("[live_variables]: Back to menu\n");
    events_logs_add("[live_variables] Back to menu");
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
    lv_table_set_cell_value_fmt(vars_table, 5 + ADC_CHANNEL_COUNT, 1, "%u",
                                 (unsigned int)watchdog_get_remaining_ms());
}

/* Refresh timer only runs while this screen is visible, to spare the LVGL
 * thread. */
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
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    lv_obj_t *back_btn = lv_button_create(live_variables_screen);
    lv_obj_set_size(back_btn, 80, 30);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_event_cb(back_btn, back_to_menu_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Menu");
    lv_obj_center(back_label);

    /* "Live" badge: green dot + text, purely visual. */
    lv_obj_t *live_badge = lv_obj_create(live_variables_screen);
    lv_obj_set_size(live_badge, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(live_badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(live_badge, lv_color_hex(0xE6F7ED), 0);
    lv_obj_set_style_bg_opa(live_badge, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(live_badge, 0, 0);
    lv_obj_set_style_pad_hor(live_badge, 10, 0);
    lv_obj_set_style_pad_ver(live_badge, 4, 0);
    lv_obj_set_style_pad_column(live_badge, 4, 0);
    lv_obj_set_flex_flow(live_badge, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(live_badge, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(live_badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(live_badge, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(live_badge, LV_ALIGN_TOP_RIGHT, -10, 16);

    lv_obj_t *live_dot = lv_obj_create(live_badge);
    lv_obj_set_size(live_dot, 8, 8);
    lv_obj_set_style_radius(live_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(live_dot, lv_color_hex(0x2F9E44), 0);
    lv_obj_set_style_bg_opa(live_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(live_dot, 0, 0);
    lv_obj_clear_flag(live_dot, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *live_label = lv_label_create(live_badge);
    lv_label_set_text(live_label, "Live");
    lv_obj_set_style_text_color(live_label, lv_color_hex(0x2F9E44), 0);

    vars_table = lv_table_create(live_variables_screen);
    lv_table_set_column_count(vars_table, 2);
    lv_table_set_row_count(vars_table, VARS_ROW_CNT + 1);
    /* Columns span the full screen width (480px). */
    lv_table_set_column_width(vars_table, 0, 340);
    lv_table_set_column_width(vars_table, 1, 140);
    lv_obj_set_style_pad_top(vars_table, 5, LV_PART_ITEMS);
    lv_obj_set_style_pad_bottom(vars_table, 5, LV_PART_ITEMS);
    /* Fixed height, to leave a visible gap above the bottom of the
     * screen instead of the table growing to fit its content. */
    lv_obj_set_height(vars_table, 197);
    lv_obj_align(vars_table, LV_ALIGN_TOP_LEFT, 0, 60);
    lv_obj_clear_flag(vars_table, LV_OBJ_FLAG_SCROLL_ELASTIC);


    lv_table_set_cell_value(vars_table, 0, 0, "Variable");
    lv_table_set_cell_value(vars_table, 0, 1, "Value");
    lv_table_set_cell_value(vars_table, 1, 0, "LVGL heap used (B)");
    lv_table_set_cell_value(vars_table, 2, 0, "LVGL heap free (B)");
    lv_table_set_cell_value(vars_table, 3, 0, "LVGL heap peak (B)");
    lv_table_set_cell_value(vars_table, 4, 0, "Thread free stack (B)");
    lv_table_set_cell_value(vars_table, 5, 0, "ADC pot0 value (mV)");
    lv_table_set_cell_value(vars_table, 6, 0, "ADC pot1 value (mV)");
    lv_table_set_cell_value(vars_table, 7, 0, "ADC pot2 value (mV)");
    lv_table_set_cell_value(vars_table, 8, 0, "ADC pot3 value (mV)");
    lv_table_set_cell_value(vars_table, 9, 0, "Watchdog - time remaining (ms)");


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
