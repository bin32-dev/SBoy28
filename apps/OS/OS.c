//
// Created by s28 on 3/6/2026.
//

#include "OS/OS.h"
#include "common/utils.h"
#include "drivers/keyboard.h"
#include "drivers/thread.h"
#include "drivers/vga.h"
#include "ui/colors.h"
#include "kernel/pmm.h"

typedef struct {
    uint32_t ticks;
    uint32_t memory_pages_used;
    uint32_t ram_total_kb;
    uint32_t ram_free_kb;
} kernel_metrics_t;

typedef struct {
    uint32_t pid;
    const char* name;
    os_app_state_t state;
    uint32_t cpu_slices;
} app_descriptor_t;

typedef struct {
    app_descriptor_t apps[OS_MAX_APPS];
    uint32_t selected_app;
    uint32_t running_apps;
    uint32_t next_pid;
} app_framework_t;

static kernel_metrics_t g_kernel_metrics;
static app_framework_t g_framework;

static const char* state_to_string(os_app_state_t state)
{
    switch (state) {
        case OS_APP_STOPPED: return "STOPPED";
        case OS_APP_RUNNING: return "RUNNING";
        case OS_APP_SUSPENDED: return "SUSPENDED";
        case OS_APP_TERMINATED: return "TERMINATED";
        default: return "UNKNOWN";
    }
}

static void clear_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint8_t color)
{
    for (uint32_t py = y; py < y + height; py++) {
        for (uint32_t px = x; px < x + width; px++) {
            vga_put_pixel(px, py, color);
        }
    }
}

static void draw_rect_outline(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint8_t color)
{
    for (uint32_t px = x; px < x + width; px++) {
        vga_put_pixel(px, y, color);
        vga_put_pixel(px, y + height - 1, color);
    }

    for (uint32_t py = y; py < y + height; py++) {
        vga_put_pixel(x, py, color);
        vga_put_pixel(x + width - 1, py, color);
    }
}

/* ---------------- Kernel-facing API (mediator layer) ---------------- */

static uint32_t kernel_alloc_pages(uint32_t pages)
{
    g_kernel_metrics.memory_pages_used += pages;
    return g_kernel_metrics.memory_pages_used;
}

static void kernel_release_pages(uint32_t pages)
{
    if (g_kernel_metrics.memory_pages_used > pages) {
        g_kernel_metrics.memory_pages_used -= pages;
    } else {
        g_kernel_metrics.memory_pages_used = 0;
    }
}

static void kernel_scheduler_tick(void)
{
    g_kernel_metrics.ticks++;
}

/* ---------------- User-space application framework ---------------- */

static void framework_register_apps(void)
{
    const char* names[OS_MAX_APPS] = {
        "HomeShell",
        "ClockApp",
        "NotesLite",
        "TaskPeek"
    };

    for (uint32_t i = 0; i < OS_MAX_APPS; i++) {
        g_framework.apps[i].pid = g_framework.next_pid++;
        g_framework.apps[i].name = names[i];
        g_framework.apps[i].state = OS_APP_STOPPED;
        g_framework.apps[i].cpu_slices = 0;
    }
}

static void framework_launch(uint32_t index)
{
    if (index >= OS_MAX_APPS) return;

    app_descriptor_t* app = &g_framework.apps[index];
    if (app->state == OS_APP_RUNNING) return;

    if (app->state == OS_APP_STOPPED || app->state == OS_APP_TERMINATED) {
        kernel_alloc_pages(2);
    }

    app->state = OS_APP_RUNNING;
    g_framework.running_apps++;
}

static void framework_suspend(uint32_t index)
{
    if (index >= OS_MAX_APPS) return;

    app_descriptor_t* app = &g_framework.apps[index];
    if (app->state != OS_APP_RUNNING) return;

    app->state = OS_APP_SUSPENDED;
    if (g_framework.running_apps > 0) g_framework.running_apps--;
}

static void framework_resume(uint32_t index)
{
    if (index >= OS_MAX_APPS) return;

    app_descriptor_t* app = &g_framework.apps[index];
    if (app->state != OS_APP_SUSPENDED) return;

    app->state = OS_APP_RUNNING;
    g_framework.running_apps++;
}

static void framework_terminate(uint32_t index)
{
    if (index >= OS_MAX_APPS) return;

    app_descriptor_t* app = &g_framework.apps[index];
    if (app->state == OS_APP_RUNNING && g_framework.running_apps > 0) {
        g_framework.running_apps--;
    }

    if (app->state != OS_APP_TERMINATED) {
        kernel_release_pages(2);
    }

    app->state = OS_APP_TERMINATED;
}

static void framework_schedule(void)
{
    for (uint32_t i = 0; i < OS_MAX_APPS; i++) {
        if (g_framework.apps[i].state == OS_APP_RUNNING) {
            g_framework.apps[i].cpu_slices++;
        }
    }
}


static void kernel_refresh_ram_metrics(void)
{
    uint32_t total_blocks = pmm_get_total_block_count();
    uint32_t free_blocks = pmm_get_free_block_count();

    g_kernel_metrics.ram_total_kb = total_blocks * 4;
    g_kernel_metrics.ram_free_kb = free_blocks * 4;
}

/* ---------------- UI / Home screen ---------------- */

static void draw_home_screen(void)
{
    char line[48];

    clear_rect(0, 0, VGA_WIDTH, VGA_HEIGHT, DARK_GREY);

    vga_draw_string(8, 8, "SBoy28 Micro OS", WHITE);
    vga_draw_string(8, 18, "Kernel mediates CPU/memory/devices/syscalls", BRIGHT_CYAN);

    draw_rect_outline(6, 30, 308, 118, WHITE);
    vga_draw_string(12, 36, "Home Screen - App Manager", YELLOW);

    vga_draw_string(12, 48, "Controls:", WHITE);
    vga_draw_string(12, 58, "1-4 Select App | L Launch | S Suspend | R Resume | T Terminate", GREY);

    for (uint32_t i = 0; i < OS_MAX_APPS; i++) {
        app_descriptor_t* app = &g_framework.apps[i];
        const char* selector = (i == g_framework.selected_app) ? ">" : " ";
        const char* state = state_to_string(app->state);
        uint32_t x = 12;
        uint32_t y = 76 + (i * 15);

        vga_draw_string(x, y, selector, WHITE);
        vga_draw_string(x + 10, y, app->name, BRIGHT_GREEN);
        vga_draw_string(x + 110, y, state, WHITE);

        itoa(app->cpu_slices, line, 10);
        vga_draw_string(x + 190, y, "Slices:", GREY);
        vga_draw_string(x + 238, y, line, BRIGHT_CYAN);
    }

    draw_rect_outline(6, 154, 308, 40, WHITE);

    itoa(g_kernel_metrics.ticks, line, 10);
    vga_draw_string(12, 162, "Kernel Tick:", WHITE);
    vga_draw_string(88, 162, line, BRIGHT_MAGENTA);

    itoa(g_kernel_metrics.memory_pages_used, line, 10);
    vga_draw_string(140, 162, "Pages Used:", WHITE);
    vga_draw_string(220, 162, line, BRIGHT_MAGENTA);

    itoa(g_framework.running_apps, line, 10);
    vga_draw_string(12, 174, "Running Apps:", WHITE);
    vga_draw_string(96, 174, line, BRIGHT_MAGENTA);

    itoa(g_kernel_metrics.ram_free_kb, line, 10);
    vga_draw_string(140, 174, "RAM:", WHITE);
    vga_draw_string(178, 174, line, BRIGHT_CYAN);
    vga_draw_string(214, 174, "KB /", WHITE);

    itoa(g_kernel_metrics.ram_total_kb, line, 10);
    vga_draw_string(246, 174, line, BRIGHT_CYAN);
    vga_draw_string(288, 174, "KB", WHITE);

    vga_render();
}

static void process_key(uint8_t scancode)
{
    if (scancode >= 0x80) return;

    if (scancode >= KEY_1 && scancode <= KEY_4) {
        g_framework.selected_app = (uint32_t)(scancode - KEY_1);
        return;
    }

    switch (scancode) {
        case KEY_L:
            framework_launch(g_framework.selected_app);
            break;
        case KEY_S:
            framework_suspend(g_framework.selected_app);
            break;
        case KEY_R:
            framework_resume(g_framework.selected_app);
            break;
        case KEY_T:
            framework_terminate(g_framework.selected_app);
            break;
        default:
            break;
    }
}

/* ---------------- App/OS entry ---------------- */

int main(void)
{
    g_kernel_metrics.ticks = 0;
    g_kernel_metrics.memory_pages_used = 0;
    g_kernel_metrics.ram_total_kb = 0;
    g_kernel_metrics.ram_free_kb = 0;

    g_framework.selected_app = 0;
    g_framework.running_apps = 0;
    g_framework.next_pid = 1;

    framework_register_apps();
    kernel_refresh_ram_metrics();
    draw_home_screen();

    while (1) {
        kernel_scheduler_tick();
        framework_schedule();
        kernel_refresh_ram_metrics();

        if (is_key_pressed()) {
            uint8_t scancode = read_key();
            process_key(scancode);
        }

        draw_home_screen();
        thread_yield();
    }

    return 0;
}
