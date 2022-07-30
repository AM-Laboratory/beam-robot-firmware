
// The remote control address, used to pair the remote control with the
// controlled device. The first two bytes of the 32 data bits sent by the
// remote control. If a command from a remote control with an address
// different from this constant is received, it is silently ignored.
#define REMOTECONTROL_ADDRESS 0x02FD

typedef struct {
	uint8_t command;
	// The 8-bit command sent by the remote control device when the button
	// is pressed. The third byte of the 32 data bits sent by the remote
	// control.

	uint8_t pwm_duty_cycle;
	// The PWM duty cycle that the bot should set, when the button is
	// pressed on the remote control device. 0 corresponds to zero duty
	// cycle (motor always off), and 255 corresponds to 100% duty cycle
	// (motor always on).
} ir_button_t;

// The list of remote control buttons known to the bot. All other buttons are
// silently ignored.
const ir_button_t IR_REMOTE_CONTROL_BUTTONS[] = {
	{
		// Power button
		.command = 0x48,
		.pwm_duty_cycle = 0 // PWM 0%
	},
	{
		// Button 1
		.command = 0x80,
		.pwm_duty_cycle = 26 // PWM 10%
	},
	{
		// Button 2
		.command = 0x40,
		.pwm_duty_cycle = 51 // PWM 20%
	},
	{
		// Button 3
		.command = 0xc0,
		.pwm_duty_cycle = 77 // PWM 30%
	},
	{
		// Button 4
		.command = 0x20,
		.pwm_duty_cycle = 102 // PWM 40%
	},
	{
		// Button 5
		.command = 0xa0,
		.pwm_duty_cycle = 127 // PWM 50%
	},
	{
		// Button 6
		.command = 0x60,
		.pwm_duty_cycle = 153 // PWM 60%
	},
	{
		// Button 7
		.command = 0xe0,
		.pwm_duty_cycle = 179 // PWM 70%
	},
	{
		// Button 8
		.command = 0x10,
		.pwm_duty_cycle = 204 // PWM 80%
	},
	{
		// Button 9
		.command = 0x90,
		.pwm_duty_cycle = 230 // PWM 90%
	},
	{
		// Button 0
		.command = 0x00,
		.pwm_duty_cycle = 255 // PWM 100%
	}
};
