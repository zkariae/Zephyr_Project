#include "app_calculator.h"
#include "launcher.h"
#include <lvgl_zephyr.h>
#include <zephyr/kernel.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CALC_THREAD_STACK_SIZE 2048
#define CALC_THREAD_PRIORITY   7
#define CALC_ENTRY_MAX         16

K_MSGQ_DEFINE(calc_msgq, sizeof(char), 16, 1);

static lv_obj_t *calculator_screen;
static lv_obj_t *display_label;

/* "\n" termine une ligne, "" termine la map (voir lv_buttonmatrix.h) */
static const char *const calc_btnm_map[] = {
    "7", "8", "9", "/", "\n",
    "4", "5", "6", "*", "\n",
    "1", "2", "3", "-", "\n",
    "C", "0", ".", "+", "\n",
    "=", "",
};

static void back_to_menu_cb(lv_event_t *e)
{
    (void)e;
    lv_screen_load(launcher_screen_get());
}

/* Callback LVGL (contexte thread workqueue LVGL) : ne fait aucun calcul,
 * pousse juste le caractere presse dans la queue pour le thread calculatrice. */
static void btnm_event_cb(lv_event_t *e)
{
    lv_obj_t *btnm = lv_event_get_target_obj(e);
    uint32_t id = lv_buttonmatrix_get_selected_button(btnm);
    const char *txt = lv_buttonmatrix_get_button_text(btnm, id);

    if (txt == NULL || txt[0] == '\0') {
        return;
    }

    char c = txt[0];

    (void)k_msgq_put(&calc_msgq, &c, K_NO_WAIT);
}

lv_obj_t *calculator_screen_get(void)
{
    return calculator_screen;
}

static double apply_op(double a, double b, char op)
{
    switch (op) {
    case '+':
        return a + b;
    case '-':
        return a - b;
    case '*':
        return a * b;
    case '/':
        return (b != 0.0) ? a / b : 0.0;
    default:
        return b;
    }
}

static void set_display(const char *text)
{
    lvgl_lock();
    lv_label_set_text(display_label, text);
    lvgl_unlock();
}

static void calculator_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    double operand1 = 0.0;
    char pending_op = 0;
    bool start_new_entry = true;
    char entry[CALC_ENTRY_MAX] = "0";
    char c;

    while (1) {
        k_msgq_get(&calc_msgq, &c, K_FOREVER);

        if (c == 'C') {
            operand1 = 0.0;
            pending_op = 0;
            start_new_entry = true;
            strcpy(entry, "0");
            set_display(entry);
            continue;
        }

        if (c >= '0' && c <= '9') {
            if (start_new_entry) {
                entry[0] = '\0';
                start_new_entry = false;
            }
            size_t len = strlen(entry);

            if (len < CALC_ENTRY_MAX - 1) {
                entry[len] = c;
                entry[len + 1] = '\0';
            }
            set_display(entry);
            continue;
        }

        if (c == '.') {
            if (start_new_entry) {
                strcpy(entry, "0");
                start_new_entry = false;
            }
            if (strchr(entry, '.') == NULL && strlen(entry) < CALC_ENTRY_MAX - 1) {
                strcat(entry, ".");
            }
            set_display(entry);
            continue;
        }

        if (c == '+' || c == '-' || c == '*' || c == '/') {
            double val = atof(entry);

            if (pending_op != 0 && !start_new_entry) {
                operand1 = apply_op(operand1, val, pending_op);
            } else {
                operand1 = val;
            }
            pending_op = c;
            start_new_entry = true;
            snprintf(entry, sizeof(entry), "%g", operand1);
            set_display(entry);
            continue;
        }

        if (c == '=') {
            if (pending_op != 0 && !start_new_entry) {
                double val = atof(entry);

                operand1 = apply_op(operand1, val, pending_op);
                pending_op = 0;
                start_new_entry = true;
                snprintf(entry, sizeof(entry), "%g", operand1);
                set_display(entry);
            }
            continue;
        }
    }
}

K_THREAD_DEFINE(calc_tid, CALC_THREAD_STACK_SIZE, calculator_thread, NULL, NULL, NULL,
                CALC_THREAD_PRIORITY, 0, 0);

void calculator_init(void)
{
    lvgl_lock();

    calculator_screen = lv_obj_create(NULL);

    display_label = lv_label_create(calculator_screen);
    lv_label_set_text(display_label, "0");
    lv_obj_align(display_label, LV_ALIGN_TOP_RIGHT, -10, 10);

    lv_obj_t *btnm = lv_buttonmatrix_create(calculator_screen);
    lv_buttonmatrix_set_map(btnm, calc_btnm_map);
    lv_obj_set_size(btnm, 280, 200);
    lv_obj_align(btnm, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_event_cb(btnm, btnm_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *back_btn = lv_button_create(calculator_screen);
    lv_obj_set_size(back_btn, 80, 30);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_event_cb(back_btn, back_to_menu_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Menu");
    lv_obj_center(back_label);

    lvgl_unlock();
}
