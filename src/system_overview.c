#include "system_overview.h"
#include "launcher.h"
#include "events_logs.h"
#include <lvgl_zephyr.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#define UPTIME_REFRESH_PERIOD_MS 1000

static lv_obj_t *system_overview_screen;
static lv_obj_t *uptime_label;

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
    lv_timer_create(uptime_timer_cb, UPTIME_REFRESH_PERIOD_MS, NULL);
    uptime_timer_cb(NULL);

    lvgl_unlock();
}
