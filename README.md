# swarmodroid-firmware

Firmware for the Swarmodroid - a bristle bot platform by Swarmtronics for active matter research. See [the paper](http://example.com) and [the Supplementary document](http://example.com) for the full description of the Swarmodroid platform.

We use the term *firmware* to distinct the bot microcontroller code from the [tracking software](https://github.com/AM-Laboratory/Multitracking).

## Description
The Swarmodroid firmware is a program written for the Microchip ATTiny13A AVR microcontroller and responsible for the following:
1. generation of the pulse-width modulated signal to drive the vibro-motor at the selected power;
2. receiving and decoding commands from an infrared remote control device to set the power to a desired level;
3. battery voltage supervision to avoid overdischarge.

The compatible remote control device must operate according to the [NEC protocol](https://techdocs.altium.com/display/FPGA/NEC+Infrared+Transmission+Protocol) have an address `0x02FD` and support the following command codes:
| Button | Command code | Swarmodroid action |
| :----- | :----------: | :----------------- |
| Power  | 0x48         | Set power to 0%\*  |
| 1      | 0x80         | Set power to 10%   |
| 2      | 0x40         | Set power to 20%   |
| 3      | 0xc0         | Set power to 30%   |
| 4      | 0x20         | Set power to 40%   |
| 5      | 0xA0         | Set power to 50%   |
| 6      | 0x60         | Set power to 60%   |
| 7      | 0xe0         | Set power to 70%   |
| 8      | 0x10         | Set power to 80%   |
| 9      | 0x90         | Set power to 90%   |
| 0      | 0x00         | Set power to 100%  |

\* Also, display the approximate battery charge level by blinking the LED 1-3 times with 3 blinks corresponding to full charge.

Specifically, we use the Rexant RX-1882 (RX-188) programmable remote control device, programmed with code 284.

Any other addresses and commands are ignored.

## Compilation and installation instructions
You can choose to either download a pre-compiled .hex file, and only use `avrdude` to program the device, or to clone the complete project to compile it yourself and program the device.

In this instructions, we assume that you know how to program an AVR microcontroller using ICSP (in-circuit serial programming), and have a programmer board handy.
### Clone and compile the project
The code requires `avr-libc` and `avr-gcc 5.4.0` or newer to compile. GNU Make is also required.
1. Clone the project.
2. Navigate to the project root.
3. Run `avr-gcc --version` and make sure that it is not older than 5.4.0.
4. Run `make` to compile the project.
5. Make sure that the folder `build` has been created and the files `main.o` and `main.hex` are there.

### Get and install a pre-compiled .hex file
Alternatively, you may download a pre-compiled .hex file. To do that, please navigate to the [Releases on Github](https://github.com/AM-Laboratory/beam-robot-firmware/releases). Select an appropriate version and download the Full.release.pre-compiled.zip archive and unpack it.

#### Note: if you use a GUI utility to program AVR microcontrollers, downloading only main.hex might be sufficient.

### Solder an ICSP plug dongle
Due to the size and weight limitations, the Swarmodroid uses a PLD2-8 plug for the ICSP connector, as opposed to the de-facto-standard PLD6 plug.
Therefore, to progam the device, you will need to solder an adapter.
1. Fetch a PBD2-8 socket, a PLD6 plug and 6 wires.
2. Solder the wires according to the picture ![in the doc/icsp_dongle.png file](https://github.com/AM-Laboratory/beam-robot-firmware/raw/ChinaPCBSimpleRobot/doc/icsp_dongle.png "Programmer ICSP plug adapter")
3. It is strongly advised to fix the wires firmly and insulate them near plugs. This can be achieved using an epoxy resin or a hot-melt adhesive. Alternatively, you may create a PCB-based adapter instead of a dongle.

### Program your device
We use the Atmel AVRISP STK500 USB ISP Programmer and `avrdude` to program the chip. If you use `avrdude`, but other programmer than STK500, you will need to edit the Makefile accordingly. The only other programmer aside of STK500 that has been tested, is the [Arduino ISP](https://docs.arduino.cc/built-in-examples/arduino-isp/ArduinoISP).

If you use another utility to program the AVR devices, run it instead of `make install` - you won't need to edit the Makefile in this case.

1. Prepare the device, which must be Swarmodroid Rev. A. Turn the power switch on - the device must be turned on during programming.
2. Connect the STK500 programmer to the PC.
3. On Windows, open Command line and run `mode` to find out the COM port of the programmer. On Linux, run `ls -l /dev/ttyUSB*` on terminal.
4. Open the Makefile for editing and change the `PROGRAMMER_TTY` variable to the actual COM port of the programmer.
5. On Linux, make sure that `/dev/ttyUSBx` you use has the write permission for you. If not, set its `chmod` to `0666`. If, for some reason, this is not possible or unadvised, change the Makefile to run `avrdude` with `sudo`.
6. Connect the Swarmodroid to the programmer using the adapter you have soldered earlier.
7. Run `make install` to program the device. During programming, the Swarmodroid should vibrate and blink its LED. If this is not happening, something's wrong.

Note: for this project, the factory fuses for ATTiny13 are used. The fuses are not changed during programming. If your device fuses had been programmed to other values, you will need to change them back to the factory values (lfuse=0x6A, hfuse=0xFF, lockbit=0xFF) manually.
