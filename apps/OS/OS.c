#include "OS/OS.h"
#include "OS/gui.h"
#include "common/utils.h"
#include "drivers/keyboard.h"
#include "drivers/mouse.h"
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

typedef struct {
    os_rect_t app_window;
    os_rect_t status_window;
    os_rect_t start_menu;
    os_rect_t start_button;
    bool start_menu_open;
} desktop_state_t;

static kernel_metrics_t g_kernel_metrics;
static app_framework_t g_framework;
static desktop_state_t g_desktop;

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

/* ---------------- UI / Desktop ---------------- */

static void desktop_init(void)
{
    g_desktop.app_window = (os_rect_t){ 12, 20, 296, 112 };
    g_desktop.status_window = (os_rect_t){ 12, 136, 296, 44 };
    g_desktop.start_menu = (os_rect_t){ 4, VGA_HEIGHT - 86, 118, 70 };
    g_desktop.start_button = (os_rect_t){ 4, VGA_HEIGHT - 14, 46, 12 };
    g_desktop.start_menu_open = false;
}

static void draw_app_list(void)
{
    char line[40];

    for (uint32_t i = 0; i < OS_MAX_APPS; i++) {
        app_descriptor_t* app = &g_framework.apps[i];
        const char* selector = (i == g_framework.selected_app) ? ">" : " ";
        const char* state = state_to_string(app->state);
        uint32_t x = (uint32_t)g_desktop.app_window.x + 6;
        uint32_t y = (uint32_t)g_desktop.app_window.y + 16 + (i * 15);

        vga_draw_string(x, y, selector, BLACK);
        vga_draw_string(x + 10, y, app->name, BLUE);
        vga_draw_string(x + 112, y, state, BLACK);

        itoa(app->cpu_slices, line, 10);
        vga_draw_string(x + 194, y, "Slices:", DARK_GREY);
        vga_draw_string(x + 244, y, line, BRIGHT_BLUE);
    }
}

static void draw_status_panel(void)
{
    char line[24];

    itoa(g_kernel_metrics.ticks, line, 10);
    vga_draw_string((uint32_t)g_desktop.status_window.x + 6, (uint32_t)g_desktop.status_window.y + 16, "Tick:", BLACK);
    vga_draw_string((uint32_t)g_desktop.status_window.x + 46, (uint32_t)g_desktop.status_window.y + 16, line, BRIGHT_BLUE);

    itoa(g_framework.running_apps, line, 10);
    vga_draw_string((uint32_t)g_desktop.status_window.x + 86, (uint32_t)g_desktop.status_window.y + 16, "Running:", BLACK);
    vga_draw_string((uint32_t)g_desktop.status_window.x + 146, (uint32_t)g_desktop.status_window.y + 16, line, BRIGHT_BLUE);

    itoa(g_kernel_metrics.ram_free_kb, line, 10);
    vga_draw_string((uint32_t)g_desktop.status_window.x + 184, (uint32_t)g_desktop.status_window.y + 16, "RAM:", BLACK);
    vga_draw_string((uint32_t)g_desktop.status_window.x + 220, (uint32_t)g_desktop.status_window.y + 16, line, BRIGHT_BLUE);
    vga_draw_string((uint32_t)g_desktop.status_window.x + 258, (uint32_t)g_desktop.status_window.y + 16, "KB", BLACK);
}

static void draw_start_menu(void)
{
    os_gui_draw_window(g_desktop.start_menu, "Menu");
    vga_draw_string((uint32_t)g_desktop.start_menu.x + 8, (uint32_t)g_desktop.start_menu.y + 16, "L: Launch", BLACK);
    vga_draw_string((uint32_t)g_desktop.start_menu.x + 8, (uint32_t)g_desktop.start_menu.y + 28, "S: Suspend", BLACK);
    vga_draw_string((uint32_t)g_desktop.start_menu.x + 8, (uint32_t)g_desktop.start_menu.y + 40, "R: Resume", BLACK);
    vga_draw_string((uint32_t)g_desktop.start_menu.x + 8, (uint32_t)g_desktop.start_menu.y + 52, "T: Kill", BLACK);
}

static void draw_home_screen(void)
{
    os_gui_draw_desktop_background();
    os_gui_draw_taskbar(g_desktop.start_menu_open);

    os_gui_draw_window(g_desktop.app_window, "SBoy28 Desktop - Apps");
    vga_draw_string((uint32_t)g_desktop.app_window.x + 6, (uint32_t)g_desktop.app_window.y + 4,
                    "1-4 Select App | L/S/R/T Manage", WHITE);
    draw_app_list();

    os_gui_draw_window(g_desktop.status_window, "System");
    draw_status_panel();

    if (g_desktop.start_menu_open) {
        draw_start_menu();
    }
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

static void process_mouse(void)
{
    int32_t mx;
    int32_t my;

    if (!mouse_consume_left_click()) {
        return;
    }

    mouse_get_position(&mx, &my);

    if (os_gui_point_in_rect(mx, my, g_desktop.start_button)) {
        g_desktop.start_menu_open = !g_desktop.start_menu_open;
        return;
    }

    if (g_desktop.start_menu_open && os_gui_point_in_rect(mx, my, g_desktop.start_menu)) {
        uint32_t row = (uint32_t)(my - g_desktop.start_menu.y);
        if (row < 24) {
            framework_launch(g_framework.selected_app);
        } else if (row < 36) {
            framework_suspend(g_framework.selected_app);
        } else if (row < 48) {
            framework_resume(g_framework.selected_app);
        } else {
            framework_terminate(g_framework.selected_app);
        }
        g_desktop.start_menu_open = false;
        return;
    }

    g_desktop.start_menu_open = false;
}

/* ---------------- App/OS entry ---------------- */

int main(void)
{
    int32_t mx = 0;
    int32_t my = 0;
    bool left = false;
    bool right = false;
    bool middle = false;

    g_kernel_metrics.ticks = 0;
    g_kernel_metrics.memory_pages_used = 0;
    g_kernel_metrics.ram_total_kb = 0;
    g_kernel_metrics.ram_free_kb = 0;

    g_framework.selected_app = 0;
    g_framework.running_apps = 0;
    g_framework.next_pid = 1;

    framework_register_apps();
    kernel_refresh_ram_metrics();
    desktop_init();

    while (1) {
        kernel_scheduler_tick();
        framework_schedule();
        kernel_refresh_ram_metrics();

        if (is_key_pressed()) {
            uint8_t scancode = read_key();
            process_key(scancode);
        }

        process_mouse();

        mouse_get_position(&mx, &my);
        mouse_get_buttons(&left, &right, &middle);

        draw_home_screen();
        os_gui_draw_cursor(mx, my, left || right || middle);
        vga_render();

        thread_yield();
    }

    return 0;
}
