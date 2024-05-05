#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <elf.h>
#include <stdio.h>

#define STACK_SIZE 8192
#define ELF_MIN_ALIGN	4096
#define ELF_PAGESTART(_v) ((_v) & ~(int)(ELF_MIN_ALIGN-1))
#define ELF_PAGEALIGN(_v) (((_v) + ELF_MIN_ALIGN - 1) & ~(ELF_MIN_ALIGN - 1))
#define ELF_PAGEOFFSET(_v) ((_v) & (ELF_MIN_ALIGN - 1))

struct binary_file {
    int argc;
    char** argv;
    char** envp;
    FILE* elf_file;
    Elf64_Ehdr* elf_ex;
    Elf64_Phdr* elf_phdata;
};

uintptr_t load_elf_binary(struct binary_file* fp);

struct binary_file *parse_file(int argc, char** argv, char** envp);

int padzero(unsigned long elf_bss);
