/*
 * w25q64.c
 *
 *  Created on: Sep 16, 2026
 *      Author: Asus
 */

#include "w25q64.h"

#define WREN   6
#define RDSR   5
#define READ   3
#define PP     2
#define SE     0x20
#define JEDEC  0x9F


static void cs(GPIO_TypeDef *p, uint16_t n, GPIO_PinState s)
{
    HAL_GPIO_WritePin(p, n, s);
}


static HAL_StatusTypeDef waitr(SPI_HandleTypeDef *s,
                               GPIO_TypeDef *p,
                               uint16_t n,
                               uint32_t to)
{
    uint8_t c = RDSR;
    uint8_t v;
    uint32_t st = HAL_GetTick();

    do
    {
        cs(p, n, GPIO_PIN_RESET);

        HAL_SPI_Transmit(s, &c, 1, 100);
        HAL_SPI_Receive(s, &v, 1, 100);

        cs(p, n, GPIO_PIN_SET);

        if (!(v & 1))
        {
            return HAL_OK;
        }

    } while ((HAL_GetTick() - st) < to);

    return HAL_TIMEOUT;
}


static HAL_StatusTypeDef wren(SPI_HandleTypeDef *s,
                              GPIO_TypeDef *p,
                              uint16_t n)
{
    uint8_t c = WREN;

    cs(p, n, GPIO_PIN_RESET);

    HAL_StatusTypeDef r =
        HAL_SPI_Transmit(s, &c, 1, 100);

    cs(p, n, GPIO_PIN_SET);

    return r;
}


HAL_StatusTypeDef W25Q64_ReadID(SPI_HandleTypeDef *s,
                                GPIO_TypeDef *p,
                                uint16_t n,
                                uint8_t id[3])
{
    uint8_t c = JEDEC;

    cs(p, n, GPIO_PIN_RESET);

    HAL_SPI_Transmit(s, &c, 1, 100);

    HAL_StatusTypeDef r =
        HAL_SPI_Receive(s, id, 3, 100);

    cs(p, n, GPIO_PIN_SET);

    return r;
}


HAL_StatusTypeDef W25Q64_Read(SPI_HandleTypeDef *s,
                              GPIO_TypeDef *p,
                              uint16_t n,
                              uint32_t a,
                              uint8_t *b,
                              uint32_t l)
{
    uint8_t h[4] =
    {
        READ,
        a >> 16,
        a >> 8,
        a
    };

    cs(p, n, GPIO_PIN_RESET);

    HAL_SPI_Transmit(s, h, 4, 100);

    HAL_StatusTypeDef r =
        HAL_SPI_Receive(s, b, l, 1000);

    cs(p, n, GPIO_PIN_SET);

    return r;
}


HAL_StatusTypeDef W25Q64_EraseSector(SPI_HandleTypeDef *s,
                                     GPIO_TypeDef *p,
                                     uint16_t n,
                                     uint32_t a)
{
    uint8_t h[4] =
    {
        SE,
        a >> 16,
        a >> 8,
        a
    };

    if (wren(s, p, n) != HAL_OK)
    {
        return HAL_ERROR;
    }

    cs(p, n, GPIO_PIN_RESET);

    HAL_SPI_Transmit(s, h, 4, 100);

    cs(p, n, GPIO_PIN_SET);

    return waitr(s, p, n, 3000);
}


HAL_StatusTypeDef W25Q64_PageProgram(SPI_HandleTypeDef *s,
                                     GPIO_TypeDef *p,
                                     uint16_t n,
                                     uint32_t a,
                                     const uint8_t *b,
                                     uint32_t l)
{
    if (!l || l > 256 || ((a & 255) + l > 256))
    {
        return HAL_ERROR;
    }

    uint8_t h[4] =
    {
        PP,
        a >> 16,
        a >> 8,
        a
    };

    if (wren(s, p, n) != HAL_OK)
    {
        return HAL_ERROR;
    }

    cs(p, n, GPIO_PIN_RESET);

    HAL_SPI_Transmit(s, h, 4, 100);

    HAL_SPI_Transmit(s, (uint8_t *)b, l, 1000);

    cs(p, n, GPIO_PIN_SET);

    return waitr(s, p, n, 1000);
}   // <-- THIS WAS MISSING


HAL_StatusTypeDef W25Q64_Write(SPI_HandleTypeDef *s,
                               GPIO_TypeDef *p,
                               uint16_t n,
                               uint32_t address,
                               const uint8_t *data,
                               uint32_t length)
{
    uint32_t chunk;
    uint32_t page_offset;

    while (length > 0)
    {
        page_offset = address & 0xFF;

        chunk = 256 - page_offset;

        if (chunk > length)
        {
            chunk = length;
        }

        if (W25Q64_PageProgram(s,
                               p,
                               n,
                               address,
                               data,
                               chunk) != HAL_OK)
        {
            return HAL_ERROR;
        }

        address += chunk;
        data += chunk;
        length -= chunk;
    }

    return HAL_OK;
}
