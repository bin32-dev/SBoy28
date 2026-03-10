#include <stddef.h>
#include <stdint.h>
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
#include "drivers/filesystem.h"
#include "OS/OS.h"
#include "lua_runtime.h"

// Thread prototypes
void mouse_render_thread(void* arg);
void background_thread(void* arg);
void time_thread(void* arg);

mutex_t vga_mutex;

/* ---------------- KERNEL-LEVEL INTEGER TO STRING ---------------- */
static void int_to_str(int val, char* buf, int width) {
    // Supports positive numbers only
    int i = width - 1;
    buf[i--] = '\0';
    if (val == 0) {
        buf[i--] = '0';
    } else {
        while (val > 0 && i >= 0) {
            buf[i--] = '0' + (val % 10);
            val /= 10;
        }
    }
    // Fill leading zeros
    while (i >= 0) buf[i--] = '0';
}

/* ---------------- FORMAT TIME LINE ---------------- */
static void format_time_line(char* out, int hours, int minutes, int seconds, const char* status) {
    // Format: [hh:mm:ss] : status
    out[0] = '[';
    int_to_str(hours, out + 1, 3);    // 2 digits + null temporary
    out[3] = ':';
    int_to_str(minutes, out + 4, 3);
    out[6] = ':';
    int_to_str(seconds, out + 7, 3);
    out[9] = ']';
    out[10] = ' ';
    // Copy status manually
    int i = 0;
    while (status && status[i] && (i + 11) < 64) {
        out[11 + i] = status[i];
        i++;
    }
    out[11 + i] = '\0';
}

/* ---------------- DRAW BOOT LOG ---------------- */
static void draw_boot_log_line(uint32_t y, const char* status, uint8_t color) {
    char line[64];
    int hours = get_hours();
    int minutes = get_minutes();
    int seconds = get_seconds();

    format_time_line(line, hours, minutes, seconds, status);

    vga_draw_string(10, y, line, color);
    vga_render();
}

/* ---------------- KMAIN ---------------- */
void kmain(multiboot_info_t* mbd) {
    (void)mbd;

    init_gdt();
    init_idt();
    pmm_init(mbd);
    vmm_init();
    kheap_init();

    vga_init();
    vga_clear(DARK_GREY);
    vga_draw_string(10, 10, "SBoy28 OS - Boot Sequence", WHITE);
    draw_boot_log_line(26, "graphics initialized successfully", BRIGHT_CYAN);

    init_pit(100);
    draw_boot_log_line(38, "pit initialized successfully", BRIGHT_CYAN);

    mouse_init();
    draw_boot_log_line(50, "mouse initialized successfully", BRIGHT_GREEN);

    thread_system_init();
    mutex_init(&vga_mutex);

    filesystem_init();
    draw_boot_log_line(62, "filesystem mounted (0:/,1:/,2:/)", BRIGHT_GREEN);

    if (lua_runtime_init() == 0) {
        draw_boot_log_line(74, "lua runtime initialized", BRIGHT_GREEN);
        if (lua_runtime_execute_startup() == 0) {
            draw_boot_log_line(86, "executed 2:/lua/init.lua", BRIGHT_GREEN);
        } else {
            draw_boot_log_line(86, "lua startup script missing/failed", YELLOW);
        }
    } else {
        draw_boot_log_line(74, "lua runtime init failed", YELLOW);
    }
    draw_boot_log_line(98, "switching to OS home screen...", YELLOW);

    Sleep(1);
    vga_clear(BLUE);
    vga_render();

    __asm__ volatile("sti");

    // Call main user application
    main();
}

/* ---------------- TIME THREAD ---------------- */
void time_thread(void* arg) {
    (void)arg;
    char buf[3]; // 2 digits + null

    while (1) {
        int minutes = get_minutes();
        int_to_str(minutes, buf, 3);

        mutex_lock(&vga_mutex);
        for (int x = 10; x < 120; x++)
            for (int y = 40; y < 50; y++)
                vga_put_pixel(x, y, BLUE);

        vga_draw_string(10, 40, "Minute:", WHITE);
        vga_draw_string(70, 40, buf, YELLOW);
        vga_render();
        mutex_unlock(&vga_mutex);

        thread_yield();
    }
}

/* ---------------- MOUSE THREAD ---------------- */
void mouse_render_thread(void* arg) {
    (void)arg;
    int32_t last_x = -1, last_y = -1;
    int32_t cur_x, cur_y;
    uint8_t saved_cursor_area[25];

    while (1) {
        mouse_get_position(&cur_x, &cur_y);

        if (cur_x != last_x || cur_y != last_y) {
            mutex_lock(&vga_mutex);

            if (last_x != -1) {
                int idx = 0;
                for (int dy = -2; dy <= 2; dy++)
                    for (int dx = -2; dx <= 2; dx++)
                        vga_put_pixel(last_x + dx, last_y + dy, saved_cursor_area[idx++]);
            }

            int idx = 0;
            for (int dy = -2; dy <= 2; dy++)
                for (int dx = -2; dx <= 2; dx++)
                    saved_cursor_area[idx++] = vga_get_pixel(cur_x + dx, cur_y + dy);

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
void background_thread(void* arg) {
    (void)arg;
    uint32_t ticks = 0;
    char buf[11]; // max 10 digits + null

    while (1) {
        ticks++;
        int_to_str(ticks, buf, 11);

        mutex_lock(&vga_mutex);
        for (int x = 250; x < 360; x++)
            for (int y = 180; y < 195; y++)
                vga_put_pixel(x, y, BLUE);

        vga_draw_string(250, 180, "Ticks:", WHITE);
        vga_draw_string(310, 180, buf, YELLOW);
        vga_render();
        mutex_unlock(&vga_mutex);

        for (volatile int i = 0; i < 100000; i++); // delay
        thread_yield();
    }
}
