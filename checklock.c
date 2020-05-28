#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if(argc < 2){
    printf(2, "checklock: enter the number of processes u want.\n");
    exit();
  }
  int nof_proc = atoi(argv[1]);
  int is_dady = 1;
  for(int i = 0; i < nof_proc; i++){
    int fk = fork();
    if(fk == 0){
      is_dady = 0;
      check_lock();
      break;
    }
    else{
      if(i == 0)
      check_lock();
    }
  }

  if(is_dady){
    for(int i = 0; i < nof_proc; i++)
      wait();
  }
  exit();
}
