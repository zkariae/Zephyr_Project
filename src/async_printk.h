#ifndef ASYNC_PRINTK_H_
#define ASYNC_PRINTK_H_

#include <stdbool.h>

#define ASYNC_PRINTK_TIME_LEN 8  /* "HH:MM:SS" */
#define ASYNC_PRINTK_DATE_LEN 10 /* "DD/MM/YYYY" */

void async_printk_init(void);

/* Lit le dernier statut de liaison recu via UART (trame
 * <HH:MM:SS,DD/MM/YYYY> envoyee par tools/send_time.py). time_out/date_out
 * doivent faire au moins ASYNC_PRINTK_TIME_LEN+1 / ASYNC_PRINTK_DATE_LEN+1
 * octets ; ils ne sont ecrits que si une trame a deja ete recue au moins
 * une fois (valeur de retour true). *connected est mis a jour dans tous
 * les cas (false si aucune trame recue depuis plus de 3 s). */
bool async_printk_get_link_status(char time_out[ASYNC_PRINTK_TIME_LEN + 1],
                                   char date_out[ASYNC_PRINTK_DATE_LEN + 1],
                                   bool *connected);

#endif /* ASYNC_PRINTK_H_ */
