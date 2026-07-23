#include "adc_input.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>


static const struct adc_dt_spec adc_chans[] = {
    ADC_DT_SPEC_GET_BY_NAME(DT_PATH(zephyr_user), pot0),
    ADC_DT_SPEC_GET_BY_NAME(DT_PATH(zephyr_user), pot1),
    ADC_DT_SPEC_GET_BY_NAME(DT_PATH(zephyr_user), pot2),
    ADC_DT_SPEC_GET_BY_NAME(DT_PATH(zephyr_user), pot3),
};

BUILD_ASSERT(ARRAY_SIZE(adc_chans) == ADC_CHANNEL_COUNT);


#define ADC_THREAD_STACK_SIZE 1024
#define ADC_THREAD_PRIORITY   5

/* Meme priorite pour les 4 threads : pot1/pot2/pot3 partagent l'instance
 * ADC3 et se serialisent sur un k_sem interne au driver (adc_context), qui
 * n'a pas d'heritage de priorite. Des priorites differentes exposeraient a
 * une inversion de priorite. */

K_THREAD_STACK_DEFINE(adc_thread_stack_0, ADC_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(adc_thread_stack_1, ADC_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(adc_thread_stack_2, ADC_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(adc_thread_stack_3, ADC_THREAD_STACK_SIZE);

static struct k_thread adc_threads[ARRAY_SIZE(adc_chans)];
static k_thread_stack_t *const adc_thread_stacks[ARRAY_SIZE(adc_chans)] = {
    adc_thread_stack_0,
    adc_thread_stack_1,
    adc_thread_stack_2,
    adc_thread_stack_3,
};

#define ADC_SAMPLE_PERIOD_MS 100

static atomic_t adc_values_mv[ARRAY_SIZE(adc_chans)];

static void adc_sample_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    int idx = (int)(intptr_t)p1;
    int64_t next_wake = k_uptime_get();

    while (1) {
        uint32_t buf = 0;
        struct adc_sequence sequence = {
            .buffer = &buf,
            .buffer_size = sizeof(buf),
        };

        int err = adc_sequence_init_dt(&adc_chans[idx], &sequence);
        if (err == 0) {
            err = adc_read_dt(&adc_chans[idx], &sequence);
        }

        if (err == 0) {
            int32_t mv = (int32_t)buf;

            err = adc_raw_to_millivolts_dt(&adc_chans[idx], &mv);
            if (err == 0) {
                atomic_set(&adc_values_mv[idx], mv);
            }
        }

        if (err != 0) {
            printk("[adc_input]: thread %d read failed with error %d\n", idx, err);
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
    for(int i = 0; i < sizeof(adc_chans)/sizeof(adc_chans[0]); i++) {
        if(!adc_is_ready_dt(&adc_chans[i])) {
            printk("[adc_input]: ADC channel %d device not ready\n", i);
            return -1;
        }else{
            printk("[adc_input]: ADC channel %d device ready\n", i);
        }
        
        if(adc_channel_setup_dt(&adc_chans[i])) {
            printk("[adc_input]: ADC channel %d setup failed\n", i);
            return -1;
        }else{
            printk("[adc_input]: ADC channel %d setup OK\n", i);
        }
    }
    for(int i = 0; i < sizeof(adc_chans)/sizeof(adc_chans[0]); i++) {
        if(!adc_is_ready_dt(&adc_chans[i])) {
            printk("[adc_input]: ADC channel %d setup failed\n", i);
            return -1;
        }
        printk("[adc_input]: ADC channel %d setup OK\n", i);
        printk("[adc_input]: ADC channel %d: device=%s, channel_id=%d, resolution=%d\n", 
                i, adc_chans[i].dev->name, adc_chans[i].channel_id, adc_chans[i].resolution);
    }

    for (int i = 0; i < ARRAY_SIZE(adc_chans); i++) {
        k_thread_create(&adc_threads[i], adc_thread_stacks[i],
                         ADC_THREAD_STACK_SIZE, adc_sample_thread,
                         (void *)(intptr_t)i, NULL, NULL,
                         ADC_THREAD_PRIORITY, 0, K_NO_WAIT);
    }

    return 0;
}

