#include "types.h"
#include "stat.h"
#include "user.h"

#define CHILD_NUM 20

int
main(int argc, char *argv[])
{
    int is_father = 0;
    int is_child = 0;
    for (int i = 0; i < CHILD_NUM; i++)
    {
        int pid;
        pid = fork();
        if (pid == 0)
        {
            is_child = 1;
            break;
        }
        if (pid > 0)
        {
            is_father = 1;
            printf(1,"child with pid: %d created.\n", pid);
            set_proc_level(pid, i < CHILD_NUM/2 ? 1 : 2);
        }
    }
    if (is_child)
    {
        is_father = 0;
        int nokhod_siah = 0;
        for (int i = 0; i < 10000*getpid(); i++)
        {
            nokhod_siah += i;
            nokhod_siah -= i/2;
        }
        exit();
    }

    if (is_father)
        for (int i = 0; i < CHILD_NUM; i++)
            wait();

    printf(1, "Done! \n");

    exit();
}
