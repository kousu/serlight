#default program to build; you can build other ones with 'make myprog.elf' or 'make myprog.hex', though.
PROGRAM = serlight


# TODO: GNU make is deciding the rm the intermediate targets automatically
# now, it doesn't do this for my latex makefile, so something is weeeird

all: $(PROGRAM).hex
	
include avr.mk
