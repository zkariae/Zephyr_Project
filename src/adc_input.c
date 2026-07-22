#include "adc_input.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>


static const struct adc_dt_spec adc_chan = ADC_DT_SPEC_GET(DT_PATH(zephyr_user));



int adc_input_init(void)
{
    if(!adc_is_ready_dt(&adc_chan)) {
        printk("[adc_input]: ADC device not ready\n");
        return -1;
    }else{
        printk("[adc_input]: ADC device ready\n");
    }
    
    if(adc_channel_setup_dt(&adc_chan)) {
        printk("[adc_input]: ADC channel setup failed\n");
        return -1;
    }else{
        printk("[adc_input]: ADC channel setup OK\n");
    }
    return 0;
}

int adc_input_read_data(int32_t *data_mv)
{
    uint32_t buf = 0;
    struct adc_sequence sequence ={
        .buffer = &buf,
        .buffer_size = sizeof(buf),
    };
    int err = adc_sequence_init_dt(&adc_chan, &sequence);   // remplit channels/resolution
    if(err != 0) {
        printk("[adc_input]: ADC read failed with error %d\n", err);
        return err;
    }

    err = adc_read_dt(&adc_chan, &sequence);   // echantillonnage
    if(err != 0) {
        printk("[adc_input]: ADC read failed with error %d\n", err);
        return err;
    }
    
    *data_mv = (int32_t)buf;
    return adc_raw_to_millivolts_dt(&adc_chan, data_mv);  //brut to mv
}


