/**
 * @file
 * @brief IWDG hardware watchdog setup and feeding.
 */

#ifndef WATCHDOG_H_
#define WATCHDOG_H_

/**
 * @brief Set up and arm the IWDG watchdog.
 *
 * @return 0 on success, -1 on failure.
 */
int watchdog_init(void);

/** @brief Feed the watchdog to prevent a reset. */
void watchdog_feed(void);

/** @brief Log the cause of the last reset (watchdog, software fault, or normal boot). */
void watchdog_report_reset_cause(void);

#endif /* WATCHDOG_H_ */
