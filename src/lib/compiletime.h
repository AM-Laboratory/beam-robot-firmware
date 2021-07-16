// Compile-time utilities
#define compile_round_to_uint16_t(floatval) ((((floatval) - ((uint16_t) (floatval)) >= 0.5) ? 1 : 0) + (uint16_t) (floatval))
#define compile_round_to_int16_t(floatval) ((((floatval) - ((int16_t) (floatval)) >= 0.5) ? 1 : 0) + (int16_t) (floatval))

