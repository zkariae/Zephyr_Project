#include "watchdog.h"
#include "events_logs.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/drivers/hwinfo.h>

/*
 * &iwdg est defini (desactive) dans zephyr/dts/arm/st/f7/stm32f7.dtsi et
 * active dans boards/stm32f7508_dk.overlay.
 */
static const struct device *const iwdg_dev = DEVICE_DT_GET(DT_NODELABEL(iwdg));
static int wdt_channel_id;

/*
 * 8s : large marge au-dessus du feed a 1s dans la boucle principale de
 * main.c (couvre aussi la sequence de boot - ~3s de splash + les
 * *_init() - qui se deroule avant le premier feed).
 */
#define WATCHDOG_TIMEOUT_MS 8000

int watchdog_init(void)
{
    if (!device_is_ready(iwdg_dev)) {
        printk("[watchdog]: IWDG device not ready\n");
        return -1;
    }

    /*
     * L'IWDG STM32 ne supporte que le reset materiel (HAS_WDT_NO_CALLBACKS,
     * cf. zephyr/drivers/watchdog/Kconfig.stm32) : pas de callback, juste
     * WDT_FLAG_RESET_SOC.
     */
    struct wdt_timeout_cfg wdt_config = {
        .window.min = 0,
        .window.max = WATCHDOG_TIMEOUT_MS,
        .callback = NULL,
        .flags = WDT_FLAG_RESET_SOC,
    };

    wdt_channel_id = wdt_install_timeout(iwdg_dev, &wdt_config);
    if (wdt_channel_id < 0) {
        printk("[watchdog]: wdt_install_timeout failed (%d)\n", wdt_channel_id);
        return -1;
    }

    /*
     * WDT_OPT_PAUSE_HALTED_BY_DBG : gele l'IWDG quand le coeur est arrete
     * par GDB (west attach), pour ne pas redemarrer la carte au premier
     * breakpoint pendant une session de debug.
     */
    if (wdt_setup(iwdg_dev, WDT_OPT_PAUSE_HALTED_BY_DBG) != 0) {
        printk("[watchdog]: wdt_setup failed\n");
        return -1;
    }

    printk("[watchdog]: IWDG arme (timeout %dms)\n", WATCHDOG_TIMEOUT_MS);
    return 0;
}

void watchdog_feed(void)
{
    wdt_feed(iwdg_dev, wdt_channel_id);
}

/* Distingue au boot un reset watchdog/logiciel (plantage) d'un demarrage normal. */
void watchdog_report_reset_cause(void)
{
    uint32_t cause = 0;
    const char *msg;

    hwinfo_get_reset_cause(&cause);
    hwinfo_clear_reset_cause();

    if (cause & RESET_WATCHDOG) {
        msg = "[boot] Reset watchdog (IWDG) - plantage probable au cycle precedent";
    } else if (cause & RESET_SOFTWARE) {
        msg = "[boot] Reset logiciel - probable fault CPU (CONFIG_RESET_ON_FATAL_ERROR)";
    } else {
        msg = "[boot] Demarrage normal (reset pin/POR/brownout)";
    }

    printk("%s\n", msg);
    events_logs_add(msg);
}
