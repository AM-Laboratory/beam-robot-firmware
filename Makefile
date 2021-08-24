# Makefile for GNU Make.
# On Windows, use Make built with MinGW

.SUFFIXES:
.SUFFIXES: .o .elf .c .hex

# The flags -Wl,-u,vfprintf -lprintf_flt -lm are required for the floating point printf to work properly;
# if omitted, printf("%f"...) will print a question mark.

# For Linux:
# TTY=/dev/ttyUSB0
# BURN=sudo avrdude

# for Windows:
TTY=COM3
BURN=avrdude

SRCDIR=./src
LIBSRCDIR=./src/lib
BUILDDIR=./build
LIBBUILDDIR=./build/lib
ASMDIR=./asm
INCLDIR=./include

# Libraries
HEADERS=$(wildcard $(INCLDIR)/*.h)
SOURCES=$(wildcard $(LIBSRCDIR)/*.c)
OBJECTS=$(patsubst $(LIBSRCDIR)/%.c,$(LIBBUILDDIR)/%.o,$(SOURCES))

DEVICE=atmega328p
CC=avr-gcc
OBJFLAGS=-mmcu=$(DEVICE) -O2 -I$(INCLDIR) -w
MAINFLAGS=-mmcu=$(DEVICE) -O2 -I$(INCLDIR) -lm -Wl,-u,vfprintf -lprintf_flt

all: $(BUILDDIR)/program.hex

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(LIBBUILDDIR):
	mkdir -p $(LIBBUILDDIR)


# Compile and assemble the libraries, but do not link
$(OBJECTS): $(LIBBUILDDIR)/%.o : $(LIBSRCDIR)/%.c $(BUILDDIR) $(LIBBUILDDIR)
	$(CC) $(OBJFLAGS) -c $< -o $@

# Compile and assemble the main program, but do not link
$(BUILDDIR)/main.o: $(SRCDIR)/main.c $(BUILDDIR)
	$(CC) $(MAINFLAGS) -c $(SRCDIR)/main.c -o $(BUILDDIR)/main.o

# Link the main program and the libraries to produce the firmware
$(BUILDDIR)/program.elf: $(BUILDDIR)/main.o $(OBJECTS)
	$(CC) $(MAINFLAGS) -o $(BUILDDIR)/program.elf $(BUILDDIR)/main.o $(OBJECTS)

# Convert to the .hex format
$(BUILDDIR)/program.hex: $(BUILDDIR)/program.elf
	rm -f $(BUILDDIR)/program.hex
	avr-objcopy -j .text -j .data -O ihex $(BUILDDIR)/program.elf $(BUILDDIR)/program.hex
	avr-size --format=avr --mcu=$(DEVICE) $(BUILDDIR)/program.elf


.PHONY: readfuse rccal burn clean
readfuse:
	$(BURN) -c avrisp -p $(DEVICE) -P $(TTY) -b 19200 -U lfuse:r:-:i -U hfuse:r:-:i -U efuse:r:-:i
rccal:
	$(BURN) -c avrisp -p $(DEVICE) -P $(TTY) -b 19200 -O
install: $(BUILDDIR)/program.hex
	$(BURN) -c arduino -p $(DEVICE) -P $(TTY) -b 57600 -U flash:w:$<:i

clean:
	rm -R build
