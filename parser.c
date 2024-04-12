#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>
#include <elf.h>
#include <errno.h>

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
};

/**
 * Routine for checking stack made for child program.
 * top_of_stack: stack pointer that will given to child program as %rsp
 * argc: Expected number of arguments
 * argv: Expected argument strings
 */
void stack_check(void* top_of_stack, uint64_t argc, char** argv) {
	printf("----- stack check -----\n");
    
    printf("top of stack: %p\n", top_of_stack);
	assert(((uint64_t)top_of_stack) % 8 == 0);
	printf("top of stack is 8-byte aligned\n");

	uint64_t* stack = top_of_stack;
	uint64_t actual_argc = *(stack++);
	printf("argc: %lu\n", actual_argc);
	assert(actual_argc == argc);

	for (int i = 0; i < argc; i++) {
		char* argp = (char*)*(stack++);
		printf("arg %d: %s\n", i, argp);
        printf("argv: %s\n", argv[i]);
		assert(strcmp(argp, argv[i]) == 0);
	}
	// Argument list ends with null pointer
	assert(*(stack++) == 0);

	int envp_count = 0;
	while (*(stack++) != 0)
		envp_count++;

	printf("env count: %d\n", envp_count);

	Elf64_auxv_t* auxv_start = (Elf64_auxv_t*)stack;
	Elf64_auxv_t* auxv_null = auxv_start;
	while (auxv_null->a_type != AT_NULL) {
		auxv_null++;
	}
	printf("aux count: %lu\n", auxv_null - auxv_start);
	printf("----- end stack check -----\n");
}

Elf64_Phdr *load_elf_phdrs(Elf64_Ehdr *elf_ex, FILE *elf_file) {
    Elf64_Phdr *elf_phdata = NULL;
	int retval, err = -1;
	unsigned int size;

    size = sizeof(Elf64_Phdr) * elf_ex->e_phnum;
    elf_phdata = malloc(size);
    if (!elf_phdata)
        goto out;

    retval = pread(fileno(elf_file), elf_phdata, size, elf_ex->e_phoff);
    if (retval < 0) {
		err = retval;
		goto out;
	}

    err = 0;
out:
    if (err) {
        free(elf_phdata);
        elf_phdata = NULL;
    }
    return elf_phdata;
}

Elf64_Ehdr *load_elf_ex(FILE *elf_file) {
    Elf64_Ehdr *elf_ex = NULL;
    int retval, err = -1;
    unsigned int size;

    size = sizeof(Elf64_Ehdr);
    elf_ex = malloc(size);
    if (!elf_ex)
        goto out;
    
    // Read ELF header.
    if (retval < 0) {
		err = retval;
		goto out;
	}

    retval = pread(fileno(elf_file), elf_ex, size, 0);
    if (retval < 0) {
		err = retval;
		goto out;
	}

    err = 0;

out:
    if (err) {
        free(elf_ex);
        elf_ex = NULL;
    }
    return elf_ex;
}

unsigned long elf_map(FILE *elf_file, unsigned long addr, Elf64_Phdr *elf_ppnt, int elf_prot, int elf_flags) {
    void *map_addr_ptr = NULL;
    unsigned long map_addr;
    unsigned long zero_start, zero_end;
    unsigned long size = elf_ppnt->p_filesz + ELF_PAGEOFFSET(elf_ppnt->p_vaddr);
	unsigned long off = elf_ppnt->p_offset - ELF_PAGEOFFSET(elf_ppnt->p_vaddr);
	addr = ELF_PAGESTART(addr);
	size = ELF_PAGEALIGN(size);

	if (!size)
		return addr;
    
    // Print mmap args on a single line:
    fprintf(stderr, "mmap(%p, %lu, %d, %x, %d, %lu)\n", (void*) addr, size, (PROT_EXEC | PROT_READ | PROT_WRITE), elf_flags, fileno(elf_file), off);
    fprintf(stderr, "Mapping from %p to %p\n", (void*) addr, (void*) (addr + size));

    map_addr_ptr = mmap((void*) addr, size, (PROT_EXEC | PROT_READ | PROT_WRITE), elf_flags, fileno(elf_file), off);
    
    if (map_addr_ptr == MAP_FAILED) {
        perror("elf_map: Failed to map ELF segment");
        return -1;
    }

    map_addr = (unsigned long) map_addr_ptr;

    if (elf_ppnt->p_memsz > elf_ppnt->p_filesz) {
        fprintf(stderr, "mappadr: %p\n", (void*) map_addr);

        addr = addr + size; // all values page aligned already.
        size = ELF_PAGEALIGN(elf_ppnt->p_memsz - elf_ppnt->p_filesz);

        // Map an anonymous region.
        map_addr_ptr = mmap((void*) addr, size, (PROT_EXEC | PROT_READ | PROT_WRITE), elf_flags | MAP_ANONYMOUS, -1, 0);
        fprintf(stderr, "Mapping from %p to %p\n", (void*) addr, (void*) (addr + size));

        if (map_addr_ptr == MAP_FAILED) {
            perror("elf_map: Failed to map ELF segment");
            return -1;
        }


        zero_start = map_addr + ELF_PAGEOFFSET(elf_ppnt->p_vaddr) + elf_ppnt->p_filesz;
        zero_end = map_addr + ELF_PAGEOFFSET(elf_ppnt->p_vaddr) + elf_ppnt->p_memsz;
        memset((void *) zero_start, 0, zero_end - zero_start + 1);
    }

    return map_addr;
}

int elf_load(FILE *elf_file, unsigned long addr, Elf64_Phdr *elf_ppnt, int elf_prot, int elf_flags) {
	unsigned long map_addr = -1;

    if (elf_ppnt->p_filesz) 
        fprintf(stderr, "FILE SIZE %lu\n", elf_ppnt->p_filesz);
        fprintf(stderr, "MEM SIZE %lu\n", elf_ppnt->p_memsz);
        map_addr = elf_map(elf_file, addr, elf_ppnt, elf_prot, elf_flags);
        if (map_addr == -1) {
            fprintf(stderr, "elf_load: Failed to map ELF segment.\n");
            return -1;
        }

    return map_addr;
}

void copy_stack_args(uintptr_t* cur_stack, char** args, char** args_end) {
    char** string_ptr;
    unsigned long length = 0;
    for (string_ptr = args_end; string_ptr > args;) {
        string_ptr--;
        length = strlen(*string_ptr) + 1;
        *cur_stack -= length;
        memcpy((void*) *cur_stack, *string_ptr, length);
    }
}

size_t calc_stack_size_till_auxv(int argc, char** argv, char** envp, Elf64_auxv_t* auxv, Elf64_auxv_t* auxv_end) {
    size_t size = 0;
    int i;

    // Calculate size of argv.
    for (i = 0; argv[i] != NULL; i++) {
        size += sizeof(char*);
    }
    size += sizeof(char*);

    // Calculate size of envp.
    for (i = 0; envp[i] != NULL; i++) {
        size += sizeof(char*);
    }
    size += sizeof(char*);

    // Calculate size of auxv.
    size += (auxv_end - auxv) * sizeof(Elf64_auxv_t);
    // size += sizeof(Elf64_auxv_t); // not needed anymore?

    // Include size of argc.
    size += sizeof(long);

    return size;
}

void* setup_stack(struct binary_file* fp, unsigned long phdr, unsigned long e_entry, unsigned long e_phnum, unsigned long e_phentsize, unsigned long entry) {
    void* stack_top = malloc(STACK_SIZE);
    void* stack_bottom = (void*) ((unsigned long )stack_top + STACK_SIZE);
    void* sp, *random_region_ptr, *platform_ptr;
    uintptr_t cur_stack = (uintptr_t) stack_bottom;
    int i;

    printf("stack_top: %p\n", stack_top);
    printf("stack_bottom: %p\n", stack_bottom);

    int argc = fp->argc; // argc already decremented.
    char** argv = fp->argv; // argv[0] is the name of the binary to load.
    char** argv_end = &argv[argc];
    char** envp = fp->envp; // envp has been unchanged.

    // Get envp end.
    char** envp_end = envp;
    while (*envp_end != NULL) {
        envp_end++;
    }

    // Get auxv start and end.
    Elf64_auxv_t* auxv = (Elf64_auxv_t*) (envp_end + 1);
    Elf64_auxv_t* auxv_end = auxv;
    while (auxv_end->a_type != AT_NULL) {
        auxv_end++;
    }

    // Copy envp and argv data.
    copy_stack_args(&cur_stack, envp, envp_end);

    // Binary name must be converted from <bin name> to ./<bin name>
    char* bin_name = argv[0];
    char* bin_name_copy = malloc(strlen(bin_name) + 3);
    bin_name_copy[0] = '.';
    bin_name_copy[1] = '/';
    strcpy(bin_name_copy + 2, bin_name);
    argv[0] = bin_name_copy;
    copy_stack_args(&cur_stack, argv, argv_end);

    char* data_start = (char*) (cur_stack);
    cur_stack &= ~0xf;

    // Copy platform string.
    char platform_str[] = "x86_64";
    cur_stack -= (strlen(platform_str) + 1);
    strcpy((char*) cur_stack, platform_str);
    platform_ptr = (void*) cur_stack;

    // Initialize random region.
    cur_stack -= 16;
    random_region_ptr = (void*) cur_stack;
    
    // Set random region data


    cur_stack &= ~0xf;
    printf("cur_stack: %p\n", (void*) cur_stack);

    // Copy argc, argv pointers, envp pointers, auxv.
    size_t remaining_size = calc_stack_size_till_auxv(argc, argv, envp, auxv, auxv_end);
    cur_stack -= remaining_size;

    // Set argc
    *(long*) cur_stack = (long) argc;
    sp = (void*) cur_stack;
    cur_stack += sizeof(long);

    unsigned long offset = 0;
    char* string_ptr;
    // Set argv pointers.
    for (i = 0; argv[i] != NULL; i++) {
        string_ptr = (char*) (data_start + offset);
        ((char**) cur_stack)[i] = string_ptr;
        offset += strlen(string_ptr) + 1;
    }
    ((char**) cur_stack)[i++] = NULL;
    
    // Set envp pointers.
    for (; envp[i - argc] != NULL; i++) {
        string_ptr = (char*) (data_start + offset);
        ((char**) cur_stack)[i] = string_ptr;
        offset += strlen(string_ptr) + 1;
    }

    ((char**) cur_stack)[i++] = NULL;

    // Copy auxv with a for loop, set program specific values as well.
    char* new_auxv_start = (char*) (cur_stack + i * sizeof(char*)); // align after copying data
    fprintf(stderr, "new_auxv_start: %p\n", new_auxv_start);
    for (i = 0; auxv[i].a_type != AT_NULL; i++) {
        ((Elf64_auxv_t*) new_auxv_start)[i] = auxv[i];

        // Convert this into a switch statement:
        switch (auxv[i].a_type) {
            case AT_PHDR:
                ((Elf64_auxv_t*) new_auxv_start)[i].a_un.a_val = phdr;
                break;
            case AT_ENTRY:
                ((Elf64_auxv_t*) new_auxv_start)[i].a_un.a_val = e_entry;
                break;
            case AT_BASE:
                ((Elf64_auxv_t*) new_auxv_start)[i].a_un.a_val = 0;
            case AT_PHNUM:
                ((Elf64_auxv_t*) new_auxv_start)[i].a_un.a_val = e_phnum;
                break;
            case AT_PHENT:
                ((Elf64_auxv_t*) new_auxv_start)[i].a_un.a_val = e_phentsize;
                break;
            case AT_RANDOM:
                ((Elf64_auxv_t*) new_auxv_start)[i].a_un.a_val = (uintptr_t) random_region_ptr;
            case AT_PLATFORM:
                ((Elf64_auxv_t*) new_auxv_start)[i].a_un.a_val = (uintptr_t) platform_ptr;
            case AT_EXECFN:
                ((Elf64_auxv_t*) new_auxv_start)[i].a_un.a_val = (uintptr_t) argv[0];
            default:
                break;
        }
    }
    printf("cur_stack: %p, %s\n", (void*) cur_stack, *((char**) cur_stack));
    ((Elf64_auxv_t*) new_auxv_start)[i] = auxv[i];
    ((Elf64_auxv_t*) new_auxv_start)[i].a_type = AT_NULL;
    printf("address: %p\n", &((Elf64_auxv_t*) new_auxv_start)[i]);

    stack_check(sp, argc, argv);

    return sp;
}

int load_elf_binary(struct binary_file* fp) {
    FILE *elf_file = fp->elf_file;
	Elf64_Phdr *elf_ppnt, *elf_phdata = NULL;
	Elf64_Ehdr *elf_ex = NULL;
	unsigned long phdr_addr = 0;
	int first_pt_load = 1;
	int retval, i, error;

    // Read ELF header.
    elf_ex = load_elf_ex(elf_file);
    if (!elf_ex) {
        fprintf(stderr, "load_elf_binary: Failed to read ELF header.\n");
        return -1;
    }

    // Read program headers.
    elf_phdata = load_elf_phdrs(elf_ex, elf_file);
    if (!elf_phdata) {
        fprintf(stderr, "load_elf_binary: Failed to read ELF program headers.\n");
        return -1;
    }

    // Start line 1024 in binfmt_elf.c
    // First loaded segment shouldn't have MAP_FIXED, rest should.
    elf_ppnt = elf_phdata;
    for (i = 0; i < elf_ex->e_phnum; i++, elf_ppnt++) {
        int elf_prot, elf_flags;

        if (elf_ppnt->p_type != PT_LOAD)
            continue;
            
        fprintf(stderr, "LOADING SEGMENT: %d\n", elf_ppnt->p_type);

	    if (elf_ppnt->p_flags & PF_R)
			elf_prot |= PROT_READ;
        if (elf_ppnt->p_flags & PF_W)
            elf_prot |= PROT_WRITE;
        if (elf_ppnt->p_flags & PF_X)
            elf_prot |= PROT_EXEC;
        
        elf_flags = MAP_PRIVATE; // | MAP_DENYWRITE | MAP_EXECUTABLE

        if (!first_pt_load) {
            elf_flags |= MAP_FIXED;
        }
        
        error = elf_load(elf_file, elf_ppnt->p_vaddr, elf_ppnt, elf_prot, elf_flags);

        if (first_pt_load) first_pt_load = 0;

        // Find segment w/ Program Header Table, map to the correct address.
        // Need this address for stack setup later.
	    if (elf_ppnt->p_offset <= elf_ex->e_phoff && elf_ex->e_phoff < elf_ppnt->p_offset + elf_ppnt->p_filesz) {
			phdr_addr = elf_ex->e_phoff - elf_ppnt->p_offset +
				    elf_ppnt->p_vaddr;
		}
    }

    free(elf_phdata);

    printf("\n\nSETTING UP STACK:\n\n");
    char* sp = setup_stack(fp, phdr_addr, elf_ex->e_entry, elf_ex->e_phnum, elf_ex->e_phentsize, elf_ex->e_entry); // stack stuff.

    if(fclose(elf_file)) {
        fprintf(stderr, "elf_load_binary: failed to close elf executable.\n");
        return -1;
    }


    asm volatile(
        "mov %0, %%rsp\n"
        "mov %1, %%rax\n"
        "jmp *%%rax\n"
        :
        : "r" (sp), "r" (elf_ex->e_entry)
    );

    return 0;
}

struct binary_file *parse_file(int argc, char** argv, char** envp) {
    struct binary_file* fp = malloc(sizeof(struct binary_file));
    if (fp == NULL) {
        fprintf(stderr, "parse_file: Failed to allocate memory for binary file.\n");
        return NULL;
    }

    fp->argc = --argc;
    fp->argv = &argv[1];
    fp->envp = envp;
    fp->elf_file = fopen(argv[1], "r+");
    if (fp->elf_file == NULL) {
        fprintf(stderr, "parse_file: Failed to open executable file.\n");
        return NULL;
    }

    return fp;
}

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