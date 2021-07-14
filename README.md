# beam-robot-firmware

В этой ветви: код под ATTiny13 для впайки (перевёрнутым) прямо в плату из вертолётика

Для кодинга под AVR я использую следующий тулчейн:
код на Си + либы avr-libc
компилятор — avr-gcc (версии 5.4 и новее\*!!!)
avr-objcopy для перевода из .o в .hex
avrdude для прошивки контроллера

GNU make для автоматизации сборки
PuTTY или python + pyserial для общения с ардуинкой через UART (для роботов это не потребуется)

Всё это без проблем работает под линуксом, и после некоторых танцев с бубном (а именно поиск avr-gcc свежей версии) работает под Windows.

\* Более ранняя версия, которая поставляется в WinAVR, содержит критические баги!!! К сожалению уже не помню, какие именно — то ли у меня он что-то не хотел компилить, то ли скомпиленная программа не заливалась, то ли, что самое вероятное, не линковалась... Кажется, линкер жаловался на то, не может правильно расписать адреса, так как в старой версии они должны были быть до 1024, а мне надо было включить либу для printf, которая сама занимает больше инструкций, и в память контроллера-то помещается, а вот в эти 1024 инструкции — нет. Суть в том, что программа, которая должна работать и из-под линукса компилилась и работала, с этим компилятором работать отказывалась, пока я его не обновил до версии 5.4


Linux:
I. Installing the toolchain
1. install binutils-avr avr-libc gcc-avr # for compilation
2. install avrdude # for burning
3. modprobe ch341 # for communication and burning (Arduino with CH340 chip)

II. Compiling

To compile, use
avr-gcc -mmcu=atmega328p -Wl,-u,vfprintf -lprintf_flt -lm
# The flags -Wl,-u,vfprintf -lprintf_flt -lm are required for the floating point printf to work properly;
# if omitted, printf("%f"...) will print a question mark.

To make an Intel HEX file, use
avr-objcopy -O ihex [input.o] [output.hex]

To burn the chip, use
sudo avrdude -c arduino -p atmega328p -P /dev/ttyUSB0 -b 57600 -U flash:w:[output.hex]:i

III. Communicating with the chip
There are several options.
1. Using cu:
sudo cu -l /dev/ttyUSB0 -9600
2. Using minicom: (?)
sudo minicom -e /dev/ttyUSB0
3. Using PuTTY (also available on Windows):
Settings (for Windows):
- Session
  * Serial line: COM4
  * Speed: 9600
- Terminal
  * Implicit CR in every LF: yes
  * Implicit LF in every CR: yes
  * Local echo: Force on
  * Local line editing: Force on
- Connection
  - Serial
    * Serial line to connect to: COM4
    * Speed (baud): 9600
    * Data bits: 8
    * Stop bits: 1
    * Parity: None
    * Flow control: None
To find out the port, use Device Manager, section `Ports (COM & LPT)`. It should normally show something like USB-SERIAL CH340 (COM4).
4. Python serial library. It should be possible in theory, but never actually worked for me.

Settings on the chip end:
#define BAUD 9600
#define F_CPU 16000000UL
#if defined F_CPU && defined BAUD
	#include <util/setbaud.h>
	#define BAUD_PRESCALLER (((F_CPU / (BAUD * 16UL))) - 1)
	#define uart_init(mask) { \
		UBRR0H = (uint8_t) (BAUD_PRESCALLER >> 8); \
		UBRR0L = (uint8_t) (BAUD_PRESCALLER); \
		UCSR0B = mask; \
		UCSR0C = _BV(UCSZ00) | _BV(UCSZ01); \
	}
#else
	#error F_CPU or BAUD undefined. Can not calculate UBRR0 value.
#endif
This corresponds to the following settings:
- Mode of operation [UMSEL]: Asynchronous
- Parity [UPM0]: None
- Stop bits [USBS0]: 1
- Data bits [UCSZ0]: 0
uart_init should be invoked like
uart_init(_BV(RXEN0) | _BV(TXEN0));
(Enable Rx & Tx; do not enable interrupts; do not enable ninth data bit).

N.B. For some reason, baud rates above 9600 don't seem to work. No idea why.

Windows10:
To compile and burn, you may use WinAVR toolchain (Windows-compiled avr-libc).

To compile, you may also use the avr-libc toolchain under WSL. Burning won't work because of absence of kernel modules. Accessing the TTY from WSL should work without module, but for some reason it does not: I see a lot of ports which do not correspond to real ones. Tried accessing COM3 as ttyS3, but line was busy.

You can communicate with the device via PuTTY (see above). Driver for CH340 should work out of box.
N.B. If the device does not show up, you may have a USB driver power problem. Reboot the system.



