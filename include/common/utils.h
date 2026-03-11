#ifndef UTILS_H
#define UTILS_H

#include "kernel/types.h"
#include <stdint.h>

void* memset(void* dest, int ch, uint32_t count);
void* memcpy(void* dest, const void* src, uint32_t count);

void kernel_main();
void print_new_line();
void print_char(char ch);
void print_string(const char *str);
void print_int(int num);
void uint32_to_str(uint32_t value, char* buffer);
uint32_t strlen(const char* str);
uint32_t digit_count(int num);
void itoa(uint32_t num, char *number, int base);
void uint64_to_str(uint64_t value, char* buffer);
#endif
