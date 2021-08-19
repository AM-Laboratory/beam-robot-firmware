DEVICE=atmega328p
COMPILE=avr-gcc -mmcu=$(DEVICE)  -O2
# The flags -Wl,-u,vfprintf -lprintf_flt -lm are required for the floating point printf to work properly;
# if omitted, printf("%f"...) will print a question mark.
AS=avr-as -mmcu=$(DEVICE)

# For Linux:
# TTY=/dev/ttyUSB0 # for Linux
# DUDE=sudo avrdude

# for Windows:
TTY=COM3
DUDE=avrdude

# Libraries
#OBJECTS=lib/ir_nec.o
OBJECTS=
# C files included from main.c
DIRTY_OBJECTS=
#DIRTY_OBJECTS=lib/timer_period.c

%.S: %.c
	$(COMPILE) -S $< -o $@
# Compile, but do not assemble

%.o: %.c
	$(COMPILE) -c $< -o $@
# Compile and assemble, but do not link

main.hex: main.elf
	rm -f main.hex
	avr-objcopy -j .text -j .data -O ihex main.elf main.hex
	avr-size --format=avr --mcu=$(DEVICE) main.elf

main.o: main.c $(DIRTY_OBJECTS)
	$(COMPILE) -c main.c -o main.o

# Link
main.elf: main.o $(OBJECTS)
	$(COMPILE) -o main.elf main.o $(OBJECTS)

# %.burn.log: %.hex
# 	$(DUDE) -c avrisp -p $(DEVICE) -P $(TTY) -b 19200 -U flash:w:$<:i


#main.o: main.c lib/*.c lib/*.h
#	$(CC) $< -o $@

all: main.hex

.PHONY: readfuse rccal burn
readfuse:
	$(DUDE) -c avrisp -p $(DEVICE) -P $(TTY) -b 19200 -U lfuse:r:-:i -U hfuse:r:-:i -U efuse:r:-:i
rccal:
	$(DUDE) -c avrisp -p $(DEVICE) -P $(TTY) -b 19200 -O
burn: main.hex
	$(DUDE) -c arduino -p $(DEVICE) -P $(TTY) -b 57600 -U flash:w:$<:i
