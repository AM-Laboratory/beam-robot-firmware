// A simple variable-PWM robot on ATTiny13
#define F_CPU 1200000UL

#include <stdio.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include <avr/sfr_defs.h>
#include <avr/eeprom.h>
#include <util/delay.h>

#define ADMUX_PB2 1
#define ADMUX_PB3 3
#define ADMUX_PB4 2
#define ADMUX_PB5 0


#define BIT_PWM 1
#define BIT_LED 2
#define BIT_IR 3

#define BIT_ADC 4
#define ADMUX_BIT ADMUX_PB4

volatile uint8_t ir_receiving_bits_flag = 0;
volatile uint8_t prev_TCNT0 = 0;
volatile uint8_t ir_received_bits_count = 0;
volatile uint32_t ir_shift_register = 0;

volatile uint8_t battery_ok;

// The addresses that the remote controls send
#define BEHOLDTV_REMOTECONTROL_ADDRESS 0x61D6
#define HYUNDAI_REMOTECONTROL_ADDRESS 0x8E40
#define REXANT_TOSHIBA_REMOTECONTROL_ADDRESS 0x02FD

#define pwm_stop() { \
PORTB &= ~(1 << BIT_PWM); \
DDRB &= ~(1 << BIT_PWM); \
}

#define pwm_start() { \
PORTB |= (1 << BIT_PWM); \
DDRB |= (1 << BIT_PWM); \
}

#define adc_fire_once(){ \
	ADCSRA |= (1 << ADSC); \
	loop_until_bit_is_set(ADCSRA, ADIF); \
}

#define led_on() { \
	PORTB |= _BV(BIT_LED); \
}

#define led_off() { \
	PORTB &= ~_BV(BIT_LED); \
}

// Battery level ADC reading, calculated as follows:
// ADC = 1024 * Vbatt * R1 / (R1 + R2) / Vref,
// where R1 = 680 Ohm, R2 = 3300 Ohm, Vref = 1.1 V.
#define BATTERY_CRITICAL	524	// 3.3 V
#define BATTERY_IDLE_LOW	572	// 3.6 V
#define BATTERY_IDLE_MEDIUM	620	// 3.9 V
#define BATTERY_IDLE_HIGH	668	// 4.2 V

#define check_battery_health() { \
	if (ADCW >= BATTERY_CRITICAL) { \
		pwm_stop(); \
		battery_ok = 0; \
	} \
}




void measure_and_show_battery_idle_voltage() {
	adc_fire_once();

	// Blink once for low level, twice for med, and three times for high
	uint8_t blinks = 0;
	if (ADCW < BATTERY_IDLE_LOW) {
		blinks++;
	}
	if (ADCW < BATTERY_IDLE_MEDIUM) {
		blinks++;
	}
	if (ADCW < BATTERY_IDLE_HIGH) {
		blinks++;
	}
	while(blinks > 0){
		led_on();
		_delay_ms(400);
		led_off();
		_delay_ms(400);
		blinks--;
	}
}

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
	if(period > 210){
		// 9000 us + 4500 us = leading pulse (approx. 250 clock cycles)
		// Start receiving bits.
		// Clear the shift register and set the flag.
		ir_receiving_bits_flag = 1;
		ir_received_bits_count = 0;
		ir_shift_register = 0;
		// Turn on LED to show that the code is received now.
		led_off();
//	ADCSRA &= ~(1 << ADIE);
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
			led_on();
//	ADCSRA |= (1 << ADIE);
		}

		// All 32 bits have successfully been received.
		if(ir_received_bits_count == 32){
			ir_receiving_bits_flag = 0;

			// Turn off LED to show that the code has been received
			led_on();

//		ADCSRA |= (1 << ADIE);
			// Process received code
			uint8_t command = (uint8_t) (ir_shift_register >> 8);

			// These checks don't work anyway, so we skip them.

			// Remote control device selectivity
			if ((ir_shift_register >> 16) != REXANT_TOSHIBA_REMOTECONTROL_ADDRESS){
				// If this code is not from our remote control, do nothing
				return;
			}
			// Correct command verification
			if (((uint8_t) command) != ((uint8_t) ~((uint8_t) ir_shift_register))){
				// Command does not match its logical inverse which is
				// sent after it, so this is a malformed command.
				// Ignore it silently.
				return;
			}
			
			
			switch(command){
			// Button POWER
			case 0x48:
				OCR0B = 0; // 0% PWM
				pwm_stop();
				measure_and_show_battery_idle_voltage();
				break;
			// Button 1
			case 0x80:
				OCR0B = 26; // 10% PWM
				pwm_start();
				break;
			// Button 2
			case 0x40:
				OCR0B = 51; // 20% PWM
				pwm_start();
				break;
			// Button 3
			case 0xc0:
				OCR0B = 77; // 30% PWM
				pwm_start();
				break;
			// Button 4
			case 0x20:
				OCR0B = 102; // 40% PWM
				pwm_start();
				break;
			// Button 5
			case 0xa0:
				OCR0B = 127; // 50% PWM
				pwm_start();
				break;
			// Button 6
			case 0x60:
				OCR0B = 153; // 60% PWM
				pwm_start();
				break;
			// Button 7
			case 0xe0:
				OCR0B = 179; // 70% PWM
				pwm_start();
				break;
			// Button 8
			case 0x10:
				OCR0B = 204; // 80% PWM
				pwm_start();
				break;
			// Button 9
			case 0x90:
				OCR0B = 230; // 90% PWM
				pwm_start();
				break;
			// Button 0
			case 0x0:
				OCR0B = 255; // 100% PWM
				pwm_start();
				break;
			}
		}
	}
	// Ignore any other pulses
}

/* Timer0 Overflow ISR. It is empty and only serves to clear the interrupt
 * flag. If we do not clear this flag, the ADC, which is fired by it, will only
 * fire once instead of triggering on each timer overflow.
 */
ISR(TIM0_OVF_vect){}

/* 
 * ADC measurement complete Interrupt service routine. We only use the ADC to
 * estimate the battery level.
 */
ISR(ADC_vect){
	check_battery_health();
}

int main(){
	// Start up the ADC
	// Select the source of the Analog signal;
	// set Internal 1.1 V voltage reference
	ADMUX = ADMUX_BIT | (1 << REFS0);
	// Do NOT disable any Digital inputs to favor the Analog input.
	DIDR0 = 0x00;
	// Pin PB3 will be read as both analog and digital input.

	// Enable the Analog-Digital converter in the Single Conversion mode; also enable
	// the ADC conversion finished Interrupt.
	// Set the frequency to 1/16 of the CPU frequency (< 200 kHz) to ensure
	// 10 bit conversion.
	ADCSRA = (1 << ADEN) | 4;
	// Also let it run once, to initialize.
	adc_fire_once();

	// Enable Write on the LED pin
	DDRB |= 1 << BIT_LED;

	battery_ok = 1;
	measure_and_show_battery_idle_voltage();
	check_battery_health();

	if(battery_ok) {
		// Enable Pin Change Interrupt
		GIMSK = 1 << PCIE;
		// Select only pin BIT_IR for Pin Change Interrupt
		PCMSK = 1 << BIT_IR;


		// Enable Write on the PWM pin
		DDRB |= 1 << BIT_PWM;
		
		// Init Timer/Counter for PWM generation and IR pulse decoding.
		// Set Fast PWM mode with generation of a Non-inverting signal on pin OC0B.
		// The 8-bit clock counts from 0 to 255 and starts again at zero. When
		// it encounters the value OCR0B, it clears the OC0B bit, and sets it
		// high again when the counter is restarted from zero.
		TCCR0A = _BV(WGM01) | _BV(WGM00) | _BV(COM0B1);
		// divide system clock by 64 (this gives 18.34 kHz), approx. 54 us per tick.
		TCCR0B = 3;

		// Do a quick self-test: briefly turn on the motor to full power and measure
		// the loaded supply voltage.
		OCR0B = 255;
		pwm_start();

		_delay_ms(50);

		check_battery_health();

		pwm_stop();

		// Set the Timer Overflow (i.e., the moment when the PWM opens the transistor)
		// as the trigger event to start the voltage measurement.
		ADCSRB = 1 << ADTS2;
		ADCSRA |= (1 << ADATE);

		TIMSK0 |= (1 << TOIE0);

		// Enable the ADC Complete Interrupt, which is used to check the battery health.
		ADCSRA |= (1 << 3);


	//	uint8_t test[4] = {0xca, 0xfe, 0xba, 0xbe};
	//	eeprom_write_block(test, 0, 4);		

		_delay_ms(1000);
		led_on();
		// We are ready to go, set Global Enable interrupts
		sei();
	}
	while (battery_ok){
		// Work
		_delay_ms(1000);
		// Every second reset the IR receiver
		ir_receiving_bits_flag = 0;
		led_on();
		ADCSRA |= (1 << ADIE);
	}

	// Sleep mode
	// Stop the PWM
	pwm_stop();
	// Disable all interrupts
	GIMSK = 0;
	// Disable all analog inputs
	ADCSRA = 0;
	// Turn off the LED
	led_off();
	// Do nothing but briefly blink the LED forever
	while (1){
		_delay_ms(3000);
		led_on();
		_delay_ms(50);
		led_off();
	}
	return 0;
}
