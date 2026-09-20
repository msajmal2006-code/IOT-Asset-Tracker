#include "esp01.h"
#include <string.h>

#define ESP01_TIMEOUT_MS 2000U

HAL_StatusTypeDef ESP01_SendJSON(UART_HandleTypeDef *huart,
                                 const char *json)
{
    if (huart == NULL || json == NULL)
        return HAL_ERROR;

    HAL_StatusTypeDef status;

    /*
     * JSON is sent as one line.
     * The ESP-01 firmware will use '\n' as the packet delimiter.
     */
    status = HAL_UART_Transmit(
        huart,
        (uint8_t *)json,
        strlen(json),
        ESP01_TIMEOUT_MS
    );

    if (status != HAL_OK)
        return status;

    const uint8_t newline = '\n';

    status = HAL_UART_Transmit(
        huart,
        (uint8_t *)&newline,
        1,
        100
    );

    return status;
}
