#include "async_printk.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/printk-hooks.h>
#include <zephyr/sys/ring_buffer.h>

#define ASYNC_PRINTK_BUF_SIZE 1024

static const struct device *console_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

RING_BUF_DECLARE(tx_rb, ASYNC_PRINTK_BUF_SIZE);
static struct k_spinlock produce_lock;

static void uart_isr(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);
    uart_irq_update(dev);

    if (!uart_irq_tx_ready(dev)) {
        return;
    }

    uint8_t c;
    while (ring_buf_get(&tx_rb, &c, 1) == 1) {
        uart_fifo_fill(dev, &c, 1);
    }
    uart_irq_tx_disable(dev);
}

/* Remplace le hook de sortie "poll driven" de printk() (uart_console.c,
 * console_out() -> uart_poll_out(), bloquant octet par octet) par un
 * buffer circulaire draine par interruption UART. Plusieurs threads
 * appellent printk() concurremment (main, threads ADC, workqueue LVGL),
 * d'ou le spinlock cote ecriture ; la lecture reste single-consumer
 * (l'ISR), donc lock-free de ce cote. Buffer plein = octets tronques,
 * pas de blocage. */
static int async_char_out(int c)
{
    uint8_t byte = (uint8_t)c;
    k_spinlock_key_t key = k_spin_lock(&produce_lock);

    if (byte == '\n') {
        ring_buf_put(&tx_rb, (const uint8_t[]){ '\r' }, 1);
    }
    ring_buf_put(&tx_rb, &byte, 1);

    k_spin_unlock(&produce_lock, key);

    uart_irq_tx_enable(console_dev);

    return c;
}

void async_printk_init(void)
{
    uart_irq_callback_set(console_dev, uart_isr);
    __printk_hook_install(async_char_out);
}
