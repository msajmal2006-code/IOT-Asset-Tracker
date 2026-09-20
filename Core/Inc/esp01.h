/*
 * esp8266.h
 *
 *  Created on: Sep 17, 2026
 *      Author: Asus
 */

#ifndef INC_ESP01_H_
#define INC_ESP01_H_




#include "main.h"

HAL_StatusTypeDef ESP01_SendJSON(UART_HandleTypeDef *huart,
                                 const char *json);


#endif /* INC_ESP01_H_ */
