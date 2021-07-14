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
 *    procedure.  This operation is performed by a finite-state automaton,
 *    which measures the pulse widths and distances using a 1777 Hz (562.5 us)
 *    software clock. This is done by the function
 *    ir_nec_process_pin_change(uint8_t new_bit),
 *    while the software clock is ticked by a function
 *    ir_nec_demodulator_fsa_tick().
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



uint8_t ir_nec_last_address = 0;
uint8_t ir_nec_last_command = 0;
uint8_t ir_nec_rx_complete_flag = 0;
#define IR_NEC_RX_COMPLETE 1
#define IR_NEC_REPEAT_CODE 2



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
	
	while(1){
		printf("Listening\n");
		// Wait until a message is received completely
		uint8_t vx = 0;
		do {PORTB ^= (1 << 5);} while (!ir_nec_rx_complete_flag );
		printf("%x, %x, %X, %x\r\n", ir_nec_rx_complete_flag, !ir_nec_rx_complete_flag, ir_nec_last_address, ir_nec_last_command);

		// If the received signal was a repeat code and we ignore them,
		// wait for another transmission.
		if (!conf->respect_repeat_codes && (ir_nec_rx_complete_flag == IR_NEC_REPEAT_CODE)) {
			// Clear the flag to enable incoming transmissions
			ir_nec_rx_complete_flag = 0;
			continue;
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


// 562.5 us-per-tick software clock used to measure the pulse widths and the
// distances between pulses, to decode a pulse-distance modulated logical code.
// As in the NEC protocol every possible pulse duration is a multiple of 562.5
// us, we choose this time quantum for the clock. As this clock is used to
// measure pulse widths, it is reset to zero at each incoming pulse edge by the
// ir_nec_demodulator_fsa_process_pin_change() function.
#define IR_NEC_CLOCK_562us 1
#define IR_NEC_CLOCK_1125us 2
#define IR_NEC_CLOCK_1687us 3
#define IR_NEC_CLOCK_2250us 4
#define IR_NEC_CLOCK_4500us 8
#define IR_NEC_CLOCK_9000us 16
uint8_t ir_nec_demodulator_fsa_clock = 0xFF;

/* Tick the finite-state automaton software clock. This function MUST be called
 * each 562.5 us (1777 Hz clock) in order to demodulate the NEC protocol
 * signals correctly.
 */
inline void ir_nec_tick(){
	ir_nec_demodulator_fsa_clock++;
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
 * After receiving the last bit, the 
 *
 * Arguments: uint8_t new_bit - new pin value (0 or 1).
 */
void ir_nec_process_pin_change(uint8_t new_bit){
	// 32-bit shift register to store the demodulated 32-bit logical sequence.
	static uint32_t shift_register = 0;
	static uint8_t bits_received = 0; // received bits count

	#define IR_NEC_FSA_STATE_IDLE		0
	// Waiting for a next incoming transmission after successfully
	// receiving a code or receiving a malformed transmission.

	#define IR_NEC_FSA_STATE_MALFORMED	0
	// Waiting for a next incoming transmission after receiving a malformed
	// transmission, which is ignored silently.
	// Note: IR_NEC_FSA_STATE_IDLE and IR_NEC_FSA_STATE_MALFORMED are
	// defined to the same value, we only distinguish them in the code for
	// better readability.

	#define IR_NEC_FSA_STATE_LEADING_PULSE	1
	// The first pin change has been observed; what is expected to be
	// received now is a 9 ms leading pulse which marks a NEC protocol
	// code.

	#define IR_NEC_FSA_STATE_LEADING_GAP	2
	// A leading 9 ms pulse has been received. Waiting for a next positive
	// pulse which may be a repeat code or a significant bit of a 32-bit
	// code.

	#define IR_NEC_FSA_STATE_MESSAGE_BODY	3
	// The 32 significant bits of a new code are now received.

	#define IR_NEC_FSA_STATE_REPEAT_CODE	4
	// This is a repeat code which consists of a 9 ms leading pulse, a 2.25
	// ms gap and a single 562.5 us pulse.

	static uint8_t fsa_state = IR_NEC_FSA_STATE_IDLE;

	#define was_positive_pulse(length) (new_bit && (ir_nec_demodulator_fsa_clock <= (length + 1)) && (ir_nec_demodulator_fsa_clock >= (length - 1)))
	#define was_negative_pulse(length) (!new_bit && (ir_nec_demodulator_fsa_clock <= (length + 1)) && (ir_nec_demodulator_fsa_clock >= (length - 1)))

	switch(fsa_state){
	case IR_NEC_FSA_STATE_IDLE:
		if(!new_bit){
			// This is the start of a new transmission.
			fsa_state = IR_NEC_FSA_STATE_LEADING_PULSE;
		}
		break;
	case IR_NEC_FSA_STATE_LEADING_PULSE:
		// Transmission has been initiated, but no bits received yet.
		// This is a start sequence.
		if (was_positive_pulse(IR_NEC_CLOCK_9000us)) {
			// This was a leading 9 ms positive pulse, next is the gap
			fsa_state = IR_NEC_FSA_STATE_LEADING_GAP;
		} else {
			// This is a malformed transmission, presumably noise.
			fsa_state = IR_NEC_FSA_STATE_MALFORMED;
			printf("LP,%d\n", ir_nec_demodulator_fsa_clock);
		}
		break;
	case IR_NEC_FSA_STATE_LEADING_GAP:
		// A 4.5 ms or 2.25 ms gap after the 9 ms leading pulse.
		// A 4.5 ms gap precedes a new 32-bit code.
		// A 2.25 ms gap precedes a single 562.5 us pulse, which marks a repeat code.
		if (was_negative_pulse(IR_NEC_CLOCK_4500us)) {
			// This was a leading 4.5 ms gap, which precedes a new incoming code.
			fsa_state = IR_NEC_FSA_STATE_MESSAGE_BODY;
			// Clear the shift register
			shift_register = 0;
			bits_received = 0;
		} else if (was_negative_pulse(IR_NEC_CLOCK_2250us)) {
			// This was a leading 2.25 ms gap, which marks a repeat code.
			fsa_state = IR_NEC_FSA_STATE_REPEAT_CODE;
		} else {
			// This is a malformed transmission, presumably noise.
			fsa_state = IR_NEC_FSA_STATE_MALFORMED;
			printf("LG,%d\n", ir_nec_demodulator_fsa_clock);
		}
		break;
	case IR_NEC_FSA_STATE_REPEAT_CODE:
		// The last pulse in the repeat code should be a single 562.5 us pulse.
		if (was_positive_pulse(IR_NEC_CLOCK_562us)) {
			// Mark 32 bits as received, but do not update the shift register value:
			// a repeat code means that the previous code is to be input one more time.
			ir_nec_rx_complete_flag = IR_NEC_REPEAT_CODE;
			fsa_state = IR_NEC_FSA_STATE_IDLE;
		} else {
			// If it was not, this is a malformed transmission
			fsa_state = IR_NEC_FSA_STATE_MALFORMED;
			printf("RC,%d\n", ir_nec_demodulator_fsa_clock);
		}
		break;
	case IR_NEC_FSA_STATE_MESSAGE_BODY:
		// Receiving a new significant pulse-distance modulated 32-bit code.
		if (was_negative_pulse(IR_NEC_CLOCK_1687us)) {
			// 1687.5 us gap duration signifies a logical 1, shift it into register
			bits_received++;
			shift_register <<= 1;
			shift_register |= 1;
		} else if (was_negative_pulse(IR_NEC_CLOCK_562us)) {
			// 562.5 us gap duration signifies a logical 0, shift
			// it into register
			bits_received++;
			shift_register <<= 1;
		} else if (!was_positive_pulse(IR_NEC_CLOCK_562us)) {
			// Only the following pulses are expected at this mode:
			// a 562.5 us positive pulse.
			// a 562.5 us negative pulse.
			// a 1687.5 us negative pulse,
			// If anything other has been received, this is a
			// malformed transmission.
			fsa_state = IR_NEC_FSA_STATE_MALFORMED;
			printf("MB,%d\n", ir_nec_demodulator_fsa_clock);
		}

		// A new 32-bit message has been received, decode it.
		if (bits_received == 32) {
			// Decode a 32-bit logical code obtained after the
			// demodulation to obtain a 8-bit command.  The 32-bit
			// code consists of the following (starting from the
			// most-significant bit):
			// 1. target device address (8 bits);
			// 2. logical inverse of target device address (8 bits);
			// 3. command (8 bits);
			// 4. logical inverse of command (8 bits).
			uint8_t rx_address =			(uint8_t)  ((shift_register >> 24) & 0xFF);
			uint8_t rx_address_verification =	(uint8_t) ~((shift_register >> 16) & 0xFF);
			uint8_t rx_command =			(uint8_t)  ((shift_register >> 8) & 0xFF);
			uint8_t rx_command_verification =	(uint8_t) ~((shift_register >> 0) & 0xFF);
			shift_register = 0;
			bits_received = 0;

			// Verify that address equals inverse of its inverse,
			// and the same holds for the command.
			if ( (rx_address == rx_address_verification)
			  || (rx_command == rx_command_verification)) {
				// If all checks passed, update the global
				// variable and set the Receive complete flag.
				ir_nec_last_address = rx_address;
				ir_nec_last_command = rx_command;
				ir_nec_rx_complete_flag = IR_NEC_RX_COMPLETE;
				fsa_state = IR_NEC_FSA_STATE_IDLE;
		printf("r%x, %x, %X, %x\r\n", ir_nec_rx_complete_flag, !ir_nec_rx_complete_flag, ir_nec_last_address, ir_nec_last_command);
			} else {
				fsa_state = IR_NEC_FSA_STATE_MALFORMED;
			} 
		}
		break;
	}
	uint8_t old_clk = ir_nec_demodulator_fsa_clock;
	// Reset the pulse-width-measuring software clock
	ir_nec_demodulator_fsa_clock = 0;
	
//	sei();
//	// THIS IS FOR DEBUG!!
	//printf("Current state %d[%d],time:%d; Buffer: %lX(%d); flag: %d. Last A:%x,C:%x.\r\n", fsa_state, new_bit, old_clk, shift_register, bits_received, ir_nec_rx_complete_flag, ir_nec_last_address, ir_nec_last_command);


}
