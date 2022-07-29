
#define REMOTECONTROL_ADDRESS 0x02FD

#define REMOTECONTROL_BUTTON_POWER	0x48
#define REMOTECONTROL_BUTTON_1		0x80
#define REMOTECONTROL_BUTTON_2		0x40
#define REMOTECONTROL_BUTTON_3		0xc0
#define REMOTECONTROL_BUTTON_4		0x20
#define REMOTECONTROL_BUTTON_5		0xA0
#define REMOTECONTROL_BUTTON_6		0x60
#define REMOTECONTROL_BUTTON_7		0xe0
#define REMOTECONTROL_BUTTON_8		0x10
#define REMOTECONTROL_BUTTON_9		0x90
#define REMOTECONTROL_BUTTON_0		0x00

typedef struct {
	uint8_t command;
	uint8_t pwm_duty_cycle;
} ir_button_t;
const ir_button_t IR_REMOTE_CONTROL_BUTTONS[] = {
	{ .command = REMOTECONTROL_BUTTON_POWER, .pwm_duty_cycle = 0},
	{ .command = REMOTECONTROL_BUTTON_1, .pwm_duty_cycle = 26},
	{ .command = REMOTECONTROL_BUTTON_2, .pwm_duty_cycle = 51},
	{ .command = REMOTECONTROL_BUTTON_3, .pwm_duty_cycle = 77},
	{ .command = REMOTECONTROL_BUTTON_4, .pwm_duty_cycle = 102},
	{ .command = REMOTECONTROL_BUTTON_5, .pwm_duty_cycle = 127},
	{ .command = REMOTECONTROL_BUTTON_6, .pwm_duty_cycle = 153},
	{ .command = REMOTECONTROL_BUTTON_7, .pwm_duty_cycle = 179},
	{ .command = REMOTECONTROL_BUTTON_8, .pwm_duty_cycle = 204},
	{ .command = REMOTECONTROL_BUTTON_9, .pwm_duty_cycle = 230},
	{ .command = REMOTECONTROL_BUTTON_0, .pwm_duty_cycle = 255}
};
