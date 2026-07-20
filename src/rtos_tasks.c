#include "rtos_tasks.h"
#include "launcher.h"
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

static void back_to_menu_cb(lv_event_t *e)
{
    (void)e;
    printk("[rtos_tasks]: Back to menu\n");
    lv_screen_load(launcher_screen_get());
}

/* Appele par k_thread_foreach_unlocked() pour chaque thread vivant. */
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
    printk("[rtos_tasks]: Refreshing tasks table, %zu tasks\n", count);
    lv_table_set_row_count(tasks_table, count + 1);
    for (size_t i = 0; i < count; i++) {
        lv_table_set_cell_value(tasks_table, i + 1, 0, tasks[i].name);
        lv_table_set_cell_value(tasks_table, i + 1, 1, tasks[i].state);
        lv_table_set_cell_value_fmt(tasks_table, i + 1, 2, "%d", tasks[i].prio);
        lv_table_set_cell_value_fmt(tasks_table, i + 1, 3, "%u", (unsigned int)tasks[i].stack_free);
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
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *back_btn = lv_button_create(rtos_tasks_screen);
    lv_obj_set_size(back_btn, 80, 30);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_event_cb(back_btn, back_to_menu_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Menu");
    lv_obj_center(back_label);

    tasks_table = lv_table_create(rtos_tasks_screen);
    lv_table_set_column_count(tasks_table, 4);
    lv_table_set_column_width(tasks_table, 0, 150);
    lv_table_set_column_width(tasks_table, 1, 90);
    lv_table_set_column_width(tasks_table, 2, 50);
    lv_table_set_column_width(tasks_table, 3, 100);
    lv_obj_set_style_pad_top(tasks_table, 4, LV_PART_ITEMS);
    lv_obj_set_style_pad_bottom(tasks_table, 4, LV_PART_ITEMS);
    lv_obj_set_size(tasks_table, 390, 190);
    lv_obj_align(tasks_table, LV_ALIGN_TOP_LEFT, 20, 60);

    lv_table_set_cell_value(tasks_table, 0, 0, "Thread");
    lv_table_set_cell_value(tasks_table, 0, 1, "Etat");
    lv_table_set_cell_value(tasks_table, 0, 2, "Prio");
    lv_table_set_cell_value(tasks_table, 0, 3, "Pile libre (o)");

    lv_timer_create(tasks_timer_cb, TASKS_REFRESH_PERIOD_MS, NULL);
    tasks_timer_cb(NULL);

    lvgl_unlock();
}
