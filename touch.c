#include "fcntl.h"
#include "types.h"
#include "stat.h"
#include "user.h"

char buf[1024];

int
main(int argc, char *argv[]){
    /*
        touch file.txt || touch -w file.txt
    */
    int fd0;
    if(argc < 2){
       printf(1, "touch : enter name of the file u wanna create!\n");
       exit();
    }
    if(argc == 2){
        if(strcmp(argv[1], "-w") == 0){
            printf(1, "touch : you have to enter the file name too!\n");
            exit();
        }
        // make the file!
        if(argv[1][0] == '-'){
            printf(1, "touch : undefined flag!!\n");
            exit();
        }
        if((fd0 = open(argv[1], O_CREATE)) < 0){
            printf(1, "touch : cannot create %s!\n", argv[1]);
            exit();
        }
        printf(1, "touch : done\n");
        close(fd0);
        exit();

    }
    if(argc == 3){
        if(strcmp(argv[1], "-w") == 0){
            if((fd0 = open(argv[2], O_CREATE|O_RDWR)) < 0){
                printf(1, "touch : cannot open or create the %s!\n", argv[1]);
                exit();
            }
            printf(1, "add ur text:\n");
            read(0, buf, sizeof(buf));
            write(fd0, buf, sizeof(buf));
            close(fd0);
            exit();
        }
        else{
            printf(1, "touch : undefined flag!\n");
            exit();
        }
    }
    printf(1, "touch : !\n");
    exit();
}