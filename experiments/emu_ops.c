/* sizeof.c: testing the sizes of various data structures under avr
 *
 * compile with:
 * `make emu_ops.elf`
 *
 * to actually work out results i'll have to disassemble since it's not like i can actually run 
 * inspect with:
 * `avr-objdump -D emu_ops.elf` and look for "main"
 */





//#include <stdio.h>
#include <inttypes.h>

int 
//uint8_t
//uint16_t
//void
main(void) {
  
  uint8_t a, b;
  a = 77;
  b = 51;

/*
  uint16_t a, b;
  a = 277;
  b = 500;
  //^ this works by the compiler loading the variables into two registers and using two operations: "add" and then "addc" (which, presumably, is "add with carry")
*/

  //a = a+b;
  //a = a * b;
  return 0;
}
