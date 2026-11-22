
#include "main_app.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "gps.h"
#include "mpu6500.h"
#include "dht11.h"
#include "esp01.h"
#include "storage.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* CubeMX peripherals */
extern I2C_HandleTypeDef hi2c1;
extern SPI_HandleTypeDef hspi1;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart4;
extern TIM_HandleTypeDef htim6;

/* Application configuration */
#define MPU_WOM_THRESHOLD       20U
#define MOTION_DEBOUNCE_MS      2000U
#define HEAVY_MOTION_DELTA_G    0.35f
#define MOTION_QUEUE_LENGTH     4U
#define TELEMETRY_BUFFER_SIZE   512U

/* Motion event transferred through the queue */
typedef struct
{
    uint8_t heavy_motion;
    MPU6500_Data_t accel;

} MotionEvent_t;

/* FreeRTOS objects */
static SemaphoreHandle_t motionSemaphore = NULL;
static QueueHandle_t motionQueue = NULL;

/* Application data */
static DHT11_Data_t dht;
static char telemetry[TELEMETRY_BUFFER_SIZE];


/* =========================================================
 * Acceleration magnitude
 * ========================================================= */

static float APP_AccelerationMagnitude(const MPU6500_Data_t *data)
{
    return sqrtf(
        (data->ax * data->ax) +
        (data->ay * data->ay) +
        (data->az * data->az)
    );
}


/* =========================================================
 * MPU6500 interrupt callback
 * ========================================================= */

void APP_MotionIRQ(void)
{
    BaseType_t higherPriorityTaskWoken = pdFALSE;

    if (motionSemaphore != NULL)
    {
        xSemaphoreGiveFromISR(
            motionSemaphore,
            &higherPriorityTaskWoken
        );

        portYIELD_FROM_ISR(higherPriorityTaskWoken);
    }
}


/* =========================================================
 * Telemetry processing
 * ========================================================= */

static void APP_SendTelemetry(const MotionEvent_t *event)
{
    GPS_Data_t gps;

    MPU6500_Data_t accel = event->accel;

    /* Get latest GPS data */
    if (!GPS_GetLatest(&gps))
    {
        memset(&gps, 0, sizeof(gps));
    }

    /* Read temperature and humidity */
    if (DHT11_Read(
            &htim6,
            GPIOA,
            GPIO_PIN_1,
            &dht) != HAL_OK)
    {
        dht.temperature = 0.0f;
        dht.humidity = 0.0f;
    }

    /* Format telemetry JSON */
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

        event->heavy_motion ? "HEAVY_MOTION" : "MOTION",

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
        (unsigned long)gps.utc_hhmmss
    );

    if ((length <= 0) ||
        (length >= (int)sizeof(telemetry)))
    {
        return;
    }

    /* Store telemetry in W25Q64 Flash */
    if (STORAGE_Save(telemetry) != HAL_OK)
    {
        /* Handle storage error if required */
    }

    /* Transmit telemetry through UART */
    if (ESP01_SendJSON(&huart1, telemetry) != HAL_OK)
    {
        /* Handle communication error if required */
    }
}


/* =========================================================
 * Motion Task - High Priority
 * ========================================================= */

static void MotionTask(void *argument)
{
    MotionEvent_t event;

    TickType_t lastAcceptedEvent = 0;

    (void)argument;

    /* Enable MPU6500 motion interrupt */
    if (MPU6500_EnableMotionInterrupt(
            &hi2c1,
            MPU_WOM_THRESHOLD) != HAL_OK)
    {
        Error_Handler();
    }

    for (;;)
    {
        /* Wait until MPU6500 generates an interrupt */
        if (xSemaphoreTake(
                motionSemaphore,
                portMAX_DELAY) == pdTRUE)
        {
            TickType_t now = xTaskGetTickCount();

            /* Motion event debounce */
            if ((now - lastAcceptedEvent) <
                pdMS_TO_TICKS(MOTION_DEBOUNCE_MS))
            {
                continue;
            }

            /* Verify motion event */
            if (!MPU6500_MotionEvent(&hi2c1))
            {
                continue;
            }

            /* Read accelerometer data */
            if (MPU6500_ReadAccel(
                    &hi2c1,
                    &event.accel) != HAL_OK)
            {
                continue;
            }

            /*
             * Stationary acceleration magnitude is
             * approximately 1 g.
             */
            float magnitude =
                APP_AccelerationMagnitude(&event.accel);

            /* Classify motion */
            event.heavy_motion =
                (fabsf(magnitude - 1.0f) >=
                 HEAVY_MOTION_DELTA_G) ? 1U : 0U;

            lastAcceptedEvent = now;

            /* Send event to telemetry queue */
            (void)xQueueSend(
                motionQueue,
                &event,
                0
            );
        }
    }
}


/* =========================================================
 * Telemetry Task - Normal Priority
 * ========================================================= */

static void TelemetryTask(void *argument)
{
    MotionEvent_t event;

    (void)argument;

    for (;;)
    {
        /* Wait for motion event from Motion Task */
        if (xQueueReceive(
                motionQueue,
                &event,
                portMAX_DELAY) == pdTRUE)
        {
            APP_SendTelemetry(&event);
        }
    }
}


/* =========================================================
 * Application Initialization
 * ========================================================= */

void APP_Init(void)
{
    /* Create binary semaphore */
    motionSemaphore = xSemaphoreCreateBinary();

    /* Create motion event queue */
    motionQueue = xQueueCreate(
        MOTION_QUEUE_LENGTH,
        sizeof(MotionEvent_t)
    );

    if ((motionSemaphore == NULL) ||
        (motionQueue == NULL))
    {
        Error_Handler();
    }

    /* Start TIM6 for DHT11 microsecond timing */
    if (HAL_TIM_Base_Start(&htim6) != HAL_OK)
    {
        Error_Handler();
    }

    /* Initialize GPS */
    GPS_Init(&huart4);
    GPS_Start();

    /* Initialize external Flash */
    if (STORAGE_Init(
            &hspi1,
            GPIOA,
            GPIO_PIN_0) != HAL_OK)
    {
        Error_Handler();
    }

    /* Initialize MPU6500 */
    if (MPU6500_Init(&hi2c1) != HAL_OK)
    {
        Error_Handler();
    }

    /*
     * Motion interrupt is enabled inside MotionTask,
     * after the FreeRTOS scheduler starts.
     */
}


/* =========================================================
 * Start FreeRTOS Tasks
 * ========================================================= */

void APP_StartRTOS(void)
{
    /* High-priority motion detection task */
    if (xTaskCreate(
            MotionTask,
            "Motion",
            512,
            NULL,
            tskIDLE_PRIORITY + 3,
            NULL) != pdPASS)
    {
        Error_Handler();
    }

    /* Normal-priority telemetry processing task */
    if (xTaskCreate(
            TelemetryTask,
            "Telemetry",
            1024,
            NULL,
            tskIDLE_PRIORITY + 2,
            NULL) != pdPASS)
    {
        Error_Handler();
    }
}
