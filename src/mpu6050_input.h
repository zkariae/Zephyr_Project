/**
 * @file
 * @brief Background sampling of the MPU6050 accel/gyro over I2C1.
 */

#ifndef MPU6050_INPUT_H_
#define MPU6050_INPUT_H_

#include <stdint.h>

/**
 * @brief Set up the MPU6050 and start background accel sampling.
 *
 * @return 0 on success, -1 if the device is not ready.
 */
int mpu6050_input_init(void);

/** @brief Get the last sampled X accel, in mm/s^2 (milli-m/s^2). */
int32_t mpu6050_input_get_accel_x_mms2(void);

/** @brief Get the last sampled Y accel, in mm/s^2 (milli-m/s^2). */
int32_t mpu6050_input_get_accel_y_mms2(void);

#endif /* MPU6050_INPUT_H_ */
