#include "main_app.h"
#include "gps.h"
#include "mpu6500.h"
#include "dht11.h"
#include "esp01.h"
#include "storage.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define HEAVY_MOTION_THRESHOLD_G  1.00f
/* =========================================================
 * External CubeMX peripherals
 * ========================================================= */

extern I2C_HandleTypeDef hi2c1;
extern SPI_HandleTypeDef hspi1;
extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef htim6;
extern UART_HandleTypeDef huart4;


/* =========================================================
 * Application variables
 * ========================================================= */

static volatile uint8_t motion_irq = 0;
static uint32_t last_motion_time = 0;


/* =========================================================
 * Sensor data
 * ========================================================= */

static MPU6500_Data_t accel;
static DHT11_Data_t dht;


/* =========================================================
 * Telemetry buffer
 * ========================================================= */

static char telemetry[512];


/* =========================================================
 * Motion interrupt
 * ========================================================= */

void APP_MotionIRQ(void)
{
    motion_irq = 1;
}

static float APP_AccelerationMagnitude(void)
{
    return sqrtf(
        (accel.ax * accel.ax) +
        (accel.ay * accel.ay) +
        (accel.az * accel.az)
    );
}
/* =========================================================
 * Send telemetry
 * ========================================================= */

static void APP_SendTelemetry(const char *event)
{
    GPS_Data_t gps;

    if (!GPS_GetLatest(&gps))
    {
        memset(&gps, 0, sizeof(gps));
    }

    if (MPU6500_ReadAccel(&hi2c1, &accel) != HAL_OK)
    {
        accel.ax = 0.0f;
        accel.ay = 0.0f;
        accel.az = 0.0f;
    }

    if (DHT11_Read(
            &htim6,
            GPIOA,
            GPIO_PIN_1,
            &dht) != HAL_OK)
    {
        dht.temperature = 0.0f;
        dht.humidity = 0.0f;
    }

    int length = snprintf(
        telemetry,
        sizeof(telemetry),

        "{"
        "\"id\":\"ASSET001\","
        "\"event\":\"%s\","
        "\"fix\":%u,"
        "\"lat\":%.6f,"
        "\"lon\":%.6f,"
        "\"temp\":%.1f,"
        "\"hum\":%.1f,"
        "\"ax\":%.3f,"
        "\"ay\":%.3f,"
        "\"az\":%.3f,"
        "\"speed\":%.2f,"
        "\"course\":%.2f,"
        "\"utc\":%lu"
        "}",

        event,

        gps.fix,
        gps.latitude,
        gps.longitude,

        dht.temperature,
        dht.humidity,

        accel.ax,
        accel.ay,
        accel.az,

        gps.speed_knots,
        gps.course_deg,
        gps.utc_hhmmss
    );

    if (length <= 0)
    {
        return;
    }

    if (length >= (int)sizeof(telemetry))
    {
        return;
    }

    /*
     * Save every telemetry event to W25Q64.
     */
    if (STORAGE_Save(telemetry) != HAL_OK)
    {
        /* Flash storage failed. */
    }

    /*
     * Send telemetry to ESP32 -> MQTT.
     */
    if (ESP01_SendJSON(
            &huart1,
            telemetry) != HAL_OK)
    {
        /* UART transmission failed. */
    }
}


/* =========================================================
 * Application initialization
 * ========================================================= */

void APP_Init(void)
{
    /* -----------------------------------------------------
     * Start TIM6
     *
     * Used by DHT11 for microsecond timing.
     * ----------------------------------------------------- */

    if (HAL_TIM_Base_Start(&htim6) != HAL_OK)
    {
        Error_Handler();
    }



    GPS_Init(&huart4);
    GPS_Start();



    if (STORAGE_Init(
            &hspi1,
            GPIOA,
            GPIO_PIN_0) != HAL_OK)
    {
        Error_Handler();
    }


    /* -----------------------------------------------------
     * Initialize MPU6500
     * ----------------------------------------------------- */

    if (MPU6500_Init(&hi2c1) != HAL_OK)
    {
        Error_Handler();
    }


    /* -----------------------------------------------------
     * Enable MPU6500 motion interrupt
     *
     * Threshold = 20
     * ----------------------------------------------------- */

    if (MPU6500_EnableMotionInterrupt(
            &hi2c1,
            20) != HAL_OK)
    {
        Error_Handler();
    }
}


/* =========================================================
 * Main application loop
 * ========================================================= */

void APP_Loop(void)
{
    uint32_t now;

    if (motion_irq)
    {
        motion_irq = 0;

        if (MPU6500_MotionEvent(&hi2c1))
        {
            now = HAL_GetTick();

            if ((now - last_motion_time) >= 2000U)
            {
                last_motion_time = now;

                /*
                 * Read current acceleration.
                 */
                if (MPU6500_ReadAccel(&hi2c1, &accel) != HAL_OK)
                {
                    accel.ax = 0.0f;
                    accel.ay = 0.0f;
                    accel.az = 0.0f;
                }

                /*
                 * Calculate total acceleration magnitude.
                 *
                 * Stationary sensor is approximately 1 g.
                 */
                float magnitude =
                    APP_AccelerationMagnitude();

                /*
                 * Classify the movement.
                 */
                if (magnitude >= HEAVY_MOTION_THRESHOLD_G)
                {
                    /*
                     * HIGH PRIORITY EVENT
                     */
                    APP_SendTelemetry("HEAVY_MOTION");
                }
                else
                {
                    /*
                     * NORMAL MOVEMENT
                     */
                    APP_SendTelemetry("MOTION");
                }
            }
        }
    }

    __WFI();
}
