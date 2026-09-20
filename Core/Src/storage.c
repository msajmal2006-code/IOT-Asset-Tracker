#include "storage.h"
#include <string.h>


/* ============================================================
 * Storage configuration
 * ============================================================ */

#define STORAGE_MAGIC        0xA55AU
#define STORAGE_HEADER_SIZE  4U


/* ============================================================
 * W25Q64 connection
 * ============================================================ */

static SPI_HandleTypeDef *storage_spi = NULL;
static GPIO_TypeDef *storage_cs_port = NULL;
static uint16_t storage_cs_pin = 0;


/* ============================================================
 * Current write position
 * ============================================================ */

static uint32_t storage_index = 0;


/* ============================================================
 * Record format
 *
 * Byte 0-1 : MAGIC
 * Byte 2-3 : JSON length
 * Byte 4.. : JSON
 *
 * Remaining bytes are unused.
 * ============================================================ */


/* ============================================================
 * Check whether slot is empty
 * ============================================================ */

static uint8_t STORAGE_IsEmpty(uint32_t address)
{
    uint8_t buffer[4];

    if (W25Q64_Read(
            storage_spi,
            storage_cs_port,
            storage_cs_pin,
            address,
            buffer,
            sizeof(buffer)) != HAL_OK)
    {
        return 0;
    }

    if (buffer[0] == 0xFF &&
        buffer[1] == 0xFF &&
        buffer[2] == 0xFF &&
        buffer[3] == 0xFF)
    {
        return 1;
    }

    return 0;
}


/* ============================================================
 * Erase complete storage area
 *
 * 64 KB = 16 sectors
 * ============================================================ */

static HAL_StatusTypeDef STORAGE_EraseAll(void)
{
    for (uint32_t address = STORAGE_START_ADDRESS;
         address < (STORAGE_START_ADDRESS + STORAGE_SIZE);
         address += 4096UL)
    {
        if (W25Q64_EraseSector(
                storage_spi,
                storage_cs_port,
                storage_cs_pin,
                address) != HAL_OK)
        {
            return HAL_ERROR;
        }
    }

    return HAL_OK;
}


/* ============================================================
 * Initialize storage
 * ============================================================ */

HAL_StatusTypeDef STORAGE_Init(
    SPI_HandleTypeDef *hspi,
    GPIO_TypeDef *cs_port,
    uint16_t cs_pin)
{
    if (hspi == NULL ||
        cs_port == NULL)
    {
        return HAL_ERROR;
    }

    storage_spi = hspi;
    storage_cs_port = cs_port;
    storage_cs_pin = cs_pin;

    storage_index = 0;

    /*
     * Find first unused slot.
     */
    for (uint32_t i = 0;
         i < STORAGE_MAX_RECORDS;
         i++)
    {
        uint32_t address =
            STORAGE_START_ADDRESS +
            (i * STORAGE_SLOT_SIZE);

        if (STORAGE_IsEmpty(address))
        {
            storage_index = i;
            return HAL_OK;
        }
    }

    /*
     * Storage area is full.
     *
     * For this prototype, erase and start again.
     */
    if (STORAGE_EraseAll() != HAL_OK)
    {
        return HAL_ERROR;
    }

    storage_index = 0;

    return HAL_OK;
}


/* ============================================================
 * Save telemetry JSON
 * ============================================================ */

HAL_StatusTypeDef STORAGE_Save(
    const char *json)
{
    if (storage_spi == NULL ||
        storage_cs_port == NULL ||
        json == NULL)
    {
        return HAL_ERROR;
    }

    if (storage_index >= STORAGE_MAX_RECORDS)
    {
        /*
         * Storage full.
         *
         * Erase and restart.
         */
        if (STORAGE_EraseAll() != HAL_OK)
        {
            return HAL_ERROR;
        }

        storage_index = 0;
    }


    uint32_t json_length =
        strlen(json);


    /*
     * Maximum JSON size:
     *
     * 256 byte slot
     * - 4 byte header
     * = 252 bytes
     */
    if (json_length == 0 ||
        json_length > (STORAGE_SLOT_SIZE - STORAGE_HEADER_SIZE))
    {
        return HAL_ERROR;
    }


    uint32_t address =
        STORAGE_START_ADDRESS +
        (storage_index * STORAGE_SLOT_SIZE);


    /*
     * Make record buffer.
     *
     * Fill with 0xFF so unused flash bytes remain erased.
     */
    uint8_t record[STORAGE_SLOT_SIZE];

    memset(record, 0xFF, sizeof(record));


    /*
     * Magic number.
     */
    record[0] =
        (uint8_t)(STORAGE_MAGIC & 0xFF);

    record[1] =
        (uint8_t)((STORAGE_MAGIC >> 8) & 0xFF);


    /*
     * JSON length.
     */
    record[2] =
        (uint8_t)(json_length & 0xFF);

    record[3] =
        (uint8_t)((json_length >> 8) & 0xFF);


    /*
     * JSON payload.
     */
    memcpy(
        &record[STORAGE_HEADER_SIZE],
        json,
        json_length
    );


    /*
     * Write complete 256-byte record.
     *
     * Address is always 256-byte aligned,
     * so this does not cross a flash page boundary.
     */
    if (W25Q64_Write(
            storage_spi,
            storage_cs_port,
            storage_cs_pin,
            address,
            record,
            STORAGE_SLOT_SIZE) != HAL_OK)
    {
        return HAL_ERROR;
    }


    storage_index++;

    return HAL_OK;
}


/* ============================================================
 * Read stored telemetry
 * ============================================================ */

HAL_StatusTypeDef STORAGE_Read(
    uint32_t index,
    char *json,
    uint32_t json_size)
{
    uint8_t header[STORAGE_HEADER_SIZE];

    if (storage_spi == NULL ||
        storage_cs_port == NULL ||
        json == NULL)
    {
        return HAL_ERROR;
    }

    if (index >= STORAGE_MAX_RECORDS)
    {
        return HAL_ERROR;
    }

    if (json_size == 0)
    {
        return HAL_ERROR;
    }


    uint32_t address =
        STORAGE_START_ADDRESS +
        (index * STORAGE_SLOT_SIZE);


    /*
     * Read header.
     */
    if (W25Q64_Read(
            storage_spi,
            storage_cs_port,
            storage_cs_pin,
            address,
            header,
            sizeof(header)) != HAL_OK)
    {
        return HAL_ERROR;
    }


    /*
     * Check magic.
     */
    uint16_t magic =
        ((uint16_t)header[1] << 8) |
        header[0];

    if (magic != STORAGE_MAGIC)
    {
        return HAL_ERROR;
    }


    /*
     * Get JSON length.
     */
    uint16_t length =
        ((uint16_t)header[3] << 8) |
        header[2];


    if (length == 0 ||
        length > (STORAGE_SLOT_SIZE - STORAGE_HEADER_SIZE))
    {
        return HAL_ERROR;
    }


    /*
     * Check destination buffer.
     */
    if ((uint32_t)length + 1U > json_size)
    {
        return HAL_ERROR;
    }


    /*
     * Read JSON.
     */
    if (W25Q64_Read(
            storage_spi,
            storage_cs_port,
            storage_cs_pin,
            address + STORAGE_HEADER_SIZE,
            (uint8_t *)json,
            length) != HAL_OK)
    {
        return HAL_ERROR;
    }


    json[length] = '\0';

    return HAL_OK;
}


/* ============================================================
 * Number of stored records
 * ============================================================ */

uint32_t STORAGE_Count(void)
{
    return storage_index;
}
