/*
 * msp430_usi_i2c_int.c
 *
 *  Created on: 12 Feb, 2014
 *      Author: Dale Hewgill
 *
 *  This is an interrupt driven implementation of an I2C master for the MSP430 USI module!
 */

#include "msp430_usi_i2c_int.h"

// #########################
// Defines and type definitions


// #########################
// Function Prototypes
//static inline void wait_i2c_transaction(void);
//static inline void usi_i2c_transact_end(uint8_t uStop);
static inline void start(void);
static inline void tx_byte(uint8_t data);
static inline void rx_byte(void);
static inline uint8_t get_rx_byte(void);
static inline void prep_stop(int stop);
static inline void stop(void);
static inline void prep_ack_nack(void);
static inline int get_ack_nack(void);
static inline void send_ack_nack(int nack);

// #########################
// Global Variables
//uint8_t gSysSleepMode;
//static volatile uint8_t				usi_i2c_error_state;
static volatile usi_i2c_sys_info_t	usi_i2c_sys_info;
static volatile i2c_transaction_t	*i2c_transact;

// #########################
// Function Definitions

//Inline primitives.
static inline void load_count(uint8_t bit_count)
{
	//USICNT = (0xe0 & USICNT) | theCount;
	//USICNT = theCount;
	USICNT |= bit_count;
}

static inline void start(void)
{
	USISRL = 0;										// msb = 0 in shift register.
	USICTL0 |= USIGE | USIOE;						// Latch/SDA output enabled.
	USICTL0 &= ~USIGE;								// Latch disabled.
}

static inline void tx_byte(uint8_t data)
{
	USICTL0 |= USIOE;								// SDA output.
	USISRL = data;									// Data into shift register.
	load_count(8);									// Shift out 8 bits.
}

static inline void rx_byte(void)
{
	USICTL0 &= ~USIOE;								// SDA input.
	load_count(8);									// Shift in 8 bits.
}
static inline uint8_t get_rx_byte(void)
{
	return USISRL;
}

static inline void prep_stop(int stop)
{
	USISRL = (stop == 0) ? 0xff : 0x7f;				// msb = 0 for stop or 1 for restart.
	USICTL0 |= USIOE;								// SDA = output.
	load_count(1);									// Shift out one bit.
}
static inline void stop(void)
{
	// Because of the previous shift of the data register the MSB now =1.
	USICTL0 |= USIGE;								// Output latch transparent.
	USICTL0 &= ~(USIOE | USIGE);					// Latch/SDA output disabled. => SDA pulled high.
}

static inline void prep_ack_nack(void)
{
	// Request N/ACK
	USICTL0 &= ~USIOE;								// SDA input.
	load_count(1);									// Get 1 bit.
}

static inline int get_ack_nack(void)
{
	return ((USISRL & 0x01) != 0);
}

static inline void send_ack_nack(int nack)
{
	// Now generate N/ACK.
	USICTL0 |= USIOE;								// SDA output.
	USISRL = (nack == 0) ? 0 : 0xff;				// Load shift register with ack or nack.
	load_count(1);									// Shift out 1 bit.
}

static inline void set_error(enum_usi_i2c_errors_t theError)
{
	usi_i2c_sys_info.error = (enum_usi_i2c_errors_t)((usi_i2c_sys_info.error & 0xf0) | theError);
}
// End inline primitives.

// Common provided functions
void usi_i2c_master_init(uint8_t usiClkSrc, uint8_t usiClkDiv)
{
	//P1SEL |= SDA_PIN | SCL_PIN;
	USICTL0 = USIPE7 | USIPE6 | USIMST | USISWRST;	// USI in master mode.  In reset.
	USICTL1 = USII2C;								// USI in i2c mode.
	//USICKCTL = USIDIV_5 | USISSEL_2 | USICKPL;		// Clock = SMCLK/32 [assumes 16MHz SMCLK].  Gives 500kHz SCL.
	USICKCTL = usiClkDiv | usiClkSrc | USICKPL;
	//USICNT |= USIIFGCC;								// Disable automatic clear control
	USICTL0 &= ~USISWRST;							// USI out of reset.
	//USICTL1 &= ~USIIFG;								// Clear pending interrupts.
	//usi_i2c_error_state = 0;
	usi_i2c_sys_info.error = USI_I2C_ERR_NONE;
}

int usi_i2c_busy(void)
{
	return ( (usi_i2c_sys_info.flags & USI_BUSY) != 0 );
}

int usi_i2c_get(void)
{
	usi_i2c_sys_info.flags |= USI_BUSY;			// Assume that the caller has already checked busy.  Otherwise there will be two consecutive calls to _check_busy() -> wasteful and unnecessary.
	return 1;
}

void usi_i2c_release(void)
{
	usi_i2c_sys_info.flags &= ~USI_BUSY;
}

void usi_i2c_raise_event(void)
{
	usi_i2c_sys_info.flags |= USI_I2C_EVENT_SIG;
}

void usi_i2c_clear_event(void)
{
	usi_i2c_sys_info.flags &= ~USI_I2C_EVENT_SIG;
}

int usi_i2c_check_event(void)
{
	return ( (usi_i2c_sys_info.flags & USI_I2C_EVENT_SIG) != 0 );
}

enum_usi_i2c_errors_t usi_i2c_get_error(void)
{
	return (usi_i2c_sys_info.error & USI_I2C_ERR_MASK);
}

// For interaction with the interrupt driver.
void usi_i2c_sleep_wait(uint8_t clear_flag)
{
	do
	{
		__bis_SR_register(gSysSleepMode | GIE);	// Sleep with interrupts enabled; wait for I2C transaction to end.
	}
	while (!usi_i2c_check_event());				// Make sure that we only continue if it's I2C that's woken us up.
	if (clear_flag)
		usi_i2c_clear_event();					// Clear the I2C event flag.
}

void usi_i2c_txrx_resume(void)
{
	USICTL1 |= USIIE;
}

void usi_i2c_txrx_stop(i2c_transaction_t *psI2cTransact)
{
	//psI2cTransact->state = I2C_S_PREP_STOP;
	psI2cTransact->numBytes = 0;
	USICTL1 |= USIIE;
}

int usi_i2c_txrx_start(i2c_transaction_t *psI2cTransact)
{
	if (psI2cTransact != NULL)
	{
		i2c_transact = psI2cTransact;
		//i2c_transact->state = I2C_S_START;
		USICTL1 |= USIIE;
		return 0;
	}
	return 1;
}

/* ***************************************
 * Implements a i2c random read.
 * Implies: TX addr and starting reg, restart, then get registers.
 * The caller must be a proper i2c_transaction_t function and must:
 * 		- take the USI.
 * 		- set numBytes in the i2c transaction structure = all bytes tx'd + rx'd [except address].
 * ***************************************/
/*void* usi_i2c_random_read(i2c_transaction_t *pI2cTrans, void *userdata)
{
	static uint8_t state = 0;
	static uint8_t bytesToRx = 0;
	static i2c_callback_fnptr_t myCallback = NULL;

	switch (state)
	{
	case 0:		// Send Address and register to address.
		myCallback = (userdata == NULL) ? NULL : (i2c_callback_fnptr_t)userdata;
		//myCallback = pI2cTrans->callbackFn;
		pI2cTrans->transactType = I2C_T_TX_RESTART;
		bytesToRx = pI2cTrans->numBytes - 1;
		pI2cTrans->numBytes = 1;
		pI2cTrans->callbackFn = usi_i2c_random_read;
		usi_i2c_txrx_start(pI2cTrans);

		state = 1;
		break;

	case 1:		// Now get the data.
		pI2cTrans->numBytes = bytesToRx;
		pI2cTrans->address |= I2C_READ_BIT;
		pI2cTrans->transactType = I2C_T_RX_STOP;
		// Should somehow reset the buffer pointer.
		usi_i2c_txrx_resume();

		state = 2;
		break;

	case 2:		// Send control back to caller.
		if (myCallback != NULL)
		{
			pI2cTrans->callbackFn = (i2c_callback_fnptr_t)myCallback;
			usi_i2c_raise_event();
		}
		else
		{
			// Clean up?  There should always be a valid 'myCallback' to this function.
		}

		myCallback = NULL;
		state = 0;
		break;

	default:
		break;
	}

	return NULL;
}*/


/******************************************************
// USI interrupt service routine
// Data Transmit : state
// Data Receive  : state
******************************************************/
#ifndef INTRPT_STYLE_2
#pragma vector=USI_VECTOR
__interrupt void USI_TXRX(void)
{
	static enum_i2c_state_t state = I2C_S_START;		// <-- May need to promote this to a static global because stuff may need to manipulate it.
	int wake = 0;

	switch(__even_in_range((state), I2C_S_STOP))
	{
	case I2C_S_START:
		start();
		tx_byte(i2c_transact->address);
		i2c_transact->address &= ~I2C_READ_BIT;			// Clear the read bit.  It's so we can get the ack for address transmit.  We'll set it back after based on the transaction type.
		state = I2C_S_PREP_ACK_NACK;
		break;

	case I2C_S_TX_BYTE:
		tx_byte(*i2c_transact->buf);
		if (i2c_transact->numBytes)						// Make sure that numBytes > 0.
		{
			i2c_transact->numBytes--;
			i2c_transact->buf++;						// Advance buffer index.
		}
		state = I2C_S_PREP_ACK_NACK;
		break;

	case I2C_S_RX_BYTE:
		rx_byte();
		i2c_transact->numBytes--;
		state = I2C_S_ACK_NACK;
		break;

	case I2C_S_PREP_ACK_NACK:
		prep_ack_nack();
		state = I2C_S_ACK_NACK;
		break;

	case I2C_S_ACK_NACK:
	{
		//Might have to check the I/O direction instead of address to figure out if the last transaction was a tx or rx.
		//This is because even in a rx transaction the address must be transmitted first and then we need to get n/ack.
		if (i2c_transact->address & 0x01)				// RX -> get data and send N/ACK
		{
			int ackNack = 0;
			*i2c_transact->buf = get_rx_byte();
			if (0 < i2c_transact->numBytes)
			{
				state = I2C_S_RX_BYTE;
				i2c_transact->buf++;
			}
			else
			{
				if (i2c_transact->transactType == I2C_T_RX_WAIT)
				{
					state = I2C_S_RX_BYTE;				// Assume that whatever's holding the i2c will need to get more.
					usi_i2c_sys_info.flags |= USI_I2C_EVENT_SIG;
					USICTL1 &= ~USIIE;					// Turn off the interrupt; main thread will need to turn it back on to resume.
					wake = 1;							// The problem here is that we might wake up before finishing the ack.  Especially true for faster processors.
				}										// Might be ok since the resume turns the interrupt back on and that will only go once the USI is ready.
				else
				{
					ackNack = 0xff;
					state = I2C_S_PREP_STOP;
				}
			}
			send_ack_nack(ackNack);
		}
		else											//TX -> get N/ACK
		{
			int rxFlag = ((i2c_transact->transactType & 0x01) == 0);	// All of the RX transaction types have even enumerations. [idle case ignored]
			if (rxFlag)													// This piece should only come into play after the address transmit
				i2c_transact->address |= I2C_READ_BIT;					// because the address transmit clears the i2c read bit so we re-set it here.

			/*int rxFlag = 0;
			if ( (i2c_transact->transactType > I2C_T_IDLE) && ((i2c_transact->transactType & 0x01) == 0) )// All of the RX transaction types have even enumerations. [Probably don't have to do idle check].
			{																						// This piece should only come into play after the address transmit
				i2c_transact->address |= I2C_READ_BIT;												// because the address transmit clears the i2c read bit so we re-set it here.
				rxFlag = 1;
			}*/

			if (get_ack_nack())
			{
				set_error(USI_I2C_ERR_NO_ACK_ON_DATA);
				state = I2C_S_PREP_STOP;
			}
			else if (0 < i2c_transact->numBytes)
			{
				state = ((rxFlag == 0) ? I2C_S_TX_BYTE : I2C_S_RX_BYTE);
				//state = ( ((i2c_transact->addr & I2C_READ_BIT) == 0) ? I2C_S_TX_BYTE : I2C_S_RX_BYTE );
			}
			else if (i2c_transact->transactType == I2C_T_TX_WAIT)
			{
				state = I2C_S_TX_BYTE;									// Assume that whatever's holding the i2c will need to send more.
				usi_i2c_sys_info.flags |= USI_I2C_EVENT_SIG;
				USICTL1 &= ~USIIE;										// Turn off the interrupt; main thread will need to turn it back on to resume.
				wake = 1;
			}
			else
				state = I2C_S_PREP_STOP;
		}
		break;
	}

	case I2C_S_PREP_STOP:
	{
		int uStop = 0;
		if (i2c_transact->transactType < I2C_T_TX_RESTART)
		{
			uStop = 1;
			state = I2C_S_STOP;
		}
		else
		{
			// It's a repeated start type transaction.
			//uStop = 0;
			state = I2C_S_START;
			usi_i2c_sys_info.flags |= USI_I2C_EVENT_SIG;
			USICTL1 &= ~USIIE;							// Turn off the interrupt; main thread will need to turn it back on to resume.
			wake = 1;
		}
		prep_stop(uStop);
		break;
	}

	case I2C_S_STOP:
		// Because of the previous shift of the data register [PREP_STOP] the MSB now =1.
		stop();
		usi_i2c_sys_info.flags |= USI_I2C_EVENT_SIG;	// Signal that that there's an I2C event ready to handle.
		state = I2C_S_START;
		// The reason that we don't set the states to idle here is because we're not sure where we'll end up when we step back to main().
		// We may end up somewhere where the next pending event will try to grab the USI and see that it's free and then clobber the data
		// we just got from, for instance, a read transaction.  Probably not a big deal for writes.
		//i2c_transact->transactType = I2C_T_IDLE;
		USICTL1 &= ~USIIE;								// Turn off the USI interrupt.
		wake = 1;										// Wake from interrupt.
		break;

	/*default:
		//_never_executed();
		// Should never get here; recover?
		break;*/
	}

	if (wake)
		__bic_SR_register_on_exit(gSysSleepMode);		// Wake up.
}
#else
// Attempt to simplify the interrupt logic by adding extra states for the address transmit.
#pragma vector=USI_VECTOR
__interrupt void USI_TXRX(void)
{
	static enum_i2c_state_t state = I2C_S_START;		// <-- May need to promote this to a static global because stuff may need to manipulate it.
	int wake = 0;

	switch(__even_in_range((state), I2C_S_STOP))
	{
	case I2C_S_START:
		start();
		tx_byte(i2c_transact->address);
		state = I2C_S_PREP_ACK_NACK_ADDR;
		break;

	case I2C_S_TX_BYTE:
		tx_byte(*i2c_transact->buf);
		if (i2c_transact->numBytes)						// Make sure that numBytes > 0.
		{
			i2c_transact->numBytes--;
			i2c_transact->buf++;						// Advance buffer index.
		}
		state = I2C_S_PREP_ACK_NACK;
		break;

	case I2C_S_RX_BYTE:
		rx_byte();
		i2c_transact->numBytes--;
		state = I2C_S_ACK_NACK;
		break;

	case I2C_S_PREP_ACK_NACK_ADDR:
		// Fall through...
	case I2C_S_PREP_ACK_NACK:
		prep_ack_nack();
		state = (state == I2C_S_PREP_ACK_NACK_ADDR) ? I2C_S_ACK_NACK_ADDR : I2C_S_ACK_NACK;
		break;

	case I2C_S_ACK_NACK_ADDR:
		// Fall through...
	case I2C_S_ACK_NACK:
		if ( ((i2c_transact->address & I2C_READ_BIT) == 0) || (state == I2C_S_ACK_NACK_ADDR) )	//TX -> get N/ACK
		{
			if (get_ack_nack())
			{
				(I2C_S_ACK_NACK_ADDR == state) ? set_error(USI_I2C_ERR_NO_ACK_ON_ADDRESS) : set_error(USI_I2C_ERR_NO_ACK_ON_DATA);
				state = I2C_S_PREP_STOP;
			}
			else if (i2c_transact->numBytes > 0)
			{
				state = (i2c_transact->address & I2C_READ_BIT) ? I2C_S_RX_BYTE : I2C_S_TX_BYTE;
			}
			else if (i2c_transact->transactType == I2C_T_TX_WAIT)
			{
				state = I2C_S_TX_BYTE;									// Assume that whatever's holding the i2c will need to send more.
				usi_i2c_sys_info.flags |= USI_I2C_EVENT_SIG;
				USICTL1 &= ~USIIE;										// Turn off the interrupt; main thread will need to turn it back on to resume.
				wake = 1;
			}
			else
				state = I2C_S_PREP_STOP;
		}
		else											// RX -> get data and send N/ACK
		{
			int ack_nack = 0;
			*i2c_transact->buf = get_rx_byte();
			if (i2c_transact->numBytes > 0)
			{
				state = I2C_S_RX_BYTE;
				i2c_transact->buf++;
			}
			else
			{
				if (i2c_transact->transactType == I2C_T_RX_WAIT)
				{
					state = I2C_S_RX_BYTE;				// Assume that whatever's holding the i2c will need to get more.
					usi_i2c_sys_info.flags |= USI_I2C_EVENT_SIG;
					USICTL1 &= ~USIIE;					// Turn off the interrupt; main thread will need to turn it back on to resume.
					wake = 1;							// The problem here is that we might wake up before finishing the ack.  Especially true for faster processors.
				}										// Might be ok since the resume turns the interrupt back on and that will only go once the USI is ready.
				else
				{
					ack_nack = 0xff;
					state = I2C_S_PREP_STOP;
				}
			}
			send_ack_nack(ack_nack);
		}
		break;

	case I2C_S_PREP_STOP:
	{
		int stop = 0;
		if (i2c_transact->transactType < I2C_T_TX_RESTART)
		{
			stop = 1;
			state = I2C_S_STOP;
		}
		else
		{
			// It's a repeated start type transaction.
			state = I2C_S_START;
			usi_i2c_sys_info.flags |= USI_I2C_EVENT_SIG;
			USICTL1 &= ~USIIE;							// Turn off the interrupt; main thread will need to turn it back on to resume.
			wake = 1;
		}
		prep_stop(stop);
		break;
	}

	case I2C_S_STOP:
		// Because of the previous shift of the data register [PREP_STOP] the MSB now =1.
		stop();
		usi_i2c_sys_info.flags |= USI_I2C_EVENT_SIG;	// Signal that that there's an I2C event ready to handle.
		state = I2C_S_START;
		// The reason that we don't set the states to idle here is because we're not sure where we'll end up when we step back to main().
		// We may end up somewhere where the next pending event will try to grab the USI and see that it's free and then clobber the data
		// we just got from, for instance, a read transaction.  Probably not a big deal for writes.
		//i2c_transact->transactType = I2C_T_IDLE;
		USICTL1 &= ~USIIE;								// Turn off the USI interrupt.
		wake = 1;										// Wake from interrupt.
		break;

	/*default:
		//_never_executed();
		// Should never get here; recover?
		break;*/
	}

	if (wake)
		__bic_SR_register_on_exit(gSysSleepMode);		// Wake up.
}
#endif
