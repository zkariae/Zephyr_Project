#include "adc_input.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/linker/section_tags.h>


static const struct adc_dt_spec adc_chans[] = {
    ADC_DT_SPEC_GET_BY_NAME(DT_PATH(zephyr_user), pot0),
    ADC_DT_SPEC_GET_BY_NAME(DT_PATH(zephyr_user), pot1),
    ADC_DT_SPEC_GET_BY_NAME(DT_PATH(zephyr_user), pot2),
    ADC_DT_SPEC_GET_BY_NAME(DT_PATH(zephyr_user), pot3),
};

BUILD_ASSERT(ARRAY_SIZE(adc_chans) == ADC_CHANNEL_COUNT);

#define POT0 0
#define POT1 1
#define POT2 2
#define POT3 3

#define ADC_THREAD_STACK_SIZE 1024
#define ADC_THREAD_PRIORITY   5
#define ADC_SAMPLE_PERIOD_MS  20

K_THREAD_STACK_DEFINE(adc_thread_stack, ADC_THREAD_STACK_SIZE);
static struct k_thread adc_thread_data;

static atomic_t adc_values_mv[ARRAY_SIZE(adc_chans)];

/*
 * __nocache (+ alignement sur la taille de ligne DCACHE, 32 octets sur ce
 * coeur) : le driver adc_stm32 refuse tout buffer DMA qui ne serait pas
 * dans une region non-cacheable, faute de cache-maintenance automatique
 * sur les transferts DMA (voir soc/st/stm32/common/stm32_cache.c).
 */

/* pot0 seul sur ADC1 (DMA2 Stream0/Channel0, cf overlay). */
static __aligned(32) uint16_t adc1_buffer[1] __nocache;

/*
 * pot1/pot2/pot3 partagent ADC3 (canaux 8/7/6) et sont scannes en une
 * seule conversion DMA (DMA2 Stream1/Channel2, cf overlay) au lieu de 3
 * lectures serialisees. Le sequenceur restitue les echantillons par ordre
 * croissant de numero de canal (6, 7, 8) - inverse de l'ordre pot1/pot2/
 * pot3 declare dans le devicetree - d'ou la table de correspondance
 * explicite plutot que de supposer l'ordre du tableau adc_chans[].
 */
static __aligned(32) uint16_t adc3_buffer[3] __nocache;
static const int adc3_scan_order_to_pot[3] = { POT3, POT2, POT1 };

static void adc_sample_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    const struct adc_sequence seq_adc1 = {
        .channels = BIT(adc_chans[POT0].channel_id),
        .buffer = adc1_buffer,
        .buffer_size = sizeof(adc1_buffer),
        .resolution = adc_chans[POT0].resolution,
    };

    const struct adc_sequence seq_adc3 = {
        .channels = BIT(adc_chans[POT1].channel_id) |
                    BIT(adc_chans[POT2].channel_id) |
                    BIT(adc_chans[POT3].channel_id),
        .buffer = adc3_buffer,
        .buffer_size = sizeof(adc3_buffer),
        .resolution = adc_chans[POT1].resolution,
    };

    int64_t next_wake = k_uptime_get();

    while (1) {
        int err = adc_read(adc_chans[POT0].dev, &seq_adc1);

        if (err == 0) {
            int32_t mv = adc1_buffer[0];

            err = adc_raw_to_millivolts_dt(&adc_chans[POT0], &mv);
            if (err == 0) {
                atomic_set(&adc_values_mv[POT0], mv);
            }
        }
        if (err != 0) {
            printk("[adc_input]: ADC1 (pot0) read failed with error %d\n", err);
        }

        err = adc_read(adc_chans[POT1].dev, &seq_adc3);
        if (err == 0) {
            for (int i = 0; i < ARRAY_SIZE(adc3_buffer); i++) {
                int pot = adc3_scan_order_to_pot[i];
                int32_t mv = adc3_buffer[i];

                if (adc_raw_to_millivolts_dt(&adc_chans[pot], &mv) == 0) {
                    atomic_set(&adc_values_mv[pot], mv);
                }
            }
        } else {
            printk("[adc_input]: ADC3 (pot1/pot2/pot3) read failed with error %d\n", err);
        }

        next_wake += ADC_SAMPLE_PERIOD_MS;
        k_sleep(K_TIMEOUT_ABS_MS(next_wake));
    }
}

int32_t adc_input_get_mv(int idx)
{
    return (int32_t)atomic_get(&adc_values_mv[idx]);
}

int adc_input_init(void)
{
    for (int i = 0; i < ARRAY_SIZE(adc_chans); i++) {
        if (!adc_is_ready_dt(&adc_chans[i])) {
            printk("[adc_input]: ADC channel %d device not ready\n", i);
            return -1;
        }

        if (adc_channel_setup_dt(&adc_chans[i])) {
            printk("[adc_input]: ADC channel %d setup failed\n", i);
            return -1;
        }

        printk("[adc_input]: ADC channel %d: device=%s, channel_id=%d, resolution=%d\n",
                i, adc_chans[i].dev->name, adc_chans[i].channel_id, adc_chans[i].resolution);
    }

    k_thread_create(&adc_thread_data, adc_thread_stack, ADC_THREAD_STACK_SIZE,
                     adc_sample_thread, NULL, NULL, NULL,
                     ADC_THREAD_PRIORITY, 0, K_NO_WAIT);

    return 0;
}
