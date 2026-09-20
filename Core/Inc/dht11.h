/*
 * dht11.h
 *
 *  Created on: Sep 16, 2026
 *      Author: Asus
 */

#ifndef INC_DHT11_H_
#define INC_DHT11_H_



#include "main.h"

typedef struct
{
    float temperature;
    float humidity;
} DHT11_Data_t;

HAL_StatusTypeDef DHT11_Read(
    TIM_HandleTypeDef *htim,
    GPIO_TypeDef *GPIOx,
    uint16_t GPIO_Pin,
    DHT11_Data_t *data
);


#endif /* INC_DHT11_H_ */
