#ifndef INC_STORAGE_H_
#define INC_STORAGE_H_

#include "main.h"
#include "w25q64.h"

#define STORAGE_START_ADDRESS    0x001000UL
#define STORAGE_SIZE             0x010000UL
#define STORAGE_SLOT_SIZE        256UL

#define STORAGE_MAX_RECORDS \
    (STORAGE_SIZE / STORAGE_SLOT_SIZE)

HAL_StatusTypeDef STORAGE_Init(
    SPI_HandleTypeDef *hspi,
    GPIO_TypeDef *cs_port,
    uint16_t cs_pin
);

HAL_StatusTypeDef STORAGE_Save(
    const char *json
);

HAL_StatusTypeDef STORAGE_Read(
    uint32_t index,
    char *json,
    uint32_t json_size
);

uint32_t STORAGE_Count(void);

#endif
