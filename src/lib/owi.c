// One-wire interface

#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#define __OWI_C
#include "owi.h"
#undef __OWI_C

#define BIT_OC1A 1
#define BIT_ICP1 0


// Reset the counter TIMER1 to zero. The Output Compare registers are also
// shifted down accordingly, so, as long as the timer is not expected to
// overflow, the events scheduled by the Output Compare Match events remain
// as-is on the time axis.
#define reset_watch() { \
	OCR1A -= TCNT1; \
	OCR1B -= TCNT1; \
	TCNT1 = 0; \
}
owi_pulsewidth_t clock_period = 0;

void (*owi_input_pulse_callback)(owi_pulse_t) = NULL;

uint8_t input_idle_logic_level = 0xFF;
uint8_t output_idle_logic_level = 0;

volatile uint16_t input_pulse_firsthalf_width = 0;
volatile uint8_t input_pulse_edgetype = 0;

int owi_set_clock(uint8_t new_clock_prescaler, owi_pulsewidth_t period){
	uint8_t clock_prescaler = TCCR1B & 0x7;
	if((new_clock_prescaler != clock_prescaler) && (owi_is_listening() || owi_has_output_pending())){
		// If there are pending output pulses, or we are listening (the
		// input callback is not NULL), and someone tried to change the
		// clock frequency, issue an error instead of actually changing
		// the settings, because changing the clock frequency in this
		// case would distort all the pulses.
		return OWI_ERROR_BUS_ALREADY_RUNNING;
	} else {
		// Set the new clock prescaler
		TCCR1B = (TCCR1B & 0xF8) | new_clock_prescaler;
		clock_period = period;
		return OWI_OK;
	}
}

/*
 * Start/stop listening on the input pin (ICR1 aka PB0).
 *
 * After the bus has already been set up, it is possible to change the
 * following settings:
 * - Noise canceller
 * - The callback function pointer (possibly to NULL, which causes the bus to
 *   stop listening)
 * - Idle logical level, if no half-pulse has been received. However, it almost
 *   certainly has been, so one should stop listening first.
 * After stopping listening, it is possible to change all settings, including
 * the clock prescaler. The latter can only be changed if there are no pending
 * output pulses - an error is returned otherwise.
 *
 * Arguments:
 * - void (*input_pulse_callback)(owi_pulse_t) - a pointer to the callback
 *   function, which will be called on each received pulse to process it. This
 *   pointer may be NULL, in which case listening is stopped.
 * - bool idle_logic_level - the idle logic level to start listening from.
 *   If the current logic level is not idle, listening will be started when it
 *   goes idle.
 * - bool use_noise_canceller - instructs the Input Capture Unit whether it
 *   should activate the noise canceller (see the atmega328p datasheet.) When
 *   the noise canceler is activated, the input from the Input Capture pin
 *   (ICP1) is filtered. The filter function requires four successive equal
 *   valued samples of the ICP1 pin for changing its output. The Input Capture
 *   is therefore delayed by four Oscillator cycles when the noise canceler is
 *   enabled.
 * Return value:
 * - OWI_OK - everything configured and the bus is listened;
 * - OWI_READING_STOPPED - the callback function pointer was NULL, so bus
 *   listening was stopped.
 * - OWI_ERROR_BUS_ALREADY_RUNNING - tried to change the idle logical level
 *   mid-pulse.
 */
int owi_configure_reading(void (*input_pulse_callback)(owi_pulse_t), uint8_t new_idle_logic_level, uint8_t use_noise_canceller){
	if (input_pulse_callback == NULL){
		// If the callback function points nowhere, listening does not
		// make sense, as all the received information will go nowhere.
		// Therefore, this case is treated as "Turn listening off."
		owi_input_pulse_callback = NULL;

		// Forget about the half-pulse we might have already received
		input_pulse_firsthalf_width = 0;

		// Disable Input Capture Interrupt
		TIMSK1 &= ~_BV(ICIE1);
		return OWI_READING_STOPPED;
	}
	if(input_pulse_firsthalf_width != 0 && new_idle_logic_level != input_idle_logic_level){
		// If we have already received a half-pulse but someone decided
		// to change the idle logical level, issue an error instead of
		// changing the settings.
		return OWI_ERROR_BUS_ALREADY_RUNNING;
	}

	owi_input_pulse_callback = (*input_pulse_callback);
	// Start listening from the idle logic level.
	// If the current logic level is active, listening will be
	// started when it goes idle.
	input_idle_logic_level = new_idle_logic_level;

	// Enable Input Capture Interrupt
	TIMSK1 |= _BV(ICIE1);

	// Select the appropriate edge for the Input Capture Unit
	// (falling for idle High, and rising for idle Low)
	if (input_idle_logic_level) {
		// Idle level is High
		// Trigger the Input Capture Unit on the falling edge
		TCCR1B &= ~_BV(ICES1);
	} else {
		// Idle level is Low
		// Trigger the Input Capture Unit on the rising edge
		TCCR1B |= _BV(ICES1);
	}
		
	if (use_noise_canceller) {
		// Turn on the noise canceller
		TCCR1B |= _BV(ICNC1);
	}

	// Disable writing to ICP1 pin
	DDRB &= ~_BV(BIT_ICP1);

	reset_watch();
	return OWI_OK;
}

uint8_t owi_is_listening(){
	return owi_input_pulse_callback != NULL;
}

/*
 * Set the idle logic level on the output pin (OC1A aka PB1).
 * The output idle level is configured to 0 by default, so it is only necessary
 * to call this procedure when a idle 1 is needed.
 */
int owi_configure_writing(uint8_t new_idle_logic_level){
	if(owi_has_output_pending() && new_idle_logic_level != output_idle_logic_level){
		// If we are sending pulses, but someone decided to change the
		// idle logical level, issue an error instead of changing the
		// settings.
		return OWI_ERROR_BUS_ALREADY_RUNNING;
	}

	output_idle_logic_level = !!new_idle_logic_level;

	// Open OC1A pin for writing
	DDRB |= _BV(BIT_OC1A);
	// Pull OC1A to the idle value
	if (output_idle_logic_level){
		PORTB |= _BV(BIT_OC1A);
	} else {
		PORTB &= ~_BV(BIT_OC1A);
	}
	// Lock writing to OC1A 
	DDRB &= ~_BV(BIT_OC1A);
	return OWI_OK;
}

volatile owi_pulse_t ** output_current_pulses_pointers = NULL;
volatile size_t output_pulses_count = 0;
volatile uint16_t output_current_halfpulse_idx = 0;

uint8_t owi_has_output_pending(){
	return (output_current_pulses_pointers != NULL);
}

/*
 * Send a sequence of pulses to the OC1A aka PB1 pin. If there already are
 * pending output pulses, the new ones will be appended to them without any
 * extra delay between the pulse trains. The pulses are supplied to this
 * procedure as an array of pointers to pulses. For a binary sequence, this
 * should be normally used as follows:
 * const owi_pulse_t ZERO = ...;
 * const owi_pulse_t ONE = ...;
 * owi_pulse_t byte = {&ZERO, &ONE, &ONE, &ZERO, &ONE, &ZERO, &ONE, &ZERO};
 * owi_send_pulses(byte, 8);
 * Using pointers to several "standard" pulses has been implemented for
 * protocols that have an initial pulse, which is neither logical one, nor
 * logical zero. However, all the supplied pulses can be different, if
 * non-binary transmission is needed.
 *
 * This function is asynchronous: the pointers are copied to
 * internally-allocated memory, and sending them at the appropriate timing is
 * done by timer-generated interrupts. As soon as all pending pulses are sent,
 * the memory is cleared.
 *
 * The user is responsible for sending only as many pulse sequences per time
 * unit as the bus capacity can handle. As all bit sequences are remembered
 * until sent and the memory is only cleared when the queue is empty, keeping
 * the queue non-empty for too long will bloat the memory.
 *
 * Arguments:
 * - owi_pulse_t ** pointers2pulses_to_transmit - an array of pointers to
 *   owi_pulse_t structures that describe all the pulses to be sent.
 * - size_t new_burst_size - the number of the owi_pulse_t structures to be
 *   sent. Must not exceed the capacity of the pointers2pulses_to_transmit
 *   array, or a buffer overflow will happen. The owi_pulse_t structures are
 *   never checked for validness, so an overflow will probably lead to sending
 *   absurdly-long pulses. This will not, however, overwrite any memory.
 */
int owi_send_pulses(owi_pulse_t ** pointers2pulses_to_transmit, size_t new_burst_size){
	if (TCCR1B & 0x7 == 0){
		// The clock prescaler was never selected, so the clock is
		// stopped and the bus has never been configured
		return OWI_ERROR_BUS_NOT_CONFIGURED;
	}
	// Append new pulses to the existing ones (if any)
	output_current_pulses_pointers = realloc(output_current_pulses_pointers, (output_pulses_count + new_burst_size) * sizeof(owi_pulse_t *));
	uint8_t i;
	for (i = output_pulses_count; i < new_burst_size + output_pulses_count; i++){
		output_current_pulses_pointers[i] = pointers2pulses_to_transmit[i];
	}
	output_pulses_count += new_burst_size;

	// If no pulse train transmission has already been initiated (or the
	// last one has been finished), perform the initiation procedures:
	if (output_current_halfpulse_idx == 0){
		DDRB |= _BV(BIT_OC1A);
		// Open OC1A for writing

		// Select the correct polarity
		TCCR1A |= _BV(COM1A1);
		if (output_current_pulses_pointers[0]->edge_type){
			// First low, then high;
			// Clear OC1A immediately
			PORTB &= ~_BV(BIT_OC1A);
			// Set OC1A on the next Compare match
			TCCR1A |= _BV(COM1A0);
		} else {
			// First high, then low;
			// Set OC1A immediately
			PORTB |= _BV(BIT_OC1A);
			// Clear OC1A on the next Compare match
			TCCR1A &= ~_BV(COM1A0);
		}
		OCR1A = TCNT1 + (uint16_t) (output_current_pulses_pointers[0]->firsthalf_pulsewidth / clock_period);
		TIMSK1 |= _BV(OCIE1A);
		// Enable Match A interrupt
		output_current_halfpulse_idx++;
		// We have now sent the first half of the first pulse, wait for the timer interrupt...
	}
}

/*
 * This interrupt is set up to occur during sending bits.
 * When it is called, it's time to send the next (half-)pulse.
 */
ISR(TIMER1_COMPA_vect){
	cli();
	if ((output_current_halfpulse_idx >> 1) == output_pulses_count){
		// Finish sending bits

		// Free the memory
		free(output_current_pulses_pointers);
		output_current_pulses_pointers = NULL;
		// Reset the index to show that 
		output_current_halfpulse_idx = 0;

		// Disable Match A interrupt
		TIMSK1 &= ~_BV(OCIE1A);

		DDRB &= ~_BV(BIT_OC1A);
		// Lock writing to OC1A 
	} else {
		volatile owi_pulse_t * current_pulse = output_current_pulses_pointers[output_current_halfpulse_idx >> 1];
		if (output_current_halfpulse_idx & 1 == 0) {
			// This is the first half of the pulse.
			OCR1A = TCNT1 + (uint16_t) (current_pulse->firsthalf_pulsewidth / clock_period);
			// Schedule a toggle after the first half-pulse width.
			TCCR1A &= ~_BV(COM1A1);
			TCCR1A |= _BV(COM1A0);
		} else {
			// This is the second half of the pulse.
			OCR1A = TCNT1 + (uint16_t) (current_pulse->secondhalf_pulsewidth / clock_period);
			// Schedule a toggle after the second half-pulse width.

		}
		output_current_halfpulse_idx++;

		// Select the logic level after the pulse
		uint8_t next_logic_level;
		if ((output_current_halfpulse_idx >> 1) == output_pulses_count) {
			// This was the last pulse, pull OC1A to the idle level
			next_logic_level = output_idle_logic_level;
		} else {
			next_logic_level = !output_current_pulses_pointers[output_current_halfpulse_idx >> 1]->edge_type;
		}
		if (next_logic_level){
			// Set OC1A on the next Compare match
			TCCR1A |= _BV(COM1A0);
		} else {
			// Clear OC1A on the next Compare match
			TCCR1A &= ~_BV(COM1A0);
		}
	}
	sei();
}

/*
 * This interrupt is set up to occur when listening if the timer overflows
 * without receiving any pulses (i.e., a timeout happens).
 * Here we generate an overflow owi_pulse_t if we have been listening to a
 * pulse.
 */
ISR(TIMER1_OVF_vect){
	uint8_t timeout_logic_level = (PORTB >> BIT_ICP1) & 1;
	// If a timeout occurred at the idle logic_level, then we probably just
	// are not receiving any pulses now. If we are, however, stuck in the
	// 'active' logic_level, this is almost certainly a problem.
	if (timeout_logic_level != input_idle_logic_level){
		owi_pulse_t received_pulse;
		if (input_pulse_firsthalf_width == 0){
			// This is the first half of the pulse.
			// Mark its first half-width as Overflow and second as zero
			received_pulse.firsthalf_pulsewidth = OWI_PULSEWIDTH_OVERFLOW;
			received_pulse.secondhalf_pulsewidth = 0;
			// Record the logic_level by the second half of the pulse, which
			// has already started by now:
			// RISING for First Low then High;
			// FALLING for First High then Low;
		} else {
			// This is the second half of the pulse.
			received_pulse.firsthalf_pulsewidth = clock_period * ((uint32_t) input_pulse_firsthalf_width);
			received_pulse.edge_type = input_pulse_edgetype;
			// Record the second half width as Overflow
			received_pulse.secondhalf_pulsewidth = OWI_PULSEWIDTH_OVERFLOW;

		}
		// Invoke the callback function
		(*owi_input_pulse_callback)(received_pulse);
	}
}

/*
 * This interrupt is set up to occur when listening.
 * If it was called, it means that a new pulse edge has been received.
 */
ISR(TIMER1_CAPT_vect){
	cli();
	PORTB ^= _BV(5);
	
	// Toggle the input capture unit edge
	TCCR1B ^= _BV(ICES1);

	if (input_pulse_firsthalf_width == 0){
		// This is the first half of the pulse.
		// Record its width
		input_pulse_firsthalf_width = ICR1;
	
		uint8_t secondhalf_logic_level = (PORTB >> BIT_ICP1) & 1;
		input_pulse_edgetype = secondhalf_logic_level;
		// Record the edge type by the second half of the pulse, which
		// has already started by now:
		// RISING for First Low then High;
		// FALLING for First High then Low;
	} else {
		// This is the second half of the pulse.
		owi_pulse_t received_pulse;
		received_pulse.firsthalf_pulsewidth = clock_period * ((uint32_t) input_pulse_firsthalf_width);
		received_pulse.edge_type = input_pulse_edgetype;
		// Record the second half width
		received_pulse.secondhalf_pulsewidth = clock_period * ((uint32_t) ICR1);
		input_pulse_firsthalf_width = 0;
		

		// Invoke the callback function
		(*owi_input_pulse_callback)(received_pulse);
	}

	reset_watch();
	
	sei();
}
