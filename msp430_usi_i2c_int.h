/*
 * msp430_usi_i2c_int.h
 *
 *  Created on: 12 Feb, 2014
 *      Author: Dale Hewgill
 */

#ifndef MSP430_USI_I2C_INT_H_
#define MSP430_USI_I2C_INT_H_

#include <msp430.h>
#include <stdint.h>
#include <stddef.h>


#define INTRPT_STYLE_2
//For MSP430G2452
#define I2C_PORT_SEL		P1SEL
#define I2C_PORT_SEL2		P1SEL2
#define SCL_PIN				BIT6
#define SDA_PIN				BIT7
#define I2C_READ_BIT		0x01
//#define I2C_SYSEV_FLAG		0x80
//#define I2C_USI_BUSY_FLAG	0x40

#define LCD_MASK		0xf8				// Should try to source these from the lcd display driver somehow.
//#define LCD_MASK		0xfa				// <-- original value
#define E_MASK			0x04
#define RS_MASK			0x02
#define BL_MASK			0x80				// Backlight mask.
#define UPPR_NBL_SHFT	1
#define LWR_NBL_SHFT	3

//Definitions for error codes.
#define USI_I2C_NO_ACK_ON_ADDRESS	0x01	// The slave did not acknowledge  the address
#define USI_I2C_NO_ACK_ON_DATA		0x02	// The slave did not acknowledge  all data
#define USI_I2C_MISSING_START_CON	0x03	// Generated Start Condition not detected on bus
#define USI_I2C_MISSING_STOP_CON	0x04	// Generated Stop Condition not detected on bus
#define USI_I2C_UE_DATA_COL			0x05	// Unexpected Data Collision (arbitration)
#define USI_I2C_UE_STOP_CON			0x06	// Unexpected Stop Condition
#define USI_I2C_UE_START_CON		0x07	// Unexpected Start Condition
#define USI_I2C_NO_DATA				0x08	// Transmission buffer is empty
#define USI_I2C_DATA_OUT_OF_BOUND	0x09	// Transmission buffer is outside SRAM space
#define USI_I2C_BAD_MEM_READ		0x0A	// Error during external memory read

#define USI_I2C_EVENT_SIG			0x80
#define USI_BUSY					0x40

//Type definitions
typedef enum
{
	USI_I2C_ERR_NONE				= 0x00,
	USI_I2C_ERR_NO_ACK_ON_ADDRESS	= 0x01,	// The slave did not acknowledge  the address
	USI_I2C_ERR_NO_ACK_ON_DATA		= 0x02,	// The slave did not acknowledge  all data
	USI_I2C_ERR_MISSING_START_CON	= 0x03,	// Generated Start Condition not detected on bus
	USI_I2C_ERR_MISSING_STOP_CON	= 0x04,	// Generated Stop Condition not detected on bus
	USI_I2C_ERR_UE_DATA_COL			= 0x05,	// Unexpected Data Collision (arbitration)
	USI_I2C_ERR_UE_STOP_CON			= 0x06,	// Unexpected Stop Condition
	USI_I2C_ERR_UE_START_CON		= 0x07,	// Unexpected Start Condition
	USI_I2C_ERR_NO_DATA				= 0x08,	// Transmission buffer is empty
	USI_I2C_ERR_DATA_OUT_OF_BOUND	= 0x09,	// Transmission buffer is outside SRAM space
	USI_I2C_ERR_BAD_MEM_READ		= 0x0a,	// Error during external memory read
	USI_I2C_ERR_MASK				= 0x0f	// Mask to retrieve the errors from the union.
} enum_usi_i2c_errors_t;

typedef enum
{
	I2C_T_IDLE				= 0,
	I2C_T_TX_STOP			= 1,
	I2C_T_RX_STOP			= 2,
	I2C_T_TX_WAIT			= 3,	// Transmit n bytes then hand control to main thread without issuing stop.
	I2C_T_RX_WAIT			= 4,	// Receive n bytes then hand control to main thread without issuing stop.
	I2C_T_TX_RESTART		= 5,
	I2C_T_RX_RESTART		= 6
	//I2C_T_RX_RNDM			= 8		// Random read; implies tx_restart followed by rx_stop.
} enum_i2c_transact_type_t;

#ifndef INTRPT_STYLE_2
typedef enum
{
	I2C_S_START				= 0,
	I2C_S_TX_BYTE			= 2,
	I2C_S_RX_BYTE			= 4,
	I2C_S_PREP_ACK_NACK		= 6,
	I2C_S_ACK_NACK			= 8,
	I2C_S_PREP_STOP			= 10,
	I2C_S_STOP				= 12
} enum_i2c_state_t;
#else
typedef enum
{
	I2C_S_START					= 0,
	I2C_S_TX_BYTE				= 2,
	I2C_S_RX_BYTE				= 4,
	I2C_S_PREP_ACK_NACK_ADDR	= 6,
	I2C_S_PREP_ACK_NACK			= 8,
	I2C_S_ACK_NACK_ADDR			= 10,
	I2C_S_ACK_NACK				= 12,
	I2C_S_PREP_STOP				= 14,
	I2C_S_STOP					= 16
} enum_i2c_state_t;
#endif

typedef struct _i2c_transaction_t i2c_transaction_t;

typedef void* (*i2c_callback_fnptr_t)( i2c_transaction_t *, void * );

struct _i2c_transaction_t
{
	volatile uint8_t					address;
	volatile uint8_t					numBytes;			// uint8_t is ok as long as numBytes < 63 for LCD transfers.
	volatile enum_i2c_transact_type_t	transactType;
	i2c_callback_fnptr_t				callbackFn;
	volatile uint8_t					*buf;
};

typedef union _usi_i2c_sys_info_t
{
	enum_usi_i2c_errors_t	error;
	uint8_t					flags;
} usi_i2c_sys_info_t;


//Globals
//volatile i2c_transaction_t * i2c_transact;
extern const uint16_t gSysSleepMode;

// Functions Provided
void usi_i2c_master_init(uint8_t usiClkSrc, uint8_t usiClkDiv);
int usi_i2c_busy(void);
int usi_i2c_get(void);
void usi_i2c_release(void);
void usi_i2c_raise_event(void);
void usi_i2c_clear_event(void);
int usi_i2c_check_event(void);
enum_usi_i2c_errors_t usi_i2c_get_error(void);

void usi_i2c_sleep_wait(uint8_t clear_flag);
void usi_i2c_txrx_resume(void);
void usi_i2c_txrx_stop(i2c_transaction_t *psI2cTransact);
int usi_i2c_txrx_start(i2c_transaction_t *psI2cTransact);
//void* usi_i2c_random_read(i2c_transaction_t *pI2cTrans, void *userdata);

#endif /* MSP430_USI_I2C_INT_H_ */
