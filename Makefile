MCU=attiny13
CC=avr-gcc -mmcu=$(MCU)  -O2
AS=avr-as -mmcu=$(MCU)
#OBJCOPY=avr-objcopy -j .text -j .data -O ihex
OBJCOPY=avr-objcopy -O ihex
# TTY=/dev/ttyUSB0
TTY=COM4
DUDE=avrdude
# DUDE=sudo avrdude

all: build/main.hex

build:
	mkdir -p build

build/main.hex: build/main.o
	avr-size --mcu=$(MCU) build/main.o
	$(OBJCOPY) $< $@

build/main.o: src/main.c build
	$(CC) $< -o $@

.PHONY: install eeprom clean

# For some reason, this command is prone to yielding "Unexpected Device
# signature, 0x0000" and "Verification error, first mismatch at byte 0x0000"
# errors.  Just run it multiple times until succeeded. If it does not, try
# moving the cables.
install: build/main.hex
#	$(DUDE) -c avrisp -p $(MCU) -P $(TTY) -b 19200 -U flash:w:$<:i -F
	$(DUDE) -c avrisp -p $(MCU) -P $(TTY) -b 19200 -U flash:w:$<:i
#	$(DUDE) -c stk500 -p $(MCU) -P $(TTY) -b 115200 -U flash:w:$<:i

# Read EEPROM memory into a file
eeprom:
	$(DUDE) -c avrisp -p $(MCU) -P $(TTY) -b 19200 -U eeprom:r:eeprom.hex:i

clean:
	rm -rf build
	rm -f eeprom.hex

