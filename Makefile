# Default target.
all: main.hex

# Compiler flags to avoid repetition
CFLAGS = -mmcu=atmega2560 -DF_CPU=16000000UL -Os -Wall

# Add include directories
INCLUDES = -I./ioLibrary_Driver/Ethernet -I./ioLibrary_Driver/Application/loopback -I./ioLibrary_Driver/Application/modbus


# Compile: create object files from C source files.
main.o: main.c
	avr-gcc $(CFLAGS) $(INCLUDES) -c main.c -o main.o

# Compile ioLibrary source files
wizchip_conf.o: ioLibrary_Driver/Ethernet/wizchip_conf.c
	avr-gcc $(CFLAGS) $(INCLUDES) -c ioLibrary_Driver/Ethernet/wizchip_conf.c -o wizchip_conf.o

loopback.o: ioLibrary_Driver/Application/loopback/loopback.c
	avr-gcc $(CFLAGS) $(INCLUDES) -c ioLibrary_Driver/Application/loopback/loopback.c -o loopback.o

modbus.o: ioLibrary_Driver/Application/modbus/modbus.c
	avr-gcc $(CFLAGS) $(INCLUDES) -c ioLibrary_Driver/Application/modbus/modbus.c -o modbus.o


w5500.o: ioLibrary_Driver/Ethernet/W5500/w5500.c
	avr-gcc $(CFLAGS) $(INCLUDES) -c ioLibrary_Driver/Ethernet/W5500/w5500.c -o w5500.o

socket.o: ioLibrary_Driver/Ethernet/socket.c
	avr-gcc $(CFLAGS) $(INCLUDES) -c ioLibrary_Driver/Ethernet/socket.c -o socket.o

# Link: create ELF output file from object files.
main.elf: main.o wizchip_conf.o loopback.o w5500.o socket.o modbus.o
	avr-gcc $(CFLAGS) -o main.elf main.o wizchip_conf.o loopback.o w5500.o socket.o modbus.o -lm -Wl,-u,vfprintf -lprintf_flt

# Convert ELF to HEX file.
main.hex: main.elf
	avr-objcopy -O ihex main.elf main.hex

# Generate a listing file.
main.lst: main.elf
	avr-objdump -h -S main.elf > main.lst

# Program the chip using avrdude.
program: main.hex
	avrdude -v -p atmega2560 -c wiring -P com8 -D -U flash:w:main.hex:i

# Clean up build files.
clean:
	rm -f main.o main.elf main.hex main.lst wizchip_conf.o loopback.o modbus.o socket.o w5500.o
