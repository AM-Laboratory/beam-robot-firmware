#define F_CPU 16000000UL
#define BAUD 9600

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <avr/interrupt.h>
#include <avr/sfr_defs.h>
#include <avr/sfr_defs.h>

#include <util/delay.h>


//#include "lib/ir_nec.h"
//#include "lib/ir_nec.c"


#include "uart.h"
#include "twi.h"
#include "bq27421_g1.h"


#define PIN_IR PINB
#define BIT_IR 3

#if !defined (__AVR_ATmega328p__)
#warning "Devices other than atmega328p not supported"
#endif


#define PAGE_SIZE 8
// This should work with PAGE_SIZE <= 16, but for some reason only works for PAGE_SIZE <= 8!!


#define SLAVE (0xA0 >> 1)
int main(){
	FILE uart_out = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);
	FILE uart_in = FDEV_SETUP_STREAM(NULL, uart_getchar, _FDEV_SETUP_READ);

	DDRB = (1 << 5) | (1 << 4);
	sei();
	
	uart_init(_BV(TXEN0));
	stdout = &uart_out;
	tw_init(TW_FREQ_100K, 1);
	printf("Hello!\n");
	printf("Requesting temperature\n");
	uint16_t temperature = bq27421_read_two_byte_data(BQ27421_G1_COMMAND_Temperature);
	printf("Temperature: %d x100 mK\r\n", temperature);
	printf("Requesting voltage\n");
	uint16_t temperature2 = bq27421_read_two_byte_data(BQ27421_G1_COMMAND_Voltage);
	printf("Voltage: %d mV\r\n", temperature2);

	printf("Requesting Device type\n");
	uint16_t devtype = bq27421_control(BQ27421_G1_CONTROL_DEVICE_TYPE);
	printf("Device type: %04x \r\n", devtype);
	printf("Requesting chem type\n");
	uint16_t devtype2 = bq27421_control(BQ27421_G1_CONTROL_CHEM_ID);
	printf("chem type: %04x \r\n", devtype2);
	return 0;
}
