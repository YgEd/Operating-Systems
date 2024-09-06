#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>

int main(){
    char too_long[] = "This message is way over 32 characters long right?";
    char msg[] = "aeiou";
    int out;

    //should error 
    printf("size of too_long is %d\n", sizeof(too_long));
    out = syscall(441, too_long);
    if (out == -1){
        printf("syscall failed with return value %d\n", out);
        printf("input string is:  %s\n", too_long);
    }

    printf("\n");
    
    //should work
    printf("string before syscall: %s\n", msg);
    out = syscall(441, msg);
    if (out == -1){
        printf("syscall failed with return value %d\n", out);
        printf("input string is:  %s\n", msg);
        return 0;
    }
    printf("syscall returned with value %d\n", out);
    printf("string after syscall: %s\n", msg);

    

    return 0;
}