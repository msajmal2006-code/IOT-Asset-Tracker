/*
 * w25q64.h
 *
 *  Created on: Sep 16, 2026
 *      Author: Asus
 */

#ifndef INC_W25Q64_H_
#define INC_W25Q64_H_

#include "main.h"
HAL_StatusTypeDef W25Q64_ReadID(SPI_HandleTypeDef*,GPIO_TypeDef*,uint16_t,uint8_t[3]);
HAL_StatusTypeDef W25Q64_Read(SPI_HandleTypeDef*,GPIO_TypeDef*,uint16_t,uint32_t,uint8_t*,uint32_t);
HAL_StatusTypeDef W25Q64_EraseSector(SPI_HandleTypeDef*,GPIO_TypeDef*,uint16_t,uint32_t);
HAL_StatusTypeDef W25Q64_PageProgram(SPI_HandleTypeDef*,GPIO_TypeDef*,uint16_t,uint32_t,const uint8_t*,uint32_t);


HAL_StatusTypeDef W25Q64_Write(
    SPI_HandleTypeDef *,
    GPIO_TypeDef *,
    uint16_t,
    uint32_t,
    const uint8_t *,
    uint32_t
);

#endif /* INC_W25Q64_H_ */
