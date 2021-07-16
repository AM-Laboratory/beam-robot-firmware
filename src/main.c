#if defined (__AVR_ATmega328p__)
	#define F_CPU 16000000UL
#elif defined (__AVR_ATtiny13__)
	#define F_CPU 1200000UL
#else
	#error "Device not supported"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <avr/interrupt.h>
#include <avr/sfr_defs.h>
#include <avr/sfr_defs.h>
#include <util/delay.h>

#define PERIOD_MEASUREMENT_SOFTWARE
#define HWTIMER_FREQUENCY 1777l
#define PERIOD_TIMER_FREQUENCY 1777l
#include "lib/timer_period.c"
#include "lib/ir_nec.c"


#if defined (__AVR_ATmega328p__)
	#define PIN_IR PINB
	#define BIT_IR 3
	#define PORT_PWM PORTB
	#define DDR_PWM DDRB
	#define BIT_PWM 1
	#define F_CPU_DIV_1777Hz 140
#elif defined (__AVR_ATtiny13__)
	#define PIN_IR PINB
	#define BIT_IR 0
	#define PORT_PWM PORTB
	#define DDR_PWM DDRB
	#define BIT_PWM 2
	#define F_CPU_DIV_1777Hz 84
#endif

ISR(PCINT0_vect){
	/* Logical pin change interrupt vector. */
	sei(); // Enable nested interrupts
	if (((PIN_IR >> BIT_IR) & 1) == 0){
		ir_nec_process_pin_change();
	}
}

volatile uint8_t mute = 0;

inline void tick();

// For some reason, these interrupts are named differently for ATMega and ATTiny
#if defined (__AVR_ATtiny13__)
	#define TIMER0_COMPA_vect TIM0_COMPA_vect
	#define TIMER0_COMPB_vect TIM0_COMPB_vect
#endif


ISR(TIMER0_COMPB_vect){
	PORT_PWM &= ~(1 << BIT_PWM);
}
ISR(TIMER0_COMPA_vect){
	tick();
	if (!mute){
		PORT_PWM |= (1 << BIT_PWM);
	}
/* Timer reached value COMPA interrupt vector. */
/* The timer generates a TIMER_FREQ Hz internal signal, which carries two functions:
 * 1. PWM for the motor;
 * 2. At the end of each period, i.e., COMPA value reached by the timer, this interrupt vector is executed,
 * which triggers all actions: secondary counter increments and IR readout, ADC readout, etc.
 */
}

inline void tick(){
	software_timer_tick((uint16_t) TCNT0);
}

static inline void init_pendulum(){
#if defined (__AVR_ATmega328p__)
	TCCR0A = _BV(WGM01) | _BV(WGM00); // CTC;
	TCCR0B = _BV(WGM02) | _BV(CS00) | _BV(CS01); // divide system clock by 64
#elif __AVR_DEVICE_NAME == attiny13
	TCCR0A = _BV(WGM01) | _BV(WGM00); // CTC; 
	TCCR0B = _BV(WGM02) | _BV(CS00) | _BV(CS01); // divide system clock by 64
#endif

	// Set up PWM by OCR0B compare

	// Set Clear Timer on Compare Match (CTC) mode. In this mode, it acts as
	// the oscillator (16 MHz) frequency divider, dividing it by the (OCR0 + 1) value.
	OCR0A = F_CPU_DIV_1777Hz; // 16 MHz / 223 = 2 x 36 kHz 
	OCR0B = 0; // 16 MHz / 223 = 2 x 36 kHz

	TIMSK0 = _BV(OCIE0A) | _BV(OCIE0B); // Enable interrupt
}

#define BEHOLDTV_REMOTECONTROL_ADDRESS 0x61D6
#define HYUNDAI_REMOTECONTROL_ADDRESS 0x8E40

int main(){
//	FILE ir_nec_file = fdev_open_ir_nec(ir_nec_in, IR_NEC_REPEAT_CODES_RESPECT, BEHOLDTV_REMOTECONTROL_ADDRESS, IR_NEC_ADDRESSMODE_EXACT);
//	FILE * ir_nec_in = &ir_nec_file;

#if defined (__AVR_ATmega328p__)
	PCICR = 1 << PCIE0;
	PCMSK0 = 1 << BIT_IR;
#elif __AVR_DEVICE_NAME == attiny13
	GIMSK = 1 << PCIE;
	PCMSK = 1 << BIT_IR;
#endif

	DDR_PWM |= 1 << BIT_PWM;
	DDRB |= 1 << 1;

	sei();
	
	init_pendulum();
	while (1){
		_delay_ms(50);
		PORTB ^= (1 << 1);
		//uint8_t command = (uint8_t) ir_nec_getchar(ir_nec_in);
		uint8_t command = 123;
		int8_t pwm = 9;
		switch(command){
		case 0:
			OCR0B = 0;
			mute = 1;
			continue;
		case 0x80:
			pwm = 1;
			break;
		case 0x40:
			pwm = 2;
			break;
		case 0xc0:
			pwm = 3;
			break;
		case 0x20:
			pwm = 4;
			break;
		case 0xa0:
			pwm = 5;
			break;
		case 0x60:
			pwm = 6;
			break;
		case 0xe0:
			pwm = 7;
			break;
		case 0x10:
			pwm = 8;
			break;
		case 0x90:
			pwm = 9;
			break;
		default:
			continue;
		}
		mute = 0;
		OCR0B = (F_CPU_DIV_1777Hz / 10) * pwm; // PWM
	}
	return 0;
}
