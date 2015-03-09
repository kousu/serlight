Architectural notes on avr
===========================

Sizes
-----

The AVR is an 8 bit architecture: all registers are 8 bits.
(you can see by disassembling a program that adds two uint8_t's and then changing it to work on uint16_t's)

However, the word size is 16, and this is the size of pointers and therefore the address space.
and a double word (dword) is 32 bits.

These types are available as:
uint8_t  - byte
uint16_t - word
uin32_t  - dword

Operations
----------

The AVR only has additions (+) and subtractions (-) in its ALU.  You can multiply,
divide (/) and mod (%) in the C code, but they cause avr-libc's inline'd implementations of those operators to get dropped in. 
It has both < (cmp) and <= (cpi) , though

..wait... there's apparently a "mulsu" instruction. maybe the attiny13a doesn't have it?

Though the architecture is 8 bit, there is an 'addc' instruction which is meant for
performing extended adds (c for 'carry'). avr-gcc knows to use it when computing on words.

The stack is built into the cpu: there's push and pop instructions.


Toolchain
---------

The tools you need for avr are:
* avr-gcc
* avr-binutils
* avr-libc

If you don't know how to use something, read up in [avr-libc](TODO), or just
look in avr-libc's headers, which are probably installed somewhere like /usr/avr/include.

What's up with PROGMEM?
-----------------------

the avr has secondary address space which is meant to be used for constants
 (like .rodata on x86). Because of architectural quirks (i.e. probably stupidness)
 you cannot access this memory directly. Instead you need to use the pgm_read*() macros.

It is possible to have pointers into this memory,
it's just that to dereference them you need to use pgm_read() instead of the * operator.

You cannot place pointers into PROGMEM, because you need to use the & operator to get pointer addresses and that means some code has to run to set that up, so they can't be const.
If you try thise you'll get an obtuse error like this
"take_two.c:163:33: error: variable 'modegroup' must be const in order to be put into read-only section by means of '__attribute__((progmem))'"
 what this is not telling you is that & implicitly makes an expression non-const (even though pointers to globals, by their nature, should be const. le sigh)

I *think* avr-gcc places PROGMEM data into an ELF section named ".stab", but I'm not sure.


