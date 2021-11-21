// vim: ts=4 shiftwidth=4
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>	// strlen
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include "sysdefs.h"
#include "usart.h"
#include "cmrpcktctrl.h"

#define GPS_TIMEOUT_PERIOD 50	// ms
#define GPS_TIMEOUT 0

volatile uint8_t sysflags;

CMR_RXPACKET cmr_modem_rxpckt;
CMR_RXPACKET cmr_bt_rxpckt;

uint8_t cmr_modem_buf[1024];
uint8_t cmr_bt_buf[1024];

uint8_t cmr_rxbuf[256];
uint8_t cmr_txbuf[256];
uint8_t cmr_length;

uint8_t GPS1_Buf[1024], GPS2_Buf[1024];
uint16_t GPS1_BufPos, GPS2_BufPos;
uint16_t GPS_TimeOut;
uint8_t SysFlags;

#if defined(MODEMBOX_CRAWLER)

#define		MINUTE_RELOAD	(60000)
volatile uint16_t	minute_ticker = MINUTE_RELOAD/6;
/** ADC reading in the range 0...65535.
 * Averaged approximately over 5 seconds. */
volatile uint16_t	adc = 0;
ISR(ADC_vect)
{
#define	ADC_COUNTER_DEPTH	15
#define	ADC_COUNTER_RELOAD	(1 << ADC_COUNTER_DEPTH)
	static uint16_t	counter = ADC_COUNTER_RELOAD;
	static uint32_t	accumulator = 0;

	const uint8_t	adcl = ADCL;
	const uint8_t	adch = ADCH;

	accumulator += (((uint16_t)adch) << 8) + adcl;
	if (--counter == 0) {
		counter = ADC_COUNTER_RELOAD;
		adc = accumulator >> (ADC_COUNTER_DEPTH-6);
		accumulator = 0;
	}
}


#endif

/*****************************************************/
ISR (TIMER0_OVF_vect)
/*****************************************************/
{	
	TCNT0 = (uint8_t) TMR0_RELOAD;

	wdt_reset();

	if (SysFlags & _BV(GPS_TIMEOUT)) {
		if (!(--GPS_TimeOut)) {
			SysFlags &= ~_BV(GPS_TIMEOUT);	
		}
	}

#if defined(MODEMBOX_CRAWLER)
	if (minute_ticker > 0) {
		--minute_ticker;
	}
#endif

	statleds_Exec();
}


/*****************************************************/
void io_Init(void)
/*****************************************************/
{
	PORTA = 0;
	DDRA = 0;

	PORTB = 0;
	DDRB = 0;

	PORTC = 0;
	DDRC = 0xff;

	PORTD = 0;
	DDRD = 0;

	PORTE = 0;
	DDRE = 0;

	PORTF = 0;
	DDRF = 0;

	PORTG = 0;
	DDRG = 0;

	PORTH = 0;
	DDRH = 0;

	PORTJ = 0;
	DDRJ = 0;

	PORTK = 0;
	DDRK = 0;

	PORTL = 0;
	DDRL = 0;

	TCCR0A = 0;
	TCCR0B = _BV(CS02);
	TCNT0 = (uint8_t) TMR0_RELOAD;
	OCR0A = 0;	
	OCR0B = 0;

	TIMSK0 = _BV(TOIE0);

#if defined(MODEMBOX_CRAWLER)
	// ADC, 64 prescaler, 56.7kHz clock, 4450 Hz sampling rate.
	ADMUX = _BV(REFS0);
	ADCSRB = 0;
	ADCSRA = _BV(ADEN) | _BV(ADSC) | _BV(ADATE) | _BV(ADIE) | _BV(ADPS2) | _BV(ADPS1);
	DIDR0 = _BV(ADC0D);
#endif
}


static char			xbuf[64];

/*****************************************************/
int main(void)
/*****************************************************/
{
	uint8_t ch;

	io_Init();
	uart_Init();
	statleds_Init();
	CMR_Init(&cmr_modem_rxpckt, cmr_modem_buf);
	CMR_Init(&cmr_bt_rxpckt, cmr_bt_buf);
	
	wdt_enable(WDTO_15MS);
	set_sleep_mode(SLEEP_MODE_IDLE);

	sysflags = 0;
	GPS1_BufPos = GPS2_BufPos = 0;
	GPS1_Buf[0] = 0;
	GPS2_Buf[0] = 1;

	sei();

	LED0
	LED1
	LED2
	LED3
	LED4
	LED5
	LED6
	LED7

	while(1)
	{
		


// Data From Modem
		if(!uart0_IsRxEmpty())
		{

			
			ch = uart0_GetChar();
			
#if defined(MODEMBOX_CRAWLER)
			uart2_PutChar(ch);
			uart3_PutChar(ch);
#else
			switch(CMR_DecodePacket(&cmr_modem_rxpckt, ch))
			{
				case CMRDECODE_OK:
					break;
				
				case CMRDECODE_COMPLETE:
					{
						int i;
						

						for(i = 0; i < cmr_modem_rxpckt.CmrLength; i++)
							uart1_PutChar(cmr_modem_rxpckt.CmrBuffer[i]);

						if(cmr_modem_rxpckt.Type == CMR_TYPE_RTCM)
						{
							for(i = 0; i < cmr_modem_rxpckt.Length; i++)
							{
								uart2_PutChar(cmr_modem_rxpckt.DataBlock[i]);
								uart3_PutChar(cmr_modem_rxpckt.DataBlock[i]);
							}
						}
					}

					break;
				
				case CMRDECODE_ERROR:
					break;

				default:
					break;

			}
#endif
			
		}    

// Data From BT

		if(!uart1_IsRxEmpty())
		{
			ch = uart1_GetChar();
			switch(CMR_DecodePacket(&cmr_bt_rxpckt, ch))
			{
				case CMRDECODE_OK:
					break;

				case CMRDECODE_COMPLETE:
#if !defined(MODEMBOX_CRAWLER)
					{
						int i;

						for(i = 0; i < cmr_bt_rxpckt.CmrLength; i++)
							uart0_PutChar(cmr_bt_rxpckt.CmrBuffer[i]);
					}					
#endif
					break;

				case CMRDECODE_ERROR:
					break;

				default:
					break;

			}
		}


// Data From GPS1

		if(!uart2_IsRxEmpty())
		{
			GPS1_Buf[GPS1_BufPos + 1] = uart2_GetChar();
			GPS1_BufPos++;
			if(GPS1_BufPos >= 100)
			{
				int i;
				
				CMR_EncodePacket(GPS1_Buf, GPS1_BufPos + 1, CMR_TYPE_SERIAL, cmr_txbuf, &cmr_length);

				for(i = 0; i < cmr_length; i++)
				{
#if !defined(MODEMBOX_CRAWLER)
					uart0_PutChar(cmr_txbuf[i]);
#endif
					uart1_PutChar(cmr_txbuf[i]);
				}

				GPS1_BufPos = 0;
			}
		}


// Data From GPS2

		if(!uart3_IsRxEmpty())
		{
			GPS2_Buf[GPS2_BufPos + 1] = uart3_GetChar();
			GPS2_BufPos++;
			if(GPS2_BufPos >= 100)
			{
				int i;

				CMR_EncodePacket(GPS2_Buf, GPS2_BufPos + 1, CMR_TYPE_SERIAL, cmr_txbuf, &cmr_length);

				for(i = 0; i < cmr_length; i++)
				{
#if !defined(MODEMBOX_CRAWLER)
					uart0_PutChar(cmr_txbuf[i]);
#endif
					uart1_PutChar(cmr_txbuf[i]);
				}
	
				GPS2_BufPos = 0;
			}
		}


// GPS Timeout

		if(!(SysFlags & _BV(GPS_TIMEOUT)))
		{
			
			if(GPS1_BufPos)
			{
				int i;

				CMR_EncodePacket(GPS1_Buf, GPS1_BufPos + 1, CMR_TYPE_SERIAL, cmr_txbuf, &cmr_length);

				for(i = 0; i < cmr_length; i++)
				{
#if !defined(MODEMBOX_CRAWLER)
					uart0_PutChar(cmr_txbuf[i]);
#endif
					uart1_PutChar(cmr_txbuf[i]);
				}

				GPS1_BufPos = 0;
			}

			if(GPS2_BufPos)
			{
				int i;

				CMR_EncodePacket(GPS2_Buf, GPS2_BufPos + 1, CMR_TYPE_SERIAL, cmr_txbuf, &cmr_length);

				for(i = 0; i < cmr_length; i++)
				{
#if !defined(MODEMBOX_CRAWLER)
					uart0_PutChar(cmr_txbuf[i]);					
#endif
					uart1_PutChar(cmr_txbuf[i]);
				}

				GPS2_BufPos = 0;
			}
			
			GPS_TimeOut = GPS_TIMEOUT_PERIOD;
			SysFlags |= _BV(GPS_TIMEOUT);
		}

#if defined(MODEMBOX_CRAWLER)
// 1 minute timer.
		if (minute_ticker == 0) {
			const float		voltage = adc * (11.0 * 5.0 / 65535.0);
			int				i;

			sprintf_P(xbuf, PSTR("modembox supplyvoltage=%f"), voltage);

			// TODO: send voltage packet. How?
			CMR_EncodePacket(xbuf, strlen(xbuf), CMR_TYPE_DIGNET, cmr_txbuf, &cmr_length);
			for(i = 0; i < cmr_length; i++)
			{
				uart1_PutChar(cmr_txbuf[i]);
			}


			// TODO: send identification packet. How?

			minute_ticker = MINUTE_RELOAD;
		}
#endif
	}
}
