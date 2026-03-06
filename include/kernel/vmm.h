#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stdbool.h>

#define PAGE_SIZE 4096

// Page entry flags (x86 specific)
#define PAGE_PRESENT  0x01
#define PAGE_RW       0x02
#define PAGE_USER     0x04

void vmm_init(void);
void vmm_switch_directory(uint32_t* dir);
void vmm_map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags);
void* vmm_alloc_page(uint32_t virtual_addr, uint32_t flags);
void vmm_free_page(uint32_t virtual_addr);

#endif // VMM_H
