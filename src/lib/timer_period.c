#include "compiletime.h"

#ifndef PERIOD_TIMER_FREQUENCY
	#error "PERIOD_TIMER_FREQUENCY not defined. #include \"timer_period_software.h\" or another period measurement implementation before timer_period_abstract.h"
#endif

/*
 * Measurement error margin, in seconds. Note that it will
 * be rounded down to an integer number of software timer periods.
 */
#ifndef TIMER_ERROR_MARGIN
	#define TIMER_ERROR_MARGIN (1.0 / PERIOD_TIMER_FREQUENCY)
#endif

#ifdef PERIOD_MEASUREMENT_ICR
	uint8_t last_ICR1 = 0;

	__attribute__((always_inline))
	inline uint8_t timer_period_length_equals_impl(uint16_t desired_length, uint16_t error_margin){
		uint8_t period_length = ICR1 - last_ICR1;
		return (period_length <= (desired_length + error_margin))
		    && (period_length >= (desired_length - error_margin));
	}

	__attribute__((always_inline))
	inline uint8_t timer_period_measurement_start(){
		last_ICR1 = ICR1;
	}
#else
	#ifdef PERIOD_MEASUREMENT_SOFTWARE
		#ifndef HWTIMER_FREQUENCY
			#error "Please #define HWTIMER_FREQUENCY to the hardware timer frequency (in Hz)."
		#endif

		__attribute__((always_inline))
		inline void software_timer_tick(uint16_t supertimer);

		__attribute__((always_inline))
		inline uint8_t timer_period_length_equals_impl(uint16_t target, uint16_t error_margin);

		uint16_t software_timer = 0;
		uint16_t last_software_timer = 0;

		#define STEPS ((uint16_t) (HWTIMER_FREQUENCY / PERIOD_TIMER_FREQUENCY))
		__attribute__((always_inline))
		inline void software_timer_tick(uint16_t hwtimer){
			if (hwtimer % STEPS == 0){
				software_timer++;
			}
		}

		__attribute__((always_inline))
		inline uint8_t timer_period_measurement_start(){
			last_software_timer = software_timer;
		}

		__attribute__((always_inline))
		inline uint8_t timer_period_length_equals_impl(uint16_t desired_length, uint16_t error_margin){
			uint16_t period_length = software_timer - last_software_timer;
			return (period_length <= (desired_length + error_margin))
			    && (period_length >= (desired_length - error_margin));
		}
	#else
		#ifdef PERIOD_MEASUREMENT_HWTIMER
		#else
			#error "Period measurement approach not chosen. Please #define either one: PERIOD_MEASUREMENT_ICR, PERIOD_MEASUREMENT_SOFTWARE or PERIOD_MEASUREMENT_HWTIMER."
			uint8_t last_TCNT2 = 0;

			__attribute__((always_inline))
			inline uint8_t timer_period_length_equals_impl(uint8_t desired_length, uint8_t error_margin){
				uint8_t period_length = TCNT2 - last_TCNT2;
				return (period_length <= (desired_length + error_margin))
				    && (period_length >= (desired_length - error_margin));
			}

			__attribute__((always_inline))
			inline uint8_t timer_period_measurement_start(){
				last_TCNT2 = TCNT2;
			}
		#endif
	#endif
#endif
/*
 * Check it the timer-measured signal period equals the expected value.
 * Arguments: expected period in seconds (float)
 */
#define timer_period_length_equals(time_s_double) timer_period_length_equals_impl(compile_round_to_uint16_t((time_s_double) * PERIOD_TIMER_FREQUENCY), compile_round_to_uint16_t((TIMER_ERROR_MARGIN) * PERIOD_TIMER_FREQUENCY))

