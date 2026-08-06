#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/sys/printk.h>

#include "system_overview.h"
#include "rtos_tasks.h"
#include "events_logs.h"
#include "live_variables.h"
#include "launcher.h"
#include "splash.h"
#include "adc_input.h"
#include "temperature.h"
#include "plot_display.h"
#include "async_printk.h"
#include "watchdog.h"

#define SPLASH_DELAY_MS 1500

int main(void)
{
    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

    async_printk_init();

    printk("[main]: Projet1 demarre, verification de l'ecran...\n");
    events_logs_add("[main] Projet1 demarre");

    if (!device_is_ready(display_dev)) {
        printk("[main]: Ecran non pret\n");
        events_logs_add("[main] Ecran non pret");
        return -1;
    }
    printk("[main]: Ecran pret\n");
    events_logs_add("[main] Ecran pret");

    if (watchdog_init() != 0) {
        printk("[main]: watchdog_init ECHEC (pas de filet de securite IWDG)\n");
        events_logs_add("[main] watchdog_init ECHEC");
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

    rtos_tasks_init();
    printk("[main]: rtos_tasks_init OK\n");

    events_logs_init();
    printk("[main]: events_logs_init OK\n");
        
    if (adc_input_init() != 0) {
        printk("[main]: adc_input_init ECHEC\n");
        events_logs_add("[main] adc_input_init ECHEC");
    } else {
        printk("[main]: adc_input_init OK\n");
    }

    if (temperature_init() != 0) {
        printk("[main]: temperature_init ECHEC\n");
        events_logs_add("[main] temperature_init ECHEC");
    } else {
        printk("[main]: temperature_init OK\n");
    }

    live_variables_init();
    printk("[main]: live_variables_init OK\n");

    plot_display_init();
    printk("[main]: plot_display_init OK\n");

    launcher_init(); /* construit le launcher et l'affiche (lv_screen_load) */
    printk("[main]: launcher_init OK, ecran charge\n");
    events_logs_add("[main] Boot termine, launcher affiche");




    while (1) {
        k_sleep(K_SECONDS(1));
        watchdog_feed();
    }

    return 0;
}
