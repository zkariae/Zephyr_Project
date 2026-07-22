#ifndef ADC_INPUT_H_
#define ADC_INPUT_H_

#include <stdint.h>

int adc_input_init(void);
int adc_input_read_data(int32_t *data_mv);

#endif /* ADC_INPUT_H_ */