#include "common/utils.h"
#include "drivers/tty.h"
#include "drivers/vga.h"
#include <stddef.h>
#include <stdint.h> // For standard integer types

// =========================================================
// Basic Output Functions
// =========================================================
void print_new_line() {
    tty_putchar('\n');
}

void print_char(char ch) {
    tty_putchar(ch);
}

void print_string(const char *str) {
    tty_putstring(str);
}

// Print 32-bit signed integer
void print_int(int num) {
    char str_num[12]; // Max 10 digits + sign + null
    if (num < 0) {
        tty_putchar('-');
        uint32_to_str((uint32_t)(-num), str_num);
    } else {
        uint32_to_str((uint32_t)num, str_num);
    }
    tty_putstring(str_num);
}

// =========================================================
// String Utilities
// =========================================================
size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len] != '\0') len++;
    return len;
}

uint32_t digit_count(int num) {
    if (num == 0) return 1;
    uint32_t count = 0;
    if (num < 0) {
        count++; // Sign
        num = -num;
    }
    while (num > 0) {
        count++;
        num /= 10;
    }
    return count;
}

// =========================================================
// Integer to String Conversion
// =========================================================
void itoa(uint32_t num, char *buffer, int base) {
    static const char digits[] = "0123456789ABCDEF";
    char temp[33];
    int i = 0;

    if (num == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    while (num > 0) {
        temp[i++] = digits[num % base];
        num /= base;
    }

    int j = 0;
    while (i > 0) {
        buffer[j++] = temp[--i];
    }
    buffer[j] = '\0';
}

// =========================================================
// 32-bit to string
// =========================================================
void uint32_to_str(uint32_t value, char* buffer) {
    char temp[11];
    int i = 0;
    if (value == 0) { buffer[0]='0'; buffer[1]=0; return; }
    while (value > 0) {
        temp[i++] = '0' + (value % 10);
        value /= 10;
    }
    int j = 0;
    while (i > 0) buffer[j++] = temp[--i];
    buffer[j] = 0;
}

// =========================================================
// 64-bit to string (manual division)
// =========================================================
static uint64_t u64_div(uint64_t n, uint64_t d) {
    uint64_t q = 0;
    uint64_t r = 0;
    for (int i = 63; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) {
            r -= d;
            q |= ((uint64_t)1 << i);
        }
    }
    return q;
}

static uint64_t u64_mod(uint64_t n, uint64_t d) {
    uint64_t r = 0;
    for (int i = 63; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) r -= d;
    }
    return r;
}

void uint64_to_str(uint64_t value, char* buffer) {
    char temp[21];
    int i = 0;
    if (value == 0) { buffer[0]='0'; buffer[1]=0; return; }

    while (value > 0) {
        temp[i++] = '0' + (u64_mod(value, 10));
        value = u64_div(value, 10);
    }

    int j = 0;
    while (i > 0) buffer[j++] = temp[--i];
    buffer[j] = 0;
}

void* memset(void* dest, int ch, size_t count) {
    uint8_t* p = (uint8_t*)dest;
    while (count--) {
        *p++ = (uint8_t)ch;
    }
    return dest;
}

void* memcpy(void* dest, const void* src, size_t count) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    while (count--) {
        *d++ = *s++;
    }
    return dest;
}
