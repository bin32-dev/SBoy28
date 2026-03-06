#ifndef KHEAP_H
#define KHEAP_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

void kheap_init(void);
void* kmalloc(size_t size);
void kfree(void* p);

#endif // KHEAP_H
