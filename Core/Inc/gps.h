/*
 * gps.h
 *
 *  Created on: Sep 16, 2026
 *      Author: Asus
 */

#ifndef INC_GPS_H_
#define INC_GPS_H_



#include "main.h"

typedef struct
{
    uint8_t valid;
    uint8_t fix;

    double latitude;
    double longitude;

    double speed_knots;
    double course_deg;

    uint32_t utc_hhmmss;

} GPS_Data_t;


void GPS_Init(UART_HandleTypeDef *huart);

void GPS_Start(void);

void GPS_ProcessByte(uint8_t b);

uint8_t GPS_GetLatest(GPS_Data_t *data);


#endif /* GPS_H */
