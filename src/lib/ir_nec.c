#include <stdio.h>
#include <stdint.h>

#include "ir_nec.h"
/*
 * NEC code infrared protocol decoder.
 *
 * See e.g., https://techdocs.altium.com/display/FPGA/NEC+Infrared+Transmission+Protocol for documentation.
 *
 * The decoding process consists of three stages:
 * 1. IR signal pulse bursts are converted to logical level voltage,
 *    according to the following rule:
 *    1: no signal
 *    0: 38 kHz pulse burst.
 *    This operation is performed by hardware, namely, by a TSOP1738 unit or a
 *    compatible infrared signal receiver with integrated filter circuitry and
 *    automatic gain control.
 * 2. Logical level voltage, which carries a pulse-distance modulated signal,
 *    is demodulated to obtain a 32-bit logical code. This signal can either
 *    carry a new 32-bit code, or be a repeat code. In the repeat code case,
 *    the previous received 32-bit code is returned by the corresponding
 *    procedure. 
 * 3. The logical 32-bit code, which consists of:
 *    1. target device address (8 bits),
 *    2. logical inverse of the target device address (8 bits),
 *    3. command (8 bits),
 *    4. logical inverse of the command (8 bits);
 *    is decoded to obtain the 8-bit command and a 8-bit address.
 *    This is done by the function
 *    ir_nec_process_pin_change(uint8_t new_bit)
 *    when all 32 bits have been received.
 *
 *    The address is then compared to the address of this device, and if this
 *    check passes - i.e., if the transmission was directed to this device, and
 *    not to something else - the 8-bit command is returned.  This operation is
 *    performed by the function
 *    ir_nec_getchar(stream),
 *    which reads IR data synchronously, and continues to wait for another
 *    incoming transmission if the destination address was wrong.
 */


volatile uint16_t ir_nec_last_address = 0;
volatile uint8_t ir_nec_last_command = 0;
volatile uint8_t ir_nec_rx_complete_flag = 0;
#define IR_NEC_RX_COMPLETE 1
#define IR_NEC_REPEAT_CODE 2

owi_pulse_t NEC_INPUT_LOGICAL_ZERO = (owi_pulse_t) {
	.firsthalf_pulsewidth = float_to_pulsewidth(562.5e-6),
	.secondhalf_pulsewidth = float_to_pulsewidth(562.5e-6),
	.edge_type = EDGE_TYPE_RISING
};

owi_pulse_t NEC_INPUT_LOGICAL_ONE = (owi_pulse_t) {
	.firsthalf_pulsewidth = float_to_pulsewidth(562.5e-6),
	.secondhalf_pulsewidth = float_to_pulsewidth(1687.5e-6),
	.edge_type = EDGE_TYPE_RISING
};

owi_pulse_t NEC_INPUT_LEADING_PULSE = (owi_pulse_t) {
	.firsthalf_pulsewidth = float_to_pulsewidth(9000e-6),
	.secondhalf_pulsewidth = float_to_pulsewidth(4500e-6),
	.edge_type = EDGE_TYPE_RISING
};

owi_pulse_t NEC_INPUT_REPEAT_CODE = (owi_pulse_t) {
	.firsthalf_pulsewidth = float_to_pulsewidth(9000e-6),
	.secondhalf_pulsewidth = float_to_pulsewidth(2250e-6),
	.edge_type = EDGE_TYPE_RISING
};

int ir_nec_input_setup(){
	owi_configure_reading(&ir_nec_process_pulse, 1, 0);
}

#define ERROR_MARGIN 200e-6

/*
 * Synchronously read one byte from the infrared receiver and decode it using
 * the NEC protocol. Perform the address check: if the received transmission
 * was not directed to this device, wait for the next one.
 * Arguments:
 * FILE * stream - a pointer to the stream obtained from ir_nec_open_rx().
 */
char ir_nec_getchar(FILE * stream){
	// Extract user data from the FILE structure
	ir_nec_conf_t * conf = (ir_nec_conf_t *) fdev_get_udata(stream);
	
	// Accept transmissions infinitely, until we get a transmission destined correctly.
	while(1){
		// Wait until a message is received completely
		do {} while (!ir_nec_rx_complete_flag);

		// If the received signal was a repeat code and we ignore them,
		// wait for another transmission.
		if (!conf->respect_repeat_codes && (ir_nec_rx_complete_flag == IR_NEC_REPEAT_CODE)) {
			// Clear the flag to enable incoming transmissions
			ir_nec_rx_complete_flag = 0;
			continue; // go accept another transmission
		}

		// Clear the flag to enable incoming transmissions
		ir_nec_rx_complete_flag = 0;
		
		// Perform an address check
		switch(conf->address_mode){
			case IR_NEC_ADDRESSMODE_EXACT:
				// Destination address must be exactly equal to this
				// device address
				if (ir_nec_last_address != conf->this_device_address){
					// Not equal, do not accept the command
					// and wait for the next incoming
					// transmission
					continue;
				}
				// go down to the next case
			case IR_NEC_ADDRESSMODE_BITMASK:
				// This device's address is a bitmask.  Destination
				// address must conform this bitmask Note: if the
				// previous case holds and the addresses are equal, the
				// bitmask check will also pass.
				if ((ir_nec_last_address & conf->this_device_address) != ir_nec_last_address) {
					// Does not conform, do not accept the command
					// and wait for the next incoming
					// transmission
					continue;
				}
				// go down to the next case
			case IR_NEC_ADDRESSMODE_REVERSE_BITMASK:
				// Destination address is a bitmask. This device's
				// address must conform this bitmask Note: if the
				// previous case holds and the addresses are equal, the
				// bitmask check will also pass.
				if ((ir_nec_last_address & conf->this_device_address) != conf->this_device_address) {
					// Does not conform, do not accept the command
					// and wait for the next incoming
					// transmission
					continue;
				}
				// go down to the next case
			case IR_NEC_ADDRESSMODE_IGNORE:
				// Accept the command unconditionally.
				return (char) ir_nec_last_command;
		}
		
	}
}

/* 
 * A finite-state automaton (FSA) used for demodulating a pulse-distance
 * modulated signal into a 32-bit logical code.  Uses a software clock
 * ir_nec_demodulator_fsa_clock, which is incremented programmatically, to
 * demodulate a pulse-distance modulation signal which is programmatically fed
 * into the FSA bit-by-bit, and to store the decoded 32-bit logical code into a
 * 32-bit shift register.
 *
 * The finite-state automaton distinguishes multiple states depending on the
 * part of the pulse-duration modulation code last received:
 * - idle or malformed message (which is ignored)
 * - initial pulse received
 * - receiving the message body
 * - receiving a repeat code
 *
 * This function makes the finite-state automaton process the next pulse edge
 * on the pin responsible for the IR receiver. This function is meant to be
 * called from a pin-change interrupt vector. It updates the finite-state
 * automaton state and returns nothing.
 *
 * This function measures the time between its previous call (which should
 * correspond to a pulse edge) and this call (which should also correspond to
 * the opposite pulse edge) using a software clock running at 1777 Hz (562.5
 * us). Depending on the measured pulse width, the corresponding part of the
 * message (leading pulse or body) is distinguished, and the FSA state is
 * updated correspondingly. In the message body case, the bit obtained by
 * measuring the distance between pulses (pulse-distance modulation) is
 * shifted into the shift register.
 *
 * After receiving the last bit, the message is decoded and verified for
 * validness.
 *
 * Arguments: uint8_t new_bit - new pin value (0 or 1).
 */
void ir_nec_process_pulse(owi_pulse_t new_pulse){
	// 32-bit shift register to store the demodulated 32-bit logical sequence.
	static uint32_t shift_register = 0;
	static uint8_t bits_received = 0; // received bits count

	#define IR_NEC_FSA_STATE_IDLE		0
	// Waiting for a next incoming transmission after successfully
	// receiving a code or receiving a malformed transmission.

	#define IR_NEC_FSA_STATE_MALFORMED	2
	// Waiting for a next incoming transmission after receiving a malformed
	// transmission, which is ignored silently.
	// Note: IR_NEC_FSA_STATE_IDLE and IR_NEC_FSA_STATE_MALFORMED are
	// defined to the same value, we only distinguish them in the code for
	// better readability.

	#define IR_NEC_FSA_STATE_LEADING_PULSE	1
	// The first pin change has been observed; what is expected to be
	// received now is a 9 ms leading pulse which marks a NEC protocol
	// code. It may be followed by either a 4.5 ms gap, which precedes
	// a new 32-bit message, or a 2.25 ms gap followed by a terminating
	// 562.5 us pulse, which is a repeat code sent repeatedly while an IR
	// remote control button is held depressed.

	#define IR_NEC_FSA_STATE_MESSAGE_BODY	3
	// The 32 significant bits of a new code are now received.

	static uint8_t fsa_state = IR_NEC_FSA_STATE_IDLE;

	switch(fsa_state){
	case IR_NEC_FSA_STATE_IDLE:
	case IR_NEC_FSA_STATE_MALFORMED:
		// We are on the rising edge of the leading pulse. The timer
		// now contains the time passed since the last transmission,
		// i.e., garbage. This leading edge will be used to measure the
		// initial period length from.
		fsa_state = IR_NEC_FSA_STATE_LEADING_PULSE;
		break;
	case IR_NEC_FSA_STATE_LEADING_PULSE:
		// Transmission has been initiated, but no bits received yet.
		// This is a start sequence.
		if (pulse_equals(new_pulse, NEC_INPUT_LEADING_PULSE, ERROR_MARGIN)) {
			// This was a leading 9 ms pulse followed by a 4.5 ms
			// gap, which precedes a new incoming code.
			fsa_state = IR_NEC_FSA_STATE_MESSAGE_BODY;
			// Clear the shift register
			shift_register = 0;
			bits_received = 0;
		} else if (pulse_equals(new_pulse, NEC_INPUT_REPEAT_CODE, ERROR_MARGIN)) {
			// This was a leading 9 ms pulse followed by a 2.25 ms
			// gap, which indicates a repeat code. We are now on
			// the rising edge of a terminating 562.5 us pulse.
			// Its falling edge will be ignored. Therefore we
			// return the FSA to the idle state here.
			ir_nec_rx_complete_flag = IR_NEC_REPEAT_CODE;
			// FIXME: what if the last code was malformed?
			// Need to add protection from repeating it.
			fsa_state = IR_NEC_FSA_STATE_IDLE;
		} else {
			// This is a malformed transmission, presumably noise.
			fsa_state = IR_NEC_FSA_STATE_MALFORMED;
		}
		break;
	case IR_NEC_FSA_STATE_MESSAGE_BODY:
		// Receiving a new significant pulse-distance modulated 32-bit code.
		if (pulse_equals(new_pulse, NEC_INPUT_LOGICAL_ONE, ERROR_MARGIN)) {
			// 562.5 us pulse followed by a 1687.5 us gap duration
			// signifies a logical 1, shift it into register
			bits_received++;
			shift_register <<= 1;
			shift_register |= 1;
		} else if (pulse_equals(new_pulse, NEC_INPUT_LOGICAL_ZERO, ERROR_MARGIN)) {
			// 562.5 us pulse followed by a 562.5 us gap duration
			// signifies a logical 0, shift it into register
			bits_received++;
			shift_register <<= 1;
		} else {
			// If anything else has been received, this is a
			// malformed transmission.
			fsa_state = IR_NEC_FSA_STATE_MALFORMED;
			break;
		}
		if (bits_received == 32){
			// All bits have been received. We are now on the rising edge
			// of a terminating 562.5 us pulse.  Its falling edge will be
			// ignored. Therefore we return the FSA to the idle state here.
			fsa_state = IR_NEC_FSA_STATE_IDLE;

			// Decode the 32-bit logical code obtained during the
			// demodulation to obtain a 8-bit command.  The 32-bit
			// code consists of the following (starting from the
			// most-significant bit):
			// 1. target device address (8 bits);
			// 2. logical inverse of target device address (8 bits);
			// 3. command (8 bits);
			// 4. logical inverse of command (8 bits).
	#ifdef NEC_VANILLA_ADDRESS
			uint8_t rx_address =			(uint8_t)  ((shift_register >> 24) & 0xFF);
			uint8_t rx_address_verification =	(uint8_t) ~((shift_register >> 16) & 0xFF);
	#else
			uint16_t rx_address =			(uint16_t) ((shift_register >> 16) & 0xFFFF);
	#endif
			uint8_t rx_command =			(uint8_t)  ((shift_register >> 8) & 0xFF);
			uint8_t rx_command_verification =	(uint8_t) ~((shift_register >> 0) & 0xFF);
			shift_register = 0;
			bits_received = 0;
			// Verify that command equals inverse of its inverse,
			if ( (rx_command == rx_command_verification)
	#ifdef NEC_VANILLA_ADDRESS
			// and, for the vanilla NEC case, the same holds for the address:
			&& (rx_address == rx_address_verification)
	#endif
			   ) {
				// If all checks passed, update the global
				// variable and set the Receive complete flag.
				ir_nec_last_address = (uint16_t) rx_address;
				ir_nec_last_command = rx_command;
				ir_nec_rx_complete_flag = IR_NEC_RX_COMPLETE;
				fsa_state = IR_NEC_FSA_STATE_IDLE;
			} else {
				// The checks have not passed, so this transmission
				// is malformed
				fsa_state = IR_NEC_FSA_STATE_MALFORMED;
			} 
		}
	}
}

