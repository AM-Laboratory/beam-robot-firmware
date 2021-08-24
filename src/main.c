#define F_CPU 16000000UL
#define BAUD 9600

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <avr/interrupt.h>
#include <avr/sfr_defs.h>
#include <avr/sfr_defs.h>

#include <util/delay.h>

#include "ir_nec.h"
#include "uart.h"


#if __AVR_DEVICE_NAME__ != atmega328
#error "Devices other than atmega328p not supported"
#endif


#define BEHOLDTV_REMOTECONTROL_ADDRESS 0x61D6
#define HYUNDAI_REMOTECONTROL_ADDRESS 0x8E40

int main(){
	uart_init(_BV(TXEN0));
	FILE uart_out = FDEV_SETUP_STREAM((void*) uart_putchar, NULL, _FDEV_SETUP_WRITE);
	FILE uart_in = FDEV_SETUP_STREAM(NULL, uart_getchar, _FDEV_SETUP_READ);
DDRD = (1 << 5); // Enable PWM out
DDRB = (1 << 5) | (1 << 4);


	stdout = &uart_out;
	printf("Hello!\n");

	float owi_clock_freq = 256.0 / F_CPU;
	int result = owi_set_clock(4, float_to_pulsewidth(owi_clock_freq));
	int result2 = ir_nec_input_setup();
	printf("setup: %d, %d\r\n", result, result2);

	FILE ir_nec_file = fdev_open_ir_nec(ir_nec_in, IR_NEC_REPEAT_CODES_RESPECT, BEHOLDTV_REMOTECONTROL_ADDRESS, IR_NEC_ADDRESSMODE_EXACT);
	FILE * ir_nec_in = &ir_nec_file;

	sei();
	
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
