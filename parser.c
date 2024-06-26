#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>
#include <elf.h>
#include <errno.h>

#include "parser.h"

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
    unsigned long size = elf_ppnt->p_filesz + ELF_PAGEOFFSET(elf_ppnt->p_vaddr); // since we want to allocate from the page right before vaddr, add the page offset
	unsigned long off = elf_ppnt->p_offset - ELF_PAGEOFFSET(elf_ppnt->p_vaddr); // since we are moving the segment to the start of the page before vaddr, subtract page offset from absolute offset.
	addr = ELF_PAGESTART(addr);
	size = ELF_PAGEALIGN(size);

	if (!size)
		return addr;
    
    // Print mmap args on a single line:
    fprintf(stderr, "mmap(%p, %lu, %d, %x, %d, %lu)\n", (void*) addr, size, (PROT_EXEC | PROT_READ | PROT_WRITE), elf_flags, fileno(elf_file), off);
    map_addr_ptr = mmap((void*) addr, size, (PROT_EXEC | PROT_READ | PROT_WRITE), elf_flags, fileno(elf_file), off);
    fprintf(stderr, "Mapping from %p to %p\n", map_addr_ptr, (void*) (((unsigned long) (map_addr_ptr)) + size));
    
    if (map_addr_ptr == MAP_FAILED) {
        perror("elf_map: Failed to map ELF segment");
        return -1;
    }

    map_addr = (unsigned long) map_addr_ptr;

    if (elf_ppnt->p_memsz > elf_ppnt->p_filesz) {
        fprintf(stderr, "mapping bss\n");

        addr = addr + size; // all values page aligned already.
        size = ELF_PAGEALIGN(elf_ppnt->p_vaddr + elf_ppnt->p_memsz) - ELF_PAGEALIGN(elf_ppnt->p_vaddr + elf_ppnt->p_filesz);

        // Map an anonymous region.
        map_addr_ptr = mmap((void*) addr, size, (PROT_EXEC | PROT_READ | PROT_WRITE), elf_flags | MAP_ANONYMOUS, -1, 0);
        fprintf(stderr, "Mapping from %p to %p with permissions %x\n", (void*) addr, (void*) (addr + size), (elf_flags | MAP_ANONYMOUS));

        if (map_addr_ptr == MAP_FAILED) {
            perror("elf_map: Failed to map ELF segment");
            return -1;
        }

        zero_start = elf_ppnt->p_vaddr + elf_ppnt->p_filesz;
        zero_end = (addr + size);
        memset((void *) zero_start, 0, zero_end - zero_start);
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

    // Include size of argc.
    size += sizeof(long);

    return size;
}

void* setup_stack(struct binary_file* fp, unsigned long phdr, unsigned long e_entry, unsigned long e_phnum, unsigned long e_phentsize, unsigned long entry) {
    void* stack_top = malloc(STACK_SIZE);
    void* stack_bottom = (void*) (((unsigned long)stack_top) + STACK_SIZE);
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

        switch (auxv[i].a_type) {
            case AT_PHDR:
                ((Elf64_auxv_t*) new_auxv_start)[i].a_un.a_val = 0;
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
                ((Elf64_auxv_t*) new_auxv_start)[i].a_un.a_val = sizeof(Elf64_Phdr);
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

int padzero(unsigned long elf_bss) {
    unsigned long nbyte;

    nbyte = ELF_PAGEOFFSET(elf_bss);
    if (nbyte) {
        nbyte = ELF_MIN_ALIGN - nbyte;
        memset((void*)elf_bss, 0, nbyte);
    }
    return 0;
}

uintptr_t load_elf_binary(struct binary_file* fp) {
    FILE *elf_file = fp->elf_file;
	Elf64_Ehdr *elf_ex = fp->elf_ex;
    Elf64_Phdr *elf_phdata = fp->elf_phdata;
	Elf64_Phdr *elf_ppnt = NULL;
	unsigned long phdr_addr = 0;
	int first_pt_load = 1;
	int retval, i, error;

    unsigned long elf_bss = 0, elf_brk = 0;
    int bss_prot = 0;

    // Start line 1024 in binfmt_elf.c
    // First loaded segment shouldn't have MAP_FIXED, rest should.
    elf_ppnt = elf_phdata;
    for (i = 0; i < elf_ex->e_phnum; i++, elf_ppnt++) {
        int elf_prot, elf_flags;

        if (elf_ppnt->p_type != PT_LOAD)
            continue;

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
#ifdef APAGER
        error = elf_load(elf_file, elf_ppnt->p_vaddr, elf_ppnt, elf_prot, elf_flags);
#elif defined(DPAGER)
        // DPAGER code here.
        printf("loading page\n");
        int fd = fileno(elf_file);
        unsigned long off = elf_ppnt->p_offset - ELF_PAGEOFFSET(elf_ppnt->p_vaddr);
        unsigned long addr = ELF_PAGESTART(elf_ppnt->p_vaddr);
        unsigned long size = 4096;
        off += addr - ELF_PAGESTART(elf_ppnt->p_vaddr);
        if (mmap((void*) addr, size, elf_prot,  MAP_PRIVATE | MAP_FIXED | MAP_EXECUTABLE, fd, off) == MAP_FAILED) {
            printf("load_elf_binary: mmap failed: %s\n", strerror(errno));
            exit(-1);
        }

        unsigned long k;
        k = elf_ppnt->p_vaddr + elf_ppnt->p_filesz;
        if (k > elf_bss) {
            elf_bss = k;
        }
        k = elf_ppnt->p_vaddr + elf_ppnt->p_memsz;
        if (k > elf_brk) {
            bss_prot = elf_prot;
            elf_brk = k;
        }
#elif defined(HPAGER)
        // HPAGER code here.
        
#endif

        if (first_pt_load) first_pt_load = 0;
        // Find segment w/ Program Header Table, map to the correct address.
        // Need this address for stack setup later.
	    if (elf_ppnt->p_offset <= elf_ex->e_phoff && elf_ex->e_phoff < elf_ppnt->p_offset + elf_ppnt->p_filesz) {
			phdr_addr = elf_ex->e_phoff - elf_ppnt->p_offset +
				    elf_ppnt->p_vaddr;
		}
    }

    // printf("\nSETTING UP STACK:\n\n");
    char* sp = setup_stack(fp, phdr_addr, elf_ex->e_entry, elf_ex->e_phnum, elf_ex->e_phentsize, elf_ex->e_entry); // stack stuff.

    asm volatile(
        "mov %0, %%rsp\n"
        "mov %1, %%rax\n"
        "xorq %%rdx, %%rdx\n" // glibc segfaults if this reg is not zeroed out 💀.
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
    
    // Open elf_file.
    fp->elf_file = fopen(argv[1], "r+");
    if (fp->elf_file == NULL) {
        fprintf(stderr, "parse_file: Failed to open executable file.\n");
        return NULL;
    }

    // Read ELF header.
    fp->elf_ex = load_elf_ex(fp->elf_file);
    if (!fp->elf_ex) {
        fprintf(stderr, "parse_file: Failed to read ELF header.\n");
        return NULL;
    }

    // Read program headers.
    fp->elf_phdata = load_elf_phdrs(fp->elf_ex, fp->elf_file);
    if (!fp->elf_phdata) {
        fprintf(stderr, "parse_file: Failed to read ELF program headers.\n");
        return NULL;
    }

    printf("file: %p, ex: %p, ph: %p\n", fp->elf_file, fp->elf_ex, fp->elf_phdata);

    return fp;
}