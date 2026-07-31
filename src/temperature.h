#ifndef TEMPERATURE_H_
#define TEMPERATURE_H_

#include <stdint.h>

int temperature_init(void);
int32_t temperature_get_die_centi_c(void);
int32_t temperature_get_vref_mv(void);

#endif /* TEMPERATURE_H_ */
