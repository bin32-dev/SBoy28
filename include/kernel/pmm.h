#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "kernel/multiboot.h"

#define PMM_BLOCK_SIZE 4096

// Initialize the physical memory manager
void pmm_init(multiboot_info_t* mbd);

// Allocate a single block of physical memory
void* pmm_alloc_block(void);

// Free a block of physical memory
void pmm_free_block(void* addr);

// Get the number of free blocks
uint32_t pmm_get_free_block_count(void);

// Get the total number of blocks
uint32_t pmm_get_total_block_count(void);

#endif // PMM_H
