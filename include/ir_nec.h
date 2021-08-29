// NEC code infrared protocol
#include <avr/io.h>
#include "owi.h"

#define IR_NEC_ADDRESSMODE_EXACT 0
#define IR_NEC_ADDRESSMODE_BITMASK 1
#define IR_NEC_ADDRESSMODE_REVERSE_BITMASK 2
#define IR_NEC_ADDRESSMODE_IGNORE 3

#define IR_NEC_REPEAT_CODES_IGNORE 0
#define IR_NEC_REPEAT_CODES_RESPECT 1


typedef enum {
	NEW_CODE,
	REPEAT_CODE,
	MALFORMED_CODE // Used internally to prevent repeating malformed codes
} ir_nec_novelty_t;
	
typedef struct {
	uint8_t command;
	uint16_t address;
	ir_nec_novelty_t new_or_repeated;
} ir_nec_code_t;


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
 */
void ir_nec_process_pulse(owi_pulse_t new_pulse, void (*ir_nec_newdata_callback)(void*));


int ir_nec_input_setup(void (*ir_nec_data_callback)(void*));
