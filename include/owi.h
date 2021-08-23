
#include <stdint.h>
#include <math.h>

#include <stdio.h>

/*
 * Pulse width in tens of nanoseconds. This type can hold pulse widths from 10
 * ns to 49 seconds.
 */
typedef uint32_t owi_pulsewidth_t;

#define OWI_PULSEWIDTH_OVERFLOW 0xFFFFFFFF

/*
 * Create a owi_pulsewidth_t from a float.
 * Arguments:
 * - float pulsewidth_seconds - target pulse width in seconds.
 */
#define float_to_pulsewidth(x) ((uint32_t) (x * 1e8))

/*
 * A floating-point representation of the pulse width.
 * Should be only used for debugging.
 * Arguments:
 * - owi_pulsewidth_t x - a pulsewidth to convert to float.
 */
#define owi_pulsewidth_to_float(x) ((x) * 1e-8)

typedef enum {
	EDGE_TYPE_RISING,
	EDGE_TYPE_FALLING
} owi_edge_type_t;

typedef struct {
	owi_pulsewidth_t firsthalf_pulsewidth;
	owi_pulsewidth_t secondhalf_pulsewidth;
	owi_edge_type_t edge_type;
} owi_pulse_t;

#define OWI_OK 0
#define OWI_READING_STOPPED 1
#define OWI_ERROR_BUS_NOT_CONFIGURED 2
#define OWI_ERROR_BUS_ALREADY_RUNNING 3
/*
 * Set the clock frequency for measuring the pulse widths. The user is
 * responsible for making sure that the clock period is not greater than the
 * shortest expected pulse and that the longest expected pulse does not exceed
 * 65535 times clock period.
 *
 * This setting cannot be changed while listening or while there are any
 * pending output pulses.
 * - uint8_t clock_prescaler - the clock prescaler for measuring input and
 *   output pulse widths will run at the (see the atmega328p datasheet.) 
 * - owi_pulsewidth_t clock_period - the real clock period in seconds (as some
 *   prescaler values specify an external clock trigger).
 * Return values:
 * - OWI_OK - the clock frequency was set sucessfully.
 * - OWI_ERROR_BUS_ALREADY_RUNNING - tried to change the clock frequency (clock
 *   prescaler) while the bus was either actively sending pulses or listening.
 */
int owi_set_clock(uint8_t clock_prescaler, owi_pulsewidth_t clock_period);

/*
 * Start/stop listening on the input pin (ICR1 aka PB0).
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
 */
int owi_configure_reading(void (*input_pulse_callback)(owi_pulse_t), uint8_t new_idle_logic_level, uint8_t use_noise_canceller);

int owi_configure_writing(uint8_t new_idle_logic_level);

uint8_t owi_is_listening();

uint8_t owi_has_output_pending();
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
int owi_send_pulses(owi_pulse_t ** pointers2pulses_to_transmit, size_t new_burst_size);


/*
 * Compare two owi_pulse_t structures, whether the pulses are equal up to a
 * specified error margin.
 *
 * Arguments:
 * - owi_pulse_t a,
 * - owi_pulse_t b - the pulses to compare.
 * - uint16_t error_margin - the pulse width difference error margin, in Timer1 periods.
 */
#define pulse_equals_tc(a, b, error_margin) ( \
	(a.edge_type == b.edge_type) \
	     && (a.firsthalf_pulsewidth <= (b.firsthalf_pulsewidth + error_margin)) \
	     && (a.firsthalf_pulsewidth >= (b.firsthalf_pulsewidth - error_margin)) \
	     && (a.secondhalf_pulsewidth <= (b.secondhalf_pulsewidth + error_margin)) \
	     && (a.secondhalf_pulsewidth >= (b.secondhalf_pulsewidth - error_margin)))

// __attribute__((always_inline))
// inline bool pulse_equals(owi_pulse_t a, owi_pulse_t b, uint16_t error_margin){
// 	return ((a.edge_type == b.edge_type)
// 	     && (a.firsthalf_pulsewidth <= (b.firsthalf_pulsewidth + error_margin))
// 	     && (a.firsthalf_pulsewidth >= (b.firsthalf_pulsewidth - error_margin))
// 	     && (a.secondhalf_pulsewidth <= (b.secondhalf_pulsewidth + error_margin))
// 	     && (a.secondhalf_pulsewidth >= (b.secondhalf_pulsewidth - error_margin)));
// }


#define compile_round_to_uint16_t(floatval) ((((floatval) - ((uint16_t) (floatval)) >= 0.5) ? 1 : 0) + (uint16_t) (floatval))
#define compile_round_to_int16_t(floatval) ((((floatval) - ((int16_t) (floatval)) >= 0.5) ? 1 : 0) + (int16_t) (floatval))

#define compile_round_to_uint32_t(floatval) ((((floatval) - ((uint32_t) (floatval)) >= 0.5) ? 1 : 0) + (uint32_t) (floatval))
#define compile_round_to_int32_t(floatval) ((((floatval) - ((int32_t) (floatval)) >= 0.5) ? 1 : 0) + (int32_t) (floatval))

//#define seconds2pulsewidth(time_s_double) compile_round_to_uint16_t((time_s_double) * OWI_CLOCK_FREQUENCY)

/*
 * Compare two owi_pulse_t structures, whether the pulses are equal up to a
 * specified error margin.
 *
 * Arguments:
 * - owi_pulse_t a,
 * - owi_pulse_t b - the pulses to compare.
 * - float error_margin - the pulse width difference error margin, in seconds.
 */
#define pulse_equals(a, b, c) pulse_equals_tc(a, b, float_to_pulsewidth((c)))
