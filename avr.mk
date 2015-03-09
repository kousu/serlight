
#the device we're compiling for; this defines the CPU, size of the RAM, and a few other thigns
MCU = attiny13

# compiler options
CC = avr-gcc
OBJCOPY = avr-objcopy
CFLAGS += -Wall -mmcu=$(MCU)
CFLAGS += -std=c99 #allow sane C syntax
CFLAGS += -g -Os   #optimizations
LDFLAGS +=
OBJS = *.o


# compile a C file to a corresponding unlinked object file
# by making all .o's depend on the Makefile we make updates to the makefile trigger updates just like updates to any other part of the code does.
%.o: %.c Makefile
	$(CC) $(CFLAGS) -o $@ -c $<

# compile and link the final program
# and then print out the sizes of the code and data sections
# since there is not a lot of space on the teensy and therefore
# being very aware of how your code is affecting your limits is important
%.elf: %.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^
	avr-size -C --mcu=$(MCU) $@

# compile the final program to a form that avrdude can flash
%.hex: %.elf
	$(OBJCOPY) -O ihex $< $@

flash: $(PROGRAM).hex
	avrdude -c usbasp -p t13 -u -Uflash:w:$< -Ulfuse:w:0x75:m -Uhfuse:w:0xFF:m

flash-example: precompiled.hex
	avrdude -c usbasp -p t13 -u -Uflash:w:precompiled.hex -Ulfuse:w:0x75:m -Uhfuse:w:0xFF:m

clean:
	rm -f $(OBJS)
	rm -f *.elf
	rm -f *.hex
