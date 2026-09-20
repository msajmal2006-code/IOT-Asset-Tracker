/*
 * mpu6500.h
 *
 *  Created on: Sep 16, 2026
 *      Author: Asus
 */

#include "mpu6500.h"

/* MPU6500 registers */
#define MPU_REG_WHO_AM_I       0x75
#define MPU_REG_PWR_MGMT_1     0x6B
#define MPU_REG_PWR_MGMT_2     0x6C

#define MPU_REG_ACCEL_CONFIG   0x1C
#define MPU_REG_ACCEL_CONFIG_2 0x1D

#define MPU_REG_LP_ACCEL_ODR   0x1E
#define MPU_REG_WOM_THR        0x1F

#define MPU_REG_ACCEL_XOUT_H   0x3B

#define MPU_REG_MOT_DETECT_CTRL 0x69

#define MPU_REG_INT_PIN_CFG    0x37
#define MPU_REG_INT_ENABLE     0x38
#define MPU_REG_INT_STATUS     0x3A


/* ----------------------------------------------------------
 * Internal register write
 * ---------------------------------------------------------- */
static HAL_StatusTypeDef MPU_WriteReg(
    I2C_HandleTypeDef *h,
    uint8_t reg,
    uint8_t value)
{
    return HAL_I2C_Mem_Write(
        h,
        MPU6500_ADDR,
        reg,
        I2C_MEMADD_SIZE_8BIT,
        &value,
        1,
        100
    );
}


/* ----------------------------------------------------------
 * MPU6500 initialization
 * ---------------------------------------------------------- */
HAL_StatusTypeDef MPU6500_Init(I2C_HandleTypeDef *h)
{
    uint8_t id;

    /* Read WHO_AM_I */
    if (HAL_I2C_Mem_Read(
            h,
            MPU6500_ADDR,
            MPU_REG_WHO_AM_I,
            I2C_MEMADD_SIZE_8BIT,
            &id,
            1,
            100) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* MPU6500 WHO_AM_I should be 0x70 */
    if (id != 0x70)
    {
        return HAL_ERROR;
    }

    /* Wake device */
    if (MPU_WriteReg(
            h,
            MPU_REG_PWR_MGMT_1,
            0x00) != HAL_OK)
    {
        return HAL_ERROR;
    }

    HAL_Delay(50);

    /*
     * Accelerometer full scale = ±2g
     *
     * ACCEL_FS_SEL = 00
     */
    if (MPU_WriteReg(
            h,
            MPU_REG_ACCEL_CONFIG,
            0x00) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /*
     * Accelerometer DLPF configuration.
     *
     * 0x01 = approximately 184 Hz bandwidth
     */
    if (MPU_WriteReg(
            h,
            MPU_REG_ACCEL_CONFIG_2,
            0x01) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}


/* ----------------------------------------------------------
 * Read accelerometer
 * ---------------------------------------------------------- */
HAL_StatusTypeDef MPU6500_ReadAccel(
    I2C_HandleTypeDef *h,
    MPU6500_Data_t *data)
{
    uint8_t buffer[6];

    if (HAL_I2C_Mem_Read(
            h,
            MPU6500_ADDR,
            MPU_REG_ACCEL_XOUT_H,
            I2C_MEMADD_SIZE_8BIT,
            buffer,
            6,
            100) != HAL_OK)
    {
        return HAL_ERROR;
    }

    int16_t raw_ax =
        (int16_t)((buffer[0] << 8) | buffer[1]);

    int16_t raw_ay =
        (int16_t)((buffer[2] << 8) | buffer[3]);

    int16_t raw_az =
        (int16_t)((buffer[4] << 8) | buffer[5]);

    /*
     * ±2g:
     * 16384 LSB/g
     */
    data->ax = (float)raw_ax / 16384.0f;
    data->ay = (float)raw_ay / 16384.0f;
    data->az = (float)raw_az / 16384.0f;

    return HAL_OK;
}


/* ----------------------------------------------------------
 * Enable MPU6500 Wake-on-Motion
 * ---------------------------------------------------------- */
HAL_StatusTypeDef MPU6500_EnableMotionInterrupt(
    I2C_HandleTypeDef *h,
    uint8_t threshold)
{
    uint8_t status;

    /*
     * Make sure the device is awake first.
     */
    if (MPU_WriteReg(
            h,
            MPU_REG_PWR_MGMT_1,
            0x00) != HAL_OK)
    {
        return HAL_ERROR;
    }

    HAL_Delay(10);


    /*
     * Disable gyroscope.
     *
     * PWR_MGMT_2:
     * bit 2 = gyro X standby
     * bit 1 = gyro Y standby
     * bit 0 = gyro Z standby
     *
     * 0x07 = gyro disabled
     */
    if (MPU_WriteReg(
            h,
            MPU_REG_PWR_MGMT_2,
            0x07) != HAL_OK)
    {
        return HAL_ERROR;
    }


    /*
     * Accelerometer DLPF.
     */
    if (MPU_WriteReg(
            h,
            MPU_REG_ACCEL_CONFIG_2,
            0x01) != HAL_OK)
    {
        return HAL_ERROR;
    }


    /*
     * Configure WOM threshold.
     *
     * Approximately 4 mg/LSB.
     *
     * threshold = 25
     * approximately 100 mg.
     */
    if (MPU_WriteReg(
            h,
            MPU_REG_WOM_THR,
            threshold) != HAL_OK)
    {
        return HAL_ERROR;
    }


    /*
     * Low-power accelerometer ODR.
     *
     * 0x06 is a valid low-power ODR setting.
     */
    if (MPU_WriteReg(
            h,
            MPU_REG_LP_ACCEL_ODR,
            0x06) != HAL_OK)
    {
        return HAL_ERROR;
    }


    /*
     * Enable accelerometer intelligence.
     *
     * ACCEL_INTEL_EN  = 1
     * ACCEL_INTEL_MODE = 1
     *
     * 0xC0 = 1100 0000
     */
    if (MPU_WriteReg(
            h,
            MPU_REG_MOT_DETECT_CTRL,
            0xC0) != HAL_OK)
    {
        return HAL_ERROR;
    }


    /*
     * Configure interrupt pin.
     *
     * 0x20:
     * LATCH_INT_EN = 1
     *
     * Interrupt remains asserted until status is read.
     */
    if (MPU_WriteReg(
            h,
            MPU_REG_INT_PIN_CFG,
            0x20) != HAL_OK)
    {
        return HAL_ERROR;
    }


    /*
     * Enable WOM interrupt.
     *
     * INT_ENABLE bit 6 = WOM_EN
     */
    if (MPU_WriteReg(
            h,
            MPU_REG_INT_ENABLE,
            0x40) != HAL_OK)
    {
        return HAL_ERROR;
    }


    /*
     * Clear any old interrupt status.
     */
    if (HAL_I2C_Mem_Read(
            h,
            MPU6500_ADDR,
            MPU_REG_INT_STATUS,
            I2C_MEMADD_SIZE_8BIT,
            &status,
            1,
            100) != HAL_OK)
    {
        return HAL_ERROR;
    }


    /*
     * Enable accelerometer cycle mode.
     *
     * PWR_MGMT_1:
     * CYCLE = bit 5
     *
     * 0x20 = CYCLE = 1
     */
    if (MPU_WriteReg(
            h,
            MPU_REG_PWR_MGMT_1,
            0x20) != HAL_OK)
    {
        return HAL_ERROR;
    }


    return HAL_OK;
}


/* ----------------------------------------------------------
 * Check motion event
 * ---------------------------------------------------------- */
uint8_t MPU6500_MotionEvent(I2C_HandleTypeDef *h)
{
    uint8_t status = 0;

    if (HAL_I2C_Mem_Read(
            h,
            MPU6500_ADDR,
            MPU_REG_INT_STATUS,
            I2C_MEMADD_SIZE_8BIT,
            &status,
            1,
            100) != HAL_OK)
    {
        return 0;
    }

    /*
     * Bit 6 = WOM interrupt
     */
    if (status & 0x40)
    {
        return 1;
    }

    return 0;
}

