/*
 * datetime.c
 *
 *  Created on: Feb 20, 2015
 *      Author: Dale Hewgill
 */

#include "datetime.h"
#include "ds3231m_lib.h"	// This include is here because the sourced functions/defs are only needed here.

#if SIMPLE_LEAP_YEAR == 0
/* *********
Takes a year as an int.
Returns 1 if leap year, 0 otherwise.
See: http://stackoverflow.com/questions/3220163/how-to-find-leap-year-programatically-in-c
for algorithm details.
********* */
int is_leap_year(DateTime *pdt)
{
	uint16_t year;
	if (pdt->bcd_format)
		year = bcdToDec8(pdt->year) + year_offset;
	else
		year = pdt->year + YEAR_OFFSET;

	return ( ((year & 3) == 0 && ((year % 25) != 0 || (year & 15) == 0)) );
}
#else
// Calculates if a year in the 21st century [2000's] is a leap year.
// The year [two digits] is implied to be within [2000,2100].
int is_leap_year(DateTime_t *pdt)
{
	uint8_t year;
	if (pdt->bcd_format)
		year = bcdToDec8(pdt->year);
	else
		year = pdt->year;

	return ( (year & 3) == 0 );
}
#endif

/* *********
* Check if we're in DST now.  Assumes North America DST rules.
* Spring ahead - second Sunday in March.  DST = <dst flag mask value>.
* Fall back - first Sunday in November.  DST = 0.  Standard time in effect.
* Return > 0 if the supplied date is in DST.
* Possible resource for help/optimization: http://stackoverflow.com/questions/5590429/calculating-daylight-savings-time-from-only-date
********* */
int is_dst(DateTime_t *pdt)
{
	const int DST = 1;
	int dstVal;

	if (pdt->bcd_format)
		convert_datetime_to_decimal(pdt);

	if ( (pdt->month > 11) || (pdt->month < 3) )
		dstVal = 0;
	else if ( (pdt->month < 11) && (pdt->month > 3) )
		dstVal = DST;
	else if (pdt->month == 3)		// March
	{
		if (pdt->dom < 8)
			dstVal = 0;
		else if ( (pdt->dom > 14) || ((pdt->dom - 7) >= pdt->dow) )
			dstVal = DST;
		else
			dstVal = 0;
	}
	else if ( pdt->month == 11 )	// November
	{
		if ( (pdt->dom > 8) || (pdt->dom >= pdt->dow) )
			dstVal = 0;
		else
			dstVal = DST;
	}
	return dstVal;
}

uint8_t days_in_month(DateTime_t *pdt)
{
	const uint8_t daysInMonths[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	uint8_t month, adjust;

	adjust = 0;
	month = (pdt->bcd_format > 0) ? bcdToDec8(pdt->month) - 1 : pdt->month - 1;

	if ( (month == 1) && is_leap_year(pdt) )	// February in a leap year.
		adjust = 1;

	return (daysInMonths[month] + adjust);
}
