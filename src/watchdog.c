/**
 * @file
 * @brief IWDG hardware watchdog setup and feeding.
 */

#include "watchdog.h"
#include "events_logs.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/drivers/hwinfo.h>

/* &iwdg is defined (disabled) in stm32f7.dtsi, enabled in our board overlay. */
static const struct device *const iwdg_dev = DEVICE_DT_GET(DT_NODELABEL(iwdg));
static int wdt_channel_id;
static int64_t last_feed_uptime;

/* 8s: comfortable margin above main.c's 1s feed period, also covers boot. */
#define WATCHDOG_TIMEOUT_MS 8000

int watchdog_init(void)
{
    if (!device_is_ready(iwdg_dev)) {
        printk("[watchdog]: IWDG device not ready\n");
        return -1;
    }

    /* STM32 IWDG only supports a hardware reset, no callback. */
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

    /* Pauses the IWDG while halted by GDB, to avoid resets on breakpoints. */
    if (wdt_setup(iwdg_dev, WDT_OPT_PAUSE_HALTED_BY_DBG) != 0) {
        printk("[watchdog]: wdt_setup failed\n");
        return -1;
    }

    printk("[watchdog]: IWDG arme (timeout %dms)\n", WATCHDOG_TIMEOUT_MS);
    last_feed_uptime = k_uptime_get();
    return 0;
}

void watchdog_feed(void)
{
    last_feed_uptime = k_uptime_get();
    wdt_feed(iwdg_dev, wdt_channel_id);
}

/* Software mirror of the IWDG's internal countdown: not readable from the
 * hardware, so we track it from the last feed timestamp instead. */
uint32_t watchdog_get_remaining_ms(void)
{
    int64_t elapsed = k_uptime_get() - last_feed_uptime;

    if (elapsed >= WATCHDOG_TIMEOUT_MS) {
        return 0;
    }
    return (uint32_t)(WATCHDOG_TIMEOUT_MS - elapsed);
}

/* Distinguishes a watchdog/software reset (crash) from a normal boot. */
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
