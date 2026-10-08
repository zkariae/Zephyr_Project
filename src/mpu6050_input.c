/**
 * @file
 * @brief Background sampling of the MPU6050 accel/gyro over I2C1.
 */

#include "mpu6050_input.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

static const struct device *const mpu6050_dev = DEVICE_DT_GET(DT_NODELABEL(mpu6050));

#define MPU6050_THREAD_STACK_SIZE 1024
#define MPU6050_THREAD_PRIORITY 5
#define MPU6050_SAMPLE_PERIOD_MS 50

#define MPU6050_CALIB_SAMPLES 32
#define MPU6050_CALIB_PERIOD_MS 20

K_THREAD_STACK_DEFINE(mpu6050_thread_stack, MPU6050_THREAD_STACK_SIZE);
static struct k_thread mpu6050_thread;

static atomic_t accel_x_mms2;
static atomic_t accel_y_mms2;

/* Written by run_calibration() (sample thread context only), read every
 * tick by that same thread; no synchronization needed. */
static float accel_x_bias_mms2;
static float accel_y_bias_mms2;

/* Set by mpu6050_input_calibrate() (any thread), consumed by the sample
 * thread - keeps the blocking calibration off the LVGL/button context. */
static atomic_t calib_requested;

static void run_calibration(void)
{
    float sum_x = 0.0f;
    float sum_y = 0.0f;
    int n = 0;

    printk("[mpu6050]: calibrating zero-offset, keep the board still...\n");

    for (int i = 0; i < MPU6050_CALIB_SAMPLES; i++) {
        struct sensor_value accel[3];

        if (sensor_sample_fetch(mpu6050_dev) == 0 &&
            sensor_channel_get(mpu6050_dev, SENSOR_CHAN_ACCEL_XYZ, accel) == 0) {
            sum_x += sensor_value_to_float(&accel[0]) * 1000.0f;
            sum_y += sensor_value_to_float(&accel[1]) * 1000.0f;
            n++;
        }
        k_sleep(K_MSEC(MPU6050_CALIB_PERIOD_MS));
    }

    if (n > 0) {
        accel_x_bias_mms2 = sum_x / n;
        accel_y_bias_mms2 = sum_y / n;
    }
    printk("[mpu6050]: calibrated bias x=%.1f y=%.1f mm/s^2\n",
           (double)accel_x_bias_mms2, (double)accel_y_bias_mms2);
}

/* sensor_sample_fetch() blocks; isolate it in its own thread and cache the
 * result in atomic_t so callers (e.g. LVGL) never block on it. */
static void mpu6050_sample_thread(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    int64_t next_wake = k_uptime_get();

    while (1) {
        struct sensor_value accel[3];

        if (atomic_cas(&calib_requested, 1, 0)) {
            run_calibration();
        }

        if (sensor_sample_fetch(mpu6050_dev) == 0 &&
            sensor_channel_get(mpu6050_dev, SENSOR_CHAN_ACCEL_XYZ, accel) == 0) {
            atomic_set(&accel_x_mms2,
                       (int32_t)(sensor_value_to_float(&accel[0]) * 1000.0f - accel_x_bias_mms2));
            atomic_set(&accel_y_mms2,
                       (int32_t)(sensor_value_to_float(&accel[1]) * 1000.0f - accel_y_bias_mms2));
        } else {
            printk("[mpu6050]: sample read failed\n");
        }

        next_wake += MPU6050_SAMPLE_PERIOD_MS;
        k_sleep(K_TIMEOUT_ABS_MS(next_wake));
    }
}

int32_t mpu6050_input_get_accel_x_mms2(void)
{
    return (int32_t)atomic_get(&accel_x_mms2);
}

int32_t mpu6050_input_get_accel_y_mms2(void)
{
    return (int32_t)atomic_get(&accel_y_mms2);
}

void mpu6050_input_calibrate(void)
{
    atomic_set(&calib_requested, 1);
}

int mpu6050_input_init(void)
{
    if (!device_is_ready(mpu6050_dev)) {
        printk("[mpu6050]: device not ready\n");
        return -1;
    }
    printk("[mpu6050]: device ready\n");

    run_calibration();

    k_thread_create(&mpu6050_thread, mpu6050_thread_stack,
                     MPU6050_THREAD_STACK_SIZE, mpu6050_sample_thread,
                     NULL, NULL, NULL,
                     MPU6050_THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&mpu6050_thread, "mpu6050_sample");

    return 0;
}
