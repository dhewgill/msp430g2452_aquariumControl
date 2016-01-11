#include <msp430.h> 
#include <stdint.h>
#include <stdlib.h>
#include <string.h>						// For memset(), strcpy()
#include "msp430_usi_i2c_int.h"
#include "lcd.h"
#include "ds3231m_lib.h"
#include "ui_update.h"
#include "datetime.h"

// #########################
// Defines and type definitions
#define SLEEP_MODE				LPM0_bits

#define SYS_BUF_SZ				24
#define TMP_BUF_SZ				4
#define ASCII_ZERO				0x30
#define ASCII_SPACE				0x20

#define HS_SYSTICK_SPD			1000u	// For 1ms high speed system tick; 100us otherwise.

#if HS_SYSTICK_SPD == 1000
#define HS_SYSTICK_TIMER_VAL	15999u	// Allows for ~1ms high speed system tick from TimerA0 based on 16MHz SMCLK.
#define SYNC_EVENT_COUNTER		250u	// Allows for a ~0.25s synchronous event.
#define BTN_DBNC				30u		// Set a common mechanical button debounce time of 30ms.
#define ASYNC_BTN_DBNCE_TMR		BTN_DBNC// ~30ms for async event button debounce.
#define RENC_BTN_DBNCE_TMR		BTN_DBNC// ~30ms for rotary encoder pushbutton debounce.
#define RENC_BTN_LNG_PRS		3000u	// 3s long press count for rotary encoder button.

#else
#define HS_SYSTICK_TIMER_VAL	1599u	// Allows for ~100us high speed system tick from TimerA0 based on 16MHz SMCLK.
#define SYNC_EVENT_COUNTER		2500u	// Allows for a ~0.25s synchronous event.
#define BTN_DBNC				300u	// Set a common mechanical button debounce time of 30ms.
#define ASYNC_BTN_DBNCE_TMR		BTN_DBNC// ~30ms for async event button debounce.
#define RENC_BTN_DBNCE_TMR		BTN_DBNC// ~30ms for rotary encoder pushbutton debounce.
#define RENC_BTN_LNG_PRS		30000u	// 3s long press count for rotary encoder button.

#endif

#define	LCD_BL_BTN				BIT3	// Port 1.3 -> mechanical switch on LP; toggles lcd backlight.  Requires debounce.
#define RENC_BTN				BIT1	// Port 1.1 -> mechanical pushbutton switch on rotary encoder.  Requires debounce.

// RTC 1-sec input; P1.5
#define RTC_INT_PIN				BIT5	// Port 1.5.  Digital input from RTC, no debounce required.

// Port 2 #defines
#define	RENC_SIGA				BIT0	// Rotary encoder signalA on P2.0 .  Does not require debounce; filtered with 1uF cap and pin pullup resistor.
#define RENC_SIGB				BIT1	// Rotary encoder signalB on P2.1 .  Does not require debounce; filtered with 1uF cap and pin pullup resistor.
//#define RENC_DBNCE_TMR			2u		// Timer for rotary encoder contact debounce.  ***Not used!***

#define DBG_LED					BIT0	// Port 1.0

// System flag definitions
#define SYSFLG_LCD_BTN_DBNCE	0x0001u	// One of the buttons is debouncing.
#define SYSFLG_ASYNCSYSEVENT	0x0002u	// An asynchronous system event.
#define SYSFLG_SYNCSYSEVENT		0x0004u	// A synchronous system event.
#define SYSFLG_RENC_ROT_EVENT	0x0008u	// The rotary encoder has detected a rotation [detent to detent].
#define SYSFLG_RENC_DIR			0x0010u	// Rotary encoder direction flag; 0 = ccw, 1 = cw.  CW is positive.
#define SYSFLG_RENC_BTN_DBNCE	0x0020u	// Rotary encoder button debouncing.
#define SYSFLG_RENC_BTN_DN		0x0040u	// Rotary encoder button down [precursor to detecting short and long press].
#define SYSFLG_RENC_BTN_SHRT	0x0080u	// Rotary encoder short button press.
#define SYSFLG_RENC_BTN_LNG		0x0100u	// Rotary encoder long button press.
#define SYSFLG_ONESEC_EVENT		0x0200u	// One second pulse received from RTC.
#define SYSFLG_LCD_BACKLIGHT	0x0400u	// Signals an LCD backlight change request to the system.
#define SYSFLG_FETCH_DATETIME	0x0800u	// Time to fetch the time from the RTC.
#define SYSFLG_DISP_DATETIME	0x1000u	// Update the date and time displayed on the lcd.
#define SYSFLG_SET_RTC_DATETIME	0x2000u	// Write a new time and date to the RTC.
#define SYSFLG_CONFIG_MODE		0x4000u	// Is the system in configuration mode?  Normal mode = 0.
#define SYSFLG_DRAW_NORM_SCRN	0x8000u	// Repaint the normal mode screen. <-- Maybe not necessary?

// For time and date
#define TIME_DELIMITER			':'
#define DATE_DELIMITER			'/'

typedef enum
{
	LCD_P_SM_SEND_START	= 0,
	LCD_P_SM_SEND_CURS	= 1,
	LCD_P_SM_SEND_CHAR	= 2,
	LCD_P_SM_END		= 3
} enum_lcd_print_sm_t;

typedef enum
{
	RTC_GET_TIME		= 0,
	RTC_DIS_DATE		= 1,
	RTC_DIS_TIME		= 2,
	RTC_DIS_END			= 3
} rtc_display_sm_t;

typedef enum
{
	DISP_NORM_START		= 0,
	DISP_NORM_LINE1		= 1,
	DISP_NORM_LINE2		= 2,
	DISP_NORM_END		= 3
} enum_display_norm_static_sm_t;

typedef enum
{
	UI_UPD_S_DAY	= 0,
	UI_UPD_S_DATE	= 1,
	UI_UPD_S_MONTH	= 2,
	UI_UPD_S_YEAR	= 3,
	UI_UPD_S_HOUR	= 4,
	UI_UPD_S_MIN	= 5,
	UI_UPD_S_SEC	= 6
} ui_dt_upd_sm_t;			// States for the update date/time state machine.

typedef struct state_vars
{
	enum_lcd_print_sm_t				lcd_print_state		:2;
	rtc_display_sm_t				rtc_display_state	:2;
	enum_display_norm_static_sm_t	disp_norm_state		:2;
	ui_dt_upd_sm_t					ui_dt_upd_state		:3;
	uint16_t						get_rtc_state		:1;
	uint16_t						unused				:6;
} state_vars_t;


// #########################
// Function Prototypes
static inline void init_timera0(void);
static inline void init_port1(void);
static inline void init_port2(void);
static inline void init_led(void);
static inline void init_lcd_int(void);
static inline void init_i2c_struct(void);
static inline int sysIsIdle(void);
static int set_lcd_backlight(uint8_t state, i2c_transaction_t *i2c_trn);
static inline int wait_for_usi_finish(i2c_transaction_t *i2c_trn);
static void* drawNrmModeStaticData(i2c_transaction_t *pI2cTrans, void *userdata);
static inline void asyncEventSM(i2c_transaction_t *pI2cTrans);
static inline void syncEventSM(i2c_transaction_t *pI2cTrans);
static inline void* fetchRtcTime(i2c_transaction_t *pI2cTrans, void *userdata);
static inline void* setRtcTime(i2c_transaction_t* pI2cTrans, void* userData);
static inline void* displayRtcDataSM(i2c_transaction_t *pI2cTrans, void *userdata);
static inline void changeDateTimeUiSM(void);
static int put_byte_to_lcd(uint8_t byteToTx, uint8_t byteIsCmd, i2c_transaction_t *i2c_trn);
static void* putstr_to_lcd_int(i2c_transaction_t *i2c_trn, void *userdata);
static int prep_str_for_lcd(uint8_t *theStr, uint8_t pos, uint8_t sz);
uint8_t* itoa(int16_t value, uint8_t *result, uint8_t base);
uint8_t* utoa(uint16_t value, uint8_t *result, uint8_t base);
static uint8_t* print_u16(uint8_t *buf, uint16_t value, uint8_t width);
static inline void prep_time_disp_str(DateTime_t *dt, uint8_t *buf);
static inline void prep_date_disp_str(DateTime_t *dt, uint8_t *buf);

// #########################
// Global Variables
const uint8_t gAsyncDispStr[] 	= "Async: ";
const uint8_t gSyncDispStr[] 	= "Sync: ";
const uint8_t gLcdRowOffsets[]	= {0x00, 0x40, 0x14, 0x54}; // Seems to be true for my cheap 20x4 LCD's.
const uint16_t gSysSleepMode	= SLEEP_MODE;

const uint8_t deg00[]			= ".00";
const uint8_t deg25[]			= ".25";
const uint8_t deg50[]			= ".50";
const uint8_t deg75[]			= ".75";
const uint8_t* const gFracDeg[]	= {deg00, deg25, deg50, deg75};

//Days of the week:
const uint8_t day0[] = "Sun";
const uint8_t day1[] = "Mon";
const uint8_t day2[] = "Tue";
const uint8_t day3[] = "Wed";
const uint8_t day4[] = "Thu";
const uint8_t day5[] = "Fri";
const uint8_t day6[] = "Sat";
const uint8_t* const gDaysOfWeek[] = {day0, day1, day2, day3, day4, day5, day6};

//Months
const uint8_t month0[] = "Jan";
const uint8_t month1[] = "Feb";
const uint8_t month2[] = "Mar";
const uint8_t month3[] = "Apr";
const uint8_t month4[] = "May";
const uint8_t month5[] = "Jun";
const uint8_t month6[] = "Jul";
const uint8_t month7[] = "Aug";
const uint8_t month8[] = "Sep";
const uint8_t month9[] = "Oct";
const uint8_t month10[] = "Nov";
const uint8_t month11[] = "Dec";
const uint8_t* const gMonths[] = {	month0, month1, month2, month3,
									month4, month5, month6, month7,
									month8, month9, month10, month11 };

volatile uint16_t				gSysFlags;
//volatile uint8_t				gSysFlags;
volatile uint8_t				gAsyncBtnDebounceCounter;
volatile uint8_t				gRencBtnDebounceCounter;
volatile uint8_t				gSysBuf[SYS_BUF_SZ];
volatile uint8_t				gTmpBuf[TMP_BUF_SZ];
uint16_t						gAsyncCount;
uint16_t						gSyncCount;
//volatile uint8_t				gUiTimeoutTmr;
i2c_transaction_t				gsI2Ctransact;
DateTime_t						gDt;
//state_vars_t					gStateVars;


// #########################
// Function Definitions

// The _system_pre_init() function will run before the global variables are initialized.
// It's a good place to stop the watchdog and set the system freq, etc...
// Just don't do anything that requires global variables yet!
// The 'return 1' is necessary to init globals on function exit.
int _system_pre_init(void)
{
    WDTCTL = WDTPW | WDTHOLD;		// Stop watchdog timer

    DCOCTL = 0;
    BCSCTL1 = CALBC1_16MHZ;
    DCOCTL = CALDCO_16MHZ;			// Set clock to 16MHz
	BCSCTL3 |= XCAP_3;				// 12.5 pF for LaunchPad crystal
	//BCSCTL2 |= DIVS_3;				// SMCLK = MCLK/8

	return 1;
}
int main(void)
{
	const uint16_t sysFlagMask = (uint16_t)(SYSFLG_ONESEC_EVENT | SYSFLG_LCD_BACKLIGHT | SYSFLG_SYNCSYSEVENT | SYSFLG_ASYNCSYSEVENT);
	//const uint8_t sysFlagMask = (uint8_t)(SYSFLG_FETCH_DATETIME | SYSFLG_LCD_BACKLIGHT | SYSFLG_SYNCSYSEVENT | SYSFLG_ASYNCSYSEVENT);

	gSysFlags = (SYSFLG_ASYNCSYSEVENT | SYSFLG_SYNCSYSEVENT | SYSFLG_RENC_DIR);// Feed events to start off.

	init_i2c_struct();

	usi_i2c_master_init(USISSEL_2, USIDIV_5);				// USI Clock = SMCLK, Divider = 32; yields 500kHz I2C.
	//usi_i2c_master_init(USISSEL_2, USIDIV_7);				// USI Clock = SMCLK, Divider = 128; yields 125kHz I2C.
	init_port1();
	init_port2();
	init_led();
	init_timera0();
	ds3231m_init(&gsI2Ctransact, gSysBuf);
	//gsI2Ctransact.buf = gSysBuf;
	//ds3231m_set_time_dbg(NULL, &gsI2Ctransact);

	__delay_cycles(600000);									// Settle DCO a bit; if it's shorter, the lcd init doesn't seem to work.

	init_lcd_int();

	prep_str_for_lcd((uint8_t *)gSyncDispStr, 0x14, 0);
	putstr_to_lcd_int(&gsI2Ctransact, NULL);
	wait_for_usi_finish(&gsI2Ctransact);

	prep_str_for_lcd((uint8_t *)gAsyncDispStr, 0x54, 0);
	putstr_to_lcd_int(&gsI2Ctransact, NULL);
	wait_for_usi_finish(&gsI2Ctransact);


	P1IE = (RENC_BTN | LCD_BL_BTN | RTC_INT_PIN);			// Enable P1.1, P1.3, P1.5 interrupts.
	P2IE = (RENC_SIGB | RENC_SIGA);							// Enable P2.0, P2.1 interrupts.
	TA0CTL |= MC_2;											// Counter in continuous mode.
	TA0CCTL0 = CCIE;										// Enable TimerA0_0 compare interrupt.

    for (;;)
    {
    	P1OUT ^= DBG_LED;

    	if (usi_i2c_check_event())							// The USI has woken the system in response to some I2C action needing attention.
    	{
    		usi_i2c_clear_event();
    		if (NULL == gsI2Ctransact.callbackFn)			// Check if there's a callback to follow.
    		{
    			usi_i2c_release();							// Release the USI and LCD since no callback.
    			lcd_release();
    			gsI2Ctransact.transactType = I2C_T_IDLE;
    		}
    		else											// Call the callback function.
    		{
    			gsI2Ctransact.callbackFn(&gsI2Ctransact, NULL);
    		}
    	}

    	if (gSysFlags & SYSFLG_RENC_ROT_EVENT)				// A rotary encoder rotation event is raised.
    	{
    		if (gSysFlags & SYSFLG_CONFIG_MODE)				// If in UI update mode.
    		{
    			changeDateTimeUiSM();						// Call the UI update state machine.
    		}
    		else											// Only raise the asynchronous event if in normal mode.
    		{
    			((gSysFlags & SYSFLG_RENC_DIR) == 0) ? gAsyncCount-- : gAsyncCount++;
    			gSysFlags |= SYSFLG_ASYNCSYSEVENT;			// Raise the async event.
    			gSysFlags &= ~SYSFLG_RENC_ROT_EVENT;		// Consume the rotary encoder rotation event.
    		}
    	}

    	if (gSysFlags & SYSFLG_ONESEC_EVENT)				// A 1 second pulse has been received from the RTC.
    	{
    		gSysFlags &= ~SYSFLG_ONESEC_EVENT;				// Consume the 1 second event.
    		if ( !(gSysFlags & (SYSFLG_CONFIG_MODE|SYSFLG_SET_RTC_DATETIME)) )	// If not in config mode or need to send a new time to the RTC.
    			gSysFlags |= SYSFLG_FETCH_DATETIME;			// Raise the date/time update event.
     	}

    	if (gSysFlags & SYSFLG_SYNCSYSEVENT)
    	{
			gSyncCount++;
			if (gSysFlags & SYSFLG_CONFIG_MODE)				// Clear sync event flag if in config mode.
					gSysFlags &= ~SYSFLG_SYNCSYSEVENT;
    	}

    	if (gSysFlags & SYSFLG_RENC_BTN_LNG)				// Detected a long rotary encoder button press.
    	{
    		if (!(gSysFlags & SYSFLG_CONFIG_MODE))			// If we're not in configuration mode.
    		{
     			gSysFlags &= ~SYSFLG_RENC_BTN_LNG;			// Clear the long button press.
    			gSysFlags |= SYSFLG_CONFIG_MODE;			// Enter config mode [UI update].
    		}
    		changeDateTimeUiSM();							// Call the UI update state machine.
    	}

    	// The events wrapped in here all depend on the USI and LCD being free
    	// and are ordered more or less by priority/importance.
    	// The if-else if structure guards against an isr setting a flag for a statemachine
    	// that also requires the usi and lcd while we're 'inside' the outer if.
    	// This way only one state machine or routine 'wins'.
    	// If all of the flag cases were evaluated then there would be the potential for
    	// one state machine to clobber the data for another.
    	if ( (gSysFlags & sysFlagMask) && !usi_i2c_busy() && !lcd_busy() )
    	{
    		if (gSysFlags & SYSFLG_SET_RTC_DATETIME)		// Write a new date and time to the RTC.
    		{
    			setRtcTime(&gsI2Ctransact, NULL);
    		}

    		else if (gSysFlags & SYSFLG_ASYNCSYSEVENT)		// Asynchronous event detected [button press].
    		{
    			asyncEventSM(&gsI2Ctransact);				// Start the async event function.
    		}

    		else if (gSysFlags & SYSFLG_SYNCSYSEVENT)		// Synchronous system event detected [timer0].
    		{
    			if (0 == (gSysFlags & SYSFLG_CONFIG_MODE))	// Only raise the synchronous event if in normal mode.  Guards if the sync event is raised while in here.
    				syncEventSM(&gsI2Ctransact);			// Start the sync event function.
    		}

    		else if (gSysFlags & SYSFLG_FETCH_DATETIME)		// A one second 'tick' has been received from the RTC and has triggered a date/time update.
    		{
    			fetchRtcTime(&gsI2Ctransact, NULL);			// Go get the date and time from the RTC.
    		}

    		else if (gSysFlags & SYSFLG_DISP_DATETIME)		// A request to update the date and time on the display has been flagged.
    		{
    			displayRtcDataSM(&gsI2Ctransact, NULL);		// Update the display with the current date and time.
    		}

    		else if (gSysFlags & SYSFLG_LCD_BACKLIGHT)		// A request to toggle the LCD backlight has been made.
    		{
        		if ( set_lcd_backlight((lcd_get_backlight_state() ^ 1), &gsI2Ctransact) )
        			gSysFlags &= ~SYSFLG_LCD_BACKLIGHT;
    		}
    	}

    	P1OUT ^= DBG_LED;
   		if (sysIsIdle())
   			__bis_SR_register(gSysSleepMode | GIE);				// Sleep with interrupts enabled;
    }

	return 0;
}	// end main.


// #########################
// Local Functions

// Inits...
/* *********************************************************************************
 * Initializes TimerA0 in up count mode.
 * CCR0 is configured to "free run".
 * Various system delays can be implemented based on the CCRx registers.
 * The high speed tick is always active and is controlled via CCR0.
********************************************************************************* */
static inline void init_timera0(void)
{
	TA0CTL = TASSEL_2 | MC_0 | TACLR;	// SMCLK; Timer stopped, Timer cleared.
	TA0CCR0 = HS_SYSTICK_TIMER_VAL;		// Will allow for a 100us system tick at SMCLK = 16MHz. [62.5ns * 1600 = 100us].
	//TA0CCTL0 = CCIE;					// Compare match interrupt enabled for TA0.0.
}

static inline void init_port1(void)
{
	P1DIR &= ~(LCD_BL_BTN | RTC_INT_PIN);			// P1.3, P1.5 input.
	P1OUT |= (LCD_BL_BTN | RTC_INT_PIN);			// P1 reset, P1.3, P1.5 high.
	P1REN |= (RENC_BTN | LCD_BL_BTN | RTC_INT_PIN);	// Pullup enabled on P1.1, P1.3, P1.5.
	P1IFG = 0;										// Clear any pending interrupts.
	P1IES |= (RENC_BTN | LCD_BL_BTN | RTC_INT_PIN);	// Enable interrupts on P1.1, P1.3, P1.5 high->low edge.
	//P1IE |= BUTTON;								// Enable P1.3 interrupt.
}

static inline void init_port2(void)
{
	P2DIR = ~(RENC_SIGB | RENC_SIGA);	// P2.0, P2.1 input.
	//P2DIR = 0;
	P2OUT = (RENC_SIGB | RENC_SIGA);	// P2.0, P2.1 high.
	P2REN = (RENC_SIGB | RENC_SIGA);	// Pullup enabled on P2.0, P2.1.
	P2IFG = 0;							// Clear pending interrupts on Port2.
	P2IES = (P2IN & (RENC_SIGB | RENC_SIGA));	// Enable the appropriately edged interrupts on Port2.
}

static inline void init_led(void)
{
	P1DIR |= DBG_LED;					// Set pin output.
	P1OUT &= ~DBG_LED;					// LED off.
}

static inline void init_lcd_int(void)
{
	if (lcd_check_io_expander_no_init_int(&gsI2Ctransact, gSysBuf))
		lcd_io_expander_init_int(&gsI2Ctransact, gSysBuf);

	lcd_init_int(&gsI2Ctransact, gTmpBuf);

	gsI2Ctransact.buf = gSysBuf;
	while (!lcd_set_backlight_int(1, &gsI2Ctransact));
	usi_i2c_txrx_start(&gsI2Ctransact);

	usi_i2c_sleep_wait(1);									// Sleep wait for the i2c transaction to be done.

	gsI2Ctransact.transactType = I2C_T_IDLE;
	gsI2Ctransact.buf = gSysBuf;

	while (!lcd_clear_int(&gsI2Ctransact));
	usi_i2c_txrx_start(&gsI2Ctransact);

	usi_i2c_sleep_wait(1);									// Sleep wait for the i2c transaction to be done.
	__delay_cycles(LCD_CLEAR_DELAY);						// The high speed timer isn't running yet so we have to spin-wait.

	gsI2Ctransact.transactType = I2C_T_IDLE;				// Clean up.
}

static inline void init_i2c_struct(void)
{
	gsI2Ctransact.callbackFn = NULL;
	gsI2Ctransact.buf = gSysBuf;
	gsI2Ctransact.transactType = I2C_T_IDLE;
}

// Utility functions...

/*
 * Used to evaluate whether we should go to sleep or not.
 * Checks if there are pending USI, Synchronous, or Asynchronous events.
 * Should probably check whether any interrupt set system flags are pending, but be careful not to keep spinning through the main loop.
 * Returns non-zero if there are no pending events.
 */
static inline int sysIsIdle(void)
{
	//const uint16_t sysFlgChk = SYSFLG_RENC_BTN_SHRT | SYSFLG_RENC_BTN_LNG | SYSFLG_RENC_ROT_EVENT;
	//return  !(usi_i2c_check_event());
	return ( !(usi_i2c_check_event()) || (0 == (gSysFlags & ~(SYSFLG_ASYNCSYSEVENT | SYSFLG_SYNCSYSEVENT))) );
}

static int set_lcd_backlight(uint8_t state, i2c_transaction_t *i2c_trn)
{
	if ( !(lcd_set_backlight_int(state, i2c_trn)) )
		return 0;
	usi_i2c_get();
	lcd_get();
	i2c_trn->callbackFn = NULL;
	usi_i2c_txrx_start(i2c_trn);

	return 1;
}

static inline int wait_for_usi_finish(i2c_transaction_t *i2c_trn)
{
	while ( usi_i2c_busy() )									// Wait for the USI [I2C] resources to be released.
	{
		usi_i2c_sleep_wait(1);									// Wait for wakeup by USI (I2C) only.
		if (NULL != i2c_trn->callbackFn)						// Make sure we don't dereference a null pointer.
			i2c_trn->callbackFn(i2c_trn, NULL);
	}
	return 0;
}


/* ********************************************************************************************
 * drawNrmModeStaticData - Draws all of the static elements in Normal mode to the LCD display.
 * Follows the usual i2c callback function prototype call.
 * i2c_transaction_t *pI2cTrans: is a pointer to a i2c transaction struct.
 * void *userdata: is used as a pointer to the callback function.
** *******************************************************************************************/
static void* drawNrmModeStaticData(i2c_transaction_t *pI2cTrans, void *userdata)
{
	static enum_display_norm_static_sm_t state = DISP_NORM_START;
	static i2c_callback_fnptr_t myCallback = NULL;

	switch(state)
	{
	case DISP_NORM_START:
		usi_i2c_get();												// Take the USI.
		lcd_get();													// Take the LCD.
		myCallback = (NULL == userdata) ? NULL : (i2c_callback_fnptr_t)userdata;
		//Clear screen?
		state = DISP_NORM_LINE1;
		// If not clear screen then can probably fall through.
		break;
	case DISP_NORM_LINE1:
		prep_str_for_lcd((uint8_t *)gSyncDispStr, 0x14, 0);
		putstr_to_lcd_int(&gsI2Ctransact, drawNrmModeStaticData);	// Put the updated data to LCD line 1 and return control here when done.
		state = DISP_NORM_LINE2;
		break;
	case DISP_NORM_LINE2:
		prep_str_for_lcd((uint8_t *)gAsyncDispStr, 0x54, 0);
		putstr_to_lcd_int(&gsI2Ctransact, drawNrmModeStaticData);	// Put the updated data to LCD line 2 and return control here when done.
		state = DISP_NORM_END;
		break;
	case DISP_NORM_END:
		if (NULL != myCallback)
		{
			pI2cTrans->callbackFn = myCallback;
			usi_i2c_raise_event();									// Re-raise the I2C system event and keep the USI and LCD.
		}
		else
		{
			pI2cTrans->callbackFn = NULL;
			usi_i2c_release();										// Release USI.
			lcd_release();											// Release LCD.
		}
		state = DISP_NORM_START;									// Reset state machine.
		pI2cTrans->transactType = I2C_T_IDLE;
		myCallback = NULL;
		break;
	}

	return NULL;
}

static inline void asyncEventSM(i2c_transaction_t *pI2cTrans)
{
	const uint8_t cursorPos = 0x54 + 7;		// Initial cursor position.
	//static uint16_t asyncCount = 0xffff;

	gSysBuf[0] = cursorPos | 0x80;
	//((gSysFlags & SYSFLG_RENC_DIR) == 0) ? asyncCount-- : asyncCount++;
	print_u16((uint8_t *)&gSysBuf[1], gAsyncCount, 5);
	gSysBuf[SYS_BUF_SZ - 1] = '\0';				// Guarantee null terminated.  If there's a valid char in the last element of the array this will clobber it!
	gSysFlags &= ~SYSFLG_ASYNCSYSEVENT;			// Clear the flag - the rest is handled by the I2C and display update state machines.
												// This way we can capture the next event while we're processing this one.
	putstr_to_lcd_int(pI2cTrans, NULL);
}

static inline void syncEventSM(i2c_transaction_t *pI2cTrans)
{
	const uint8_t cursorPos = 0x14 + 6;
	//static uint16_t syncCount = 0;

	gSysBuf[0] = cursorPos | 0x80;
	//print_u16((uint8_t *)&gSysBuf[1], syncCount++, 5);
	print_u16((uint8_t *)&gSysBuf[1], gSyncCount, 5);
	gSysBuf[SYS_BUF_SZ - 1] = '\0';				// Guarantee null terminated.  If not careful when initially filling the array, will clobber the last value.
	gSysFlags &= ~SYSFLG_SYNCSYSEVENT;			// Clear the flag - the rest is handled by the I2C and display update state machines.
												// This way we can capture the next event while we're processing this one.
	putstr_to_lcd_int(pI2cTrans, NULL);
}

static inline void* fetchRtcTime(i2c_transaction_t *pI2cTrans, void *userdata)
{
	static uint8_t state = 0;

	if (0 == state)
	{
		usi_i2c_get();											// Take the USI.
		lcd_get();												// Take the LCD.
		pI2cTrans->buf = gSysBuf;
		pI2cTrans->callbackFn = fetchRtcTime;
		//ds3231m_get_time(pI2cTrans);
		ds3231m_get_all(pI2cTrans);
		usi_i2c_txrx_start(pI2cTrans);
		state = 1;
	}
	else
	{
		convert_array_to_datetime((uint8_t *)gSysBuf, &gDt, 1);	// Update the datetime structure with the RTC time.
		pI2cTrans->callbackFn = NULL;
		pI2cTrans->transactType = I2C_T_IDLE;
		usi_i2c_release();										// Release USI.
		lcd_release();											// Release LCD.
		gSysFlags &= ~SYSFLG_FETCH_DATETIME;					// Clear the fetch datetime flag.
		gSysFlags |= SYSFLG_DISP_DATETIME;						// Signal to the system that it's time to display an updated date and time.
		state = 0;
	}

	return NULL;
}

static inline void* setRtcTime(i2c_transaction_t* pI2cTrans, void* userData)
{
	static uint8_t state = 0;

	if (0 == state)
	{
		usi_i2c_get();											// Take the USI.
		lcd_get();												// Take the LCD.
		pI2cTrans->buf = gSysBuf;
		pI2cTrans->callbackFn = setRtcTime;
		ds3231m_set_time(&gDt, pI2cTrans);
		state = 1;
		usi_i2c_txrx_start(pI2cTrans);
	}
	else
	{
		pI2cTrans->callbackFn = NULL;
		pI2cTrans->transactType = I2C_T_IDLE;
		usi_i2c_release();										// Release USI.
		lcd_release();											// Release LCD.
		gSysFlags &= ~SYSFLG_SET_RTC_DATETIME;					// Clear the set RTC flag.
		state = 0;
	}

	return NULL;
}

static inline void* displayRtcDataSM(i2c_transaction_t *pI2cTrans, void *userdata)
{
	static uint8_t state = 0;

	if (0 == state)										// Display the date.
	{
		usi_i2c_get();									// Take the USI.
		lcd_get();										// Take the LCD.
		pI2cTrans->callbackFn = displayRtcDataSM;		// Set the I2C callback to this function.
		pI2cTrans->buf = gSysBuf;						// Reset the buffer pointer.
		gSysBuf[0] = 0x80 | gLcdRowOffsets[0];			// LCD line 1
		convert_datetime_to_bcd(&gDt);					// Require the datetime struct to be in BCD format for display.
		prep_date_disp_str(&gDt, (uint8_t *)&gSysBuf[1]);
		state = 1;
		putstr_to_lcd_int(pI2cTrans, displayRtcDataSM);
	}
	else												// Display the time.
	{
		pI2cTrans->buf = gSysBuf;						// Reset the buffer pointer.
		gSysBuf[0] = 0x80 | gLcdRowOffsets[1];			// LCD line 2
		prep_time_disp_str(&gDt, (uint8_t *)&gSysBuf[1]);
		state = 0;
		putstr_to_lcd_int(pI2cTrans, NULL);				// Let <putstr_to_lcd_int> clean up [saves a call]; it will release the USI and the LCD, and set the I2C callback=NULL.
		gSysFlags &= ~SYSFLG_DISP_DATETIME;				// Clear the display update system flag.
	}

	return NULL;
}

/*  This is the old display function.
static inline void* displayRtcDataSM(i2c_transaction_t *pI2cTrans, void *userdata)
{
	static rtc_display_sm_t state = RTC_GET_TIME;

	//if necessary, override the time update here.

	switch(state)
	{
	case RTC_GET_TIME:
		usi_i2c_get();									// Take the USI.
		lcd_get();										// Take the LCD.
		gSysFlags &= ~SYSFLG_FETCH_DATETIME;
		pI2cTrans->buf = gSysBuf;
		pI2cTrans->callbackFn = displayRtcDataSM;
		//ds3231m_get_time(pI2cTrans);
		ds3231m_get_all(pI2cTrans);
		state = RTC_DIS_DATE;
		usi_i2c_txrx_start(pI2cTrans);
		break;
	case RTC_DIS_DATE:
		convert_array_to_datetime((uint8_t *)gSysBuf, &gDt, 1);
		pI2cTrans->buf = gSysBuf;						// Reset the buffer pointer.
		gSysBuf[0] = 0x80 | gLcdRowOffsets[0];			// LCD line 1
		prep_date_disp_str(&gDt, (uint8_t *)&gSysBuf[1]);
		state = RTC_DIS_TIME;
		putstr_to_lcd_int(pI2cTrans, displayRtcDataSM);
		break;
	case RTC_DIS_TIME:
		pI2cTrans->buf = gSysBuf;						// Reset the buffer pointer.
		gSysBuf[0] = 0x80 | gLcdRowOffsets[1];			// LCD line 2
		prep_time_disp_str(&gDt, (uint8_t *)&gSysBuf[1]);
		state = RTC_GET_TIME;							// Let <putstr_to_lcd_int> clean up [saves a call].
		putstr_to_lcd_int(pI2cTrans, NULL);
		break;
	//case RTC_DIS_END:
		// If we didn't want <put_to_lcd_int> to clean up then we could clean up here.
	//default:											// Should never get here. [only if not cleaning up in this function].
	//	pI2cTrans->callbackFn = NULL;
	//	pI2cTrans->transactType = I2C_T_IDLE;
	//	usi_i2c_release();								// Release USI.
	//	lcd_release();									// Release LCD.
	//	state = RTC_GET_TIME;
	//	break;
	}
	return NULL;
} */


/* *****************************************************************************************************************************************************
 * static void* putstr_to_lcd_int(i2c_transaction_t *i2c_trn, void *userdata)
 * i2c_transaction_t *i2c_trn: is a pointer to an i2c_transaction_t struct and will hold all of the transaction data.
 * void* userdata: is a generic pointer used to transfer user defined info into the routine.  Currently used to point to a callback.
 * 				   It is not typed so beware when typecasting!
 *
 * This function puts a string to the LCD for display.  The string must be null terminated.
 * This function may be called with a callback; in this case, when the string send is finished this routine will issue a STOP to I2C and register the
 * callback in the I2C struct.  When the processor wakes because of the I2C event it will follow the callback.
 *
 * Requires that the buffer in the i2c struct be filled with the data to send.  It also requires a 'helper' 4-byte buffer to hold data.
 * The first element of the buffer must be the cursor position to display the first character of the string.
 * The next elements of the buffer are the characters to display [sequentially] on the LCD.
 * The string must be null terminated or bad things will happen.
 **************************************************************************************************************************************************** */
static void* putstr_to_lcd_int(i2c_transaction_t *i2c_trn, void *userdata)
{
	// Consider allowing *userdata to be a callback function pointer.
	// This function will need to implement a static variable to store the callback.
	// This will allow multiline screen updates to happen without interruption [such as date and time display].
	static enum_lcd_print_sm_t state = LCD_P_SM_SEND_START;
	static uint8_t bufIndx = 0;
	static i2c_callback_fnptr_t myCallback = NULL;

	switch(state)
	{
	case LCD_P_SM_SEND_START:
		usi_i2c_get();									// Take the USI.  <-- Might not be necessary; the caller should have already taken care of this.
		lcd_get();										// Take the LCD.  <-- See above.
		myCallback = (NULL == userdata) ? NULL : (i2c_callback_fnptr_t)userdata;
		bufIndx = 0;
		gTmpBuf[0] = IO_EXP_IO_REG;
		i2c_trn->address = IO_EXPANDER_ADDR;
		i2c_trn->numBytes = 1;
		i2c_trn->callbackFn = putstr_to_lcd_int;
		i2c_trn->buf = gTmpBuf;
		i2c_trn->transactType = I2C_T_TX_WAIT;
		state = LCD_P_SM_SEND_CURS;
		usi_i2c_txrx_start(i2c_trn);
		break;
	case LCD_P_SM_SEND_CURS:
		i2c_trn->buf = gTmpBuf;
		i2c_trn->numBytes = 4;
		i2c_trn->transactType = I2C_T_TX_WAIT;
		lcd_write_int(gSysBuf[bufIndx++], 1, 0, gTmpBuf);
		state = LCD_P_SM_SEND_CHAR;
		usi_i2c_txrx_resume();
		break;
	case LCD_P_SM_SEND_CHAR:
		if (gSysBuf[bufIndx])
		{
			lcd_write_int(gSysBuf[bufIndx++], 1, 1, gTmpBuf);
			i2c_trn->buf = gTmpBuf;
			i2c_trn->numBytes = 4;

			if (gSysBuf[bufIndx] == (uint8_t)'\0')// Look ahead for null termination.
			{									// If the next char is null then the transaction can end after this.
				i2c_trn->transactType = I2C_T_TX_STOP;
				state = LCD_P_SM_END;
			}
			usi_i2c_txrx_resume();
		}
		else									// This will likely never get executed, but it's safer if we somehow miss the null termination in the previous block.
		{
			i2c_trn->transactType = I2C_T_TX_STOP;
			state = LCD_P_SM_END;
			usi_i2c_txrx_stop(i2c_trn);
		}
		break;
	case LCD_P_SM_END:
		if (NULL != myCallback)
		{
			i2c_trn->callbackFn = myCallback;
			usi_i2c_raise_event();				// Re-raise the I2C system event and keep the USI and LCD.
		}
		else
		{
			i2c_trn->callbackFn = NULL;
			usi_i2c_release();					// Release USI.
			lcd_release();						// Release LCD.
		}
		state = LCD_P_SM_SEND_START;			// Reset state machine.
		i2c_trn->transactType = I2C_T_IDLE;
		myCallback = NULL;
		break;
	/*default:									// Clean up here.  Will also reset everything if things have gone nuts.
		i2c_trn->callbackFn = NULL;
		usi_i2c_release();						// Release USI.
		lcd_release();							// Release LCD.
		state = LCD_P_SM_SEND_START;			// Reset state machine.
		i2c_trn->transactType = I2C_T_IDLE;
		myCallback = NULL;
		break;*/
	}
	return NULL;
}


/* *****************************************************************************************
 * Need to call this in response to a rotary encoder rotation event when in update mode.
 * Each rotary encoder rotation event will update a time variable and then signal a time
 * update to the system.
 * A rotary encoder button press moves through the date/time and commits the time to the
 * RTC after updating the seconds.
 *
 * Pseudocode exploring:
 * step 0: keep track of the variable that we need to update [and the final desired cursor position?]
 * step 1: Update the proper field in the datetime struct.
 * step 2: Turn off cursor blinking.
 * step 3: Set SYSFLG_DISP_DATETIME to direct the system to display the updated date & time.
 *         - or -
 *         Call the screen update directly so we can pass our function as the
 *         callback and get back into our state machine to reset the cursor position and to
 *         re-enable cursor blinking.
 * if we're not at the end of the state machine:
 * step 4: Reset the cursor position.
 * step 5: Set the cursor to blink.
 * else:
 * step 4: Direct the system to update the RTC with the new date and time.  Exit config
 *         mode.
 ***************************************************************************************** */
static inline void changeDateTimeUiSM(void)
{
	// Declaration of a constant function pointer table:
	//const uint8_t (* const fnTbl[])(DateTime_t *, int8_t) = {change_day_of_week, change_day_of_month, change_month, change_year, change_hour, change_minute, change_second};
	static ui_dt_upd_sm_t state = UI_UPD_S_DAY;
	static uint8_t (*fp)(DateTime_t *, int8_t) = NULL;	// Pointer to a function that returns uint8_t
														// and takes DateTime_t * and uint8_t args.
	// Set the cursor to the right spot.
	switch (state)										// In this switch set the function pointer to the appropriate ui_update function.
	{
	case UI_UPD_S_DAY:
		fp = change_day_of_week;
		break;
	case UI_UPD_S_DATE:
		fp = change_day_of_month;
		break;
	case UI_UPD_S_MONTH:
		fp = change_month;
		break;
	case UI_UPD_S_YEAR:
		fp = change_year;
		break;
	case UI_UPD_S_HOUR:
		fp = change_hour;
		break;
	case UI_UPD_S_MIN:
		fp = change_minute;
		break;
	case UI_UPD_S_SEC:
		fp = change_second;
		break;
	}

	if (gSysFlags & SYSFLG_RENC_BTN_LNG)		// long press - abort the ui update.
	{
		state = UI_UPD_S_DAY;
		gSysFlags &= ~(SYSFLG_RENC_BTN_LNG | SYSFLG_CONFIG_MODE);
		gSysFlags |= (SYSFLG_FETCH_DATETIME);	// Flag the system to fetch the time from the RTC.  The time fetch will trigger a screen repaint.
	}
	else if (gSysFlags & SYSFLG_RENC_BTN_SHRT)	// short press - advance the state [which will move the current function pointer].
	{
		if (state < UI_UPD_S_SEC)				// Not at the last state, update the state.
		{
			state++;
		}
		else									// At the last state; exit and flag the system to write the new datetime to the RTC.
		{
			state = UI_UPD_S_DAY;
			gSysFlags &= ~(SYSFLG_RENC_BTN_SHRT|SYSFLG_CONFIG_MODE);
			gSysFlags |= SYSFLG_SET_RTC_DATETIME;
		}
	}
	else if (gSysFlags & SYSFLG_RENC_ROT_EVENT)	// Rotary encoder event detected; increment or decrement the variable to be modified.
	{
		if (gSysFlags & SYSFLG_RENC_DIR)
			fp(&gDt, 1);
		else
			fp(&gDt, 0);

		gSysFlags &= ~SYSFLG_RENC_ROT_EVENT;
		gSysFlags |= SYSFLG_DISP_DATETIME;		// Flag to update the datetime on the screen.
	}
	else
	{
		// don't know why we are here.  Update screen?
	}
}

static int prep_str_for_lcd(uint8_t *theStr, uint8_t pos, uint8_t sz)
{
	gSysBuf[0] = pos | 0x80;

	if (sz)												// Number of characters is specified.
		memcpy((uint8_t *)&gSysBuf[1], theStr, sz);
	else												// Or NUL terminated string.
		strcpy((char *)&gSysBuf[1], (const char *)theStr);

	return 1;
}


uint8_t* itoa(int16_t value, uint8_t *result, uint8_t base)
{
	// check that the base is valid
	if ( (2 > base) || (16 < base) )
	{
		*result = '\0';
		return result;
	}

	uint8_t* ptr = result, *ptr1 = result, tmp_char;
	int16_t tmp_value;

	do
	{
		tmp_value = value;
		value /= base;
		*ptr++ = "fedcba9876543210123456789abcdef" [15 + (tmp_value - value * base)];
	} while (value);

	// Apply negative sign
	if (tmp_value < 0) *ptr++ = '-';
	*ptr-- = '\0';
	while(ptr1 < ptr)
	{
		tmp_char = *ptr;
		*ptr--= *ptr1;
		*ptr1++ = tmp_char;
	}
	return result;
}

uint8_t* utoa(uint16_t value, uint8_t *result, uint8_t base)
{
	// check that the base is valid
	if ( (2 > base) || (36 < base) )
	{
		*result = '\0';
		return result;
	}

	uint8_t* ptr = result, *ptr1 = result, tmp_char;
	uint16_t n, tmp_value;

	do
	{
		tmp_value = value;
		value /= base;
		n = (tmp_value - value * base);
		*ptr++ = (n < 10)? '0' + n: ('a' - 10) + n;	//lowercase only.
	} while (value);

	*ptr-- = '\0';
	while(ptr1 < ptr)
	{
		tmp_char = *ptr;
		*ptr--= *ptr1;
		*ptr1++ = tmp_char;
	}
	return result;
}

// Prints an input uint16_t value in an arbitrary width field.
// Ensures that when decrementing numbers over a magnitude boundary
// There is not garbage left behind to display; for instance stepping
// down from 10 -> 9.
// The 'empty' digits are filled with ASCII space.
static uint8_t* print_u16(uint8_t *buf, uint16_t value, uint8_t width)
{
	register uint8_t indx = 0;

	utoa(value, buf, 10);						// Convert number to null-terminated ascii string.
	while (buf[indx++]);						// Find the null termination.
	memset(&buf[--indx], 0x20, width - indx);	// Fill rest with ASCII space.
	buf[width] = '\0';							// Ensure null termination.
	return buf;
}

static inline void prep_time_disp_str(DateTime_t *dt, uint8_t *buf)
{
	// Assumes dt is in BCD format.
	// Display format is HH:MM:SS  [24hr]
	buf[0] = ((dt->hours >> 4) & 0x0f) + ASCII_ZERO;
	buf[1] = (dt->hours & 0x0f) + ASCII_ZERO;
	buf[2] = TIME_DELIMITER;
	buf[3] = ((dt->minutes >> 4) & 0x0f) + ASCII_ZERO;
	buf[4] = (dt->minutes & 0x0f) + ASCII_ZERO;
	buf[5] = TIME_DELIMITER;
	buf[6] = ((dt->seconds >> 4) & 0x0f) + ASCII_ZERO;
	buf[7] = (dt->seconds & 0x0f) + ASCII_ZERO;
	buf[8] = '\0';	//NUL string terminator.
}

static inline void prep_date_disp_str(DateTime_t *dt, uint8_t *buf)
{
	// Assumes dt is in BCD format.
	// This displays "DOW DD/MMM/YY"
	memcpy(&buf[0], gDaysOfWeek[(uint8_t)(dt->dow - 1)], 3);
	buf[3] = ' ';
	buf[4] = ((dt->dom >> 4) & 0x0f) + ASCII_ZERO;
	buf[5] = (dt->dom & 0x0f) + ASCII_ZERO;
	buf[6] = DATE_DELIMITER;
	memcpy(&buf[7], gMonths[(uint8_t)(dt->month - 1)], 3);
	buf[10] = DATE_DELIMITER;
	buf[11] = ((dt->year >> 4) & 0x0f) + ASCII_ZERO;
	buf[12] = (dt->year & 0x0f) + ASCII_ZERO;
	buf[13] = '\0';	//NUL string terminator.

	// This displays "YY/MMM/DD DOW" <-- Makes it easier in UI to filter for invalid month/day combinations.
	/*buf[0] = ((dt->year >> 4) & 0x0f) + ASCII_ZERO;
	buf[1] = (dt->year & 0x0f) + ASCII_ZERO;
	buf[2] = DATE_DELIMITER;
	memcpy(&buf[3], gMonths[(uint8_t)(dt->month - 1)], 3);
	buf[6] = DATE_DELIMITER;
	buf[7] = ((dt->dom >> 4) & 0x0f) + ASCII_ZERO;
	buf[8] = (dt->dom & 0x0f) + ASCII_ZERO;
	buf[9] = DATE_DELIMITER;
	memcpy(&buf[10], gDaysOfWeek[(uint8_t)(dt->dow - 1)], 3);
	buf[13] = '\0'; */
}


// #########################
// Interrupt Routine Definitions
#pragma vector=PORT1_VECTOR
__interrupt void PORT1_ISR(void)
{
	//Start the process of debouncing the button press - debounce time is set to ~30ms.
	if (P1IFG & LCD_BL_BTN)							// Port 1.3 - lcd backlight toggle button.
	{
		P1IE &= ~LCD_BL_BTN;						// Turn off P1.3 interrupt.
		gAsyncBtnDebounceCounter = ASYNC_BTN_DBNCE_TMR;
		gSysFlags |= SYSFLG_LCD_BTN_DBNCE;
		P1IFG &= ~LCD_BL_BTN;						// Clear P1.3 interrupt.
	}

	if (P1IFG & RENC_BTN)							// Port 1.1 - Rotary encoder button press.
	{
		P1IE &= ~RENC_BTN;							// Turn off P1.1 interrupt.
		gRencBtnDebounceCounter = RENC_BTN_DBNCE_TMR;
		gSysFlags |= SYSFLG_RENC_BTN_DBNCE;
		P1IFG &= ~RENC_BTN;							// Clear P1.1 interrupt.
	}

	if (P1IFG & RTC_INT_PIN)						// No debounce necessary - 1s event input from RTC.
	{
		gSysFlags |= SYSFLG_ONESEC_EVENT;
		P1IFG &= ~RTC_INT_PIN;
	}
}

#pragma vector=PORT2_VECTOR
__interrupt void PORT2_ISR(void)
{
	//Provides the interrupt for the rotary encoder.
	//direction: cw = 1, ccw = 0.
	const uint8_t cw_seq = 0x87;
	const uint8_t ccw_seq = 0x4b;
	static uint8_t state = 0;
	int wake;

	// This really requires [at least minimal] hw debounce or it doesn't work at all.
	// 0.1uF ceramic capacitors + internal port pullups used - seems to work well.
	// Note that if sigB != Px.1 and sigA != Px.0 then shifts need to be incorporated.
	state |= P2IN & (RENC_SIGB | RENC_SIGA);
	if (cw_seq == state)
	{
		gSysFlags |= (SYSFLG_RENC_ROT_EVENT | SYSFLG_RENC_DIR);
		wake = 1;
	}
	else if (ccw_seq == state)
	{
		gSysFlags = (gSysFlags | SYSFLG_RENC_ROT_EVENT) & ~SYSFLG_RENC_DIR;
		wake = 1;
	}
	state <<= 2;

	if (P2IFG & RENC_SIGA)
	{
		P2IES ^= RENC_SIGA;
		P2IFG &= ~RENC_SIGA;
	}
	else if (P2IFG & RENC_SIGB)
	{
		P2IES ^= RENC_SIGB;
		P2IFG &= ~RENC_SIGB;
	}

	if (wake)
		__bic_SR_register_on_exit(SLEEP_MODE);
}

#pragma vector=TIMER0_A0_VECTOR
__interrupt void TIMER0_A0_ISR(void)
{
#if HS_SYSTICK_SPD == 1000								// Interrupt called every 1ms.
	static uint8_t syncEventCounter = SYNC_EVENT_COUNTER;
#else													// Interrupt called every 100us.
	static uint16_t syncEventCounter = SYNC_EVENT_COUNTER;
#endif
	static uint16_t rencBtnTimer = 0;
	int wake = 0;

	TA0CCR0 += HS_SYSTICK_TIMER_VAL;					// Set TA0CCR0 for next interval.

	if (gSysFlags & SYSFLG_LCD_BTN_DBNCE)				// Debouncing the LCD backlight button.
	{
		if (0 == --gAsyncBtnDebounceCounter)
		{
			gSysFlags &= ~SYSFLG_LCD_BTN_DBNCE;			// Not debouncing any more.
			if ( 0 == (P1IN & LCD_BL_BTN) )				// If button is still pressed [active low!].
			{
				gSysFlags |= SYSFLG_LCD_BACKLIGHT;		// Signal a backlight change event to the system.
				wake = 1;								// Wake up.
			}
			P1IFG &= ~LCD_BL_BTN;						// Clear the button interrupt flag (might be set from bounce).
			P1IE |= LCD_BL_BTN;							// Turn the button interrupt back on.
		}
	}

	// Note that we either debounce the rotary encoder pushbutton or check for short or long press, but not both.
	if (gSysFlags & SYSFLG_RENC_BTN_DBNCE)				// Debouncing the rotary encoder pushbutton.
	{
		if (0 == --gRencBtnDebounceCounter)
		{
			gSysFlags &= ~SYSFLG_RENC_BTN_DBNCE;		// Debounce timer expired so stop debouncing.
			if ( 0 == (P1IN & RENC_BTN) )				// Only raise "button down" if the input is 0.  Note that if the button is released while there is an active "button down" flag then this won't clobber that.
			{											// However, there will be an additional delay in detecting the button press from the edge of the first bounce to the edge of the last bounce +30ms.
				gSysFlags |= SYSFLG_RENC_BTN_DN;		// Set the rotary encoder button down flag.
			}
			else										// Handle the case where it's bounce/noise.
			{
				P1IFG &= ~RENC_BTN;						// Clear the button interrupt flag (might be set from bounce).
				P1IE |= RENC_BTN;						// Turn the button interrupt back on.  We're done - nothing to see here...
			}
		}
	}
	else if (gSysFlags & SYSFLG_RENC_BTN_DN)			// Rotary encoder button is [or was] down.
	{
		int flag = 0;
		if ( P1IN & RENC_BTN )							// Check to see if the rotary encoder button is now up.
		{
			gSysFlags |= SYSFLG_RENC_BTN_SHRT;			// If button up then don't need to check the long press timer since:
			flag = 1;									// 1. The button is debounced at this point and was down before [legitimately]; if it's up now and a long press hasn't been flagged, then it's a short press.
														// 2. If there was a long press then it should be flagged in the next block first.
		}
		else if (++rencBtnTimer >= RENC_BTN_LNG_PRS)
		{
			gSysFlags |= SYSFLG_RENC_BTN_LNG;			// Flag a long press immediately - don't wait for the button to go up.
			flag = 1;
		}

		if (flag)
		{
			gSysFlags &= ~SYSFLG_RENC_BTN_DN;			// Cancel the rotary encoder button down flag.
			rencBtnTimer = 0;							// Reset the button down timer since we've detected either a long or short press.
			P1IFG &= ~RENC_BTN;							// Clear the button interrupt flag (might be set from bounce).
			P1IE |= RENC_BTN;							// Turn the button interrupt back on.
			wake = 1;									// Wake up to handle rotary encoder button press.
		}
	}

	if (0 == --syncEventCounter)
	{
		syncEventCounter = SYNC_EVENT_COUNTER;			// Reload the synchronous event counter.
		gSysFlags |= SYSFLG_SYNCSYSEVENT;				// Send signal to main to handle sync event.
		wake = 1;										// Wake up.
	}

	if (wake)
		__bic_SR_register_on_exit(SLEEP_MODE);
}
