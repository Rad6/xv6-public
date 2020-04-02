#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if(argc > 2){
    printf(2, "too many args!\n");
    exit();
  }
  if(argc < 2){
      printf(2, "enter the value u wanna set!\n");
      exit();
  }
  set_edx(atoi(argv[1]));
  exit();
}
