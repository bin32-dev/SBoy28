#include "kernel/pmm.h"
#include "kernel/kernel.h"
#include "common/utils.h"

// External symbol from linker script marking the end of the kernel's bss section
extern uint8_t _ebss[];

static uint32_t* pmm_bitmap = 0;
static uint32_t pmm_total_blocks = 0;
static uint32_t pmm_used_blocks = 0;

static inline void pmm_bitmap_set(uint32_t bit) {
    pmm_bitmap[bit / 32] |= (1 << (bit % 32));
}

static inline void pmm_bitmap_unset(uint32_t bit) {
    pmm_bitmap[bit / 32] &= ~(1 << (bit % 32));
}

static inline bool pmm_bitmap_test(uint32_t bit) {
    return (pmm_bitmap[bit / 32] & (1 << (bit % 32))) != 0;
}

void pmm_init(multiboot_info_t* mbd) {
    (void)mbd; // mbd is not provided correctly by the custom bootsector.
    
    // Assume 32 MB of memory for simple QEMU usage.
    uint32_t mem_size = 32 * 1024 * 1024;
    pmm_total_blocks = mem_size / PMM_BLOCK_SIZE;

    // Place the bitmap just after the kernel
    pmm_bitmap = (uint32_t*)_ebss;

    // Calculate the size of the bitmap in DWORDS (32-bit units)
    uint32_t bitmap_size_dwords = pmm_total_blocks / 32;
    if (pmm_total_blocks % 32 != 0) {
        bitmap_size_dwords++;
    }

    // By default, mark all memory as used
    for (uint32_t i = 0; i < bitmap_size_dwords; i++) {
        pmm_bitmap[i] = 0xFFFFFFFF;
    }
    pmm_used_blocks = pmm_total_blocks;

    // We don't have a memory map, so mark all blocks from 1MB to mem_size as free.
    // Memory below 1MB contains BIOS, VGA, and custom bootsector.
    uint32_t start_block_to_free = (1024 * 1024) / PMM_BLOCK_SIZE;
    uint32_t end_block_to_free = pmm_total_blocks;

    for (uint32_t i = start_block_to_free; i < end_block_to_free; i++) {
        pmm_bitmap_unset(i);
        pmm_used_blocks--;
    }

    // Reserve the memory region occupied by the kernel AND the PMM bitmap itself
    // Kernel starts at 0x10000 (64KB). We will re-reserve up to bitmap_end.
    uint32_t bitmap_end = (uint32_t)_ebss + (bitmap_size_dwords * 4);
    uint32_t reserve_end_block = (bitmap_end + PMM_BLOCK_SIZE - 1) / PMM_BLOCK_SIZE;

    for (uint32_t i = 0; i < reserve_end_block; i++) {
        if (!pmm_bitmap_test(i)) {
            pmm_bitmap_set(i);
            pmm_used_blocks++;
        }
    }
}

void* pmm_alloc_block(void) {
    uint32_t bitmap_size_dwords = pmm_total_blocks / 32;
    if (pmm_total_blocks % 32 != 0) {
        bitmap_size_dwords++;
    }

    for (uint32_t i = 0; i < bitmap_size_dwords; i++) {
        if (pmm_bitmap[i] != 0xFFFFFFFF) { // Optimization: check if there's any free bit in the DWORD
            for (uint32_t j = 0; j < 32; j++) {
                uint32_t bit = i * 32 + j;
                if (!pmm_bitmap_test(bit) && bit < pmm_total_blocks) {
                    pmm_bitmap_set(bit);
                    pmm_used_blocks++;
                    return (void*)(bit * PMM_BLOCK_SIZE);
                }
            }
        }
    }
    return NULL; // Out of physical memory
}

void pmm_free_block(void* addr) {
    uint32_t block = (uint32_t)addr / PMM_BLOCK_SIZE;
    if (block < pmm_total_blocks) {
        if (pmm_bitmap_test(block)) {
            pmm_bitmap_unset(block);
            pmm_used_blocks--;
        }
    }
}

uint32_t pmm_get_free_block_count(void) {
    return pmm_total_blocks - pmm_used_blocks;
}

uint32_t pmm_get_total_block_count(void) {
    return pmm_total_blocks;
}
