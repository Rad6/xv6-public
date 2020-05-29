#include "types.h"
#include "stat.h"
#include "user.h"

#define NCHILDS 4 

int
main(int argc, char *argv[])
{

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
    
    write(1, "Writing to file ...\n", strlen("Writing to file ...\n") );
    write(1, "Writing to file ...\n", strlen("Writing to file ...\n") );
    write(1, "Writing to file ...\n", strlen("Writing to file ...\n") );

    if (is_father)
    {
        for (int i = 0; i < NCHILDS; i++)
            wait();
        printf(1, "\n");
        sys_count();
    }    
    exit();
}