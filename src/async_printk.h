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
 * @brief Get the last time/date received from the host link (see
 *        tools/send_time.py), and whether the link is currently up.
 *
 * @param time_out Buffer of at least ASYNC_PRINTK_TIME_LEN+1 bytes, filled
 *                 only if a frame has already been received.
 * @param date_out Buffer of at least ASYNC_PRINTK_DATE_LEN+1 bytes, filled
 *                 only if a frame has already been received.
 * @param connected Set to false if no frame was received in the last 3 s.
 * @return true if at least one frame has ever been received.
 */
bool async_printk_get_link_status(char time_out[ASYNC_PRINTK_TIME_LEN + 1],
                                   char date_out[ASYNC_PRINTK_DATE_LEN + 1],
                                   bool *connected);

#endif /* ASYNC_PRINTK_H_ */
