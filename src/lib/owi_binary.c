#include <stdlib.h>
#include "owi.h"


owi_pulse_t * OWI_BINARY_OUTPUT_ZERO;
owi_pulse_t * OWI_BINARY_OUTPUT_ONE;
owi_pulse_t * OWI_BINARY_LEADING;
owi_pulse_t * OWI_BINARY_TRAILING;

typedef enum {
	LSB_FIRST,
	MSB_FIRST
} manchester_data_order_t;

manchester_data_order_t output_order;

void owi_binary_output_setup(owi_pulse_t * zero, owi_pulse_t * one, manchester_data_order_t order, owi_pulse_t * leading, owi_pulse_t * trailing){
	OWI_BINARY_OUTPUT_ZERO = zero;
	OWI_BINARY_OUTPUT_ONE = one;
	output_order = order;
	OWI_BINARY_LEADING = leading;
	OWI_BINARY_TRAILING = trailing;
}

int owi_send_binary(uint8_t * data, size_t bit_count){
	// Array of pointers to the "constant" pulses
	uint8_t leading_pulses, trailing_pulses;
	if (OWI_BINARY_LEADING == NULL){
		leading_pulses = 0;
	} else {
		leading_pulses = 1;
	}
	if (OWI_BINARY_TRAILING == NULL){
		trailing_pulses = 0;
	} else {
		trailing_pulses = 1;
	}
	owi_pulse_t ** pulses = malloc((bit_count + leading_pulses + trailing_pulses) * sizeof(owi_pulse_t *));
	if (leading_pulses){
		pulses[0] = OWI_BINARY_LEADING;
	}
	if (trailing_pulses){
		pulses[bit_count + leading_pulses] = OWI_BINARY_LEADING;
	}
	int i;
	if (output_order == MSB_FIRST) {
		for(i = 0; i < bit_count; i++){
			int byte = i / 8, bit = i % 8;
			if ((data[byte] >> (7 - bit)) & 1) {
				pulses[i + leading_pulses] = OWI_BINARY_OUTPUT_ONE;
			} else {
				pulses[i + leading_pulses] = OWI_BINARY_OUTPUT_ZERO;
			}
		}
	} else {
		for(i = 0; i < bit_count; i++){
			int i2 = bit_count - 1 - i;
			int byte = i2 / 8, bit = i2 % 8;
			if ((data[byte] >> bit) & 1) {
				pulses[i + leading_pulses] = OWI_BINARY_OUTPUT_ONE;
			} else {
				pulses[i + leading_pulses] = OWI_BINARY_OUTPUT_ZERO;
			}
		}
	}
	owi_send_pulses(pulses, bit_count);
	free(pulses);
}

// TODO: receive
