// init: The initial user-level program

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

char *argv[] = { "sh", 0 };

int
main(void)
{
  int pid, wpid;
  // int alarmhandler_pid;

  if(open("console", O_RDWR) < 0){
    mknod("console", 1, 1);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr

  for(;;){
    printf(1, "init: starting sh\n");
    pid = fork();
    if(pid < 0){
      printf(1, "init: fork failed\n");
      exit();
    }
    // if (pid > 0)
    // {
    //   alarmhandler_pid = fork();
    //   if (alarmhandler_pid < 0)
    //   {
    //     printf(1, "init: fork alarm handler failed\n");
    //     exit();
    //   }
    //   if (alarmhandler_pid == 0)
    //     handle_alarms();
    // }
    if(pid == 0){
      exec("sh", argv);
      printf(1, "init: exec sh failed\n");
      exit();
    }
    printf(1, "Houmaan Chamani - Kiarash Norouzi - Hadi Heidari Rad\n");
    while((wpid=wait()) >= 0 && wpid != pid)
      printf(1, "zombie!\n");
  }
}
