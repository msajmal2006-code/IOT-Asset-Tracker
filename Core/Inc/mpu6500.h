/*
 * mpu6500.h
 *
 *  Created on: Sep 16, 2026
 *      Author: Asus
 */

#ifndef INC_MPU6500_H_
#define INC_MPU6500_H_


#include "main.h"

#define MPU6500_ADDR       (0x68 << 1)

typedef struct
{
    float ax;
    float ay;
    float az;
} MPU6500_Data_t;

HAL_StatusTypeDef MPU6500_Init(I2C_HandleTypeDef *h);

HAL_StatusTypeDef MPU6500_ReadAccel(
    I2C_HandleTypeDef *h,
    MPU6500_Data_t *data
);

/*
 * Enable MPU6500 Wake-on-Motion interrupt.
 *
 * threshold:
 * approximately 4 mg/LSB.
 *
 * Example:
 *   25 ≈ 100 mg
 */
HAL_StatusTypeDef MPU6500_EnableMotionInterrupt(
    I2C_HandleTypeDef *h,
    uint8_t threshold
);

uint8_t MPU6500_MotionEvent(I2C_HandleTypeDef *h);


#endif /* INC_MPU6500_H_ */
