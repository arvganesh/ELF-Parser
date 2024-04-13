#include "parser.h"
#include <stdio.h>

#define APAGER

int main(int argc, char** argv, char** envp) {
    if (argc == 1) {
        fprintf(stderr, "main: No program specified.\n");
        exit(EXIT_FAILURE);
    }

    struct binary_file* fp = NULL;
    fp = parse_file(argc, argv, envp);
    if (fp == NULL) {
        fprintf(stderr, "main: Failed to parse file.\n");
        exit(EXIT_FAILURE);
    }

    if (load_elf_binary(fp) != 0) {
        fprintf(stderr, "main: Loading ELF binary failed.\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}