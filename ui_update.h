/*
 * ui_update.h
 *
 *  Created on: Jul 18, 2014
 *      Author: Dale Hewgill
 */

#ifndef UI_UPDATE_H_
#define UI_UPDATE_H_

#include <msp430.h>
#include <stdint.h>
#include "datetime.h"


/* Provided functions: */
uint8_t change_day_of_week(DateTime_t *dt, int8_t dir);
uint8_t change_day_of_month(DateTime_t *dt, int8_t dir);
uint8_t change_year(DateTime_t *dt, int8_t dir);
uint8_t change_month(DateTime_t *dt, int8_t dir);
uint8_t change_hour(DateTime_t *dt, int8_t dir);
uint8_t change_minute(DateTime_t *dt, int8_t dir);
uint8_t change_second(DateTime_t *dt, int8_t dir);

#endif /* UI_UPDATE_H_ */

