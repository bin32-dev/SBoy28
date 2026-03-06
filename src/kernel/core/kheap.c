#include "kernel/kheap.h"
#include "kernel/vmm.h"
#include "kernel/kernel.h"
#include "common/utils.h"

// Basic linked list based block allocator
typedef struct kheap_block {
    struct kheap_block* next;
    size_t size;          // Size of the data portion (excluding this header)
    bool is_free;
    uint32_t magic;       // For corruption detection
} kheap_block_t;

#define KHEAP_MAGIC 0xC001C0DE
#define KHEAP_INITIAL_PAGES 1024 // 4 MB

// Start mapping the heap starting from 0xD0000000 
static uint32_t heap_virtual_address = 0xD0000000;
static kheap_block_t* heap_head = NULL;

void kheap_init(void) {
    // Map initial heap pages
    for (int i = 0; i < KHEAP_INITIAL_PAGES; i++) {
        vmm_alloc_page(heap_virtual_address + (i * PAGE_SIZE), PAGE_RW);
    }

    heap_head = (kheap_block_t*)heap_virtual_address;
    heap_head->size = (KHEAP_INITIAL_PAGES * PAGE_SIZE) - sizeof(kheap_block_t);
    heap_head->is_free = true;
    heap_head->next = NULL;
    heap_head->magic = KHEAP_MAGIC;
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;

    // Align size to 4 bytes
    if (size % 4 != 0) {
        size += 4 - (size % 4);
    }

    kheap_block_t* curr = heap_head;
    while (curr != NULL) {
        if (curr->is_free && curr->size >= size) {
            // See if we can split the block
            if (curr->size >= size + sizeof(kheap_block_t) + 4) {
                kheap_block_t* new_block = (kheap_block_t*)((uint8_t*)curr + sizeof(kheap_block_t) + size);
                new_block->is_free = true;
                new_block->size = curr->size - size - sizeof(kheap_block_t);
                new_block->next = curr->next;
                new_block->magic = KHEAP_MAGIC;

                curr->size = size;
                curr->next = new_block;
            }
            curr->is_free = false;
            return (void*)((uint8_t*)curr + sizeof(kheap_block_t));
        }
        curr = curr->next;
    }

    // Out of memory (ideally we would call vmm_alloc_page here for more space!)
    return NULL;
}

void kfree(void* p) {
    if (!p) return;

    kheap_block_t* block = (kheap_block_t*)((uint8_t*)p - sizeof(kheap_block_t));
    if (block->magic == KHEAP_MAGIC) {
        block->is_free = true;

        // Merge with next block if free
        if (block->next && block->next->is_free) {
            block->size += sizeof(kheap_block_t) + block->next->size;
            block->next = block->next->next;
        }

        // Merge with previous block if free
        // We have to iterate from head since we don't use a doubly-linked list
        kheap_block_t* curr = heap_head;
        while (curr && curr->next != block) {
            curr = curr->next;
        }

        if (curr && curr->is_free) {
            curr->size += sizeof(kheap_block_t) + block->size;
            curr->next = block->next;
        }
    }
}
