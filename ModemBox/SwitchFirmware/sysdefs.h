#ifndef _SYSDEFS_H
#define _SYSDEFS_H

#ifndef CR_TAB
#define CR_TAB "\n\t"
#endif

// rev. a:
//#define F_CPU 6000000ul
// rev. b:
//#define F_CPU 3686400ul

#define TMR0_FREQ 1000ul
#define TMR0_PRESC 256ul
#define TMR0_RELOAD (0ul - (F_CPU / (TMR0_FREQ * TMR0_PRESC)))

#define TRUE 1
#define FALSE 0

extern volatile uint8_t ledtrig;

uint8_t do_crc(uint8_t data, uint8_t crc);
uint8_t check_crc(uint8_t *p, int size);
uint8_t gen_crc(uint8_t *p, int size);

void statleds_Init(void);
void statleds_Exec(void);

#define LED0 ledtrig |= _BV(0);
#define LED1 ledtrig |= _BV(1);
#define LED2 ledtrig |= _BV(2);
#define LED3 ledtrig |= _BV(3);
#define LED4 ledtrig |= _BV(4);
#define LED5 ledtrig |= _BV(5);
#define LED6 ledtrig |= _BV(6);
#define LED7 ledtrig |= _BV(7);


#endif
