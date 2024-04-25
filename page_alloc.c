#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
    char* p = (char*) malloc(4096);
    printf("Hello, World!\n");
    strcpy(p, "Hello, World!");
    printf("Pointer: %s\n", p);
    return 0;
}