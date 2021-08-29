#include <stdint.h>
#include "ir_nec_synchronous.h"


volatile uint8_t ir_nec_rx_complete_flag = 0;
volatile ir_nec_code_t ir_nec_last_code;
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
	
	// Accept transmissions infinitely, until we get a transmission destined correctly.
	while(1){
		// Wait until a message is received completely
		do {} while (!ir_nec_rx_complete_flag);

		// If the received signal was a repeat code and we ignore them,
		// wait for another transmission.
		if (!conf->respect_repeat_codes && (ir_nec_last_code.new_or_repeated == REPEAT_CODE)) {
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
				if (ir_nec_last_code.address != conf->this_device_address){
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
				if ((ir_nec_last_code.address & conf->this_device_address) != ir_nec_last_code.address) {
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
				if ((ir_nec_last_code.address & conf->this_device_address) != conf->this_device_address) {
					// Does not conform, do not accept the command
					// and wait for the next incoming
					// transmission
					continue;
				}
				// go down to the next case
			case IR_NEC_ADDRESSMODE_IGNORE:
				// Accept the command unconditionally.
				return (char) ir_nec_last_code.command;
		}
		
	}
}


void ir_nec_synchronous_callback(ir_nec_code_t * new_code){
	ir_nec_last_code = *new_code;
	ir_nec_rx_complete_flag = 1;
}
