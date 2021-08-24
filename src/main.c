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
	uint8_t addr;
	for (addr = 0; addr < 25; addr++){
		uint8_t page_addr = addr << 4;
		uint8_t * data = malloc(PAGE_SIZE + 1);
		data[0] = page_addr;
		printf("Writing %d bytes to address %x: ", PAGE_SIZE, data[0]);
		uint8_t idx;
		for (idx = 1; idx <= PAGE_SIZE; idx++){
			data[idx] = (uint8_t) rand();
			printf("%02x", data[idx]);
		}
		printf("\r\n");

		ret_code_t ret = tw_master_transmit(SLAVE, data, PAGE_SIZE + 1, 0);
		printf("Return code: %d\r\n", ret);

		for (idx = 1; idx <= PAGE_SIZE; idx++){
			data[idx] = 0;
		}
		_delay_ms(500);
		printf("Receiving bytes...\r\n");
		ret = tw_master_transmit(SLAVE, &page_addr, 1, 0);
		printf("Requested address: %x; return code %d\r\n", page_addr, ret);
		ret = tw_master_receive(SLAVE, data, PAGE_SIZE);
		printf("Return code: %d\r\n", ret);
		printf("Received %d bytes from address %x: ", PAGE_SIZE, page_addr);
		for (idx = 0; idx < PAGE_SIZE; idx++){
			printf("%02x", data[idx]);
		}
		printf("\r\n");
		printf("---------------------------------------------------------------\r\n");
		printf("\r\n");
		free(data);
		_delay_ms(1500);
	}
	return 0;
}
