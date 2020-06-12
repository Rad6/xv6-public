#include "types.h"
#include "stat.h"
#include "user.h"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
int
main(int argc, char *argv[])
{
    int* ptr = (int*)0x0;
    *ptr = 12;
    exit();
}
