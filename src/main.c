/**
 * @file
 * @brief Application entry point: boots the display, runs the splash
 *        sequence, then initializes every screen module and the
 *        background watchdog feed loop.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/sys/printk.h>
#include <lvgl_mem.h>

#include "system_overview.h"
#include "task_management.h"
#include "pc_profiler.h"
#include "events_logs.h"
#include "live_variables.h"
#include "launcher.h"
#include "splash.h"
#include "temperature.h"
#include "mpu6050_input.h"
#include "async_printk.h"
#include "watchdog.h"

#define SPLASH_DELAY_MS 1500

int main(void)
{
    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

    async_printk_init();

    watchdog_report_reset_cause();

    printk("[main]: Projet1 demarre, verification de l'ecran...\n");
    events_logs_add("[main] Projet1 starting");

    if (!device_is_ready(display_dev)) {
        printk("[main]: Ecran non pret\n");
        events_logs_add("[main] Display not ready");
        return -1;
    }
    printk("[main]: Ecran pret\n");
    events_logs_add("[main] Display ready");

    if (watchdog_init() != 0) {
        printk("[main]: watchdog_init ECHEC (pas de filet de securite IWDG)\n");
        events_logs_add("[main] watchdog_init FAILED");
    }

    splash_show_image1();

    display_blanking_off(display_dev);
    printk("[main]: splash image1 affichee\n");
    k_sleep(K_MSEC(SPLASH_DELAY_MS));

    splash_show_image2();
    printk("[main]: splash image2 affichee\n");
    k_sleep(K_MSEC(SPLASH_DELAY_MS));

    splash_cleanup();

    system_overview_init();
    printk("[main]: system_overview_init OK\n");

    pc_profiler_init();
    printk("[main]: pc_profiler_init OK\n");

    task_management_init();
    printk("[main]: task_management_init OK\n");

    events_logs_init();
    printk("[main]: events_logs_init OK\n");
        
    if (temperature_init() != 0) {
        printk("[main]: temperature_init ECHEC\n");
        events_logs_add("[main] temperature_init FAILED");
    } else {
        printk("[main]: temperature_init OK\n");
    }

    if (mpu6050_input_init() != 0) {
        printk("[main]: mpu6050_input_init ECHEC\n");
        events_logs_add("[main] mpu6050_input_init FAILED");
    } else {
        printk("[main]: mpu6050_input_init OK\n");
    }

    live_variables_init();
    printk("[main]: live_variables_init OK\n");

    launcher_init(); /* Builds and loads the launcher screen (lv_screen_load). */
    printk("[main]: launcher_init OK, ecran charge\n");
    events_logs_add("[main] Boot complete, launcher displayed");

    int uptime_s = 0;

    while (1) {
        k_sleep(K_SECONDS(1));
        watchdog_feed();

        uptime_s++;
        if (uptime_s % 5 == 0) {
            struct sys_memory_stats stats;

            lvgl_heap_stats(&stats);
            events_logs_add("[mem] free=%u allocated=%u max_allocated=%u",
                             (unsigned)stats.free_bytes, (unsigned)stats.allocated_bytes,
                             (unsigned)stats.max_allocated_bytes);
        }
    }

    return 0;
}
