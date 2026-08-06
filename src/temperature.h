/**
 * @file
 * @brief Background sampling of the die temperature and VREF+ sensors.
 */

#ifndef TEMPERATURE_H_
#define TEMPERATURE_H_

#include <stdint.h>

/**
 * @brief Set up the die temperature and VREF+ sensors and start sampling.
 *
 * @return 0 on success, -1 if a device is not ready.
 */
int temperature_init(void);

/** @brief Get the last sampled die temperature, in hundredths of a degree C. */
int32_t temperature_get_die_centi_c(void);

/** @brief Get the last sampled VREF+ voltage, in millivolts. */
int32_t temperature_get_vref_mv(void);

#endif /* TEMPERATURE_H_ */
