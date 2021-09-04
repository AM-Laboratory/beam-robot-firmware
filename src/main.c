// A simple variable-PWM robot on ATTiny13
#define F_CPU 1200000UL

#include <stdio.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include <avr/sfr_defs.h>
#include <avr/eeprom.h>
#include <util/delay.h>

#define BIT_IR 3
#define BIT_PWM 1
#define BIT_LED 2

volatile uint8_t ir_receiving_bits_flag = 0;
volatile uint8_t prev_TCNT0 = 0;
volatile uint8_t ir_received_bits_count = 0;
volatile uint32_t ir_shift_register = 0;

// The addresses that the remote controls send
#define BEHOLDTV_REMOTECONTROL_ADDRESS 0x61D6
#define HYUNDAI_REMOTECONTROL_ADDRESS 0x8E40


/* Logical pin change interrupt vector. */
ISR(PCINT0_vect){
	if((PINB >> BIT_IR) & 1){
		// Only trigger on falling edge
		return;
	}

	// Read the 8-bit Timer/Counter and calculate the time difference with
	// the previous falling edge. This allows measuring periods from 54 us
	// to 14 ms.
	uint8_t period = TCNT0 - prev_TCNT0;
	prev_TCNT0 = TCNT0;
	
	// The NEC code consists of a 9000 us negative + 4500 us positive = leading pulse,
	// and 32 pulse-period-modulated bits which can be either
	// 560 us pos. + 1680 us neg. for logical 1, or
	// 560 us pos. + 560 us neg. for logical 0.
	// An IR remote control also sends repeat codes if the key is held
	// pressed, but we ignore them here.
	if(period > 240){
		// 9000 us + 4500 us = leading pulse (approx. 250 clock cycles)
		// Start receiving bits.
		// Clear the shift register and set the flag.
		ir_receiving_bits_flag = 1;
		ir_received_bits_count = 0;
		ir_shift_register = 0;
		// Turn on LED to show that the code is received now.
		PORTB |= _BV(BIT_LED);
	} else if(ir_receiving_bits_flag){
		if(period > 15 && period < 25){
			// 560 us + 560 us (approx. 20 clock cycles) = logical 0
			ir_shift_register <<= 1;
			ir_received_bits_count++;
		} else if (period > 35 && period < 45){
			// 560 us + 1680 us (approx. 40 clock cycles) = logical 1
			ir_shift_register <<= 1;
			ir_shift_register |= 1;
			ir_received_bits_count++;
		} else {
			// If received anything else, this is an error, stop receiving bits.
			ir_receiving_bits_flag = 0;
		}

		// All 32 bits have successfully been received.
		if(ir_received_bits_count == 32){
			ir_receiving_bits_flag = 0;

			// Turn off LED to show that the code has been received
			PORTB &= ~_BV(BIT_LED);

			// Process received code
			uint8_t command = (uint8_t) (ir_shift_register >> 8);

//			// These checks don't work anyway, so we skip them.
//
//			// Remote control device selectivity
//			if ((ir_shift_register >> 16) != BEHOLDTV_REMOTECONTROL_ADDRESS){
//				// If this code is not from our remote control, do nothing
//				return;
//			}
//			// Correct command verification
//			if (command != ~((uint8_t) ir_shift_register)){
//				// Command does not match its logical inverse which is
//				// sent after it, so this is a malformed command.
//				// Ignore it silently.
//				return;
//			}
			
			
			switch(command){
			// Button POWER
			case 0x48:
				OCR0B = 0; // 0% PWM
				break;
			// Button 1
			case 0x80:
				OCR0B = 26; // 10% PWM
				break;
			// Button 2
			case 0x40:
				OCR0B = 51; // 20% PWM
				break;
			// Button 3
			case 0xc0:
				OCR0B = 77; // 30% PWM
				break;
			// Button 4
			case 0x20:
				OCR0B = 102; // 40% PWM
				break;
			// Button 5
			case 0xa0:
				OCR0B = 127; // 50% PWM
				break;
			// Button 6
			case 0x60:
				OCR0B = 153; // 60% PWM
				break;
			// Button 7
			case 0xe0:
				OCR0B = 179; // 70% PWM
				break;
			// Button 8
			case 0x10:
				OCR0B = 204; // 80% PWM
				break;
			// Button 9
			case 0x90:
				OCR0B = 230; // 90% PWM
				break;
			// Button 0
			case 0x0:
				OCR0B = 255; // 100% PWM
				break;
			}
		}
	}
	// Ignore any other pulses
}

int main(){
	// Enable Pin Change Interrupt
	GIMSK = 1 << PCIE;
	// Select only pin BIT_IR for Pin Change Interrupt
	PCMSK = 1 << BIT_IR;

	// Open PWM and LED bits for writing
	DDRB |= 1 << BIT_PWM;
	DDRB |= 1 << BIT_LED;

	// Global Enable interrupts
	sei();
	
	// Init Timer/Counter for PWM generation and IR pulse decoding.
	// Set Fast PWM mode with generation of a Non-inverting signal on pin OC0B.
	// The 8-bit clock counts from 0 to 255 and starts again at zero. When
	// it encounters the value OCR0B, it clears the OC0B bit, and sets it
	// high again when the counter is restarted from zero.
	TCCR0A = _BV(WGM01) | _BV(WGM00) | _BV(COM0B1);
	// divide system clock by 64 (this gives 18.34 kHz), approx. 54 us per tick.
	TCCR0B = 3;
	
//	uint8_t test[4] = {0xca, 0xfe, 0xba, 0xbe};
//	eeprom_write_block(test, 0, 4);		
	while (1){
		_delay_ms(250);
	}
	return 0;
}
