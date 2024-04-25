#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <elf.h>
#include <assert.h>

#include "parser.h"

struct binary_file* fp = NULL;

Elf64_Phdr *find_fault_hdr(void* fault_addr_ptr) {
    uintptr_t fault_addr = (uintptr_t) fault_addr_ptr;
    Elf64_Phdr *elf_phdata = fp->elf_phdata;
    Elf64_Ehdr *elf_ex = fp->elf_ex;
    Elf64_Phdr *elf_it = elf_phdata;
    int i;
    uintptr_t page_start, page_end;

    fprintf(stderr, "calling fault hdr\n");

    // Linear search for segment containing faulting address.
    for (i = 0; i < elf_ex->e_phnum; i++, elf_it++) {
        if (elf_it->p_type != PT_LOAD)
            continue;
        
        page_start = ELF_PAGESTART(elf_it->p_vaddr);
        page_end = ELF_PAGEALIGN(elf_it->p_vaddr + elf_it->p_memsz);

        fprintf(stderr, "page_start: %p, page_end: %p, fault_addr: %p\n", (void*) page_start, (void*) page_end, (void*) fault_addr);

        if (page_start <= fault_addr && fault_addr < page_end) {
            break;
        }
    }
    assert(i < elf_ex->e_phnum);
    return elf_it;
}

int allocate_page(Elf64_Phdr *fault_hdr, void* fault_addr_ptr) {
    uintptr_t fault_addr = (uintptr_t) fault_addr_ptr;
    uintptr_t segment_start = ELF_PAGESTART(fault_hdr->p_vaddr);
    uintptr_t fault_page_start = ELF_PAGESTART(fault_addr);
    unsigned long segment_start_file_off = fault_hdr->p_offset - ELF_PAGEOFFSET(fault_hdr->p_vaddr);
    unsigned long bss_start = ELF_PAGEALIGN(segment_start + fault_hdr->p_filesz);
    void *map_addr_ptr = NULL;

    // Faulting address falls in BSS, allocate anonymous region.
    if (fault_hdr->p_memsz > fault_hdr->p_filesz && fault_addr > bss_start) {
        map_addr_ptr = mmap((void*) fault_page_start, 4096, (PROT_EXEC | PROT_READ | PROT_WRITE), MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (map_addr_ptr == MAP_FAILED) {
            perror("allocate_page: bss mmap failed\n");
            return -1;
        }

        memset(map_addr_ptr, 0, 4096); // zero out the page.
        return 0;
    }

    assert(fault_page_start >= segment_start);

    // Faulting address is mapped to file.
    unsigned long off = segment_start_file_off + (fault_page_start - segment_start); // increment by # of pages between the start of the segment and the page we need to map.
    map_addr_ptr = mmap((void*) fault_page_start, 4096, (PROT_EXEC | PROT_READ | PROT_WRITE), MAP_PRIVATE | MAP_FIXED, fileno(fp->elf_file), off);
    if (map_addr_ptr == MAP_FAILED) {
        perror("allocate_page: mmap failed\n");
        return -1;
    }

    return 0;
}

void demand_pager(int signal, siginfo_t *si, void *arg) {
    // fprintf(stderr, "Faulting with address %p\n", si->si_addr);
    Elf64_Phdr *fault_hdr = find_fault_hdr(si->si_addr);
    if (allocate_page(fault_hdr, si->si_addr) == -1) {
        perror("demand_pager: failed to allocate page.\n");
        exit(-1);
    }
}

void install_segfault_handler() {
    struct sigaction sa = {0};
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = demand_pager;
    sa.sa_flags = SA_SIGINFO;

    int ret = sigaction(SIGSEGV, &sa, NULL);
    if (ret == -1) {
        perror("install_segfault_handler: sigaction failed\n");
    }
}

int main(int argc, char** argv, char** envp) {
    if (argc == 1) {
        fprintf(stderr, "main: No program specified.\n");
        exit(EXIT_FAILURE);
    }

    fp = parse_file(argc, argv, envp);
    if (fp == NULL) {
        fprintf(stderr, "main: Failed to parse file.\n");
        exit(EXIT_FAILURE);
    }
    
    install_segfault_handler();
    // find_fault_hdr((void*) 0x10002a40);

    if (load_elf_binary(fp) != 0) {
        fprintf(stderr, "main: Loading ELF binary failed.\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}