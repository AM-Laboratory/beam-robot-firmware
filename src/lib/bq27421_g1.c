#include "bq27421_g1.h"

#define DO_STOP 0
#define DO_NOT_STOP 1

uint16_t bq27421_read_two_byte_data(uint8_t command){
	uint8_t buffer[2] = {command, command + 1};
	tw_master_transmit(BQ27421_G1_I2C_ADDRESS, &buffer, 1, DO_NOT_STOP);
	tw_master_receive(BQ27421_G1_I2C_ADDRESS, &buffer, 2);
	return ((uint16_t) buffer[1] << 8) | buffer[0];
}

uint16_t bq27421_control(uint16_t subcommand){
	uint8_t buffer[3] = {
		BQ27421_G1_COMMAND_Control,
		(uint8_t) (subcommand & 0xFF),
		(uint8_t) (subcommand >> 8)
	};
	tw_master_transmit(BQ27421_G1_I2C_ADDRESS, &buffer, 3, DO_STOP);
	tw_master_transmit(BQ27421_G1_I2C_ADDRESS, &buffer, 1, DO_NOT_STOP);
	tw_master_receive(BQ27421_G1_I2C_ADDRESS, &buffer, 2);
	return ((uint16_t) buffer[1] << 8) | buffer[0];
}
