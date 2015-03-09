
# target all the .c files
PROGRAMS = $(basename $(wildcard *.c))

all: $(addsuffix .elf, $(PROGRAMS))
	
#^note the hidden tab here. otherwise GNU make complains "no rule to make target 'all'"
# weird. I'm sure I've used no-rule targets before. maybe 'all' is special??

include ../avr.mk
