#include <OS/Grapich/gui.h>
#include "OS/OS.h"
#include "OS/Grapich/windows.h"
#include "common/utils.h"
#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "drivers/thread.h"
#include "kernel/pmm.h"

/* ---------------- Kernel Metrics ---------------- */

typedef struct {
    uint32_t ticks;
    uint32_t memory_pages_used;
    uint32_t ram_total_kb;
    uint32_t ram_free_kb;
} kernel_metrics_t;

static kernel_metrics_t g_kernel_metrics;

/* ---------------- Application Framework ---------------- */

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

static app_framework_t g_framework;

/* ---------------- Desktop / UI State ---------------- */

typedef struct {
    os_rect_t app_window;
    os_rect_t status_window;
    os_rect_t start_menu;
    os_rect_t start_button;
    bool start_menu_open;
} desktop_state_t;

static desktop_state_t g_desktop;

/* ---------------- Helpers ---------------- */

static const char* state_to_string(os_app_state_t state)
{
    switch (state) {
        case OS_APP_STOPPED:    return "STOPPED";
        case OS_APP_RUNNING:    return "RUNNING";
        case OS_APP_SUSPENDED:  return "SUSPENDED";
        case OS_APP_TERMINATED: return "TERMINATED";
        default: return "UNKNOWN";
    }
}

/* ---------------- Kernel Operations ---------------- */

static uint32_t kernel_alloc_pages(uint32_t pages)
{
    g_kernel_metrics.memory_pages_used += pages;
    return g_kernel_metrics.memory_pages_used;
}

static void kernel_release_pages(uint32_t pages)
{
    if (g_kernel_metrics.memory_pages_used > pages)
        g_kernel_metrics.memory_pages_used -= pages;
    else
        g_kernel_metrics.memory_pages_used = 0;
}

static void kernel_scheduler_tick(void) { g_kernel_metrics.ticks++; }

static void kernel_refresh_ram_metrics(void)
{
    g_kernel_metrics.ram_total_kb = pmm_get_total_block_count() * 4;
    g_kernel_metrics.ram_free_kb  = pmm_get_free_block_count() * 4;
}

/* ---------------- Application Framework ---------------- */

static void framework_register_apps(void)
{
    const char* names[OS_MAX_APPS] = { "HomeShell", "ClockApp", "NotesLite", "TaskPeek" };

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

    if (app->state == OS_APP_STOPPED || app->state == OS_APP_TERMINATED)
        kernel_alloc_pages(2);

    app->state = OS_APP_RUNNING;
    g_framework.running_apps++;
}

static void framework_suspend(uint32_t index)
{
    if (index >= OS_MAX_APPS) return;

    app_descriptor_t* app = &g_framework.apps[index];
    if (app->state == OS_APP_RUNNING) {
        app->state = OS_APP_SUSPENDED;
        if (g_framework.running_apps > 0) g_framework.running_apps--;
    }
}

static void framework_resume(uint32_t index)
{
    if (index >= OS_MAX_APPS) return;

    app_descriptor_t* app = &g_framework.apps[index];
    if (app->state == OS_APP_SUSPENDED) {
        app->state = OS_APP_RUNNING;
        g_framework.running_apps++;
    }
}

static void framework_terminate(uint32_t index)
{
    if (index >= OS_MAX_APPS) return;

    app_descriptor_t* app = &g_framework.apps[index];
    if (app->state == OS_APP_RUNNING && g_framework.running_apps > 0)
        g_framework.running_apps--;

    if (app->state != OS_APP_TERMINATED)
        kernel_release_pages(2);

    app->state = OS_APP_TERMINATED;
}

static void framework_schedule(void)
{
    for (uint32_t i = 0; i < OS_MAX_APPS; i++) {
        if (g_framework.apps[i].state == OS_APP_RUNNING)
            g_framework.apps[i].cpu_slices++;
    }
}

/* ---------------- Desktop / UI ---------------- */

static void desktop_init(void)
{
    g_desktop.app_window    = (os_rect_t){12, 20, 296, 112};
    g_desktop.status_window = (os_rect_t){12, 136, 296, 44};
    g_desktop.start_menu    = (os_rect_t){4, VGA_HEIGHT - 86, 118, 70};
    g_desktop.start_button  = (os_rect_t){4, VGA_HEIGHT - 14, 46, 12};
    g_desktop.start_menu_open = false;
}

static void draw_app_list(void)
{
    for (uint32_t i = 0; i < OS_MAX_APPS; i++) {
        app_descriptor_t* app = &g_framework.apps[i];
        const char* selector = (i == g_framework.selected_app) ? ">" : " ";
        char slices[16];
        itoa(app->cpu_slices, slices, 10);

        windows_draw_string(g_desktop.app_window.x + 6, g_desktop.app_window.y + 16 + i*15, selector, BLACK);
        windows_draw_string(g_desktop.app_window.x + 16, g_desktop.app_window.y + 16 + i*15, app->name, BLUE);
        windows_draw_string(g_desktop.app_window.x + 112, g_desktop.app_window.y + 16 + i*15, state_to_string(app->state), BLACK);
        windows_draw_string(g_desktop.app_window.x + 194, g_desktop.app_window.y + 16 + i*15, "Slices:", DARK_GREY);
        windows_draw_string(g_desktop.app_window.x + 244, g_desktop.app_window.y + 16 + i*15, slices, BRIGHT_BLUE);
    }
}

static void draw_status_panel(void)
{
    char buf[16];

    itoa(g_kernel_metrics.ticks, buf, 10);
    windows_draw_string(g_desktop.status_window.x + 6, g_desktop.status_window.y + 16, "Tick:", BLACK);
    windows_draw_string(g_desktop.status_window.x + 46, g_desktop.status_window.y + 16, buf, BRIGHT_BLUE);

    itoa(g_framework.running_apps, buf, 10);
    windows_draw_string(g_desktop.status_window.x + 86, g_desktop.status_window.y + 16, "Running:", BLACK);
    windows_draw_string(g_desktop.status_window.x + 146, g_desktop.status_window.y + 16, buf, BRIGHT_BLUE);

    itoa(g_kernel_metrics.ram_free_kb, buf, 10);
    windows_draw_string(g_desktop.status_window.x + 184, g_desktop.status_window.y + 16, "RAM:", BLACK);
    windows_draw_string(g_desktop.status_window.x + 220, g_desktop.status_window.y + 16, buf, BRIGHT_BLUE);
    windows_draw_string(g_desktop.status_window.x + 258, g_desktop.status_window.y + 16, "KB", BLACK);
}

static void draw_start_menu(void)
{
    windows_draw_window(g_desktop.start_menu, "Menu");
    windows_draw_string(g_desktop.start_menu.x + 8, g_desktop.start_menu.y + 16, "L: Launch", BLACK);
    windows_draw_string(g_desktop.start_menu.x + 8, g_desktop.start_menu.y + 28, "S: Suspend", BLACK);
    windows_draw_string(g_desktop.start_menu.x + 8, g_desktop.start_menu.y + 40, "R: Resume", BLACK);
    windows_draw_string(g_desktop.start_menu.x + 8, g_desktop.start_menu.y + 52, "T: Kill", BLACK);
}

static void draw_home_screen(void)
{
    //windows_draw_desktop_background();
    os_gui_draw_desktop_background()
    windows_draw_taskbar(g_desktop.start_menu_open);

    windows_draw_window(g_desktop.app_window, "Desktop");
    draw_app_list();

    windows_draw_window(g_desktop.status_window, "System");
    draw_status_panel();

    if (g_desktop.start_menu_open)
        draw_start_menu();
}

/* ---------------- Input Handling ---------------- */

static void process_key(uint8_t scancode)
{
    if (scancode >= 0x80) return;

    if (scancode >= KEY_1 && scancode <= KEY_4) {
        g_framework.selected_app = scancode - KEY_1;
        return;
    }

    switch (scancode) {
        case KEY_L: framework_launch(g_framework.selected_app); break;
        case KEY_S: framework_suspend(g_framework.selected_app); break;
        case KEY_R: framework_resume(g_framework.selected_app); break;
        case KEY_T: framework_terminate(g_framework.selected_app); break;
        default: break;
    }
}

static void process_mouse(void)
{
    int32_t mx, my;
    if (!mouse_consume_left_click()) return;
    mouse_get_position(&mx, &my);

    if (windows_point_in_rect(mx, my, g_desktop.start_button)) {
        g_desktop.start_menu_open = !g_desktop.start_menu_open;
        return;
    }

    if (g_desktop.start_menu_open && windows_point_in_rect(mx, my, g_desktop.start_menu)) {
        uint32_t row = my - g_desktop.start_menu.y;
        if (row < 24) framework_launch(g_framework.selected_app);
        else if (row < 36) framework_suspend(g_framework.selected_app);
        else if (row < 48) framework_resume(g_framework.selected_app);
        else framework_terminate(g_framework.selected_app);

        g_desktop.start_menu_open = false;
        return;
    }

    g_desktop.start_menu_open = false;
}

/* ---------------- Main Loop ---------------- */

int main(void)
{
    int32_t mx = 0, my = 0;
    bool left = false, right = false, middle = false;

    g_kernel_metrics = (kernel_metrics_t){0};
    g_framework = (app_framework_t){0};
    g_framework.next_pid = 1;

    framework_register_apps();
    kernel_refresh_ram_metrics();
    desktop_init();

    while (1) {
        kernel_scheduler_tick();
        framework_schedule();
        kernel_refresh_ram_metrics();

        if (is_key_pressed())
            process_key(read_key());

        process_mouse();
        mouse_get_position(&mx, &my);
        mouse_get_buttons(&left, &right, &middle);

        draw_home_screen();
        windows_draw_cursor(mx, my, left || right || middle);

        thread_yield();
    }

    return 0;
}