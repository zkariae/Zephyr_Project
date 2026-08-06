/**
 * @file
 * @brief Interrupt-driven printk() backend and UART link-status decoder.
 */

#include "async_printk.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/printk-hooks.h>
#include <zephyr/sys/ring_buffer.h>
#include <string.h>

#define ASYNC_PRINTK_BUF_SIZE 1024
#define UART_RX_BUF_SIZE 128

/* Frame format sent by tools/send_time.py: "<HH:MM:SS,DD/MM/YYYY>",
 * '\n'-terminated. */
#define FRAME_LEN (1 + ASYNC_PRINTK_TIME_LEN + 1 + ASYNC_PRINTK_DATE_LEN + 1)
#define CONNECTED_TIMEOUT_MS 3000

static const struct device *console_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

RING_BUF_DECLARE(tx_rb, ASYNC_PRINTK_BUF_SIZE);
static struct k_spinlock produce_lock;

/* RX (PC -> board): the ISR only drains the UART FIFO into this ring
 * buffer; parsing is deferred to rx_work on the system workqueue to keep
 * ISR context light. */
RING_BUF_DECLARE(rx_rb, UART_RX_BUF_SIZE);
static struct k_work rx_work;

/* Last decoded link status, guarded by link_lock (written by
 * rx_work_handler on the system workqueue, read by the launcher's lv_timer
 * on the LVGL workqueue). */
static struct {
    char time_str[ASYNC_PRINTK_TIME_LEN + 1];
    char date_str[ASYNC_PRINTK_DATE_LEN + 1];
    int64_t last_rx_uptime;
    bool has_data;
} link_status;
static struct k_spinlock link_lock;

/* Current line accumulator (between two '\n'), reset each frame. */
static char line_buf[FRAME_LEN];
static size_t line_len;
static bool line_overflow;

static bool frame_is_valid(const char *buf, size_t len)
{
    return len == FRAME_LEN &&
           buf[0] == '<' && buf[FRAME_LEN - 1] == '>' &&
           buf[3] == ':' && buf[6] == ':' &&
           buf[9] == ',' && buf[12] == '/' && buf[15] == '/';
}

static void frame_apply(const char *buf)
{
    k_spinlock_key_t key = k_spin_lock(&link_lock);

    memcpy(link_status.time_str, &buf[1], ASYNC_PRINTK_TIME_LEN);
    link_status.time_str[ASYNC_PRINTK_TIME_LEN] = '\0';
    memcpy(link_status.date_str, &buf[10], ASYNC_PRINTK_DATE_LEN);
    link_status.date_str[ASYNC_PRINTK_DATE_LEN] = '\0';
    link_status.last_rx_uptime = k_uptime_get();
    link_status.has_data = true;

    k_spin_unlock(&link_lock, key);
}

static void rx_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    uint8_t byte;

    while (ring_buf_get(&rx_rb, &byte, 1) == 1) {
        if (byte == '\n') {
            if (!line_overflow && frame_is_valid(line_buf, line_len)) {
                frame_apply(line_buf);
            }
            line_len = 0;
            line_overflow = false;
            continue;
        }

        if (line_len < FRAME_LEN) {
            line_buf[line_len++] = (char)byte;
        } else {
            /* Line longer than expected (noise/desync): ignore it until
             * the next '\n' instead of overflowing line_buf. */
            line_overflow = true;
        }
    }
}

bool async_printk_get_link_status(char time_out[ASYNC_PRINTK_TIME_LEN + 1],
                                   char date_out[ASYNC_PRINTK_DATE_LEN + 1],
                                   bool *connected)
{
    k_spinlock_key_t key = k_spin_lock(&link_lock);

    bool has_data = link_status.has_data;

    if (has_data) {
        memcpy(time_out, link_status.time_str, sizeof(link_status.time_str));
        memcpy(date_out, link_status.date_str, sizeof(link_status.date_str));
    }
    *connected = has_data &&
                 (k_uptime_get() - link_status.last_rx_uptime) < CONNECTED_TIMEOUT_MS;

    k_spin_unlock(&link_lock, key);

    return has_data;
}

static void uart_isr(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);
    uart_irq_update(dev);

    if (uart_irq_rx_ready(dev)) {
        uint8_t c;
        while (uart_fifo_read(dev, &c, 1) == 1) {
            ring_buf_put(&rx_rb, &c, 1);
        }
        k_work_submit(&rx_work);
    }

    if (!uart_irq_tx_ready(dev)) {
        return;
    }

    uint8_t c;
    while (ring_buf_get(&tx_rb, &c, 1) == 1) {
        uart_fifo_fill(dev, &c, 1);
    }
    uart_irq_tx_disable(dev);
}

/* Ring-buffer printk() output, drained via UART IRQ instead of the default
 * blocking uart_poll_out(). Write side locked (multiple callers); ISR read
 * side is single-consumer, lock-free. Full buffer truncates, never blocks. */
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
    k_work_init(&rx_work, rx_work_handler);
    uart_irq_callback_set(console_dev, uart_isr);
    uart_irq_rx_enable(console_dev);
    __printk_hook_install(async_char_out);
}
