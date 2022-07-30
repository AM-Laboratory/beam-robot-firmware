// Verified to compile correctly with avr-gcc 5.4.0

// The device must run at factory fuses: 9.6 MHz frequency with CKDIV8 enabled,
// therefore the CPU clock must run at 1.2 MHz.
#define F_CPU 1200000UL // this line must be before #include <util/delay.h>!

// We assume 20% accuracy of the CPU frequency, so the Timer/Counter accuracy
// is also assumed to be equal to 20%.
#define F_CPU_ACCURACY_PERCENT 20


#include <stdio.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include <avr/sfr_defs.h>
#include <avr/eeprom.h>
#include <util/delay.h>

// IR remote control address and command codes are defined here
#include "ir_remote_control_codes.h"

// ADC multiplexer constants, as defined by the ATTiny13A docs (ADMUX
// register), to select the ADC listening pin
#define ADC_ON_PB2 1
#define ADC_ON_PB3 3
#define ADC_ON_PB4 2
#define ADC_ON_PB5 0

// MCU pin functions, as defined by the bot electrical circuit diagram
// Motor PWM on PB1. Note that the PWM pin must be OCR0B, the Timer/Counter
// output, so changing this pin will break the PWM.
#define BIT_PWM 1
// Signalling LED on PB2
#define BIT_LED 2
// IR receiver on PB3
#define BIT_IR 3
// Voltage divider to measure the battery voltage on PB4
#define BIT_ADC 4

#define led_on() { \
	PORTB |= 1 << BIT_LED; \
}

#define led_off() { \
	PORTB &= ~(1 << BIT_LED); \
}




// States of the IR pulse-period demodulator state machine.
typedef enum {
	IR_STATE_IDLE,
	// Input is steady positive, waiting for a falling edge that initiates
	// an incoming transmission. This is the default state.

	IR_STATE_LEADING_9000ms,
	// Incoming transmission falling edge has been encountered, a 9 ms
	// negative leading pulse now being received - waiting for the rising
	// edge.

	IR_STATE_LEADING_4500ms,
	// A 9 ms negative leading pulse has been received, a 4.5 ms positive
	// leading pulse now being received - waiting for the falling edge.

	IR_STATE_DATA_BITS
	// 32 data bits being received. Here, we measure full periods (falling
	// edge to falling edge), so the timer is only read on falling edges.
} ir_state_t;

volatile ir_state_t ir_state = IR_STATE_IDLE;

// Reset the IR receiver to the idle state
#define ir_hangup() { \
	ir_state = IR_STATE_IDLE; \
	led_on(); \
}

// Received bits count, from 0 to 32
volatile uint8_t ir_received_bits_count = 0;

// A shift register to store the 32 sequentially received bits, MSB received
// first
uint32_t ir_shift_register = 0;



// Previous value of the 8-bit timer/counter TCNT0
volatile uint8_t previous_TCNT0_value = 0;

// We use the difference between the current timer/counter value and its
// previous value to measure the pulse widths. This allows correct measurement
// even if a timer overflow has happened once, but not twice. We therefore use
// the second overflow as a trigger to hang up the IR receive.
volatile uint8_t timer_overflow_flag;

// Remember the current time as the previous measurement time for future
// measurements. Clear the timer overflow flag, as the previous_TCNT0_value has been
// updated.
#define start_time_interval_measurement() \
	previous_TCNT0_value = TCNT0; \
	timer_overflow_flag = 0

// Return the time_interval - the difference between the current time
// (timer/counter value TCNT0) and the previous measurement time, - and
// remember the current time as the previous measurement time for future
// measurements.
#define get_time_interval_since_last_measurement() \
	TCNT0 - previous_TCNT0_value; \
	start_time_interval_measurement()

// Timer/Counter prescaler. We set the Timer/Counter to run at f = F_CPU / 64.
#define TCNT_PRESCALER	64

// Convert microseconds to Timer/Counter clock cycles at compile time. As the
// CPU frequency deviates significantly from the configured value, we introduce
// a second argument 'error_percent', which is the supposed deviation in
// an integer number of percents. This is used to compute intervals, given by
// the CPU frequency accuracy.
#define usec_to_cycles(time_us, error_percent) \
	(uint8_t) (F_CPU / 1000UL * (100 + (error_percent)) * (time_us) / TCNT_PRESCALER / 1000UL / 100)


// Disable PWM output and pull it low
#define pwm_stop() { \
	PORTB &= ~(1 << BIT_PWM); \
	DDRB &= ~(1 << BIT_PWM); \
}

#define pwm_start() { \
	PORTB |= (1 << BIT_PWM); \
	DDRB |= (1 << BIT_PWM); \
}

// Set the PWM duty cycle in a zero-duty-cycle-friendly manner.
// For a nonzero duty cycle, we make sure that PWM is turned on. If the duty
// cycle is zero, we "stop the PWM", i.e., explicitly write a logical zero into
// the PWM pin, as the least duty cycle supported by the Timer/Counter PWM is
// 1/256.
// For convenient battery level checking, the battery voltage is also indicated
// by LED blinking, if a zero duty cycle has been selected.
#define pwm_set_duty_cycle(duty_cycle) { \
	OCR0B = duty_cycle; \
	if(duty_cycle){ \
		pwm_start(); \
	} else { \
		pwm_stop(); \
		measure_and_show_battery_idle_voltage(); \
	} \
}



// Launch the ADC once and wait for it to finish (synchronous).
// The 10-bit reading will be stored in the 16-bit register ADCW.
#define adc_fire_once(){ \
	ADCSRA |= (1 << ADSC); \
	loop_until_bit_is_set(ADCSRA, ADIF); \
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

#define ensure_battery_level_above_critical() { \
	if (ADCH <= BATTERY_CRITICAL) { \
		pwm_stop(); \
		battery_status_critical = 1; \
	} \
}

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
	// The NEC code consists of a (9000 us negative + 4500 us positive)
	// leading pulse pair, and 32 pulse-period-modulated bits which can be
	// either
	// (560 us pos. + 1680 us neg.) for logical 1, or
	// (560 us pos. + 560 us neg.) for logical 0.
	// An IR remote control also sends repeat codes if the key is held
	// pressed, but we ignore them here.
	uint8_t is_rising_edge = ((PINB >> BIT_IR) & 1);
	switch(ir_state){
	case IR_STATE_IDLE:
		if(!is_rising_edge){
			// Only trigger on falling edge.
			// Do nothing, just reset the timer and change into the
			// next state.
			start_time_interval_measurement();
			ir_state = IR_STATE_LEADING_9000ms;
		}
		return;
	case IR_STATE_LEADING_9000ms:
		if(is_rising_edge){
			// Only trigger on rising edge
			uint8_t time_interval = get_time_interval_since_last_measurement();
			if(time_interval > usec_to_cycles(9000, -F_CPU_ACCURACY_PERCENT)
			&& time_interval < usec_to_cycles(9000, +F_CPU_ACCURACY_PERCENT)){
			// 9000 us negative leading pulse (approx. 170 clock cycles)
				ir_state = IR_STATE_LEADING_4500ms;
			} else {
				ir_state = IR_STATE_IDLE;
			}
		}
		return;
	case IR_STATE_LEADING_4500ms:
		if(!is_rising_edge){
			uint8_t time_interval = get_time_interval_since_last_measurement();
			// Only trigger on falling edge
			if(time_interval > usec_to_cycles(4500, -F_CPU_ACCURACY_PERCENT)
			&& time_interval < usec_to_cycles(4500, +F_CPU_ACCURACY_PERCENT)){
			// 4500 us positive leading pulse (approx. 85 clock cycles)
				// Start receiving the data bits.
				// Clear the shift register and set the flag.
				ir_state = IR_STATE_DATA_BITS;
				ir_received_bits_count = 0;
				ir_shift_register = 0;
				// Turn on LED to show that the code is received now.
				led_off();
			} else {
				ir_state = IR_STATE_IDLE;
			}
		}
		return;
	case IR_STATE_DATA_BITS:
		if(is_rising_edge){
			// Only trigger on falling edge
			return;
		}
		uint8_t time_interval = get_time_interval_since_last_measurement();
		uint8_t new_bit;
		if(time_interval > usec_to_cycles(560 + 560, -F_CPU_ACCURACY_PERCENT)
		&& time_interval < usec_to_cycles(560 + 560, +F_CPU_ACCURACY_PERCENT)){
		// 560 us + 560 us (approx. 21 clock cycles) = logical 0
			new_bit = 0;
		} else if(time_interval > usec_to_cycles(560 + 1680, -F_CPU_ACCURACY_PERCENT)
		       && time_interval < usec_to_cycles(560 + 1680, +F_CPU_ACCURACY_PERCENT)){
		// 560 us + 1680 us (approx. 42 clock cycles) = logical 1
			new_bit = 1;
		} else {
		// If received anything else, this is an error, stop receiving
		// bits.
			ir_hangup();
			return;
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
			ir_hangup();

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
			// control buttons and set a corresponding duty cycle
			// if a known button has been pressed.
			uint8_t i;
			for (i = 0; i < sizeof(IR_REMOTE_CONTROL_BUTTONS) / sizeof(ir_button_t); i++){
				ir_button_t maybe_this_button = IR_REMOTE_CONTROL_BUTTONS[i];
				if(command == maybe_this_button.command){
					pwm_set_duty_cycle(maybe_this_button.pwm_duty_cycle);
					// We do not break here to keep the
					// timing consistent.
				}
			}
		}
	}
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
	// A Timer/Counter overflow has happened
	if(timer_overflow_flag){
		// This is the second or subsequent overflow, so calculating
		// the difference between the Timer/Counter values does not
		// make sense anymore. This should not happen with legal IR
		// pulses, so what we have received must be garbage. So we
		// reset the receiver to the default "waiting for incoming
		// transmission" state.
		//
		// 14 to 28 ms from the last IR pulse may pass until hangup,
		// depending on the Timer/Counter value at the last IR pulse.
		ir_hangup();
	} else {
		// This is the first overflow. Here we remember that it
		// happened.
		timer_overflow_flag = 1;
	}
}

/* 
 * ADC measurement complete Interrupt service routine. We only use the ADC to
 * estimate the battery level.
 */
ISR(ADC_vect){
	ensure_battery_level_above_critical();
}

int main(){
	// Start up the ADC:
	{
		// - select the source of the Analog signal;
		// - left-adjust the result
		// - set Internal 1.1 V voltage reference.
		ADMUX = ADC_ON_PB4 | (1 << REFS0) | (1 << ADLAR);

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

		measure_and_show_battery_idle_voltage();

		// battery_status_critical is updated here to 1 if the voltage
		// is below critical.
		ensure_battery_level_above_critical();
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
		// on pin OC0B, which is the same pin as PB1 aka PWM pin.  The
		// 8-bit clock counts from 0 to 255 and starts again at zero.
		// When it encounters the value OCR0B, it clears the OC0B bit,
		// and sets it high again when the counter is restarted from
		// zero.
		//
		// The Timer/Counter serves three purposes at the same time.
		// First, it is used to drive the PWM on the OC0B (PB1) pin.
		// Second, it is used to measure the pulse widths for the
		// pulse-period demodulation to decode the IR remote control
		// signals. To measure the pulse lengths, we read the
		// Timer/Counter value and store it in a variable. By
		// calculating the difference between the current and the
		// previous readings, we may evaluate the pulse period. As we
		// carefully select the Timer/Counter frequency to 18.75 kHz
		// (54 us per tick), pulse widths from 54 us to 14 ms can be
		// measured. The NEC IR protocol uses pulse widths from 560 us
		// to 9 ms. We also use the Timer overflow interrupt to hang up
		// the IR code receive as soon as the timer overflows for the
		// second time (14 to 28 ms after the last pulse has been
		// transmitted).
		// Third, Timer/Counter overflows are used to trigger periodic
		// battery level checks.
		{
			// - set Fast PWM mode with 0xFF as TOP;
			// - set Clear OC0B on Compare Match.
			TCCR0A = ((1 << WGM01) | (1 << WGM00)) | (1 << COM0B1);

			// - set 64 as Timer/Counter prescaler, i.e., divide
			// system clock by 64 for the Timer/Counter frequency
			// (this gives 18.75 kHz), approx. 54 us per tick.
			TCCR0B = 3;
		}

		// Do a quick self-test: briefly turn on the motor to full
		// power and measure the loaded battery voltage.
		{
			OCR0B = 255; // PWM 100% duty cycle
			pwm_start();

			_delay_ms(50);

			adc_fire_once();
			ensure_battery_level_above_critical();

			pwm_stop();
		}

		// Set the Timer Overflow (i.e., the moment when the PWM opens
		// the transistor - we want the loaded voltage for critical
		// discharge checks) as the trigger event to start the voltage
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
		// ensure_battery_level_above_critical() is called at the corresponding
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
