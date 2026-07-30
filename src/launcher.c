#include "launcher.h"
#include "system_overview.h"
#include "rtos_tasks.h"
#include "events_logs.h"
#include "live_variables.h"
#include "plot_display.h"
#include <lvgl_zephyr.h>

#define TILE_W 160
#define TILE_H 136

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

/* Cree une tuile bouton pleine case (160x136), fond colore, texte blanc
 * aligne en haut a gauche, sans coins arrondis ni bordure. */
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
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 10, 10);

    return btn;
}

/* Tuile haut-gauche : statut/horloge statique (pas de RTC dans le projet). */
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

    lv_obj_t *connected_label = lv_label_create(tile);
    lv_label_set_text(connected_label, "Connected");
    lv_obj_set_style_text_color(connected_label, lv_color_black(), 0);
    lv_obj_align(connected_label, LV_ALIGN_TOP_LEFT, 8, 6);

    lv_obj_t *time_label = lv_label_create(tile);
    lv_label_set_text(time_label, "12:11");
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(time_label, lv_color_black(), 0);
    lv_obj_align(time_label, LV_ALIGN_LEFT_MID, 8, 4);

    lv_obj_t *date_label = lv_label_create(tile);
    lv_label_set_text(date_label, "30/07/2026");
    lv_obj_set_style_text_color(date_label, lv_color_black(), 0);
    lv_obj_align(date_label, LV_ALIGN_BOTTOM_LEFT, 8, -8);
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

    /* Ligne 1 */
    create_status_tile(launcher_screen, 0, 0);
    create_tile_button(launcher_screen, TILE_W, 0, lv_color_hex(0xB71C1C),
                        "SYSTEM\nOVERVIEW", open_system_overview);
    create_tile_button(launcher_screen, 2 * TILE_W, 0, lv_color_hex(0x8E2C8F),
                        "RTOS\nTASKS", open_rtos_info);

    /* Ligne 2 */
    create_tile_button(launcher_screen, 0, TILE_H, lv_color_hex(0x00A651),
                        "PLOT\nDISPLAY", open_plot_display);
    create_tile_button(launcher_screen, TILE_W, TILE_H, lv_color_hex(0xFFC107),
                        "EVENT\nLOGS", open_event_info);
    create_tile_button(launcher_screen, 2 * TILE_W, TILE_H, lv_color_hex(0x1B5E73),
                        "LIVE\nVARIABLES", open_live_variables);

    lv_screen_load(launcher_screen);

    lvgl_unlock();
}
