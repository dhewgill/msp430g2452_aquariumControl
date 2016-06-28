/*
 * ui_update.c
 *
 *  Created on: Jul 18, 2014
 *      Author: Dale Hewgill
 */
#include "ui_update.h"

static uint8_t change_param(uint8_t param, int8_t dir, const uint8_t lbound, const uint8_t ubound)
{
	if (param > lbound && param < ubound)
	{
		return param + dir;
	}
	else if (param == lbound)
	{
		return ((0 < dir) ? lbound + 1 : ubound);
	}
	else if (param == ubound)
	{
		return ((dir > 0) ? lbound : ubound - 1);
	}
	return 0xff;	// something went wrong.
}

/*
 * 1=Sunday, 7=Saturday
 */
uint8_t change_day_of_week(DateTime_t *dt, int8_t dir)
{
	dt->dow = change_param(dt->dow, dir, 1, 7);
	return dt->dow;
}

/*
 * Right now it doesn't account for current month or leap year!
 */
uint8_t change_day_of_month(DateTime_t *dt, int8_t dir)
{
	dt->dom = change_param(dt->dom, dir, 1, 31);
	return dt->dom;
}

uint8_t change_year(DateTime_t *dt, int8_t dir)
{
	dt->year = change_param(dt->year, dir, 0, 99);
	return dt->year;
}

uint8_t change_month(DateTime_t *dt, int8_t dir)
{
	dt->month = change_param(dt->month, dir, 1, 12);
	return dt->month;
}

uint8_t change_hour(DateTime_t *dt, int8_t dir)
{
	dt->hours = change_param(dt->hours, dir, 0, 23);
	return dt->hours;
}

uint8_t change_minute(DateTime_t *dt, int8_t dir)
{
	dt->minutes = change_param(dt->minutes, dir, 0, 59);
	return dt->minutes;
}

uint8_t change_second(DateTime_t *dt, int8_t dir)
{
	dt->seconds = change_param(dt->seconds, dir, 0, 59);
	return dt->seconds;
}
