#include "types.h"
#include "user.h"


int main(int argc,char* argv[]) 
{
    if (argc < 2)
    {
        printf(1, "input arguement missing \n");
        exit();
    }
    set_alarm(atoi(argv[1]));
    exit();
}
