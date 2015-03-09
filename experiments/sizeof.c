/* sizeof.c: testing the sizes of various data structures under avr
 *
 * compile with:
 * `make sizeof.elf`
 *
 * to actually work out results i'll have to disassemble since it's not like i can actually run 
 * inspect with:
 * `avr-objdump -D sizeof.elf` and look for where "main" pushes values to the stack
 */

#include <stdio.h>


struct {
  uint8_t controlling:4; //which light we are currently controlling (i.e. cycling through the modes of) 
  uint8_t offering_switch:4;  //if we are offering to switch modes (indicated by blinking the light we would switch to)
} state = {
  1, 0
};

void (*func_void)(void);
void (*func_1p)(void*);

int main(void) {
  printf("%d LA LA LA \n", sizeof(uint8_t)); //1 byte
  printf("%d LA LA LA \n", sizeof(uint16_t)); //2 bytes
  printf("%d LA LA LA \n", sizeof(uint32_t)); //4 bytes
  printf("%d LA LA LA \n", sizeof(func_void)); //2 bytes
  printf("%d LA LA LA \n", sizeof(func_1p)); //2 bytes
  printf("%d LA LA LA \n", sizeof(state)); //1 byte as expected
  return 0; //can I ax this?
}
