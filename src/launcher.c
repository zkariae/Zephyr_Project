/**
 * @file
 * @brief Home screen: grid of tiles to open each app screen.
 */

#include "launcher.h"
#include "system_overview.h"
#include "task_management.h"
#include "events_logs.h"
#include "live_variables.h"
#include "plot_display.h"
#include "async_printk.h"
#include <lvgl_zephyr.h>
#include <string.h>

#define TILE_W 160
#define TILE_H 136
#define STATUS_TILE_REFRESH_MS 1000

static lv_obj_t *launcher_screen;
static lv_obj_t *status_connected_label;
static lv_obj_t *status_time_label;
static lv_obj_t *status_date_label;

static void open_system_overview(lv_event_t *e)
{
    (void)e;
    printk("[launcher]: Opening system overview\n");
    events_logs_add("[launcher] Ouverture System Overview");
    lv_screen_load(system_overview_screen_get());
}

static void open_task_management(lv_event_t *e)
{
    (void)e;
    printk("[launcher]: Opening task management\n");
    events_logs_add("[launcher] Ouverture Task Management");
    lv_screen_load(task_management_screen_get());
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

/* Full-tile (160x136) button: solid color, white top-left text, no
 * rounded corners or border. */
static lv_obj_t *create_tile_button(lv_obj_t *parent, int x, int y, lv_color_t color,
                                     const char *text, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, TILE_W, TILE_H);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_radius(btn, 0, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_outline_width(btn, 0, 0);
    lv_obj_set_style_bg_color(btn, color, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 10, 10);

    return btn;
}

/* Only writes a label when its text actually changed, to avoid needless
 * LVGL invalidations each tick. */
static void status_tile_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    char time_str[ASYNC_PRINTK_TIME_LEN + 1];
    char date_str[ASYNC_PRINTK_DATE_LEN + 1];
    bool connected;
    bool has_data = async_printk_get_link_status(time_str, date_str, &connected);

    const char *connected_text = connected ? LV_SYMBOL_USB " Connected" : LV_SYMBOL_USB " Disconnected";
    if (strcmp(lv_label_get_text(status_connected_label), connected_text) != 0) {
        lv_label_set_text(status_connected_label, connected_text);
        lv_obj_set_style_text_color(status_connected_label,
                                     connected ? lv_color_hex(0x00A651) : lv_color_hex(0xB71C1C),
                                     0);
    }

    if (has_data) {
        if (strcmp(lv_label_get_text(status_time_label), time_str) != 0) {
            lv_label_set_text(status_time_label, time_str);
        }
        if (strcmp(lv_label_get_text(status_date_label), date_str) != 0) {
            lv_label_set_text(status_date_label, date_str);
        }
    }
}

/* Top-left tile: status/clock/date received live over UART from
 * tools/send_time.py (no RTC on the board, the PC is the time source). */
static void create_status_tile(lv_obj_t *parent, int x, int y)
{
    lv_obj_t *tile = lv_obj_create(parent);
    lv_obj_set_size(tile, TILE_W, TILE_H);
    lv_obj_set_pos(tile, x, y);
    lv_obj_set_style_radius(tile, 0, 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_set_style_bg_color(tile, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_CLICKABLE);

    status_connected_label = lv_label_create(tile);
    lv_label_set_text(status_connected_label, "Disconnected");
    lv_obj_set_style_text_color(status_connected_label, lv_color_hex(0xB71C1C), 0);
    lv_obj_align(status_connected_label, LV_ALIGN_TOP_LEFT, 8, 6);

    status_time_label = lv_label_create(tile);
    lv_label_set_text(status_time_label, "--:--:--");
    lv_obj_set_style_text_font(status_time_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(status_time_label, lv_color_black(), 0);
    lv_obj_align(status_time_label, LV_ALIGN_LEFT_MID, 8, 4);

    status_date_label = lv_label_create(tile);
    lv_label_set_text(status_date_label, "--/--/----");
    lv_obj_set_style_text_color(status_date_label, lv_color_black(), 0);
    lv_obj_align(status_date_label, LV_ALIGN_BOTTOM_LEFT, 8, -8);

    lv_timer_create(status_tile_timer_cb, STATUS_TILE_REFRESH_MS, NULL);
}

lv_obj_t *launcher_screen_get(void)
{
    return launcher_screen;
}

void launcher_init(void)
{
    lvgl_lock();

    launcher_screen = lv_obj_create(NULL);
    lv_obj_set_style_pad_all(launcher_screen, 0, 0);
    lv_obj_set_style_border_width(launcher_screen, 0, 0);
    lv_obj_clear_flag(launcher_screen, LV_OBJ_FLAG_SCROLLABLE);

    /* Row 1 */
    create_status_tile(launcher_screen, 0, 0);
    create_tile_button(launcher_screen, TILE_W, 0, lv_color_hex(0xB71C1C),
                        "SYSTEM\nOVERVIEW", open_system_overview);
    create_tile_button(launcher_screen, 2 * TILE_W, 0, lv_color_hex(0x8E2C8F),
                        "TASK\nMANAGEMENT", open_task_management);

    /* Row 2 */
    create_tile_button(launcher_screen, 0, TILE_H, lv_color_hex(0x00A651),
                        "PLOT\nDISPLAY", open_plot_display);
    create_tile_button(launcher_screen, TILE_W, TILE_H, lv_color_hex(0xFFC107),
                        "EVENT\nLOGS", open_event_info);
    create_tile_button(launcher_screen, 2 * TILE_W, TILE_H, lv_color_hex(0x1B5E73),
                        "LIVE\nVARIABLES", open_live_variables);

    lv_screen_load(launcher_screen);

    lvgl_unlock();
}
