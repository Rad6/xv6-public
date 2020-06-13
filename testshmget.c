#include "types.h"
#include "stat.h"
#include "user.h"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

void
help(){
    printf(1, "\nUse:\n\
    'testshmget fork'\n\t\
    or\n\
    'testshmget simple read'\n\t\
    or\n\
    'testshmget simple write'\n\n");
}

int
main(int argc, char *argv[])
{
    if(argc < 2 || argc > 3){
        printf(1, "\ntestshmget: Wrong format to call!\n");
        help();
        exit();
    }
    if(strcmp(argv[1], "fork") == 0){
        printf(1, "fork is chosen\n");
        shmget(1); // write and read
        shmget(2); // read
        int fk = fork();
        if(fk == 0){
            sleep(20);
            shmget(3); // read
        } else {
            
            wait();
            shmget(4);
        }
        exit();
    }
    if(strcmp(argv[1], "simple") == 0){
        if(strcmp(argv[2], "read") == 0){
            shmget(2); // read
            exit();
        }
        if(strcmp(argv[2], "write") == 0){
            shmget(1); // write and read
            shmget(2); // read 
            exit();
        }
    }
    help();
    exit();
}
