#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include "sysdefs.h"


#define P_LED0 PC7
#define P_LED1 PC6
#define P_LED2 PC5
#define P_LED3 PC4
#define P_LED4 PC3
#define P_LED5 PC2
#define P_LED6 PC1
#define P_LED7 PC0


#define LED_ON_TIME 50
#define LED_OFF_TIME 75


volatile uint8_t ledstate;
volatile uint8_t ledtrig;
int led_tmr[8];

#define led_ctrl(portbase, portnum, ledsel)		\
({												\
	if(ledstate & _BV(ledsel))					\
	{											\
		led_tmr[ledsel]--;						\
		if(!led_tmr[ledsel])					\
		{										\
			if(portbase & _BV(portnum))			\
			{									\
				portbase &= ~_BV(portnum);		\
				led_tmr[ledsel] = LED_OFF_TIME;	\
			}									\
			else								\
				ledstate &= ~_BV(ledsel);		\
		}										\
	}											\
	else										\
	{											\
		if(ledtrig & _BV(ledsel))				\
		{										\
			ledstate |= _BV(ledsel);			\
			portbase |= _BV(portnum);			\
			led_tmr[ledsel] = LED_ON_TIME;		\
		}										\
	}											\
})											


/*****************************************************/
void statleds_Init(void)
/*****************************************************/
{
	ledstate = 0;
	ledtrig = 0;
}

/*****************************************************/
void statleds_Exec(void)
/*****************************************************/
{
	led_ctrl(PORTC, P_LED0, 0);
	led_ctrl(PORTC, P_LED1, 1);
	led_ctrl(PORTC, P_LED2, 2);
	led_ctrl(PORTC, P_LED3, 3);
	led_ctrl(PORTC, P_LED4, 4);
	led_ctrl(PORTC, P_LED5, 5);
	led_ctrl(PORTC, P_LED6, 6);
	led_ctrl(PORTC, P_LED7, 7);

	ledtrig = 0;
}
