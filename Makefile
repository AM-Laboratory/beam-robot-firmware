MCU=attiny13
CC=avr-gcc -mmcu=$(MCU)  -O2
MAKEHEX=avr-objcopy -O ihex
AVRDUDE=avrdude

# Make sure the programmer port is correct for you! For security reasons, we do
# not script automatic port detection here. Please find the correct port
# manually and change the variable PROGRAMMER_TTY here accordingly.
ifeq ($(OS),Windows_NT)
    # Use the command `mode` to find the COM port on Windows.
    PROGRAMMER_TTY=COM4
else
    # Use the command `ls /dev/ttyUSB*` to find the port on Linux. Please make sure
    # the correct /dev/ttyUSBx has write permissions for everyone (at least 0666).
    # If for some reason you cannot change those, change the line
    # 	AVRDUDE=avrdude 
    # above to 
    # 	AVRDUDE=sudo avrdude
    # and program the chip with sudo permissions.
    PROGRAMMER_TTY=/dev/ttyUSB0
endif

# Make sure this is your programmer board. We assume STK500 as the default.
PROGRAMMER_BOARD=stk500
# Only stk500 and avrisp (Arduino ISP) have been tested, so you will have to
# set the PROGRAMMER_BAUDRATE variable manually if you use any other
# programmer.

ifeq ($(PROGRAMMER_BOARD),stk500)
    PROGRAMMER_BAUDRATE=115200
endif
ifeq ($(PROGRAMMER_BOARD),avrisp)
    # Arduino ISP - a DIY programmer made from Arduino, see
    # https://docs.arduino.cc/built-in-examples/arduino-isp/ArduinoISP
    PROGRAMMER_BAUDRATE=19200
endif

all: build/main.hex

build:
	mkdir -p build

build/main.hex: build/main.o
	avr-size --mcu=$(MCU) build/main.o
	$(MAKEHEX) $< $@

build/main.o: src/main.c build
	$(CC) $< -o $@

.PHONY: install clean

# For some reason, this command is prone to yielding "Unexpected Device
# signature, 0x0000" and "Verification error, first mismatch at byte 0x0000"
# errors.  Just run it multiple times until succeeded. If it does not, try
# moving the wires.
install: build/main.hex
	$(AVRDUDE) -c $(PROGRAMMER_BOARD) -p $(MCU) -P $(PROGRAMMER_TTY) -b $(PROGRAMMER_BAUDRATE) -U flash:w:$<:i

clean:
	rm -rf build

