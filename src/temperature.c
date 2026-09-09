/**
 * @file
 * @brief Background sampling of the die temperature and VREF+ sensors.
 */

#include "temperature.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

/* die_temp/vref nodes are defined in stm32f7.dtsi, enabled in our board
 * overlay. The driver handles the ADC channel and factory calibration
 * itself. */
static const struct device *const die_temp_dev = DEVICE_DT_GET(DT_NODELABEL(die_temp));
static const struct device *const vref_dev = DEVICE_DT_GET(DT_NODELABEL(vref));

#define TEMPERATURE_THREAD_STACK_SIZE 1024

#define TEMPERATURE_THREAD_PRIORITY 5
#define TEMPERATURE_SAMPLE_PERIOD_MS 1000

K_THREAD_STACK_DEFINE(temperature_thread_stack, TEMPERATURE_THREAD_STACK_SIZE);
static struct k_thread temperature_thread;

static atomic_t die_centi_c;
static atomic_t vref_mv;

/* sensor_sample_fetch() blocks; isolate it in its own thread and cache the
 * result in atomic_t so callers (e.g. LVGL) never block on it. */
static void temperature_sample_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    int64_t next_wake = k_uptime_get();

    while (1) {
        struct sensor_value val;

        if (sensor_sample_fetch(die_temp_dev) == 0 &&
            sensor_channel_get(die_temp_dev, SENSOR_CHAN_DIE_TEMP, &val) == 0) {
            atomic_set(&die_centi_c, val.val1 * 100 + val.val2 / 10000);
        } else {
            printk("[temperature]: die_temp read failed\n");
        }

        if (sensor_sample_fetch(vref_dev) == 0 &&
            sensor_channel_get(vref_dev, SENSOR_CHAN_VOLTAGE, &val) == 0) {
            atomic_set(&vref_mv, (int32_t)(sensor_value_to_float(&val) * 1000.0f));
        } else {
            printk("[temperature]: vref read failed\n");
        }

        next_wake += TEMPERATURE_SAMPLE_PERIOD_MS;
        k_sleep(K_TIMEOUT_ABS_MS(next_wake));
    }
}

int32_t temperature_get_die_centi_c(void)
{
    return (int32_t)atomic_get(&die_centi_c);
}

int32_t temperature_get_vref_mv(void)
{
    return (int32_t)atomic_get(&vref_mv);
}

int temperature_init(void)
{
    if (!device_is_ready(die_temp_dev)) {
        printk("[temperature]: die_temp device not ready\n");
        return -1;
    }
    printk("[temperature]: die_temp device ready\n");

    if (!device_is_ready(vref_dev)) {
        printk("[temperature]: vref device not ready\n");
        return -1;
    }
    printk("[temperature]: vref device ready\n");

    k_thread_create(&temperature_thread, temperature_thread_stack,
                     TEMPERATURE_THREAD_STACK_SIZE, temperature_sample_thread,
                     NULL, NULL, NULL,
                     TEMPERATURE_THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&temperature_thread, "temperature_sample");

    return 0;
}
