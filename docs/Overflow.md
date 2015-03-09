Sources of Overflow in avr programs
===================================

the attinty13a has 64 bytes of RAM (i.e. working memory), 64 bytes of EEPROM (i.e. disk), and 1024K of ROM (i.e. where your program goes).
So it requires careful microoptimizations to fit everything in.


AVR programs are statically linked (because there's no kernel or loader to help)
so every library routine you call bloats the final executable.
(but not every library routine you *include*: only ones you actually call)

Further, using int16 adds adds a few (~4 per op) because it uses two registers instead of one and has to handle carry

Further, multiplications cause a library routine to fall in, because the CPU's ALU doesn't have multiplication (LOL).

Returning 'int' from main causes extra bytes..

Returning from main at all causes an extra few instructions. Just set main to void to save bytes.

Inlining functions bloats the code (duh). Weirdly
*except* if a function is only called once you can inline it to save the rcall and reti instructions (and associated stack fiddling)

On the teeny, const data needs to be put in the "PROGMEM" section, which is a special part in the program code section
And you need to access this with the pgm_*() macros.
The total data sections get loaded into RAM of which you only have 64 bytes so if you overflow this with defines you're fucked.

Apparently, arrays are allocated in the .data section in blocks 2bytes at a time, so you might be able to squeeze a few more bytes by rearranging how you store data

TODO:
* [ ] is it more expensive to use a bitfield struct or to manually compress and decompress? 
     the bitfield is less C to write but that might be hiding a lot of asm code to actually make it work
     then again, it might not actually make a difference
* [ ] investigate if I can save space by making each mode's array separate


#-----------------------------
with %64

this is (e+1)%64, under gcc's optimizer
[...]
  56:   90 e0           ldi     r25, 0x00       ; 0
  58:   01 96           adiw    r24, 0x01       ; 1
  5a:   8f 73           andi    r24, 0x3F       ; 63
  5c:   99 27           eor     r25, r25   //wtf??


this is (e+1)%63, under gcc's optimizer
00000048 <save>:
[...]
  56:   8f 5f           subi    r24, 0xFF       ; 255 //-255 == +1, for an 8 bit int
  58:   8d 70           andi    r24, 0x0D       ; 13
[...]