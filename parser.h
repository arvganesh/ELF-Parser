#include <stdlib.h>
#include <stdint.h>

struct binary_file; /* defined in parser.c */

uintptr_t load_elf_binary(struct binary_file* fp);

struct binary_file *parse_file(int argc, char** argv, char** envp);



