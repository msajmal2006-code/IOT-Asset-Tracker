/*
 * dht11.c
 *
 *  Created on: Sep 16, 2026
 *      Author: Asus
 */
#include "dht11.h"

static TIM_HandleTypeDef *dht_tim;
static GPIO_TypeDef *dht_port;
static uint16_t dht_pin;


/* ============================================================
 * Microsecond delay
 * TIM6 must be running at 1 MHz
 * ============================================================ */

static void DHT11_DelayUs(uint16_t us)
{
    uint16_t start = __HAL_TIM_GET_COUNTER(dht_tim);

    while ((uint16_t)(__HAL_TIM_GET_COUNTER(dht_tim) - start) < us)
    {
    }
}


/* ============================================================
 * Change DATA pin to open-drain output
 * ============================================================ */

static void DHT11_Output(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = dht_pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(dht_port, &GPIO_InitStruct);
}


/* ============================================================
 * Change DATA pin to input
 * ============================================================ */

static void DHT11_Input(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = dht_pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(dht_port, &GPIO_InitStruct);
}


/* ============================================================
 * Wait for pin to reach desired state
 *
 * Returns:
 *   1 = success
 *   0 = timeout
 * ============================================================ */

static uint8_t DHT11_WaitForState(
    GPIO_PinState state,
    uint16_t timeout_us)
{
    uint16_t start = __HAL_TIM_GET_COUNTER(dht_tim);

    while (HAL_GPIO_ReadPin(dht_port, dht_pin) != state)
    {
        if ((uint16_t)(__HAL_TIM_GET_COUNTER(dht_tim) - start)
                >= timeout_us)
        {
            return 0;
        }
    }

    return 1;
}


/* ============================================================
 * Start communication
 * ============================================================ */

static void DHT11_Start(void)
{
    DHT11_Output();

    /* Pull DATA LOW for at least 18 ms */
    HAL_GPIO_WritePin(
        dht_port,
        dht_pin,
        GPIO_PIN_RESET
    );

    HAL_Delay(20);

    /* Release DATA */
    HAL_GPIO_WritePin(
        dht_port,
        dht_pin,
        GPIO_PIN_SET
    );

    /* Wait approximately 30 us */
    DHT11_DelayUs(30);

    /* Sensor controls the line */
    DHT11_Input();
}


/* ============================================================
 * Check DHT11 response
 *
 * Sensor response:
 *
 * LOW  ~80 us
 * HIGH ~80 us
 * LOW  ~50 us
 * ============================================================ */

static uint8_t DHT11_CheckResponse(void)
{
    if (!DHT11_WaitForState(GPIO_PIN_RESET, 100))
        return 0;

    if (!DHT11_WaitForState(GPIO_PIN_SET, 100))
        return 0;

    if (!DHT11_WaitForState(GPIO_PIN_RESET, 100))
        return 0;

    return 1;
}


/* ============================================================
 * Read one byte
 * ============================================================ */

static uint8_t DHT11_ReadByte(uint8_t *value)
{
    uint8_t data = 0;

    for (uint8_t i = 0; i < 8; i++)
    {
        /*
         * Each bit starts with approximately
         * 50 us LOW followed by HIGH.
         */

        if (!DHT11_WaitForState(GPIO_PIN_SET, 100))
            return 0;

        /*
         * Sample approximately 40 us into HIGH.
         *
         * ~26-28 us HIGH = 0
         * ~70 us HIGH    = 1
         */

        DHT11_DelayUs(40);

        if (HAL_GPIO_ReadPin(dht_port, dht_pin)
                == GPIO_PIN_SET)
        {
            data |= (1U << (7 - i));
        }

        /*
         * Wait for the HIGH pulse to finish.
         */

        if (!DHT11_WaitForState(GPIO_PIN_RESET, 100))
            return 0;
    }

    *value = data;

    return 1;
}


/* ============================================================
 * Public DHT11 read
 * ============================================================ */

HAL_StatusTypeDef DHT11_Read(
    TIM_HandleTypeDef *htim,
    GPIO_TypeDef *GPIOx,
    uint16_t GPIO_Pin,
    DHT11_Data_t *data)
{
    uint8_t raw[5] = {0};

    if (htim == NULL || GPIOx == NULL || data == NULL)
        return HAL_ERROR;

    dht_tim  = htim;
    dht_port = GPIOx;
    dht_pin  = GPIO_Pin;

    /*
     * Make sure TIM6 is running.
     */
    if ((dht_tim->Instance->CR1 & TIM_CR1_CEN) == 0)
    {
        if (HAL_TIM_Base_Start(dht_tim) != HAL_OK)
            return HAL_ERROR;
    }

    /*
     * Default values.
     * These are retained if reading fails.
     */
    data->temperature = 0.0f;
    data->humidity = 0.0f;

    /*
     * Start DHT11 transaction.
     */
    DHT11_Start();

    /*
     * Check sensor response.
     */
    if (!DHT11_CheckResponse())
        return HAL_TIMEOUT;

    /*
     * Read 5 bytes:
     *
     * raw[0] = humidity integer
     * raw[1] = humidity decimal
     * raw[2] = temperature integer
     * raw[3] = temperature decimal
     * raw[4] = checksum
     */
    for (uint8_t i = 0; i < 5; i++)
    {
        if (!DHT11_ReadByte(&raw[i]))
            return HAL_TIMEOUT;
    }

    /*
     * Verify checksum.
     */
    if ((uint8_t)(
            raw[0] +
            raw[1] +
            raw[2] +
            raw[3]
        ) != raw[4])
    {
        return HAL_ERROR;
    }

    /*
     * DHT11 normally provides integer values.
     */
    data->humidity =
        (float)raw[0] + ((float)raw[1] / 10.0f);

    data->temperature =
        (float)raw[2] + ((float)raw[3] / 10.0f);

    return HAL_OK;
}
