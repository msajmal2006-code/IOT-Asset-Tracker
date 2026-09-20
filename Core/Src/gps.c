/*
 * gps.c
 *
 * NEO-6M GPS driver
 *
 * UART4:
 *   PC11 <- NEO-6M TX
 *
 * Baud:
 *   9600 8N1
 *
 * Supported NMEA:
 *   $GPRMC
 *   $GNRMC
 */

#include "gps.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>


/* =========================================================
 * Configuration
 * ========================================================= */

#define GPS_LINE_SIZE 128


/* =========================================================
 * Private variables
 * ========================================================= */

/*
 * UART handle used by GPS_Start().
 */
static UART_HandleTypeDef *gps_uart = NULL;


/*
 * Set to 1 when a complete RMC sentence is ready.
 *
 * This variable is accessed by both:
 *   - main application
 *   - UART interrupt
 */
static volatile uint8_t ready_flag = 0;


/*
 * Current incoming NMEA sentence.
 */
static char line[GPS_LINE_SIZE];


/*
 * Completed RMC sentence.
 */
static char ready[GPS_LINE_SIZE];


/*
 * Current position inside line[].
 */
static uint16_t idx = 0;


/*
 * UART receive byte.
 *
 * IMPORTANT:
 * This variable is NOT static because
 * uart_callbacks.c accesses it with:
 *
 * extern uint8_t gps_rx_byte;
 */
uint8_t gps_rx_byte = 0;


/* =========================================================
 * NMEA coordinate conversion
 * =========================================================
 *
 * NMEA latitude:
 *
 *     DDMM.MMMM
 *
 * Example:
 *
 *     1106.0480
 *
 * becomes:
 *
 *     11.100800 degrees
 *
 *
 * NMEA longitude:
 *
 *     DDDMM.MMMM
 *
 * Example:
 *
 *     07657.3480
 *
 * becomes:
 *
 *     76.955800 degrees
 * ========================================================= */

static double nmea_to_decimal(
    const char *value,
    char direction)
{
    double raw;
    double degrees;
    double minutes;
    double decimal;


    if (value == NULL ||
        value[0] == '\0')
    {
        return 0.0;
    }


    raw = atof(value);


    /*
     * Extract degrees.
     */
    degrees =
        (int)(raw / 100.0);


    /*
     * Extract minutes.
     */
    minutes =
        raw -
        (degrees * 100.0);


    /*
     * Convert minutes to decimal degrees.
     */
    decimal =
        degrees +
        (minutes / 60.0);


    /*
     * South and West coordinates
     * are negative.
     */
    if (direction == 'S' ||
        direction == 'W')
    {
        decimal = -decimal;
    }


    return decimal;
}


/* =========================================================
 * Process one received GPS byte
 * ========================================================= */

void GPS_ProcessByte(uint8_t b)
{
    /*
     * -----------------------------------------------------
     * End of NMEA sentence
     * -----------------------------------------------------
     */
    if (b == '\n')
    {
        /*
         * Only store RMC sentences.
         *
         * We need RMC because it contains:
         *
         *   UTC
         *   Fix status
         *   Latitude
         *   Longitude
         *   Speed
         *   Course
         */
        if ((idx >= 6) &&
            ((strncmp(
                line,
                "$GPRMC",
                6
            ) == 0) ||
             (strncmp(
                line,
                "$GNRMC",
                6
            ) == 0)))
        {
            /*
             * Do not overwrite an RMC sentence
             * that has not yet been processed.
             */
            if (!ready_flag)
            {
                memcpy(
                    ready,
                    line,
                    idx
                );


                /*
                 * Add string terminator.
                 */
                if (idx < GPS_LINE_SIZE - 1)
                {
                    ready[idx] = '\0';
                }
                else
                {
                    ready[GPS_LINE_SIZE - 1] = '\0';
                }


                ready_flag = 1;
            }
        }


        /*
         * Start collecting the next sentence.
         */
        idx = 0;

        return;
    }


    /*
     * -----------------------------------------------------
     * Ignore carriage return
     * -----------------------------------------------------
     */
    if (b == '\r')
    {
        return;
    }


    /*
     * -----------------------------------------------------
     * Store received byte
     * -----------------------------------------------------
     */
    if (idx < GPS_LINE_SIZE - 1)
    {
        line[idx++] = (char)b;
    }
    else
    {
        /*
         * Sentence is too long.
         *
         * Discard it and start again.
         */
        idx = 0;
    }
}


/* =========================================================
 * Initialize GPS
 * ========================================================= */

void GPS_Init(
    UART_HandleTypeDef *huart)
{
    /*
     * Store UART handle.
     */
    gps_uart = huart;


    /*
     * Reset parser.
     */
    idx = 0;

    ready_flag = 0;

    gps_rx_byte = 0;


    /*
     * Clear buffers.
     */
    memset(
        line,
        0,
        sizeof(line)
    );

    memset(
        ready,
        0,
        sizeof(ready)
    );
}


/* =========================================================
 * Start GPS UART interrupt reception
 * ========================================================= */

void GPS_Start(void)
{
    if (gps_uart != NULL)
    {
        HAL_UART_Receive_IT(
            gps_uart,
            &gps_rx_byte,
            1
        );
    }
}


/* =========================================================
 * Parse latest RMC sentence
 * ========================================================= */

uint8_t GPS_GetLatest(
    GPS_Data_t *data)
{
    char sentence[GPS_LINE_SIZE];

    char *token;
    char *saveptr;

    char *field[20];

    int field_count = 0;


    /*
     * Validate output pointer.
     */
    if (data == NULL)
    {
        return 0;
    }


    /*
     * Clear output structure.
     */
    memset(
        data,
        0,
        sizeof(GPS_Data_t)
    );


    /*
     * -----------------------------------------------------
     * Copy completed sentence
     *
     * ready_flag is modified from the UART ISR,
     * so protect this small critical section.
     * -----------------------------------------------------
     */

    __disable_irq();


    if (!ready_flag)
    {
        __enable_irq();

        return 0;
    }


    /*
     * Copy the completed RMC sentence.
     */
    strncpy(
        sentence,
        ready,
        GPS_LINE_SIZE - 1
    );


    sentence[GPS_LINE_SIZE - 1] =
        '\0';


    /*
     * Mark sentence as consumed.
     */
    ready_flag = 0;


    __enable_irq();


    /*
     * -----------------------------------------------------
     * Verify sentence type
     * -----------------------------------------------------
     */

    if ((strncmp(
            sentence,
            "$GPRMC",
            6
        ) != 0) &&
        (strncmp(
            sentence,
            "$GNRMC",
            6
        ) != 0))
    {
        return 0;
    }


    /*
     * -----------------------------------------------------
     * Split NMEA sentence by commas
     * -----------------------------------------------------
     */

    token =
        strtok_r(
            sentence,
            ",",
            &saveptr
        );


    while ((token != NULL) &&
           (field_count < 20))
    {
        field[field_count++] =
            token;


        token =
            strtok_r(
                NULL,
                ",",
                &saveptr
            );
    }


    /*
     * -----------------------------------------------------
     * RMC structure
     *
     * 0  = $GPRMC / $GNRMC
     * 1  = UTC time
     * 2  = Status
     * 3  = Latitude
     * 4  = N/S
     * 5  = Longitude
     * 6  = E/W
     * 7  = Speed over ground
     * 8  = Course over ground
     * 9  = Date
     * -----------------------------------------------------
     */

    if (field_count < 9)
    {
        return 0;
    }


    /* =====================================================
     * UTC
     * ===================================================== */

    if (field[1][0] != '\0')
    {
        /*
         * Example:
         *
         * 120533.00
         *
         * becomes:
         *
         * 120533
         */
        data->utc_hhmmss =
            (uint32_t)atof(
                field[1]
            );
    }


    /* =====================================================
     * GPS FIX STATUS
     * =====================================================
     *
     * A = valid
     * V = invalid
     * ===================================================== */

    if (field[2][0] == 'A')
    {
        data->valid = 1;
        data->fix = 1;
    }
    else
    {
        data->valid = 0;
        data->fix = 0;
    }


    /*
     * If there is no valid GPS fix,
     * do not use coordinate/speed/course fields.
     */
    if (data->fix == 0)
    {
        data->latitude = 0.0;
        data->longitude = 0.0;

        data->speed_knots = 0.0;
        data->course_deg = 0.0;

        return 1;
    }


    /* =====================================================
     * LATITUDE
     * ===================================================== */

    if ((field[3][0] != '\0') &&
        (field[4][0] != '\0'))
    {
        data->latitude =
            nmea_to_decimal(
                field[3],
                field[4][0]
            );
    }


    /* =====================================================
     * LONGITUDE
     * ===================================================== */

    if ((field[5][0] != '\0') &&
        (field[6][0] != '\0'))
    {
        data->longitude =
            nmea_to_decimal(
                field[5],
                field[6][0]
            );
    }


    /* =====================================================
     * SPEED
     * =====================================================
     *
     * Unit:
     *     knots
     * ===================================================== */

    if (field[7][0] != '\0')
    {
        data->speed_knots =
            atof(
                field[7]
            );
    }


    /* =====================================================
     * COURSE
     * =====================================================
     *
     * Unit:
     *     degrees
     *
     * Valid:
     *     0 <= course <= 360
     * ===================================================== */

    if (field[8][0] != '\0')
    {
        double course;


        course =
            atof(
                field[8]
            );


        /*
         * Protect against malformed GPS data.
         */
        if ((course >= 0.0) &&
            (course <= 360.0))
        {
            data->course_deg =
                course;
        }
        else
        {
            data->course_deg =
                0.0;
        }
    }


    /*
     * Successfully parsed RMC.
     */
    return 1;
}
