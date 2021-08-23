#include <stdio.h>
#include <avr/io.h>

#if defined F_CPU && defined BAUD
	#include <util/setbaud.h>
	#define BAUD_PRESCALLER (((F_CPU / (BAUD * 16UL))) - 1)
	#define uart_init(mask) { \
		UBRR0H = (uint8_t) (BAUD_PRESCALLER >> 8); \
		UBRR0L = (uint8_t) (BAUD_PRESCALLER); \
		UCSR0B = mask; \
		UCSR0C = _BV(UCSZ00) | _BV(UCSZ01); \
	}
#else
	#error F_CPU or BAUD undefined. Can not calculate UBRR0 value.
#endif
int uart_putchar(char data, FILE * stream);
char uart_getchar(FILE * stream);

FILE uart_out = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);
FILE uart_in = FDEV_SETUP_STREAM(NULL, uart_getchar, _FDEV_SETUP_READ);
