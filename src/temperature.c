#include "temperature.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

/*
 * die_temp et vref sont deja definis dans zephyr/dts/arm/st/f7/stm32f7.dtsi
 * ("st,stm32-temp-cal" / "st,stm32-vref"), juste desactives par defaut -
 * on les active dans boards/stm32f7508_dk.overlay. Contrairement aux
 * potentiometres (adc_input.c), le driver Zephyr lit lui-meme le canal ADC1
 * concerne (18 = capteur de temperature interne, 17 = VREFINT) et applique
 * la formule de calibration usine (TS_CAL1/TS_CAL2 en OTP) : pas besoin de
 * refaire ces calculs a la main avec adc_read().
 */
static const struct device *const die_temp_dev = DEVICE_DT_GET(DT_NODELABEL(die_temp));
static const struct device *const vref_dev = DEVICE_DT_GET(DT_NODELABEL(vref));

#define TEMPERATURE_THREAD_STACK_SIZE 1024

/*
 * Meme priorite que ADC_THREAD_PRIORITY (adc_input.c) : die_temp/vref
 * partagent l'instance ADC1 avec le thread pot0, serialises sur le meme
 * k_sem interne au driver (adc_context) sans heritage de priorite. Une
 * priorite differente exposerait a la meme inversion de priorite que
 * celle documentee dans adc_input.c pour pot1/pot2/pot3 sur ADC3.
 */
#define TEMPERATURE_THREAD_PRIORITY 5
#define TEMPERATURE_SAMPLE_PERIOD_MS 1000

K_THREAD_STACK_DEFINE(temperature_thread_stack, TEMPERATURE_THREAD_STACK_SIZE);
static struct k_thread temperature_thread;

static atomic_t die_centi_c;
static atomic_t vref_mv;

/*
 * sensor_sample_fetch() est bloquant (conversion ADC + eventuelle attente
 * du semaphore adc_context partage avec pot0). L'isoler dans son propre
 * thread, avec le resultat mis en cache dans des atomic_t, evite de
 * bloquer l'appelant (notamment le thread LVGL, qui rafraichit l'ecran
 * Live Variables chaque seconde) - meme logique que adc_input.c pour
 * les potentiometres.
 */
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
