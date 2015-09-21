/*
 * lcd.h
 *
 *  Created on: Apr 25, 2014
 *      Author: Dale Hewgill
 */

/* Simple library to control a HD44780 based 16x2 LCD through
 * Adafruit I2C based port expander 'backpack'.
 * Backpack connections are:
 * GP7:	Backlight
 * GP6:	DB7
 * GP5:	DB6
 * GP4:	DB5
 * GP3:	DB4
 * GP2:	E
 * GP1:	RS -> 0 selects configuration, 1 selects ddram or cgram
 * GP0:	NC
 *
 * Some things that we can exploit from the character set A00:
 * 		There are no characters with high byte b0001, can maybe use this to tag a command?
 * 		Also no characters with high byte b1000 or b1001.
 */


#ifndef LCD_H_
#define LCD_H_

#include <msp430.h>
#include <stdint.h>
#include <string.h>
#include "msp430_usi_i2c_int.h"

#define IO_EXPANDER_ADDR	0x40
#define IO_EXP_CONF_REG		0x05
#define IO_EXP_IO_REG		0x09
#define BACKLIGHT_PORT		7
#define DB7_PORT			6
#define DB6_PORT			5
#define DB5_PORT			4
#define DB4_PORT			3
#define E_PORT				2
#define RS_PORT				1

// Delays - for feeding into __delay_cycles(); adjust F_BRCLK as necessary.
#ifndef F_BRCLK
#define F_BRCLK				16000000ul			// Should be the same as the clock feeding USI.
#endif
#define DBG_DELAYS			0
#define DELAY_1US			((F_BRCLK)/1000000ul)//1 microsecond
#if DBG_DELAYS == 1
#define LCD_STD_CMD_DELAY	F_BRCLK/5000ul		//200 microseconds
#define LCD_CLEAR_DELAY		F_BRCLK/10ul		//10 milliseconds
#define LCD_HOME_DELAY		F_BRCLK/10ul		//10 milliseconds
#define LCD_INIT_DELAY_1	(DELAY_1US)*100000	//100 milliseconds
#define LCD_INIT_DELAY_2	F_BRCLK/10ul		//100 milliseconds
#else
#define LCD_STD_CMD_DELAY	((F_BRCLK)/50000ul)	//20 microseconds
//#define LCD_STD_CMD_DELAY	0					//0 microseconds
#define LCD_CLEAR_DELAY		((F_BRCLK)/500ul)	//2 milliseconds
#define LCD_HOME_DELAY		((F_BRCLK)/500ul)	//2 milliseconds
#define LCD_INIT_DELAY_1	((DELAY_1US)*4100)	//4.1 milliseconds
#define LCD_INIT_DELAY_2	((F_BRCLK)/10000ul)	//100 microseconds
#endif
#define LCD_BLINK_DELAY		((F_BRCLK)/4)		//250 milliseconds


// Defines for LCD states and flags.
#define LCD_BACKLIGHT_STATE	0x01
#define LCD_DISPLAY_ON		0x02
#define LCD_CURSOR_SHOW		0x04
#define LCD_CURSOR_BLINK	0x08
#define LCD_BUSY			0x40
#define LCD_EVENT_SIG		0x80


typedef union _lcd_sys_info_t
{
	uint8_t	flags;
	uint8_t	states;
} lcd_sys_info_t;


// Variables
extern const uint16_t gSysSleepMode;

// Exposed Functions
//void delay_us(uint16_t count);
int lcd_busy(void);
int lcd_get(void);
int lcd_release(void);
void lcd_raise_event(void);
void lcd_clear_event(void);

int lcd_check_io_expander_no_init_int(i2c_transaction_t * i2c_trn, volatile uint8_t *buf);
void lcd_io_expander_init_int(i2c_transaction_t * i2c_trn, volatile uint8_t *buf);
void lcd_init_int(i2c_transaction_t * i2c_trn, volatile uint8_t *buf);
int lcd_write_int(uint8_t val, uint8_t nibbleMode, uint8_t rs, volatile uint8_t *buf);
int lcd_set_backlight_int(int state, i2c_transaction_t *i2c_trans);
int lcd_clear_int(i2c_transaction_t *i2c_trans);
int lcd_home_int(i2c_transaction_t *i2c_trans);
int lcd_blink_cursor_int(i2c_transaction_t *i2c_trans);
int lcd_show_cursor_int(i2c_transaction_t *i2c_trans);
int lcd_get_backlight_state(void);

#endif /* LCD_H_ */
