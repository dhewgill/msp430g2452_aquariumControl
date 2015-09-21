/*
 * datetime.h
 *
 *  Created on: Feb 20, 2015
 *      Author: Dale Hewgill
 */

#ifndef DATETIME_H_
#define DATETIME_H_
#include <stdint.h>
//#include "ds3231m_lib.h"

//Defines:
#define SIMPLE_LEAP_YEAR		1		// Relies on a 2-digit year [i.e. 10].  Assumes offset from 2000.
#define YEAR_OFFSET				2000

// Provided types:
typedef struct _DateTime_t
{
	uint8_t seconds;
	uint8_t minutes;
	uint8_t hours;
	uint8_t dow;		//day of week; [1-7], 1=Sunday.
	uint8_t dom;		//day of month; [1-31]
	uint8_t month;		//month; [1-12], 1=January
	uint8_t year;
	uint8_t bcd_format;	//Is this in BCD or decimal format; 1=bcd.
} DateTime_t;

// Globals:
// const uint16_t year_offset = 2000;


// Provided functions:

#endif /* DATETIME_H_ */
