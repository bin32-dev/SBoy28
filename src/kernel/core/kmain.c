#include <stddef.h>
#include "kernel/kernel.h"
#include "kernel/gdt.h"
#include "kernel/idt.h"
#include "kernel/kheap.h"
#include "kernel/pmm.h"
#include "kernel/vmm.h"
#include "drivers/vga.h"
#include "ui/ui.h"
#include "common/utils.h"
#include "drivers/rtc.h"
#include "drivers/keyboard.h"
#include "kernel/multiboot.h"
#include "drivers/mouse.h"
#include "drivers/thread.h"
#include "drivers/pit.h"

// Thread prototypes
void mouse_render_thread(void* arg);
void background_thread(void* arg);
void time_thread(void* arg);

mutex_t vga_mutex;

void kmain(multiboot_info_t* mbd)
{
    (void)mbd;

    /* Core CPU structures */
    init_gdt();
    init_idt();

    /* Memory Management */
    pmm_init(mbd);
    vmm_init();
    kheap_init();

    /* Graphics subsystem */
    vga_init();
    vga_clear(DARK_GREY);

    vga_draw_string(10, 10, "SBoy28 OS - Graphical Mode Active", WHITE);
    vga_draw_string(10, 20, "Threading & Mouse Driver Started", BRIGHT_CYAN);
    vga_render();

    /* Hardware timers and drivers */
    init_pit(100);
    mouse_init();

    /* Scheduler */
    thread_system_init();
    mutex_init(&vga_mutex);

    /* Thread registration */
    thread_create(time_thread, NULL, 0);
    thread_create(mouse_render_thread, NULL, 0);
    thread_create(background_thread, NULL, 0);

    /* Enable interrupts */
    __asm__ volatile("sti");

    /* Idle loop */
    while (1) {
        __asm__ volatile("hlt");
    }
}

/* ---------------- TIME THREAD ---------------- */

void time_thread(void* arg)
{
    (void)arg;

    char buf[16];

    while (1) {

        int minutes = get_minutes();
        itoa(minutes, buf, 10);

        mutex_lock(&vga_mutex);

        /* Clear previous text area */
        for (int x = 10; x < 120; x++) {
            for (int y = 40; y < 50; y++) {
                vga_put_pixel(x, y, BLUE);
            }
        }

        vga_draw_string(10, 40, "Minute:", WHITE);
        vga_draw_string(70, 40, buf, YELLOW);

        vga_render();

        mutex_unlock(&vga_mutex);

        thread_yield();
    }
}

/* ---------------- MOUSE THREAD ---------------- */

void mouse_render_thread(void* arg)
{
    (void)arg;

    int32_t last_x = -1;
    int32_t last_y = -1;

    int32_t cur_x;
    int32_t cur_y;

    uint8_t saved_cursor_area[25];

    while (1) {

        mouse_get_position(&cur_x, &cur_y);

        if (cur_x != last_x || cur_y != last_y) {

            mutex_lock(&vga_mutex);

            /* Restore previous pixels */
            if (last_x != -1) {
                int idx = 0;

                for (int dy = -2; dy <= 2; dy++) {
                    for (int dx = -2; dx <= 2; dx++) {
                        vga_put_pixel(last_x + dx, last_y + dy, saved_cursor_area[idx++]);
                    }
                }
            }

            /* Save new background */
            int idx = 0;

            for (int dy = -2; dy <= 2; dy++) {
                for (int dx = -2; dx <= 2; dx++) {
                    saved_cursor_area[idx++] =
                            vga_get_pixel(cur_x + dx, cur_y + dy);
                }
            }

            /* Draw cursor */
            for (int dy = -2; dy <= 2; dy++)
                vga_put_pixel(cur_x, cur_y + dy, WHITE);

            for (int dx = -2; dx <= 2; dx++)
                vga_put_pixel(cur_x + dx, cur_y, WHITE);

            vga_render();
            
            mutex_unlock(&vga_mutex);

            last_x = cur_x;
            last_y = cur_y;
        }
        thread_yield();
    }
}

/* ---------------- BACKGROUND THREAD ---------------- */

void background_thread(void* arg)
{
    (void)arg;

    uint32_t ticks = 0;
    char buf[32];

    while (1) {

        ticks++;
        itoa(ticks, buf, 10);

        mutex_lock(&vga_mutex);

        /* Clear counter region */
        for (int x = 250; x < 360; x++) {
            for (int y = 180; y < 195; y++) {
                vga_put_pixel(x, y, BLUE);
            }
        }

        vga_draw_string(250, 180, "Ticks:", WHITE);
        vga_draw_string(310, 180, buf, YELLOW);

        vga_render();

        mutex_unlock(&vga_mutex);

        /* Artificial delay */
        for (volatile int i = 0; i < 100000; i++);

        thread_yield();
    }
}