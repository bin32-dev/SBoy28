#include "kernel/vmm.h"
#include "kernel/pmm.h"
#include "kernel/idt.h"
#include "common/utils.h"
#include "drivers/tty.h"

// The page directory - must be 4KB aligned!
static uint32_t* page_directory = NULL;

static void page_fault(registers_t* regs) {
    uint32_t faulting_address;
    char num_buf[12];
    __asm__ volatile("mov %%cr2, %0" : "=r" (faulting_address));

    tty_putstring("\n[PAGE FAULT] addr=0x");
    itoa(faulting_address, num_buf, 16);
    tty_putstring(num_buf);

    tty_putstring(" eip=0x");
    itoa(regs->eip, num_buf, 16);
    tty_putstring(num_buf);

    tty_putstring(" err=0x");
    itoa(regs->err_code, num_buf, 16);
    tty_putstring(num_buf);
    tty_putstring("\n");

    __asm__ volatile("cli");
    while (1) {
        __asm__ volatile("hlt");
    }
}

void vmm_init(void) {
    // Allocate a page for the page directory
    page_directory = (uint32_t*)pmm_alloc_block();
    
    // Initialize everything to not present and Supervisor/RW
    for (int i = 0; i < 1024; i++) {
        page_directory[i] = PAGE_RW; // Read/write, not present
    }

    // Identity map all physical memory so the kernel can access it easily.
    // In a higher-half kernel, we would map the kernel to 3GB+.
    // Here we use an identity mapping for simplicity as requested.
    uint32_t total_blocks = pmm_get_total_block_count();
    uint32_t required_page_tables = (total_blocks + 1023) / 1024;
    
    for (uint32_t pt = 0; pt < required_page_tables; pt++) {
        uint32_t* page_table = (uint32_t*)pmm_alloc_block();
        for (int i = 0; i < 1024; i++) {
            uint32_t phys_addr = (pt * 1024 + i) * 0x1000;
            if (phys_addr < total_blocks * 0x1000) {
                page_table[i] = phys_addr | PAGE_PRESENT | PAGE_RW;
            } else {
                page_table[i] = 0;
            }
        }
        page_directory[pt] = ((uint32_t)page_table) | PAGE_PRESENT | PAGE_RW;
    }

    // Register our page fault handler
    register_interrupt_handler(14, page_fault);

    // Switch to our page directory and enable paging!
    vmm_switch_directory(page_directory);
}

void vmm_switch_directory(uint32_t* dir) {
    uint32_t cr0;
    
    // Put page directory into CR3
    __asm__ volatile("mov %0, %%cr3":: "r"(dir));
    
    // Enable paging bit in CR0 (bit 31)
    __asm__ volatile("mov %%cr0, %0": "=r"(cr0));
    cr0 |= 0x80000000;
    __asm__ volatile("mov %0, %%cr0":: "r"(cr0));
}

void vmm_map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags) {
    uint32_t pdindex = virtual_addr >> 22;
    uint32_t ptindex = (virtual_addr >> 12) & 0x03FF;

    uint32_t* page_table;

    // Check if the page table exists
    if (page_directory[pdindex] & PAGE_PRESENT) {
        // Page table is present. Extract physical address.
        page_table = (uint32_t*)(page_directory[pdindex] & ~0xFFF);
    } else {
        // We need to create a new page table
        // For a hobby OS, assuming identity mapping is fine.
        page_table = (uint32_t*)pmm_alloc_block();
        for (int i = 0; i < 1024; i++) {
            page_table[i] = 0; // Not present
        }
        page_directory[pdindex] = ((uint32_t)page_table) | PAGE_PRESENT | PAGE_RW | (flags & PAGE_USER);
    }

    // Map the page in the page table
    page_table[ptindex] = (physical_addr & ~0xFFF) | (flags & 0xFFF) | PAGE_PRESENT;
}

void* vmm_alloc_page(uint32_t virtual_addr, uint32_t flags) {
    void* phys_block = pmm_alloc_block();
    if (phys_block) {
        vmm_map_page(virtual_addr, (uint32_t)phys_block, flags);
    }
    return (void*)virtual_addr;
}

void vmm_free_page(uint32_t virtual_addr) {
    uint32_t pdindex = virtual_addr >> 22;
    uint32_t ptindex = (virtual_addr >> 12) & 0x03FF;

    if (page_directory[pdindex] & PAGE_PRESENT) {
        uint32_t* page_table = (uint32_t*)(page_directory[pdindex] & ~0xFFF);
        if (page_table[ptindex] & PAGE_PRESENT) {
            uint32_t phys_addr = page_table[ptindex] & ~0xFFF;
            pmm_free_block((void*)phys_addr);
            page_table[ptindex] = 0; // Unmark present
            // Invalidate TLB
            __asm__ volatile("invlpg (%0)" ::"r" (virtual_addr) : "memory");
        }
    }
}
