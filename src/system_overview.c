#include "system_overview.h"
#include "launcher.h"
#include "events_logs.h"
#include "temperature.h"
#include "async_printk.h"
#include <lvgl_zephyr.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <stdio.h>
#include <string.h>

#define REFRESH_PERIOD_MS 1000

#define SCREEN_W 480
#define SCREEN_H 272

/* "card" : rectangle blanc, sans ombre (cout de rendu logiciel trop eleve
 * sur ce MCU, voir CONFIG_STM32_LTDC_FB_NUM=2 dans prj.conf pour le
 * precedent deja rencontre sur ce projet). */
#define CARD_MARGIN 8
#define CARD_W (SCREEN_W - 2 * CARD_MARGIN)
#define CARD_H (SCREEN_H - 2 * CARD_MARGIN)

#define COLOR_DIVIDER     0x24406B
#define COLOR_ICON_BG     0x24406B
#define COLOR_ICON_FG     0x7FB2FF
#define COLOR_LABEL_FG    0x555555
#define COLOR_BAR_TRACK   0x0F1F35
#define COLOR_BAR_FILL    0x2F9E44

#define ROW_PAD_X    14
#define ROW_ICON_SIZE 22
#define ROW_H        26
#define ROW_H_BAR    32
#define ROWS_START_Y 42

/* Plages de remplissage des barres : jonction STM32F7 (0-85 C, limite haute
 * datasheet) et VDDA nominal (3.0-3.6 V, plage de tolerance alimentation du
 * chip) - donnent une lecture visuelle immediate au lieu d'une barre
 * toujours pleine. */
#define DIE_TEMP_BAR_MIN_CENTI_C 0
#define DIE_TEMP_BAR_MAX_CENTI_C 8500
#define VREF_BAR_MIN_MV 3000
#define VREF_BAR_MAX_MV 3600

static lv_obj_t *system_overview_screen;
static lv_obj_t *card;
static lv_obj_t *clock_label;
static lv_obj_t *uptime_value_label;
static lv_obj_t *die_temp_value_label;
static lv_obj_t *die_temp_bar;
static lv_obj_t *vref_value_label;
static lv_obj_t *vref_bar;
static lv_timer_t *refresh_timer;

static void back_to_menu_cb(lv_event_t *e)
{
    (void)e;
    printk("[system_overview]: Back to menu\n");
    events_logs_add("[system_overview] Retour menu");
    lv_screen_load(launcher_screen_get());
}

/* Met a jour un label uniquement si le texte formatte a change, pour ne
 * pas invalider/redessiner un widget dont la valeur affichee est identique
 * (meme logique que status_tile_timer_cb dans launcher.c). */
static void label_set_text_if_changed(lv_obj_t *label, const char *text)
{
    if (strcmp(lv_label_get_text(label), text) != 0) {
        lv_label_set_text(label, text);
    }
}

/* Cree une ligne "icone + libelle + valeur" dans la carte, a la position Y
 * donnee. Si bar_out n'est pas NULL, ajoute une fine barre de progression
 * sous la valeur (temperature/VREF) ; l'appelant est alors responsable de
 * reserver ROW_H_BAR (au lieu de ROW_H) pour cette ligne. Retourne le label
 * de valeur, pour que l'appelant puisse le mettre a jour periodiquement. */
static lv_obj_t *create_row(lv_obj_t *parent, int32_t y, const char *icon,
                             const char *label_text, const char *initial_value,
                             lv_obj_t **bar_out)
{
    lv_obj_t *icon_bg = lv_obj_create(parent);
    lv_obj_set_size(icon_bg, ROW_ICON_SIZE, ROW_ICON_SIZE);
    lv_obj_set_pos(icon_bg, ROW_PAD_X, y);
    lv_obj_set_style_radius(icon_bg, 6, 0);
    lv_obj_set_style_bg_color(icon_bg, lv_color_hex(COLOR_ICON_BG), 0);
    lv_obj_set_style_bg_opa(icon_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(icon_bg, 0, 0);
    lv_obj_clear_flag(icon_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(icon_bg, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *icon_label = lv_label_create(icon_bg);
    lv_label_set_text(icon_label, icon);
    lv_obj_set_style_text_color(icon_label, lv_color_hex(COLOR_ICON_FG), 0);
    lv_obj_center(icon_label);

    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, label_text);
    lv_obj_set_style_text_color(label, lv_color_hex(COLOR_LABEL_FG), 0);
    lv_obj_set_pos(label, ROW_PAD_X + ROW_ICON_SIZE + 10, y + 3);

    lv_obj_t *value = lv_label_create(parent);
    lv_label_set_text(value, initial_value);
    lv_obj_set_style_text_color(value, lv_color_black(), 0);
    lv_obj_align(value, LV_ALIGN_TOP_RIGHT, -ROW_PAD_X, y + 3);

    if (bar_out != NULL) {
        lv_obj_t *bar = lv_bar_create(parent);
        lv_obj_set_size(bar, CARD_W - 2 * ROW_PAD_X, 4);
        lv_obj_set_pos(bar, ROW_PAD_X, y + ROW_ICON_SIZE + 3);
        lv_obj_set_style_radius(bar, 2, LV_PART_MAIN);
        lv_obj_set_style_bg_color(bar, lv_color_hex(COLOR_BAR_TRACK), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(bar, 2, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(bar, lv_color_hex(COLOR_BAR_FILL), LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
        *bar_out = bar;
    }

    return value;
}

static void refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    char buf[40];

    char time_str[ASYNC_PRINTK_TIME_LEN + 1];
    char date_str[ASYNC_PRINTK_DATE_LEN + 1];
    bool connected;

    if (async_printk_get_link_status(time_str, date_str, &connected)) {
        label_set_text_if_changed(clock_label, time_str);
    }

    uint32_t uptime_s = (uint32_t)(k_uptime_get() / 1000);
    snprintf(buf, sizeof(buf), "%u s", uptime_s);
    label_set_text_if_changed(uptime_value_label, buf);

    /* temperature_get_die_centi_c()/temperature_get_vref_mv() ne font
     * qu'un atomic_get (voir temperature.c) : aucun risque de bloquer le
     * thread LVGL ici, contrairement a un sensor_sample_fetch() direct. */
    int32_t die_centi_c = temperature_get_die_centi_c();
    int32_t whole = die_centi_c / 100;
    int32_t frac = die_centi_c % 100;

    if (frac < 0) {
        frac = -frac;
    }
    snprintf(buf, sizeof(buf), "%d.%02d C", (int)whole, (int)frac);
    label_set_text_if_changed(die_temp_value_label, buf);
    lv_bar_set_value(die_temp_bar, (int32_t)die_centi_c, LV_ANIM_OFF);

    int32_t vref_mv = temperature_get_vref_mv();
    snprintf(buf, sizeof(buf), "%d mV", (int)vref_mv);
    label_set_text_if_changed(vref_value_label, buf);
    lv_bar_set_value(vref_bar, vref_mv, LV_ANIM_OFF);
}

/* Le timer ne tourne que lorsque cet ecran est reellement affiche, pour
 * ne pas charger le thread LVGL en permanence pour un ecran invisible. */
static void system_overview_visibility_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_SCREEN_LOADED) {
        lv_timer_resume(refresh_timer);
        lv_timer_ready(refresh_timer);
    } else {
        lv_timer_pause(refresh_timer);
    }
}

lv_obj_t *system_overview_screen_get(void)
{
    return system_overview_screen;
}

void system_overview_init(void)
{
    lvgl_lock();

    /* Pas de bg_color explicite ici : on garde le theme par defaut, pour
     * que le fond soit exactement le meme que celui de Live Variables
     * (qui ne le surcharge pas non plus). */
    system_overview_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(system_overview_screen, LV_OBJ_FLAG_SCROLLABLE);

    card = lv_obj_create(system_overview_screen);
    lv_obj_set_size(card, CARD_W, CARD_H);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(card, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_shadow_width(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *back_btn = lv_button_create(card);
    lv_obj_set_size(back_btn, 28, 28);
    lv_obj_set_pos(back_btn, 8, 6);
    lv_obj_set_style_radius(back_btn, 6, 0);
    lv_obj_set_style_shadow_width(back_btn, 0, 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(COLOR_ICON_BG), 0);
    lv_obj_add_event_cb(back_btn, back_to_menu_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_BARS);
    lv_obj_center(back_label);

    lv_obj_t *title = lv_label_create(card);
    lv_label_set_text(title, LV_SYMBOL_SETTINGS " SYSTEM OVERVIEW");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    clock_label = lv_label_create(card);
    lv_label_set_text(clock_label, "--:--:--");
    lv_obj_set_style_text_color(clock_label, lv_color_hex(COLOR_LABEL_FG), 0);
    lv_obj_align(clock_label, LV_ALIGN_TOP_RIGHT, -ROW_PAD_X, 12);

    /* Separateur fin sous l'en-tete : simple rectangle plat (pas de flou),
     * dessine une seule fois - ne rejoue jamais dans le timer de rafraichissement. */
    lv_obj_t *divider = lv_obj_create(card);
    lv_obj_set_size(divider, CARD_W - 2 * ROW_PAD_X, 1);
    lv_obj_set_pos(divider, ROW_PAD_X, ROWS_START_Y - 6);
    lv_obj_set_style_bg_color(divider, lv_color_hex(COLOR_DIVIDER), 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_clear_flag(divider, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(divider, LV_OBJ_FLAG_CLICKABLE);

    /* Valeurs sourcees depuis .config/devicetree/linker.cmd (build_projet1) :
     * CPU = CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC (PLL HSE 25MHz*432/25/2 = 216MHz) ;
     * Flash = fenetre XIP QSPI a 0x90000000 (CONFIG_FLASH_SIZE/BASE_ADDRESS) ;
     * RAM = SRAM interne (CONFIG_SRAM_SIZE) + SDRAM externe (sdram1, framebuffer LVGL). */
    int32_t y = ROWS_START_Y;

    create_row(card, y, LV_SYMBOL_HOME, "Board", "STM32F7508-DK", NULL);
    y += ROW_H;
    create_row(card, y, LV_SYMBOL_REFRESH, "CPU", "216 MHz (Cortex-M7)", NULL);
    y += ROW_H;
    create_row(card, y, LV_SYMBOL_SD_CARD, "Flash (XIP)", "1 MB @ 0x90000000", NULL);
    y += ROW_H;
    create_row(card, y, LV_SYMBOL_DRIVE, "RAM", "256 KB SRAM + 8 MB SDRAM", NULL);
    y += ROW_H;
    uptime_value_label = create_row(card, y, LV_SYMBOL_LOOP, "Uptime", "0 s", NULL);
    y += ROW_H;
    die_temp_value_label = create_row(card, y, LV_SYMBOL_TINT, "Die temperature", "-- C", &die_temp_bar);
    lv_bar_set_range(die_temp_bar, DIE_TEMP_BAR_MIN_CENTI_C, DIE_TEMP_BAR_MAX_CENTI_C);
    y += ROW_H_BAR;
    vref_value_label = create_row(card, y, LV_SYMBOL_CHARGE, "VREF+ (VDDA)", "-- mV", &vref_bar);
    lv_bar_set_range(vref_bar, VREF_BAR_MIN_MV, VREF_BAR_MAX_MV);

    refresh_timer = lv_timer_create(refresh_timer_cb, REFRESH_PERIOD_MS, NULL);
    lv_timer_pause(refresh_timer);
    refresh_timer_cb(NULL);

    lv_obj_add_event_cb(system_overview_screen, system_overview_visibility_cb,
                         LV_EVENT_SCREEN_LOADED, NULL);
    lv_obj_add_event_cb(system_overview_screen, system_overview_visibility_cb,
                         LV_EVENT_SCREEN_UNLOADED, NULL);

    lvgl_unlock();
}
