#include <stdio.h>
#include "ir_nec.h"

/*
 * Synchronously read one byte from the infrared receiver and decode it using
 * the NEC protocol. Perform the address check: if the received transmission
 * was not directed to this device, wait for the next one.
 * Arguments:
 * FILE * stream - a pointer to the stream obtained from ir_nec_open_rx().
 */
char ir_nec_getchar(FILE * stream);

typedef struct {
	// This flag governs whether or not to respect repeat codes sent by the
	// remote control while its key is held depressed. These codes are sent
	// each 108 ms and correspond to re-entry of the command. E.g., on a TV
	// these codes make the volume bar continuously go up while the volume
	// up key is held depressed.
	uint8_t respect_repeat_codes;
	// The address of this device, or a bitmask.
	uint16_t this_device_address;
	// This setting determines how to deal with the address: equivalence
	// check, bitmask check, or ignore the address check.
	uint8_t address_mode;
} ir_nec_conf_t;

void ir_nec_synchronous_callback(ir_nec_code_t * new_code);

#define ir_nec_synchronous_input_setup() ir_nec_input_setup(&ir_nec_synchronous_callback)

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
 * uint16_t this_device_address: The address of this device, or a bitmask.
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

