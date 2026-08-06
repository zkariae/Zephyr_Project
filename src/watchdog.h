#ifndef WATCHDOG_H_
#define WATCHDOG_H_

int watchdog_init(void);
void watchdog_feed(void);
void watchdog_report_reset_cause(void);

#endif /* WATCHDOG_H_ */
