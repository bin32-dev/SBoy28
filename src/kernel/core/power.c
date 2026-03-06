#include "kernel/power.h"
#include "common/ports.h"

static void halt_forever(void)
{
    __asm__ volatile ("cli");
    while (1) {
        __asm__ volatile ("hlt");
    }
}

void kernel_shutdown(void)
{
    /* QEMU/Bochs/VirtualBox-compatible shutdown sequences. */
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);

    halt_forever();
}

void kernel_restart(void)
{
    uint8_t good = 0x02;

    while (good & 0x02) {
        good = inb(0x64);
    }

    outb(0x64, 0xFE);
    halt_forever();
}
