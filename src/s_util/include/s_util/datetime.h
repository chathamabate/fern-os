
#pragma once

#include <stdint.h>

/*
 * Some date and time structures.
 *
 * Declaring these here so that there aren't redundant datetime structures littlered
 * throughout this project.
 *
 * NOTE: I have decided not to use bitfields here because I don't think I am that tight
 * on memory.
 */

/**
 * Date information.
 */
typedef struct _fernos_date_t {
    /**
     * Absolute year. Not some offset from 1980 or something.
     */
    uint16_t year; 

    /**
     * 1 - 12
     */ 
    uint8_t month; 

    /**
     * 1 - 31
     */
    uint8_t day;
} fernos_date_t;

/**
 * Time information.
 */
typedef struct _fernos_time_t {
    /**
     * 0 - 23
     */
    uint8_t hours;

    /**
     * 0 - 60
     */
    uint8_t seconds; 
} fernos_time_t;

/**
 * Date time tuple.
 */
typedef struct _fernos_datetime_t {
    fernos_date_t d;
    fernos_time_t t;
} fernos_datetime_t;
