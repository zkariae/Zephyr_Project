/**
 * @file
 * @brief Background sampling of the 4 potentiometer ADC channels.
 */

#ifndef ADC_INPUT_H_
#define ADC_INPUT_H_

#include <stdint.h>

#define ADC_CHANNEL_COUNT 4

/**
 * @brief Set up the ADC channels and start the sampling thread.
 *
 * @return 0 on success, -1 if a channel is not ready or setup fails.
 */
int adc_input_init(void);

/**
 * @brief Get the last sampled value of a channel, in millivolts.
 *
 * @param idx Channel index (0..ADC_CHANNEL_COUNT-1).
 * @return Latest sample in millivolts.
 */
int32_t adc_input_get_mv(int idx);

#endif /* ADC_INPUT_H_ */