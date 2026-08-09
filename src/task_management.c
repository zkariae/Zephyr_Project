/**
 * @file
 * @brief Task Management screen: PC-sampling profiler table.
 */

#include "task_management.h"
#include "pc_profiler.h"
#include "launcher.h"
#include "events_logs.h"
#include <lvgl_zephyr.h>
#include <string.h>

#define TASK_MGMT_REFRESH_PERIOD_MS 1000
#define TASK_MGMT_MAX_ROWS 20

static lv_obj_t *task_management_screen;
static lv_obj_t *profiler_table;
static struct pc_profile_entry rows[TASK_MGMT_MAX_ROWS];
static struct pc_profile_entry rows_prev[TASK_MGMT_MAX_ROWS];
static size_t rows_prev_count;
static uint32_t long_tail_prev = UINT32_MAX;
static uint32_t unresolved_prev = UINT32_MAX;
static lv_timer_t *profiler_timer;

static void back_to_menu_cb(lv_event_t *e)
{
    (void)e;
    printk("[task_management]: Back to menu\n");
    events_logs_add("[task_management] Retour menu");
    lv_screen_load(launcher_screen_get());
}

static void profiler_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    size_t count = pc_profiler_get_top(rows, TASK_MGMT_MAX_ROWS);
    uint32_t total = pc_profiler_total_samples();

    if (count != rows_prev_count) {
        /* header + function rows + "Autres (connues)" + "Non resolu" */
        lv_table_set_row_count(profiler_table, count + 3);
        rows_prev_count = count;
        /* Both summary rows moved, so their old content (if any) is now
         * sitting in the wrong row; force them to be rewritten below. */
        long_tail_prev = UINT32_MAX;
        unresolved_prev = UINT32_MAX;
    }

    uint32_t displayed = 0;

    for (size_t i = 0; i < count; i++) {
        displayed += rows[i].samples;

        /* rows[i].name always points into the static symtab array, so
         * identical entries have identical pointers across refreshes. */
        if (memcmp(&rows[i], &rows_prev[i], sizeof(rows[i])) == 0) {
            continue;
        }

        double pct = total ? (100.0 * rows[i].samples / total) : 0.0;

        lv_table_set_cell_value(profiler_table, i + 1, 0, rows[i].name);
        lv_table_set_cell_value_fmt(profiler_table, i + 1, 1, "%.2f%%", pct);
        lv_table_set_cell_value_fmt(profiler_table, i + 1, 2, "%u", rows[i].samples);
        lv_table_set_cell_value_fmt(profiler_table, i + 1, 3, "0x%08x", rows[i].addr);
        lv_table_set_cell_value_fmt(profiler_table, i + 1, 4, "0x%x", rows[i].size);
        rows_prev[i] = rows[i];
    }

    /* Split what's missing from the table above into two different things:
     * - "Autres (connues)": real, named functions ranked below
     *   TASK_MGMT_MAX_ROWS -- expected long tail on a busy LVGL screen,
     *   not a problem.
     * - "Non resolu": PCs that fell outside every known symbol range --
     *   the actual coverage sanity check, should stay close to 0%. */
    uint32_t unresolved = pc_profiler_unresolved_samples();
    uint32_t long_tail = total > displayed + unresolved ? total - displayed - unresolved : 0;

    if (long_tail != long_tail_prev) {
        double pct = total ? (100.0 * long_tail / total) : 0.0;
        size_t row = count + 1;

        lv_table_set_cell_value(profiler_table, row, 0, "Autres (connues)");
        lv_table_set_cell_value_fmt(profiler_table, row, 1, "%.2f%%", pct);
        lv_table_set_cell_value_fmt(profiler_table, row, 2, "%u", long_tail);
        lv_table_set_cell_value(profiler_table, row, 3, "-");
        lv_table_set_cell_value(profiler_table, row, 4, "-");
        long_tail_prev = long_tail;
    }

    if (unresolved != unresolved_prev) {
        double pct = total ? (100.0 * unresolved / total) : 0.0;
        size_t row = count + 2;

        lv_table_set_cell_value(profiler_table, row, 0, "Non resolu");
        lv_table_set_cell_value_fmt(profiler_table, row, 1, "%.2f%%", pct);
        lv_table_set_cell_value_fmt(profiler_table, row, 2, "%u", unresolved);
        lv_table_set_cell_value(profiler_table, row, 3, "-");
        lv_table_set_cell_value(profiler_table, row, 4, "-");
        unresolved_prev = unresolved;
    }
}

/* Refresh timer only runs while this screen is visible, to spare the LVGL
 * thread. */
static void task_management_visibility_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_SCREEN_LOADED) {
        lv_timer_resume(profiler_timer);
        lv_timer_ready(profiler_timer);
    } else {
        lv_timer_pause(profiler_timer);
    }
}

lv_obj_t *task_management_screen_get(void)
{
    return task_management_screen;
}

void task_management_init(void)
{
    lvgl_lock();

    task_management_screen = lv_obj_create(NULL);

    lv_obj_t *title = lv_label_create(task_management_screen);
    lv_label_set_text(title, "TASK MANAGEMENT");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    lv_obj_t *back_btn = lv_button_create(task_management_screen);
    lv_obj_set_size(back_btn, 80, 30);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_event_cb(back_btn, back_to_menu_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Menu");
    lv_obj_center(back_label);

    profiler_table = lv_table_create(task_management_screen);
    lv_table_set_column_count(profiler_table, 5);
    /* Columns span the full screen width (480px). */
    lv_table_set_column_width(profiler_table, 0, 140);
    lv_table_set_column_width(profiler_table, 1, 80);
    lv_table_set_column_width(profiler_table, 2, 80);
    lv_table_set_column_width(profiler_table, 3, 100);
    lv_table_set_column_width(profiler_table, 4, 80);
    lv_obj_set_style_pad_top(profiler_table, 5, LV_PART_ITEMS);
    lv_obj_set_style_pad_bottom(profiler_table, 5, LV_PART_ITEMS);
    lv_obj_set_style_text_font(profiler_table, &lv_font_montserrat_14, LV_PART_ITEMS);
    /* Fixed height, to leave a visible gap above the bottom of the
     * screen instead of the table growing to fit its content. */
    lv_obj_set_height(profiler_table, 197);
    lv_obj_align(profiler_table, LV_ALIGN_TOP_LEFT, 0, 60);
    lv_obj_clear_flag(profiler_table, LV_OBJ_FLAG_SCROLL_ELASTIC);

    lv_table_set_cell_value(profiler_table, 0, 0, "Fonction");
    lv_table_set_cell_value(profiler_table, 0, 1, "% CPU");
    lv_table_set_cell_value(profiler_table, 0, 2, "Echantillons");
    lv_table_set_cell_value(profiler_table, 0, 3, "Adresse");
    lv_table_set_cell_value(profiler_table, 0, 4, "Taille");

    profiler_timer = lv_timer_create(profiler_timer_cb, TASK_MGMT_REFRESH_PERIOD_MS, NULL);
    lv_timer_pause(profiler_timer);
    profiler_timer_cb(NULL);

    lv_obj_clear_flag(profiler_table, LV_OBJ_FLAG_SCROLL_ELASTIC);

    lv_obj_add_event_cb(task_management_screen, task_management_visibility_cb,
                         LV_EVENT_SCREEN_LOADED, NULL);
    lv_obj_add_event_cb(task_management_screen, task_management_visibility_cb,
                         LV_EVENT_SCREEN_UNLOADED, NULL);

    lvgl_unlock();
}
