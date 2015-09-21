/*
 * ds3231m_lib.h
 *
 *  Created on: Dec 8, 2013
 *      Author: Dale Hewgill
 */

#ifndef DS3231M_LIB_H_
#define DS3231M_LIB_H_

#include <stdint.h>
#include "msp430_usi_i2c_int.h"
#include "datetime.h"


#define RTC_ADDR		0xd0 //Maxim DS3231M+ RTC I2C Slave address.
// Defines for Maxim DS3231M+ clock registers.
#define RTC_SEC					0x00
#define RTC_MIN					0x01
#define RTC_HRS					0x02
#define RTC_DOW					0x03	//Day of week.
#define RTC_DATE				0x04
#define RTC_MONTH				0x05	//Also contains a Century bit.
#define RTC_YEAR				0x06
#define RTC_ALM1_SEC			0x07	//MSB is bit 1 of alarm 1 mask.
#define RTC_ALM1_MIN			0x08	//MSB is bit 2 of alarm 1 mask.
#define RTC_ALM1_HR				0x09	//MSB is bit 3 if alarm 1 mask.
#define RTC_ALM1_DAY			0x0a	//MSB is bit 4 of alarm 1 mask.
#define RTC_ALM2_MIN			0x0b	//MSB is bit 1 of alarm 2 mask.
#define RTC_ALM2_HR				0x0c	//MSB is bit 2 if alarm 2 mask.
#define RTC_ALM2_DAY			0x0d	//MSB is bit 3 of alarm 2 mask.
#define RTC_CONTROL				0x0e
	#define RTC_CONTROL_EOSCb	0x80	//Active low!
	#define RTC_CONTROL_BBSQW	0x40
	#define RTC_CONTROL_CONV	0x20
	#define RTC_CONTROL_RS2		0x10
	#define RTC_CONTROL_RS1		0x08
	#define RTC_CONTROL_INTCN	0x04
	#define RTC_CONTROL_A2IE	0x02
	#define RTC_CONTROL_A1IE	0x01

#define RTC_STATUS				0x0f
	#define RTC_STATUS_OSF		0x80
	#define RTC_STATUS_EN32KHZ	0x08
	#define RTC_STATUS_BSY		0x04
	#define RTC_STATUS_A2F		0x02
	#define RTC_STATUS_A1F		0x01

#define RTC_AGE_OFFS			0x10
#define RTC_TEMP_MSB			0x11
#define RTC_TEMP_LSB			0x12


// *******************
// Provided functions:
/*uint8_t rtc_get_regs(uint8_t *msgBuf, uint8_t startAddr, uint8_t numRegs);
uint8_t rtc_set_regs(uint8_t *msgBuf, uint8_t startAddr, uint8_t numRegs);
uint8_t rtc_init(uint8_t ctlRegVal, uint8_t statRegVal);
uint8_t rtc_read_all(uint8_t *msgBuf);
uint8_t rtc_get_time(uint8_t *msgBuf);
uint8_t rtc_get_status(uint8_t *msgBuf);
uint8_t rtc_set_status(uint8_t *msgBuf, uint8_t regVal);
uint8_t rtc_get_control(uint8_t *msgBuf);
uint8_t rtc_set_control(uint8_t *msgBuf, uint8_t regVal);
uint8_t rtc_enable_alarm1(uint8_t *msgBuf);
uint8_t rtc_enable_alarm2(uint8_t *msgBuf);*/
uint8_t ds3231m_init(i2c_transaction_t* i2c_trn, volatile uint8_t* buf);
uint8_t ds3231m_get_time(i2c_transaction_t* i2c_trn);
uint8_t ds3231m_get_all(i2c_transaction_t* i2c_trn);
void convert_array_to_datetime(uint8_t* msgBuf, DateTime_t* dt, uint8_t keepBcd);
void convert_datetime_to_array(uint8_t* buf, DateTime_t* pdt);
void convert_datetime_to_decimal(DateTime_t* dt);
void convert_datetime_to_bcd(DateTime_t* dt);
void ds3231m_set_time(DateTime_t* pdt, i2c_transaction_t* pi2ct);
uint8_t rtc_get_temperature(uint8_t *msgBuf);
uint8_t decToBcd8(uint8_t val);
uint8_t bcdToDec8(uint8_t val);

//Debug functions
void ds3231m_set_time_dbg(DateTime_t* pdt, i2c_transaction_t *pi2ct);

#endif /* DS3231M_LIB_H_ */
