#include "common.h"

int
main(int argc, char *argv[])
{   
        int i = 1;
        //printf("size is %d \n", argc);
	for (i = 1; i < argc; i++){
            printf("%s\n", argv[i]);
        }
        return 0;
}
