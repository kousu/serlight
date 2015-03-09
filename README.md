Serifus's Magical Dual-Mode AVR-controlled Flashlight
=====================================================

Our starting point was Selene Scriven (@toykeeper)'s [GPL flashlight firmware](https://code.launchpad.net/~toykeeper/flashlight-firmware/).


Requirements
===========

The avr toolchain:
```
sudo pacman -S avrdude avr-gcc avr-libc avr-binutils
```

avr-gcc and friends is the cross-compiler. avrdude is for flashing the chip.
You might also want simavr and avr-gdb but that's up to you.
(and make sure you have make installed, of course)

To test that you've gotten the toolchain working just use the Makefile:

```
[kousu@galleon serifuslight]$ make
avr-gcc -Wall -mmcu=attiny13      -std=c99  -g -Os    -o serlight.o -c serlight.c
serlight.c: In function 'save':
serlight.c:212:11: warning: cast to pointer from integer of different size [-Wint-to-pointer-cast]
     eepos=(uint8_t*)cycle((uint16_t)eepos, EEPROM_SIZE);
           ^
serlight.c: At top level:
serlight.c:453:6: warning: return type of 'main' is not 'int' [-Wmain]
 void main() { //returning void saves a couple bytes (it's not like there's anyone to return to)
      ^
avr-gcc -Wall -mmcu=attiny13      -std=c99  -g -Os     -o serlight.elf serlight.o
avr-size -C --mcu=attiny13      serlight.elf
AVR Memory Usage
----------------
Device: attiny13

Program:     942 bytes (92.0% Full)
(.text + .data + .bootloader)

Data:         35 bytes (54.7% Full)
(.data + .bss + .noinit)


avr-objcopy -O ihex serlight.elf serlight.hex
rm serlight.o serlight.elf
[kousu@galleon serifuslight]$ 
```

If you see anything else you're probably missing a package or three.



