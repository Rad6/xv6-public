#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if(argc < 2){
    printf(2, "enter ur digit too!\n");
    exit();
  }
  int _arg = atoi(argv[1]);
  int count, last_reg_value;

  asm volatile(
    "movl %%ebx, %0;" // save the prev value of ebx register
    "movl %1, %%ebx;"
    : "=r" (last_reg_value) //OUTPUT
    : "r"(_arg) // INPUT
    : "%ebx" // make GCC not to use it for optimization stuff
  );


  if((count = count_of_digit(_arg)) < 0){
    printf(2, "count_of_digit: failed to count!\n");
    exit();
  }

  printf(2, "number of digits: %d\n", count);

  asm volatile(
    "movl %0, %%ebx" 
    : // OUTPUT
    : "r"(last_reg_value) // INPUT
  ); // make ebx back to the prev value

  exit();
}
