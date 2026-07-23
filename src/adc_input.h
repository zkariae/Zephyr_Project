#ifndef ADC_INPUT_H_
#define ADC_INPUT_H_

#include <stdint.h>

#define ADC_CHANNEL_COUNT 4

int adc_input_init(void);
int32_t adc_input_get_mv(int idx);

#endif /* ADC_INPUT_H_ */