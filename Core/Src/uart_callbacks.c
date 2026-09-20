/*
 * uart_callbacks.c
 *
 * Created on: Sep 16, 2026
 * Author: Asus
 */

#include "main.h"
#include "gps.h"

extern UART_HandleTypeDef huart4;


/*
 * This is the SAME receive byte used by gps.c.
 */
extern uint8_t gps_rx_byte;


/* ----------------------------------------------------------
 * Start GPS UART reception
 * ---------------------------------------------------------- */

void GPS_UART_Start_Receive(void)
{
    HAL_UART_Receive_IT(
        &huart4,
        &gps_rx_byte,
        1
    );
}


/* ----------------------------------------------------------
 * UART receive callback
 * ---------------------------------------------------------- */

void HAL_UART_RxCpltCallback(
    UART_HandleTypeDef *huart)
{
    if (huart->Instance == UART4)
    {
        /*
         * Process the byte received by UART4.
         */
        GPS_ProcessByte(gps_rx_byte);


        /*
         * Immediately arm the next byte.
         */
        HAL_UART_Receive_IT(
            &huart4,
            &gps_rx_byte,
            1
        );
    }
}
