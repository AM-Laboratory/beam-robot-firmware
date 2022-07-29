// A simple variable-PWM robot on ATTiny13
#define F_CPU 1200000UL

#include <stdio.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include <avr/sfr_defs.h>
#include <avr/eeprom.h>
#include <util/delay.h>

// IR remote control address and command codes are defined here
#include "ir_remote_control_codes.h"

// ADC multiplexer constants, as defined by the ATTiny13A docs, to select the
// pin to connect the ADC to
#define ADMUX_PB2 1
#define ADMUX_PB3 3
#define ADMUX_PB4 2
#define ADMUX_PB5 0

// MCU pin functions, as defined by the bot electrical circuit diagram
// Motor PWM on PB1. Note that this pin is also used as OCR0B, the
// Timer/Counter output, so changing this will break the PWM.
#define BIT_PWM 1
// Signalling LED on PB2
#define BIT_LED 2
// IR receiver on PB3
#define BIT_IR 3
// Voltage divider to measure the battery voltage on PB4
#define BIT_ADC 4
#define ADMUX_BIT ADMUX_PB4

// A flag indicating that receive of 32 command bits from the remote control
// has been initiated and not yet finished
volatile uint8_t ir_receiving_bits_flag = 0;
// Received bits count, from 0 to 32
volatile uint8_t ir_received_bits_count = 0;
// A shift register to store the 32 sequentially received bits, MSB received
// first
uint32_t ir_shift_register = 0;

// Previous value of the 8-bit timer/counter TCNT0
volatile uint8_t prev_TCNT0 = 0;

// We use the difference between the current timer/counter value and its
// previous value to measure the pulse widths. This allows correct measurement
// even if a timer overflow has happened once, but not twice. We therefore use
// the second overflow as a trigger to hang up the IR receive.
volatile uint8_t timer_overflow_twice;

// Disable PWM output and pull it low
#define pwm_stop() { \
	PORTB &= ~(1 << BIT_PWM); \
	DDRB &= ~(1 << BIT_PWM); \
}

#define pwm_start() { \
	PORTB |= (1 << BIT_PWM); \
	DDRB |= (1 << BIT_PWM); \
}

// Launch the ADC once and wait for it to finish (synchronous).
// The 10-bit reading will be stored in the 16-bit register ADCW.
#define adc_fire_once(){ \
	ADCSRA |= (1 << ADSC); \
	loop_until_bit_is_set(ADCSRA, ADIF); \
}

#define led_on() { \
	PORTB |= 1 << BIT_LED; \
}

#define led_off() { \
	PORTB &= ~(1 << BIT_LED); \
}

// = 0 if battery voltage has fallen down to critical discharge and
// 1 otherwise.
volatile uint8_t battery_status_critical;

// Battery level ADC reading, calculated as follows:
//   ADC = (1024 * Vbatt * R1 / (R1 + R2) / Vref),
// where R1 = 680 Ohm, R2 = 3300 Ohm, Vref = 1.1 V.
// We configure the ADC for left-aligned 10-bit-in-uint16 storage, therefore
//   ADCW = ADC << 6,
//   ADCH = ADC >> 2.
// Therefore,
// -------------------------------------
// | Bat. level | Voltage | ADC | ADCH |
// | critical   | 3.3 V   | 524 | 131  |
// | low        | 3.6 V   | 572 | 143  |
// | medium     | 3.9 V   | 620 | 155  |
// | full       | 4.2 V   | 668 | 167  |
// -------------------------------------
// The space between the levels is 12.
#define BATTERY_CRITICAL	131	// 3.3 V
#define BATTERY_LEVEL_SPACING	12	// 0.3 V

#define update_battery_status() { \
}
//	if (ADCH <= BATTERY_CRITICAL) { \
//		pwm_stop(); \
//		battery_status_critical = 1; \
//	} \
//}

// Measure the battery voltage and indicate it by blinking the signal LED
// several times: once for low level, twice for med, and three times for high
void measure_and_show_battery_idle_voltage() {
	adc_fire_once();

	// This value is positive if level is >= low (3.6 V)
	int8_t battery_level = ADCH - (BATTERY_CRITICAL + BATTERY_LEVEL_SPACING);
	while(battery_level >= 0){
		led_on();
		_delay_ms(400);
		led_off();
		_delay_ms(400);
		battery_level -= BATTERY_LEVEL_SPACING;
	}
}

/* Logical pin change interrupt vector. We use it to decode the IR remote
 * control codes, as defined by the NEC protocol. */
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
	timer_overflow_twice = 0;
	
	// The NEC code consists of a (9000 us negative + 4500 us positive)
	// leading pulse, and 32 pulse-period-modulated bits which can be
	// either
	// (560 us pos. + 1680 us neg.) for logical 1, or
	// (560 us pos. + 560 us neg.) for logical 0.
	// An IR remote control also sends repeat codes if the key is held
	// pressed, but we ignore them here.
	if(period > 210 || period < 15){
		// 9000 us + 4500 us = leading pulse (approx. 250 clock cycles)
		// Start receiving bits.
		// Clear the shift register and set the flag.
		ir_receiving_bits_flag = 1;
		ir_received_bits_count = 0;
		ir_shift_register = 0;
		// Turn on LED to show that the code is received now.
		led_off();
	} else if(ir_receiving_bits_flag){
		uint8_t new_bit;
		if(period > 15 && period < 25){
		// 560 us + 560 us (approx. 20 clock cycles) = logical 0
			new_bit = 0;
		} else if (period > 35 && period < 50){
		// 560 us + 1680 us (approx. 40 clock cycles) = logical 1
			new_bit = 1;
		} else {
		// If received anything else, this is an error, stop receiving
		// bits.
			ir_receiving_bits_flag = 0;
			led_on();
		}
		ir_shift_register = (ir_shift_register << 1) | new_bit;
		ir_received_bits_count++;

		// All 32 bits have successfully been received. They consist of
		// 16 address bits (which may, in turn, consist of a 8 bit
		// address followed by its logical inversion, but this is not
		// always the case) followed by a 8-bit command, which is in
		// turn followed by its logical inversion. We decode this here.
		// We first verify that the address is correct (the command is
		// from our remote control, i.e., directed to our bot, not to
		// an air conditioner nor a projector), and then verify that
		// ~command == command_logical_inverse.
		//
		// Then, if everything is correct, we execute the action
		// corresponding to the command immediately.
		if(ir_received_bits_count == 32){
			// Clear the flag and turn off LED to show that the
			// code has been received
			ir_receiving_bits_flag = 0;
			led_on();

			// Remote control device selectivity
			if ((ir_shift_register >> 16) != REMOTECONTROL_ADDRESS){
				// If this code is not from our remote control,
				// do nothing
				return;
			}

			// Squeeze the command out of the 32-bit code
			uint8_t command = (uint8_t) (ir_shift_register >> 8);
			uint8_t not_not_command = (uint8_t) ~((uint8_t) ir_shift_register);

			// Verify that the command is correct
			if (command != not_not_command) {
				// Command does not match its logical inverse
				// which is sent after it, so this is a
				// malformed command.  Ignore it silently.
				return;
			}
			
			
			// Process commands corresponding to different remote
			// control buttons. Buttons Power and 0-9 are used,
			// with Power corresponding to motor power off, 1-9
			// corresponding to PWM 10-90% and 0 corresponding to
			// full power (100% PWM).
			uint8_t i;
			for (i = 0; i < sizeof(IR_REMOTE_CONTROL_BUTTONS) / sizeof(ir_button_t); i++){
				ir_button_t button = IR_REMOTE_CONTROL_BUTTONS[i];
				if(command == button.command){
					OCR0B = button.pwm_duty_cycle;
					if(button.pwm_duty_cycle){
						pwm_start();
					} else {
						pwm_stop();
						//measure_and_show_battery_idle_voltage();
					}
				}
			}
		}
	}
	// Ignore any other pulses
}

/* Timer0 Overflow ISR. This overflow serves two purposes: 
 * 1. Clear the interrupt flag. If we do not clear this flag, the ADC, which is
 * fired by it, will only fire once instead of triggering on each timer
 * overflow.
 * 2. Hang up the IR transmission if the timer has overflowed twice since the
 * last bit has been received.  We use the difference between the current
 * timer/counter value and its previous value to measure the pulse widths. This
 * allows correct measurement even if a timer overflow has happened once, but
 * not twice. We therefore use the second overflow as a trigger to hang up the
 * IR receive, to avoid locking it up in the case an incomplete receive has happened.
 */
ISR(TIM0_OVF_vect){
	if(timer_overflow_twice){
		// Reset the IR receiver
		ir_receiving_bits_flag = 0;
		led_on();
	}
	timer_overflow_twice++;
}

/* 
 * ADC measurement complete Interrupt service routine. We only use the ADC to
 * estimate the battery level.
 */
ISR(ADC_vect){
	update_battery_status();
}

int main(){
	// Start up the ADC:
	{
		// - select the source of the Analog signal;
		// - set Internal 1.1 V voltage reference.
		ADMUX = ADMUX_BIT | (1 << REFS0);

		// - enable the Analog-Digital converter in the Single Conversion mode; 
		// - set the frequency to 1/16 of the CPU frequency (< 200 kHz) to
		// ensure 10 bit conversion.
		ADCSRA = (1 << ADEN) | 4;

		// Also let it run once, to initialize.
		adc_fire_once();
	}

	// Enable Write on the LED pin
	DDRB |= 1 << BIT_LED;


	// Run the initial idle battery check. Indicate the result, and jump
	// straight to the power-saving mode if the battery level is below
	// critical.
	{
		// Initialize as not critical
		battery_status_critical = 0;

		//measure_and_show_battery_idle_voltage();

		// battery_status_critical is updated here to 1 if the voltage
		// is below critical.
		update_battery_status();
	}
	if(!battery_status_critical) {
	// Run the startup sequence, or skip it going directly to the
	// power-saving mode if the battery voltage is below the critical
	// level.

		// Enable Pin Change Interrupt which we use to process the IR
		// remote control codes.
		GIMSK = 1 << PCIE;

		// Select only pin BIT_IR for Pin Change Interrupt
		PCMSK = 1 << BIT_IR;


		// Enable Write on the PWM pin
		DDRB |= 1 << BIT_PWM;
	

		// Init Timer/Counter for PWM generation and IR pulse decoding:
		// Set Fast PWM mode with generation of a Non-inverting signal
		// on pin OC0B, which is the same pin as PB1 aka PWM pin.
		// The 8-bit clock counts from 0 to 255 and starts again at zero. When
		// it encounters the value OCR0B, it clears the OC0B bit, and sets it
		// high again when the counter is restarted from zero.
		//
		// The Timer/Counter serves two purposes at the same time.
		// First, it is used to drive the PWM on the OC0B (PB1) pin.
		// Second, it is used to measure the pulse widths for the
		// pulse-period demodulation to decode the IR remote control
		// signals. To measure the pulse lengths, we read the
		// Timer/Counter value and store it in a variable. By
		// calculating the difference between the current and the
		// previous readings, we may evaluate the pulse period. As we
		// carefully select the Timer/Counter frequency to 18.34 kHz
		// (54 us per tick), pulse widths from 54 us to 14 ms can be
		// measured. The NEC IR protocol uses pulse widths from 560 us
		// to 9 ms. We also use the Timer overflow interrupt to hang up
		// the IR code receive as soon as the timer overflows for the
		// second time (i.e., 28 ms after the last pulse has been
		// transmitted).
		{
			// - set Fast PWM mode with 0xFF as TOP;
			// - set Clear OC0B on Compare Match.
			TCCR0A = ((1 << WGM01) | (1 << WGM00)) | (1 << COM0B1);

			// Divide system clock by 64 (this gives 18.34 kHz),
			// approx. 54 us per tick.
			// - set 64 as system clock divider
			TCCR0B = 3;
		}

		// Do a quick self-test: briefly turn on the motor to full
		// power and measure the loaded supply voltage.
		{
			OCR0B = 255; // PWM 100% duty cycle
			pwm_start();

			_delay_ms(50);

			adc_fire_once();
			update_battery_status();

			pwm_stop();
		}

		// Set the Timer Overflow (i.e., the moment when the PWM opens
		// the transistor) as the trigger event to start the voltage
		// measurement.
		{
			// - set Timer/Counter Overflow as the ADC Auto Trigger
			// Source
			ADCSRB = (1 << ADTS2);

			// - set ADC Auto Trigger Enable
			ADCSRA |= (1 << ADATE);

			// - set Timer/Counter Overflow Interrupt Enable
			TIMSK0 |= (1 << TOIE0);
			// The corresponding interrupt vector is declared but
			// empty, and is only used to clear the interrupt flag.

		}
		// - set Enable the ADC Complete Interrupt
		ADCSRA |= (1 << ADIE);
		// update_battery_status() is called at the corresponding
		// interrupt vector.

		// Introduce a one-second delay before becoming responsive
		_delay_ms(1000);

		// The LED is constantly shining to indicate the bot working
		// correctly, and blinked to indicate that something is
		// happening.
		led_on();

		// We are ready to go, set Global Enable interrupts
		sei();
	}
	while (!battery_status_critical){
	// Main loop, normally running forever. It is only broken out of if the
	// battery level falls below critical. The PWM and IR remote control
	// command receives run asynchronously, so we do nothing but wait
	// forever here.
		_delay_ms(1000);
	}

	// Power-saving mode. It is entered if the battery voltage falls below
	// critical (around 3.3 V) at any point, and is used to prevent the
	// battery overdischarge leading to quick battery deterioration. In
	// this mode the normal operation is suspended, the motor is stopped
	// and the bot becomes unresponsive to all inputs. To indicate that the
	// bot is still powered on (in the sense that the power switch on the
	// PCB is in the closed position), the LED is briefly blinked once
	// every 3 seconds.
	{
		pwm_stop();

		// Disable all interrupts
		cli();
		// Stop the Timer/Counter
		TCCR0B = 0;
		// Disable all analog inputs
		ADCSRA = 0;
		// Turn off the LED
		led_off();
	}
	while (1){
	// Do nothing but briefly blink the LED forever
		_delay_ms(3000);
		led_on();
		_delay_ms(50);
		led_off();
	}
	return 0;
}
