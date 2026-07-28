#include "events_logs.h"
#include "launcher.h"
#include <lvgl_zephyr.h>
#include <zephyr/kernel.h>
#include <stdarg.h>
#include <stdbool.h>

#define EVENTS_LOG_MAX_LINES     20
#define EVENTS_LOG_LINE_LEN      56
#define EVENTS_LOG_REFRESH_MS    500

struct event_line {
    uint32_t uptime_ms;
    char msg[EVENTS_LOG_LINE_LEN];
};

static lv_obj_t *events_logs_screen;
static lv_obj_t *console_container;
static lv_obj_t *console_label;
static lv_timer_t *console_timer;

static struct event_line events[EVENTS_LOG_MAX_LINES];
static size_t events_write_idx;
static size_t events_count;

/* Buffer d'affichage : une ligne horodatee par entree, la plus recente en
 * bas (comme un terminal). +1 pour le '\n' entre lignes. */
static char console_text[EVENTS_LOG_MAX_LINES * (EVENTS_LOG_LINE_LEN + 1) + 1];

void events_logs_add(const char *fmt, ...)
{
    struct event_line *line = &events[events_write_idx];
    va_list args;

    line->uptime_ms = (uint32_t)k_uptime_get();

    va_start(args, fmt);
    vsnprintk(line->msg, sizeof(line->msg), fmt, args);
    va_end(args);

    events_write_idx = (events_write_idx + 1) % EVENTS_LOG_MAX_LINES;
    if (events_count < EVENTS_LOG_MAX_LINES) {
        events_count++;
    }
}

static void back_to_menu_cb(lv_event_t *e)
{
    (void)e;
    printk("[events_logs]: Back to menu\n");
    events_logs_add("[events_logs] Retour menu");
    lv_screen_load(launcher_screen_get());
}

static void console_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    /* Ne recolle en bas que si l'utilisateur n'a pas scrolle vers le haut :
     * sinon chaque tick (500 ms) lui arrachait le geste tactile des mains. */
    bool was_at_bottom = lv_obj_get_scroll_bottom(console_container) <= 4;
    size_t start = (events_count < EVENTS_LOG_MAX_LINES) ? 0 : events_write_idx;
    size_t off = 0;

    for (size_t i = 0; i < events_count; i++) {
        const struct event_line *line = &events[(start + i) % EVENTS_LOG_MAX_LINES];
        int n = snprintk(&console_text[off], sizeof(console_text) - off,
                         "%s[%u.%03u] %s",
                         (i == 0) ? "" : "\n",
                         line->uptime_ms / 1000, line->uptime_ms % 1000,
                         line->msg);

        if (n < 0 || (size_t)n >= sizeof(console_text) - off) {
            break;
        }
        off += (size_t)n;
    }
    console_text[off] = '\0';

    lv_label_set_text(console_label, console_text);
    if (was_at_bottom) {
        lv_obj_scroll_to_y(console_container, LV_COORD_MAX, LV_ANIM_OFF);
    }
}

/* Le timer ne tourne que lorsque cet ecran est reellement affiche, pour
 * ne pas charger le thread LVGL en permanence pour un ecran invisible.
 * events_logs_add() continue d'ecrire dans le ring buffer independamment
 * de l'etat du timer : aucune entree n'est perdue pendant la pause. */
static void events_logs_visibility_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_SCREEN_LOADED) {
        lv_timer_resume(console_timer);
        lv_timer_ready(console_timer);
    } else {
        lv_timer_pause(console_timer);
    }
}

lv_obj_t *events_logs_screen_get(void)
{
    return events_logs_screen;
}

void events_logs_init(void)
{
    lvgl_lock();

    events_logs_screen = lv_obj_create(NULL);

    lv_obj_t *title = lv_label_create(events_logs_screen);
    lv_label_set_text(title, "EVENT LOGS");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *back_btn = lv_button_create(events_logs_screen);
    lv_obj_set_size(back_btn, 80, 30);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_event_cb(back_btn, back_to_menu_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Menu");
    lv_obj_center(back_label);

    console_container = lv_obj_create(events_logs_screen);
    lv_obj_set_size(console_container, 440, 190);
    lv_obj_align(console_container, LV_ALIGN_TOP_LEFT, 20, 60);
    lv_obj_set_scroll_dir(console_container, LV_DIR_VER);

    console_label = lv_label_create(console_container);
    lv_obj_set_width(console_label, lv_pct(100));
    lv_label_set_long_mode(console_label, LV_LABEL_LONG_MODE_WRAP);

    console_timer = lv_timer_create(console_timer_cb, EVENTS_LOG_REFRESH_MS, NULL);
    lv_timer_pause(console_timer);
    console_timer_cb(NULL);

    lv_obj_add_event_cb(events_logs_screen, events_logs_visibility_cb,
                         LV_EVENT_SCREEN_LOADED, NULL);
    lv_obj_add_event_cb(events_logs_screen, events_logs_visibility_cb,
                         LV_EVENT_SCREEN_UNLOADED, NULL);

    lvgl_unlock();
}
