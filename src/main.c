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

#define SPLASH_DELAY_MS 1500

int main(void)
{
    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

    printk("Projet1 demarre, verification de l'ecran...\n");

    if (!device_is_ready(display_dev)) {
        printk("Ecran non pret\n");
        return -1;
    }
    printk("Ecran pret\n");

    splash_show_image1();

    display_blanking_off(display_dev);
    printk("splash image1 affichee\n");
    k_sleep(K_MSEC(SPLASH_DELAY_MS));

    splash_show_image2();
    printk("splash image2 affichee\n");
    k_sleep(K_MSEC(SPLASH_DELAY_MS));

    splash_cleanup();

    system_overview_init();
    printk("system_overview_init OK\n");

    rtos_tasks_init();
    printk("rtos_tasks_init OK\n");

    events_logs_init();
    printk("events_logs_init OK\n");

    live_variables_init();
    printk("live_variables_init OK\n");

    launcher_init(); /* construit le launcher et l'affiche (lv_screen_load) */
    printk("launcher_init OK, ecran charge\n");

    while (1) {
        k_sleep(K_SECONDS(1));
    }

    return 0;
}
