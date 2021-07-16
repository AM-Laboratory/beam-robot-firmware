MCU=attiny13
#MCU=atmega328p
CC=avr-gcc -mmcu=$(MCU)  -O2
# The flags -Wl,-u,vfprintf -lprintf_flt -lm are required for the floating point printf to work properly;
# if omitted, printf("%f"...) will print a question mark.
#
AS=avr-as -mmcu=$(MCU)
#OBJCOPY=avr-objcopy -j .text -j .data -O ihex
OBJCOPY=avr-objcopy -O ihex
# TTY=/dev/ttyUSB0
TTY=COM3
DUDE=avrdude
# DUDE=sudo avrdude


%.S: %.c
	$(CC) -S $< -o $@

%.o: %.c
	$(CC) $< -o $@

%.hex: %.o
	$(OBJCOPY) $< $@

%.burn.log: %.hex
	$(DUDE) -c avrisp -p $(MCU) -P $(TTY) -b 19200 -U flash:w:$<:i

# %.burn.log: %.hex
# 	$(DUDE) -c arduino -p $(MCU) -P $(TTY) -b 57600 -U flash:w:$<:i

main.o: main.c lib/*.c lib/*.h
	$(CC) $< -o $@

.PHONY: readfuse rccal
readfuse:
	$(DUDE) -c avrisp -p $(MCU) -P $(TTY) -b 19200 -U lfuse:r:-:i -U hfuse:r:-:i -U efuse:r:-:i
rccal:
	$(DUDE) -c avrisp -p $(MCU) -P $(TTY) -b 19200 -O
