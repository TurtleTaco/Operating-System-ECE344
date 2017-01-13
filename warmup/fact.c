#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int factorial (int n){
    if (n >= 1)
        return n*factorial(n-1);
    else
        return 1;
}

int main(int argc, char* argv[]) {
    char zero[] = "0";
    char **input = argv;
    input++;
    char* index;
    float input_float = 1.0;
    int input_integer = 1;
    /*if (* argv[1] == zero[0])
        printf("ok\n");*/
    /*for (input = 1; input < argc; input++){
        printf("%s\n", argv[input]);
    }*/
    if (argc == 1){
        printf("Huh?\n");
        return 0;
    }
    long int converted_num = strtol(*input, &index, 10);
    converted_num++;
    int identify = atoi(index);
    //printf("!!%d\n", *index);
    if (*index != 0){
    if (identify == 0){
       printf("Huh?\n");
       return 0;
    }}
    if (*argv[1] == zero[0]){
        printf("Huh?\n");
        return 0;
    }
    input_integer = atoi(argv[1]); //returns 0 if no conversion is being performed
    input_float = atof(argv[1]);
    //printf("--%d\n", input_integer);
    if (input_integer < 0){
        printf("Huh?\n");
        return 0;
    }
    float floor_num;
    floor_num  = floor(input_float);
    if (floor_num < input_float){
        printf("Huh?\n");
        return 0;
    }
    if (input_float > 12){
        printf("Overflow\n");
        return 0;
    }
    if (input_integer == 0)
        printf("Huh?\n");
    else printf("%d\n",factorial(input_integer));
    return 0;
}
