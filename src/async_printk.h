/**
 * @file
 * @brief Interrupt-driven printk() backend and UART link-status decoder.
 */

#ifndef ASYNC_PRINTK_H_
#define ASYNC_PRINTK_H_

#include <stdbool.h>

#define ASYNC_PRINTK_TIME_LEN 8  /* "HH:MM:SS" */
#define ASYNC_PRINTK_DATE_LEN 10 /* "DD/MM/YYYY" */

/** @brief Install the interrupt-driven printk() backend and UART RX handling. */
void async_printk_init(void);

/**
 * @brief Last time/date from the host link (tools/send_time.py) and link state.
 * @param time_out/date_out Filled only once a frame has been received.
 * @param connected False if no frame arrived in the last 3 s.
 * @return true if at least one frame has ever been received.
 */
bool async_printk_get_link_status(char time_out[ASYNC_PRINTK_TIME_LEN + 1],
                                   char date_out[ASYNC_PRINTK_DATE_LEN + 1],
                                   bool *connected);

#endif /* ASYNC_PRINTK_H_ */
