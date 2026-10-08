/**
 * @file
 * @brief RTOS thread monitor (DISABLED, not built - see CMakeLists.txt);
 *        replaced by task_management.c + pc_profiler.c, kept for reference.
 */

#include "rtos_tasks.h"
#include "launcher.h"
#include "events_logs.h"
#include <lvgl_zephyr.h>
#include <zephyr/kernel.h>
#include <string.h>

#define TASKS_REFRESH_PERIOD_MS 1000
#define TASKS_MAX_ROWS 10

struct task_row {
    char name[24];
    char state[16];
    int prio;
    size_t stack_free;
};

static lv_obj_t *rtos_tasks_screen;
static lv_obj_t *tasks_table;
static struct task_row tasks[TASKS_MAX_ROWS];
static struct task_row tasks_prev[TASKS_MAX_ROWS];
static size_t tasks_prev_count;
static lv_timer_t *tasks_timer;

static void back_to_menu_cb(lv_event_t *e)
{
    (void)e;
    printk("[rtos_tasks]: Back to menu\n");
    events_logs_add("[rtos_tasks] Back to menu");
    lv_screen_load(launcher_screen_get());
}

/* Called by k_thread_foreach_unlocked() for each live thread. */
static void thread_collect_cb(const struct k_thread *cthread, void *user_data)
{
    size_t *count = user_data;
    struct k_thread *thread = (struct k_thread *)cthread;
    struct task_row *row;
    const char *name;
    size_t stack_free;

    if (*count >= TASKS_MAX_ROWS) {
        return;
    }
    row = &tasks[*count];

    name = k_thread_name_get((k_tid_t)thread);
    if (!name || name[0] == '\0') {
        snprintk(row->name, sizeof(row->name), "%p", (void *)thread);
    } else {
        strncpy(row->name, name, sizeof(row->name) - 1);
        row->name[sizeof(row->name) - 1] = '\0';
    }

    k_thread_state_str((k_tid_t)thread, row->state, sizeof(row->state));
    row->prio = thread->base.prio;

    if (k_thread_stack_space_get(thread, &stack_free) != 0) {
        stack_free = 0;
    }
    row->stack_free = stack_free;

    (*count)++;
}

static void tasks_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    size_t count = 0;

    k_thread_foreach_unlocked(thread_collect_cb, &count);

    if (count != tasks_prev_count) {
        lv_table_set_row_count(tasks_table, count + 1);
        tasks_prev_count = count;
    }

    for (size_t i = 0; i < count; i++) {
        if (memcmp(&tasks[i], &tasks_prev[i], sizeof(tasks[i])) == 0) {
            continue;
        }
        lv_table_set_cell_value(tasks_table, i + 1, 0, tasks[i].name);
        lv_table_set_cell_value(tasks_table, i + 1, 1, tasks[i].state);
        lv_table_set_cell_value_fmt(tasks_table, i + 1, 2, "%d", tasks[i].prio);
        lv_table_set_cell_value_fmt(tasks_table, i + 1, 3, "%u", (unsigned int)tasks[i].stack_free);
        tasks_prev[i] = tasks[i];
    }
}

/* Refresh timer only runs while this screen is visible, to spare the LVGL
 * thread. */
static void rtos_tasks_visibility_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_SCREEN_LOADED) {
        lv_timer_resume(tasks_timer);
        lv_timer_ready(tasks_timer);
    } else {
        lv_timer_pause(tasks_timer);
    }
}

lv_obj_t *rtos_tasks_screen_get(void)
{
    return rtos_tasks_screen;
}

void rtos_tasks_init(void)
{
    lvgl_lock();

    rtos_tasks_screen = lv_obj_create(NULL);

    lv_obj_t *title = lv_label_create(rtos_tasks_screen);
    lv_label_set_text(title, "RTOS TASKS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    lv_obj_t *back_btn = lv_button_create(rtos_tasks_screen);
    lv_obj_set_size(back_btn, 80, 30);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_event_cb(back_btn, back_to_menu_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Menu");
    lv_obj_center(back_label);

    tasks_table = lv_table_create(rtos_tasks_screen);
    lv_table_set_column_count(tasks_table, 4);
    /* Columns span the full screen width (480px). */
    lv_table_set_column_width(tasks_table, 0, 190);
    lv_table_set_column_width(tasks_table, 1, 110);
    lv_table_set_column_width(tasks_table, 2, 60);
    lv_table_set_column_width(tasks_table, 3, 120);
    lv_obj_set_style_pad_top(tasks_table, 5, LV_PART_ITEMS);
    lv_obj_set_style_pad_bottom(tasks_table, 5, LV_PART_ITEMS);
    /* Fixed height, to leave a visible gap above the bottom of the
     * screen instead of the table growing to fit its content. */
    lv_obj_set_height(tasks_table, 197);
    lv_obj_align(tasks_table, LV_ALIGN_TOP_LEFT, 0, 60);
    lv_obj_clear_flag(tasks_table, LV_OBJ_FLAG_SCROLL_ELASTIC);

    lv_table_set_cell_value(tasks_table, 0, 0, "Thread");
    lv_table_set_cell_value(tasks_table, 0, 1, "Etat");
    lv_table_set_cell_value(tasks_table, 0, 2, "Prio");
    lv_table_set_cell_value(tasks_table, 0, 3, "Pile libre (o)");

    tasks_timer = lv_timer_create(tasks_timer_cb, TASKS_REFRESH_PERIOD_MS, NULL);
    lv_timer_pause(tasks_timer);
    tasks_timer_cb(NULL);

    lv_obj_add_event_cb(rtos_tasks_screen, rtos_tasks_visibility_cb,
                         LV_EVENT_SCREEN_LOADED, NULL);
    lv_obj_add_event_cb(rtos_tasks_screen, rtos_tasks_visibility_cb,
                         LV_EVENT_SCREEN_UNLOADED, NULL);

    lvgl_unlock();
}
