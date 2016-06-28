/*
 * ds3231m_lib.c
 *
 *  Created on: Dec 8, 2013
 *      Author: Dale Hewgill
 */


#include "ds3231m_lib.h"

// Internal functions.
static void reset_addr_ptr_to_start(i2c_transaction_t* i2c_trn)
{
	// Set the ds3231m register pointer back to the start.
	i2c_trn->address = RTC_ADDR;
	i2c_trn->transactType = I2C_T_TX_STOP;
	i2c_trn->buf[0] = RTC_SEC;
	i2c_trn->numBytes = 1;
	usi_i2c_txrx_start(i2c_trn);
	//usi_i2c_sleep_wait(1);

}

static inline uint8_t ds3231m_get_regs(i2c_transaction_t* i2c_trn, uint8_t startReg, uint8_t numRegs)
{
	// The i2c buffer must be set by the caller.
	if (startReg == RTC_SEC)
	{
		i2c_trn->address = RTC_ADDR | I2C_READ_BIT;
		i2c_trn->numBytes = numRegs;
		i2c_trn->transactType = I2C_T_RX_STOP;
	}
	else
	{

	}

	return 0;
}

static inline uint8_t ds3231m_set_regs(i2c_transaction_t* i2c_trn, uint8_t startReg, uint8_t numRegs)
{
	// The i2c buffer must be set by caller, and the data as well.
	i2c_trn->address = RTC_ADDR;
	i2c_trn->numBytes = numRegs;
	i2c_trn->transactType = I2C_T_TX_STOP;
	return 0;
}


// Public functions.
uint8_t ds3231m_init(i2c_transaction_t* i2c_trn, volatile uint8_t* buf)
{
	uint8_t status;

	// Get the status register.
	i2c_trn->buf = buf;
	i2c_trn->address = RTC_ADDR | I2C_READ_BIT;
	i2c_trn->buf[0] = RTC_STATUS;
	i2c_trn->transactType = I2C_T_TX_RESTART;
	i2c_trn->numBytes = 1;
	usi_i2c_txrx_start(i2c_trn);
	usi_i2c_sleep_wait(1);
	i2c_trn->buf = buf;
	i2c_trn->address = RTC_ADDR;
	i2c_trn->transactType = I2C_T_RX_STOP;
	i2c_trn->numBytes = 1;
	usi_i2c_txrx_resume();
	usi_i2c_sleep_wait(1);
	status = buf[0];

	// Set up the RTC and clear all flags.
	i2c_trn->buf = buf;
	i2c_trn->address = RTC_ADDR;
	i2c_trn->transactType = I2C_T_TX_STOP;
	i2c_trn->buf[0] = RTC_CONTROL;
	i2c_trn->buf[1] = 0x00;
	i2c_trn->buf[2] = 0x00;
	i2c_trn->numBytes = 3;
	usi_i2c_txrx_start(i2c_trn);
	usi_i2c_sleep_wait(1);

	// Set the ds3231m register pointer back to the start.
	i2c_trn->buf = buf;
	reset_addr_ptr_to_start(i2c_trn);
	usi_i2c_sleep_wait(1);
	/*i2c_trn->address = RTC_ADDR;
	i2c_trn->transactType = I2C_T_TX_STOP;
	i2c_trn->buf[0] = RTC_SEC;
	i2c_trn->numBytes = 1;
	usi_i2c_txrx_start(i2c_trn);
	usi_i2c_sleep_wait(1);*/


	// Return the contents of the status register.
	return status;
}

uint8_t ds3231m_get_time(i2c_transaction_t* i2c_trn)
{
	return ds3231m_get_regs(i2c_trn, RTC_SEC, 7);
}

uint8_t ds3231m_get_all(i2c_transaction_t* i2c_trn)
{
	return ds3231m_get_regs(i2c_trn, RTC_SEC, 19);
}

// #########################
// Utility functions

uint8_t decToBcd8(uint8_t val)
{
   return ( (val/10*16) + (val%10) );
}

uint8_t bcdToDec8(uint8_t val)
{
   return( (val/16*10) + (val%16) );
}

/*
Convert an array that contains all of the DS3231 time register values
and put them into a Datetime structure.
Format the entries as per the keepBcd variable [keepBcd > 0 implies retain BCD formatting].
*/
void convert_array_to_datetime(uint8_t* msgBuf, DateTime_t* dt, uint8_t keepBcd)
{
	dt->seconds = msgBuf[0] & 0x7f;
	dt->minutes = msgBuf[1] & 0x7f;
	dt->hours = msgBuf[2] & 0x3f;	//Using 24hr time format.
	dt->dow = msgBuf[3] & 0x07;
	dt->dom = msgBuf[4] & 0x3f;
	dt->month = msgBuf[5] & 0x1f;
	dt->year = msgBuf[6];
	dt->bcd_format = 1;

	if (keepBcd == 0)
	{
		//convert to decimal.
		convert_datetime_to_decimal(dt);
	}
}

void convert_datetime_to_array(uint8_t* buf, DateTime_t* pdt)
{
	//buf[0] = RTC_SEC;
	buf[0] = pdt->seconds;
	buf[1] = pdt->minutes;
	buf[2] = pdt->hours;
	buf[3] = pdt->dow;
	buf[4] = pdt->dom;
	buf[5] = pdt->month;
	buf[6] = pdt->year;
}

void convert_datetime_to_decimal(DateTime_t* dt)
{
	if (dt->bcd_format)
	{
		dt->seconds = bcdToDec8(dt->seconds);
		dt->minutes = bcdToDec8(dt->minutes);
		dt->hours = bcdToDec8(dt->hours);
		dt->dow = bcdToDec8(dt->dow);
		dt->dom = bcdToDec8(dt->dom);
		dt->month = bcdToDec8(dt->month);
		dt->year = bcdToDec8(dt->year);
		dt->bcd_format = 0;
	}
}

void convert_datetime_to_bcd(DateTime_t* dt)
{
	if (dt->bcd_format == 0)
	{
		dt->seconds = decToBcd8(dt->seconds);
		dt->minutes = decToBcd8(dt->minutes);
		dt->hours = decToBcd8(dt->hours);
		dt->dow = decToBcd8(dt->dow);
		dt->dom = decToBcd8(dt->dom);
		dt->month = decToBcd8(dt->month);
		dt->year = decToBcd8(dt->year);
		dt->bcd_format = 1;
	}
}

void ds3231m_set_time(DateTime_t* pdt, i2c_transaction_t* pi2ct)
{
	if (pdt->bcd_format == 0)				// Convert datetime to bcd, if necessary.
	{
		convert_datetime_to_bcd(pdt);
	}

	convert_datetime_to_array(pi2ct->buf, pdt);

	pi2ct->address = RTC_ADDR;
	pi2ct->numBytes = 8;
	pi2ct->transactType = I2C_T_TX_STOP;
	//Caller can now start the i2c_transaction.
}

void ds3231m_set_time_dbg(DateTime_t* pdt, i2c_transaction_t* pi2ct)
{
	DateTime_t dt = {0x15, 0x45, 0x01, 0x02, 0x14, 0x02, 0x05, 0x01};	// Monday, 14-Feb-2005, 01:45:15, BCD format.
	volatile uint8_t* bufptr = pi2ct->buf;
	if (pdt == NULL)
		pdt = &dt;

	pi2ct->buf[0] = RTC_SEC;
	pi2ct->buf[1] = pdt->seconds;
	pi2ct->buf[2] = pdt->minutes;
	pi2ct->buf[3] = pdt->hours;
	pi2ct->buf[4] = pdt->dow;
	pi2ct->buf[5] = pdt->dom;
	pi2ct->buf[6] = pdt->month;
	pi2ct->buf[7] = pdt->year;

	ds3231m_set_regs(pi2ct, RTC_SEC, 8);
	usi_i2c_txrx_start(pi2ct);
	usi_i2c_sleep_wait(1);

	// Set the ds3231m register pointer back to the start.
	pi2ct->buf = bufptr;
	reset_addr_ptr_to_start(pi2ct);
	usi_i2c_sleep_wait(1);

	pi2ct->transactType = I2C_T_IDLE;
}
