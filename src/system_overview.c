#include "system_overview.h"
#include "launcher.h"
#include "events_logs.h"
#include "temperature.h"
#include <lvgl_zephyr.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#define UPTIME_REFRESH_PERIOD_MS 1000

static lv_obj_t *system_overview_screen;
static lv_obj_t *uptime_label;
static lv_obj_t *die_temp_label;
static lv_obj_t *vref_label;
static lv_timer_t *uptime_timer;

static void back_to_menu_cb(lv_event_t *e)
{
    (void)e;
    printk("[system_overview]: Back to menu\n");
    events_logs_add("[system_overview] Retour menu");
    lv_screen_load(launcher_screen_get());
}

static void uptime_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    uint32_t uptime_s = (uint32_t)(k_uptime_get() / 1000);
    printk("[system_overview]: Uptime: %u s\n", uptime_s);
    lv_label_set_text_fmt(uptime_label, "Uptime: %u s", uptime_s);

    /* temperature_get_die_centi_c()/temperature_get_vref_mv() ne font
     * qu'un atomic_get (voir temperature.c) : aucun risque de bloquer le
     * thread LVGL ici, contrairement a un sensor_sample_fetch() direct. */
    int32_t die_centi_c = temperature_get_die_centi_c();
    int32_t whole = die_centi_c / 100;
    int32_t frac = die_centi_c % 100;

    if (frac < 0) {
        frac = -frac;
    }
    lv_label_set_text_fmt(die_temp_label, "Die temp: %d.%02d C", (int)whole, (int)frac);

    lv_label_set_text_fmt(vref_label, "VREF+ (VDDA): %d mV", (int)temperature_get_vref_mv());
}

/* Le timer ne tourne que lorsque cet ecran est reellement affiche, pour
 * ne pas charger le thread LVGL en permanence pour un ecran invisible. */
static void system_overview_visibility_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_SCREEN_LOADED) {
        lv_timer_resume(uptime_timer);
        lv_timer_ready(uptime_timer);
    } else {
        lv_timer_pause(uptime_timer);
    }
}

lv_obj_t *system_overview_screen_get(void)
{
    return system_overview_screen;
}

void system_overview_init(void)
{
    lvgl_lock();

    system_overview_screen = lv_obj_create(NULL);

    lv_obj_t *title = lv_label_create(system_overview_screen);
    lv_label_set_text(title, "SYSTEM OVERVIEW");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *back_btn = lv_button_create(system_overview_screen);
    lv_obj_set_size(back_btn, 80, 30);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_event_cb(back_btn, back_to_menu_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Menu");
    lv_obj_center(back_label);

    /* Valeurs sourcees depuis .config/devicetree/linker.cmd (build_projet1) :
     * CPU = CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC (PLL HSE 25MHz*432/25/2 = 216MHz) ;
     * Flash = fenetre XIP QSPI a 0x90000000 (CONFIG_FLASH_SIZE/BASE_ADDRESS) ;
     * RAM = SRAM interne (CONFIG_SRAM_SIZE) + SDRAM externe (sdram1, framebuffer LVGL). */
    static const char *const info_lines[] = {
        "Board: STM32F7508-DK",
        "CPU: 216 MHz (Cortex-M7)",
        "Flash (XIP): 1 MB @ 0x90000000",
        "RAM: 256 KB SRAM + 8 MB SDRAM",
    };

    for (size_t i = 0; i < ARRAY_SIZE(info_lines); i++) {
        lv_obj_t *label = lv_label_create(system_overview_screen);
        lv_label_set_text(label, info_lines[i]);
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 20, 60 + (int32_t)i * 30);
    }

    uptime_label = lv_label_create(system_overview_screen);
    lv_obj_align(uptime_label, LV_ALIGN_TOP_LEFT, 20, 60 + (int32_t)ARRAY_SIZE(info_lines) * 30);

    die_temp_label = lv_label_create(system_overview_screen);
    lv_obj_align(die_temp_label, LV_ALIGN_TOP_LEFT, 20, 60 + (int32_t)(ARRAY_SIZE(info_lines) + 1) * 30);

    vref_label = lv_label_create(system_overview_screen);
    lv_obj_align(vref_label, LV_ALIGN_TOP_LEFT, 20, 60 + (int32_t)(ARRAY_SIZE(info_lines) + 2) * 30);

    uptime_timer = lv_timer_create(uptime_timer_cb, UPTIME_REFRESH_PERIOD_MS, NULL);
    lv_timer_pause(uptime_timer);
    uptime_timer_cb(NULL);

    lv_obj_add_event_cb(system_overview_screen, system_overview_visibility_cb,
                         LV_EVENT_SCREEN_LOADED, NULL);
    lv_obj_add_event_cb(system_overview_screen, system_overview_visibility_cb,
                         LV_EVENT_SCREEN_UNLOADED, NULL);

    lvgl_unlock();
}
