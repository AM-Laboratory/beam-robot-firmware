// NEC code infrared protocol

#define IR_NEC_ADDRESSMODE_EXACT 0
#define IR_NEC_ADDRESSMODE_BITMASK 1
#define IR_NEC_ADDRESSMODE_REVERSE_BITMASK 2
#define IR_NEC_ADDRESSMODE_IGNORE 3

#define IR_NEC_REPEAT_CODES_IGNORE 0
#define IR_NEC_REPEAT_CODES_RESPECT 1


/*
 * Synchronously read one byte from the infrared receiver and decode it using
 * the NEC protocol. Perform the address check: if the received transmission
 * was not directed to this device, wait for the next one.
 * Arguments:
 * FILE * stream - a pointer to the stream obtained from ir_nec_open_rx().
 */
char ir_nec_getchar(FILE * stream);

/* Tick the finite-state automaton software clock. This function MUST be called
 * each 562.5 us (1777 Hz clock) in order to demodulate the NEC protocol
 * signals correctly.
 */
inline void ir_nec_tick();


typedef struct {
	// This flag governs whether or not to respect repeat codes sent by the
	// remote control while its key is held depressed. These codes are sent
	// each 108 ms and correspond to re-entry of the command. E.g., on a TV
	// these codes make the volume bar continuously go up while the volume
	// up key is held depressed.
	uint8_t respect_repeat_codes;
	// The address of this device, or a bitmask.
	uint8_t this_device_address;
	// This setting determines how to deal with the address: equivalence
	// check, bitmask check, or ignore the address check.
	uint8_t address_mode;
} ir_nec_conf_t;


/*
 * This macro sets up an infrared sensor using the NEC protocol as an input stream
 * and returns a FILE object.
 *
 * Note: the memory for the FILE object is allocated on the stack, so the stream
 * should be closed using the fdev_close( ) macro, not the fclose() function.
 *
 * Arguments:
 * uint8_t respect_repeat_codes: This flag governs whether or not to respect
 *                               repeat codes sent by the remote control while
 *                               its key is held depressed. These codes are
 *                               sent each 108 ms and correspond to re-entry of
 *                               the command. E.g., on a TV these codes make
 *                               the volume bar continuously go up while the
 *                               volume up key is held depressed.
 *                               0 - ignore the repeat codes, only accept new
 *                                   codes;
 *                               1 - respect the repeat codes.
 * uint8_t this_device_address: The address of this device, or a bitmask.
 * uint8_t address_mode: This setting determines how to deal with the address:
 *                       equivalence check, bitmask check, or ignore the
 *                       address check. Allowed values:
 *                       0 - EXACT, message destination address must be equal
 *                           to this device's address;
 *                       1 - BITMASK, this device's address is a bitmask, and
 *                           the message destination address must match it.
 *                       2 - INVERSE_BITMASK, the message destination address
 *                           is a bitmask, and this device's address address
 *                           must match it.
 *         any other value - IGNORE, do not perform the address
 *                           check, accept commands from all IR remote
 *                           controls.
 *
 */
#define fdev_open_ir_nec(streamname, a,b,c) ( \
	(FILE) { \
		.get = ((void*) 0), \
		.put = (void*) ir_nec_getchar, \
		.flags = _FDEV_SETUP_READ, \
		.udata = &( \
			(ir_nec_conf_t) { \
				.respect_repeat_codes = a, \
				.this_device_address = b, \
				.address_mode = c \
			} \
		) \
	} \
)

// #define ir_nec_open_rx(streamname, a,b,c)  \
// 	ir_nec_conf_t conf = {.respect_repeat_codes = a, .this_device_address = b, .address_mode = c}; \
// 	FILE __stream = FDEV_SETUP_STREAM(NULL, ir_nec_getchar, _FDEV_SETUP_READ); \
// 	FILE * streamname = &__stream; \
// 	fdev_set_udata(streamname, &conf);

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
void ir_nec_process_pin_change(uint8_t new_bit);
