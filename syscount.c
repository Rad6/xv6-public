#include "types.h"
#include "stat.h"
#include "user.h"

#define NCHILDS 4 

int
main(int argc, char *argv[])
{
    write(1, "Writing to file ...\n", strlen("Writing to file ...\n") );
    write(1, "Writing to file ...\n", strlen("Writing to file ...\n") );
    write(1, "Writing to file ...\n", strlen("Writing to file ...\n") );
    
    int is_father = 1;

    for (int i = 0; i < NCHILDS; i++)
    {
        int pid = fork();
        if (pid == 0)
        {
            is_father = 0;
            break;
        }
    }
    
    if (is_father)
    {
        sys_count();
        for (int i = 0; i < NCHILDS; i++)
            wait();
    }    
    exit();
}