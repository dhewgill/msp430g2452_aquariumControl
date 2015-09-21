/*
 * lcd.c
 *
 *  Created on: Apr 25, 2014
 *      Author: Dale Hewgill
 */

#include "lcd.h"

//Globals
static lcd_sys_info_t lcd_info;


//Function Prototypes
static int send_lcd_cmd_int(uint8_t val, i2c_transaction_t *i2c_trans);


//Implementation
static inline void delay_us(uint16_t count)
{
	while (count--)
		__delay_cycles(16);
}

int lcd_busy(void)
{
	return ( 0 != (lcd_info.states & LCD_BUSY) );
}

int lcd_get(void)
{
	lcd_info.states |= LCD_BUSY;
	return 1;
}

int lcd_release(void)
{
	lcd_info.states &= ~LCD_BUSY;
	return 1;
}

void lcd_raise_event(void)
{
	lcd_info.flags |= LCD_EVENT_SIG;
}

void lcd_clear_event(void)
{
	lcd_info.flags &= ~LCD_EVENT_SIG;
}

int lcd_check_event(void)
{
	return (0 != (lcd_info.flags & LCD_BUSY));
}

int lcd_check_io_expander_no_init_int(i2c_transaction_t * i2c_trn, volatile uint8_t *buf)
{
	i2c_trn->address = (IO_EXPANDER_ADDR | 0x01);
	i2c_trn->numBytes = 1;
	i2c_trn->callbackFn = NULL;
	i2c_trn->transactType = I2C_T_RX_STOP;
	i2c_trn->buf = buf;

	usi_i2c_txrx_start(i2c_trn);
	usi_i2c_sleep_wait(1);

	i2c_trn->transactType = I2C_T_IDLE;
	i2c_trn->buf = buf;

	return (i2c_trn->buf[0] == 0xff);
}

void lcd_io_expander_init_int(i2c_transaction_t * i2c_trn, volatile uint8_t *buf)
{
	i2c_trn->address = IO_EXPANDER_ADDR;
	i2c_trn->numBytes = 7;
	i2c_trn->callbackFn = NULL;
	i2c_trn->transactType = I2C_T_TX_STOP;
	i2c_trn->buf = buf;
	memset((uint8_t *)i2c_trn->buf, 0, 6);							// Make sure the buffer is zeroed.

	i2c_trn->buf[6] = 0x20;

	usi_i2c_txrx_start(i2c_trn);
	usi_i2c_sleep_wait(1);

	i2c_trn->transactType = I2C_T_IDLE;
}

void lcd_init_int(i2c_transaction_t * i2c_trn, volatile uint8_t *buf)
{
	const uint16_t delays[] = {	LCD_INIT_DELAY_1/16,
								LCD_INIT_DELAY_2/16,
								LCD_INIT_DELAY_2/16,
								LCD_STD_CMD_DELAY/16,
								LCD_STD_CMD_DELAY/16,
								LCD_STD_CMD_DELAY/16,
								LCD_CLEAR_DELAY/16,
								LCD_STD_CMD_DELAY/16,
								LCD_HOME_DELAY/16,
								LCD_STD_CMD_DELAY/16
	};

	const uint8_t cmds[] = {	0x30,	// init
								0x30,	// init
								0x30,	// init; display now reset.
								0x20,	// Put display in 4-bit mode [sent in 8-bit mode]
								0x28,	// 4-bit mode, #lines=2, 5x8dot format.
								0x08,	// Display off, cursor off, blinking off.
								0x01,	// Clear display.
								0x06,	// Set entry mode, cursor move, no display shift.
								0x02,	// Return home.
								0x0c	// Display on + no display cursor + no blinking on.
								//0x0f	// Display on + display cursor + blinking on.
	};

	uint8_t i, cmdIndx, dlyIndx; //, tmp;

	cmdIndx = 0;
	dlyIndx = 0;

	i2c_trn->address = IO_EXPANDER_ADDR;
	i2c_trn->numBytes = 1;
	i2c_trn->buf = buf;
	i2c_trn->transactType = I2C_T_TX_WAIT;
	i2c_trn->callbackFn = NULL;

	buf[0] = IO_EXP_IO_REG;
	usi_i2c_txrx_start(i2c_trn);
	usi_i2c_sleep_wait(1);

	// Initialization where interface is still in 8-bit mode...
	for (i = 4; i > 0; i--)
	{
		i2c_trn->buf = buf;
		lcd_write_int(cmds[cmdIndx++], 0, 0, buf);
		i2c_trn->numBytes = 2;
		usi_i2c_txrx_resume();
		usi_i2c_sleep_wait(1);
		delay_us(delays[dlyIndx++]);
	}

	// Initialization where interface is now in 4-bit mode...
	for (i = 6; i > 0; i--)
	{
		i2c_trn->buf = buf;
		if (i == 0)
			i2c_trn->transactType = I2C_T_TX_STOP;
		lcd_write_int(cmds[cmdIndx++], 1, 0, buf);
		i2c_trn->numBytes = 4;
		usi_i2c_txrx_resume();
		usi_i2c_sleep_wait(1);
		delay_us(delays[dlyIndx++]);
	}

	// Clean up.
	i2c_trn->transactType = I2C_T_IDLE;
}

/*
Fills a buffer to transfer a byte to the lcd through the port expander.
Sending a byte via the IO expander causes a write amplification of 4x [for HD44780 4bit mode] on the I2C bus.
It's because:
	1. Send 4msbs + E-line high.
	2. Send 4msbs + E-line low.
	3. Send 4lsbs + E-line high.
	4. Send 4lsbs + E-line low.
The actual send is done by the caller.
Assumes that all transfers are based on a 4-bit data interface.
Inputs:
- val:	the value to be written on the data bus.
- mode:	either byte [0] or nibble [1] transfer mode.
- rs:	state of the register select line into the hd44780. [0 = command, 1 = data/char].
Mode refers to byte or nibble transfer mode [0 = nibble].
Assumptions:
- There is an active i2c session to the device.
- The i2c session will be terminated elsewhere.
*/
int lcd_write_int(uint8_t val, uint8_t nibbleMode, uint8_t rs, volatile uint8_t *buf)
{
	const uint8_t mask =	(1 << BACKLIGHT_PORT) |
							(1 << DB7_PORT) | (1 << DB6_PORT) | (1 << DB5_PORT) | (1 << DB4_PORT) |
							(1 << RS_PORT);	//0xfa

	uint8_t temp;
	uint8_t backlightState = lcd_info.states & LCD_BACKLIGHT_STATE;

	rs = (rs > 0) ? 1 : 0;

	//A "byte" transfer is really just the upper nibble.
	temp = (((val & 0xf0) >> (7 - DB7_PORT)) | (rs << RS_PORT) | (backlightState << BACKLIGHT_PORT)) & mask;	//upper nibble
	buf[0] = temp | (1 << E_PORT);
	buf[1] = temp;

	if (nibbleMode)
	{
		temp = (((val & 0x0f) << DB4_PORT) | (rs << RS_PORT) | (backlightState << BACKLIGHT_PORT)) & mask;	//lower nibble
		buf[2] = temp | (1 << E_PORT);
		buf[3] = temp;
	}
	return 1;		// The caller handles the rest.
}

/*
Turn the backlight pin on or off.
0 = off, 1 = on.
*/
int lcd_set_backlight_int(int state, i2c_transaction_t *i2c_trans)
{
	if (usi_i2c_busy())
		return 0;									// I2C is busy.  <-- Probably don't need to check this as the caller(s) should have already checked.

	lcd_info.states = (lcd_info.states & ~LCD_BACKLIGHT_STATE) | ((0 == state) ? 0 : LCD_BACKLIGHT_STATE);
	i2c_trans->address = IO_EXPANDER_ADDR;
	i2c_trans->buf[0] = IO_EXP_IO_REG;
	i2c_trans->buf[1] = (lcd_info.states & LCD_BACKLIGHT_STATE) << (BACKLIGHT_PORT - LCD_BACKLIGHT_STATE - 1);
	i2c_trans->numBytes = 2;
	i2c_trans->transactType = I2C_T_TX_STOP;
	return 1;										// The caller will handle the rest.
}

static int send_lcd_cmd_int(uint8_t val, i2c_transaction_t *i2c_trans)
{
	if (usi_i2c_busy())
		return 0;									// I2C is busy.

	i2c_trans->address = IO_EXPANDER_ADDR;
	i2c_trans->buf[0] = IO_EXP_IO_REG;
	i2c_trans->buf[1] = val;
	lcd_write_int(val, 1, 0, &(i2c_trans->buf[1]));
	i2c_trans->numBytes = 5;
	i2c_trans->transactType = I2C_T_TX_STOP;
	return 1;										// The caller will handle the rest.
}

int lcd_clear_int(i2c_transaction_t *i2c_trans)
{
	return send_lcd_cmd_int(0x01, i2c_trans);
	// Need to figure out how to set delay.
}

int lcd_home_int(i2c_transaction_t *i2c_trans)
{
	return send_lcd_cmd_int(0x02, i2c_trans);
	// Need to figure out how to set delay; maybe main thread does this?
}

int lcd_blink_cursor_int(i2c_transaction_t *i2c_trans)
{
	//return send_lcd_cmd_int(0x09, i2c_trans);
	if (lcd_info.flags & LCD_CURSOR_BLINK)
		return 1;

	if (send_lcd_cmd_int(0x09, i2c_trans))
	{
		lcd_info.flags |= LCD_CURSOR_BLINK;
		return 1;
	}
	return 0;
}

int lcd_show_cursor_int(i2c_transaction_t *i2c_trans)
{
	//return send_lcd_cmd_int(0x0a, i2c_trans);
	if (lcd_info.flags & LCD_CURSOR_SHOW)
		return 1;

	if (send_lcd_cmd_int(0x0a, i2c_trans))
	{
		lcd_info.flags |= LCD_CURSOR_SHOW;
		return 1;
	}
	return 0;
}

int lcd_display_off_int(i2c_transaction_t *i2c_trans)
{
	return send_lcd_cmd_int(0x0c, i2c_trans);
}

int lcd_get_backlight_state(void)
{
	return ((lcd_info.states & LCD_BACKLIGHT_STATE) != 0);
}
