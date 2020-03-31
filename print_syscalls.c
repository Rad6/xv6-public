#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if(argc > 1){
    printf(2, "too many args!\n");
    exit();
  }
  print_syscalls();
  exit();
}
