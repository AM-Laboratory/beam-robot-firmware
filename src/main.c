#define F_CPU 16000000UL
#define BAUD 9600

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


#include "uart.h"
#include "uart.c"


#define PORT_IR PINB
#define PIN_IR 3

#if __AVR_DEVICE_NAME__ != atmega328
#error "Devices other than atmega328p not supported"
#endif


ISR(PCINT0_vect){
	/* Logical pin change interrupt vector. */
	sei(); // Enable nested interrupts
	if (((PORT_IR >> PIN_IR) & 1) == 0){
		ir_nec_process_pin_change();
	}
}


uint8_t supertimer = 0xFF;

inline void tick();

ISR(TIMER0_COMPA_vect){
	tick();
/* Timer reached value COMPA interrupt vector. */
/* The timer generates a TIMER_FREQ Hz internal signal, which carries two functions:
 * 1. PWM for the motor;
 * 2. At the end of each period, i.e., COMPA value reached by the timer, this interrupt vector is executed,
 * which triggers all actions: secondary counter increments and IR readout, ADC readout, etc.
 */
}

inline void tick(){

	software_timer_tick((uint16_t) TCNT0);
//	supertimer++;
//	if (supertimer % MULTIPLE_1777Hz == 0) {
//		ir_nec_tick();
//	}
}

static inline void init_pendulum(){
//	TCCR0A = _BV(WGM01) | _BV(COM0B1); // CTC; Set OC0B on Compare Match
	TCCR0A = _BV(WGM01) | _BV(WGM00) | _BV(COM0B1); // CTC; Set OC0B on Compare Match
	TCCR0B = _BV(WGM02) | _BV(CS00) | _BV(CS01); // divide system clock by 64

	// Set up PWM by OCR0B compare

	// Set Clear Timer on Compare Match (CTC) mode. In this mode, it acts as
	// the oscillator (16 MHz) frequency divider, dividing it by the (OCR0 + 1) value.
	OCR0A = 140; // 16 MHz / 223 = 2 x 36 kHz 
	OCR0B = 0; // 16 MHz / 223 = 2 x 36 kHz

	TIMSK0 = _BV(OCIE0A); // Enable interrupt
}

#define BEHOLDTV_REMOTECONTROL_ADDRESS 0x61D6
#define HYUNDAI_REMOTECONTROL_ADDRESS 0x8E40

int main(){
	FILE ir_nec_file = fdev_open_ir_nec(ir_nec_in, IR_NEC_REPEAT_CODES_RESPECT, BEHOLDTV_REMOTECONTROL_ADDRESS, IR_NEC_ADDRESSMODE_EXACT);
	FILE * ir_nec_in = &ir_nec_file;

	DDRD = (1 << 5); // Enable PWM out
	DDRB = (1 << 5) | (1 << 4);
	PCICR = 1 << PCIE0;
	PCMSK0 = 1 << PIN_IR;

	sei();
	
	uart_init(_BV(TXEN0));
	stdout = &uart_out;
	printf("Hello!\n");
	init_pendulum();
	while (1){
		_delay_ms(50);
		uint8_t command = (uint8_t) ir_nec_getchar(ir_nec_in);
		printf("Received command %d: %x\r\n", command, (uint8_t) command);
		int8_t pwm = 0;
		switch(command){
		case 0:
			pwm = 0;
			break;
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
		OCR0B = 14 * pwm; // PWM
		printf("Set OCR0B: %d/140\r\n", OCR0B);
	}
	return 0;
}
