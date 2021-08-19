#include <stdio.h>
#include <avr/io.h>

int uart_putchar(char data, FILE * stream){
#ifdef CRLF
	if(data == '\n')
		uart_putchar('\r', stream);
#endif
	loop_until_bit_is_set(UCSR0A, UDRE0);
	UDR0 = data;
	return 0;
}

char uart_getchar(FILE * stream){
	loop_until_bit_is_set(UCSR0A, RXC0);
#ifdef NO_CR
	if(UDR0 == '\r')
		return '\n';
#endif
	return UDR0;
}
