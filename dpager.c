#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include <elf.h>
#include <assert.h>

#include "parser.h"

struct binary_file* fp = NULL;
int counter = 0;

Elf64_Phdr *find_fault_hdr(void* fault_addr_ptr) {
    uintptr_t fault_addr = (uintptr_t) fault_addr_ptr;
    Elf64_Phdr *elf_phdata = fp->elf_phdata;
    Elf64_Ehdr *elf_ex = fp->elf_ex;
    Elf64_Phdr *elf_it = elf_phdata;
    int i;
    uintptr_t seg_start, seg_end;

    // printf("elf ex: %p\n", fp->elf_ex);
    // printf("elf phdata: %p\n", fp->elf_phdata);
    // printf("elf files: %p\n", fp->elf_file);

    // Linear search for segment containing faulting address.
    for (i = 0; i < elf_ex->e_phnum; i++, elf_it++) {
        seg_start = elf_it->p_vaddr;
        seg_end = elf_it->p_vaddr + elf_it->p_memsz; 

        if (elf_it->p_type != PT_LOAD)
            continue;

        if (seg_start <= fault_addr && fault_addr <= seg_end) {
            break;
        }
    }

    if (i == elf_ex->e_phnum) {
        printf("no pheader found\n");
    }

    return elf_it;
}

int allocate_page(Elf64_Phdr *fault_hdr, void* fault_addr_ptr) {
    uintptr_t fault_addr = (uintptr_t) fault_addr_ptr;
    uintptr_t segment_start = ELF_PAGESTART(fault_hdr->p_vaddr);
    uintptr_t fault_page_start = ELF_PAGESTART(fault_addr);
    uintptr_t fault_page_end = fault_page_start + 4096;
    unsigned long bss_start = fault_hdr->p_vaddr + fault_hdr->p_filesz;

    int prot = 0;

    if (fault_hdr->p_flags & PF_R) {
        // printf("r");
        prot |= PROT_READ;
    }
    if (fault_hdr->p_flags & PF_W) {
        prot |= PROT_WRITE;
        // printf("w");
    }
    if (fault_hdr->p_flags & PF_X) {
        prot |= PROT_EXEC;
        // printf("e");
    }  
    // printf("\n");

    void *map_addr_ptr = NULL;

    // Entire faulting page falls in BSS, allocate anonymous region.
    if (fault_hdr->p_memsz > fault_hdr->p_filesz && fault_page_start >= bss_start) {
        map_addr_ptr = mmap((void*) fault_page_start, 4096, prot, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        if (map_addr_ptr == MAP_FAILED) {
            perror("allocate_page: bss mmap failed\n");
            return -1;
        }

        // entire allocated page falls in BSS.
        memset(map_addr_ptr, 0, 4096);
        return 0;
    }

    assert(fault_page_start >= segment_start);
    // Faulting address is mapped to file.
    // maint that fault_hdr->p_offset maps to fault_hdr->p_vaddr
    unsigned long off = fault_hdr->p_offset + fault_page_start - fault_hdr->p_vaddr;
    printf("file-backed mapping: file %d, offset %lu, size %lu, address %p\n", fileno(fp->elf_file), off, 4096, (void*)fault_page_start);
    map_addr_ptr = mmap((void*) fault_page_start, 4096, PROT_EXEC | PROT_READ | PROT_WRITE, MAP_PRIVATE, fileno(fp->elf_file), off);
    if (map_addr_ptr == MAP_FAILED) {
        printf("mapping failed\n");
        return -1;
    }

    // pad zeroes if part of the mapped page falls in the bss.
    if (fault_hdr->p_memsz > fault_hdr->p_filesz && fault_page_start <= bss_start && bss_start <= fault_page_end) {
        memset((void*) bss_start, 0, fault_page_end - bss_start);
    }

    return 0;
}

void demand_pager(int signal, siginfo_t *si, void *arg) {
    if (si->si_code != SEGV_MAPERR) {
        printf("hi\n");
        struct sigaction sa = {0};
        sigemptyset(&sa.sa_mask);
        sa.sa_handler = SIG_DFL;
        int ret = sigaction(SIGSEGV, &sa, NULL);
        if (ret == -1) {
            printf("demand_pager: %s\n", strerror(errno));
            exit(-1);
        }
    } else {
        Elf64_Phdr *fault_hdr = find_fault_hdr(si->si_addr);
        if (allocate_page(fault_hdr, si->si_addr) == -1) {
            printf("demand_pager: %s\n", strerror(errno));
            exit(-1);
        }
    }
}

void install_segfault_handler() {
    struct sigaction sa = {0};
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = demand_pager;
    sa.sa_flags = SA_SIGINFO;
    int ret = sigaction(SIGSEGV, &sa, NULL);
    if (ret == -1) {
        printf("install_segfault_handler: %s\n", strerror(errno));
        exit(-1);
    }
}

int main(int argc, char** argv, char** envp) {
    if (argc == 1) {
        printf( "main: No program specified.\n");
        exit(EXIT_FAILURE);
    }

    fp = parse_file(argc, argv, envp);
    if (fp == NULL) {
        printf( "main: Failed to parse file.\n");
        exit(EXIT_FAILURE);
    }
    
    install_segfault_handler();

    if (load_elf_binary(fp) != 0) {
        printf( "main: Loading ELF binary failed.\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}