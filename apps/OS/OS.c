#include "OS/OS.h"
#include "OS/Grapich/gui.h"
#include "OS/Grapich/windows.h"
#include "common/utils.h"
#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "drivers/thread.h"
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
    RECT app_window;
    RECT status_window;
    RECT start_menu;
    RECT start_button;
    BOOL start_menu_open;
} desktop_state_t;

#define OS_WM_LAUNCH   (WM_APP + 1)
#define OS_WM_SUSPEND  (WM_APP + 2)
#define OS_WM_RESUME   (WM_APP + 3)
#define OS_WM_TERMINATE (WM_APP + 4)

static kernel_metrics_t g_kernel_metrics;
static app_framework_t g_framework;
static desktop_state_t g_desktop;
static HWND g_main_window;

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

static void desktop_init(void)
{
    g_desktop.app_window.left = 12;
    g_desktop.app_window.top = 20;
    g_desktop.app_window.right = 308;
    g_desktop.app_window.bottom = 132;

    g_desktop.status_window.left = 12;
    g_desktop.status_window.top = 136;
    g_desktop.status_window.right = 308;
    g_desktop.status_window.bottom = 180;

    g_desktop.start_menu.left = 4;
    g_desktop.start_menu.top = VGA_HEIGHT - 86;
    g_desktop.start_menu.right = 122;
    g_desktop.start_menu.bottom = VGA_HEIGHT - 16;

    g_desktop.start_button.left = 4;
    g_desktop.start_button.top = VGA_HEIGHT - 14;
    g_desktop.start_button.right = 50;
    g_desktop.start_button.bottom = VGA_HEIGHT - 2;

    g_desktop.start_menu_open = FALSE;
}

static int rect_width(RECT rect) { return rect.right - rect.left; }
static int rect_height(RECT rect) { return rect.bottom - rect.top; }

static void text_out_uint(HDC dc, int x, int y, uint32_t value)
{
    char line[24];
    itoa(value, line, 10);
    TextOutA(dc, x, y, line, (int)strlen(line));
}

static void draw_app_list(HDC dc)
{
    for (uint32_t i = 0; i < OS_MAX_APPS; i++) {
        char line[64];
        app_descriptor_t* app = &g_framework.apps[i];
        const char* selector = (i == g_framework.selected_app) ? ">" : " ";
        const char* state = state_to_string(app->state);
        int x = g_desktop.app_window.left + 6;
        int y = g_desktop.app_window.top + 16 + (int)(i * 15);

        TextOutA(dc, x, y, selector, 1);
        TextOutA(dc, x + 10, y, app->name, (int)strlen(app->name));
        TextOutA(dc, x + 112, y, state, (int)strlen(state));

        itoa(app->cpu_slices, line, 10);
        TextOutA(dc, x + 194, y, "Slices:", 7);
        TextOutA(dc, x + 244, y, line, (int)strlen(line));
    }
}

static void draw_status_panel(HDC dc)
{
    int y = g_desktop.status_window.top + 16;

    TextOutA(dc, g_desktop.status_window.left + 6, y, "Tick:", 5);
    text_out_uint(dc, g_desktop.status_window.left + 46, y, g_kernel_metrics.ticks);

    TextOutA(dc, g_desktop.status_window.left + 86, y, "Running:", 8);
    text_out_uint(dc, g_desktop.status_window.left + 146, y, g_framework.running_apps);

    TextOutA(dc, g_desktop.status_window.left + 184, y, "RAM:", 4);
    text_out_uint(dc, g_desktop.status_window.left + 220, y, g_kernel_metrics.ram_free_kb);
    TextOutA(dc, g_desktop.status_window.left + 258, y, "KB", 2);
}

static void draw_start_menu(HDC dc)
{
    os_rect_t menu_rect = {
        g_desktop.start_menu.left,
        g_desktop.start_menu.top,
        rect_width(g_desktop.start_menu),
        rect_height(g_desktop.start_menu)
    };

    os_gui_draw_window(menu_rect, "Menu");
    TextOutA(dc, g_desktop.start_menu.left + 8, g_desktop.start_menu.top + 16, "L: Launch", 9);
    TextOutA(dc, g_desktop.start_menu.left + 8, g_desktop.start_menu.top + 28, "S: Suspend", 10);
    TextOutA(dc, g_desktop.start_menu.left + 8, g_desktop.start_menu.top + 40, "R: Resume", 9);
    TextOutA(dc, g_desktop.start_menu.left + 8, g_desktop.start_menu.top + 52, "T: Kill", 7);
}

static void draw_home_screen(HDC dc)
{
    os_rect_t app_rect = {
        g_desktop.app_window.left,
        g_desktop.app_window.top,
        rect_width(g_desktop.app_window),
        rect_height(g_desktop.app_window)
    };

    os_rect_t status_rect = {
        g_desktop.status_window.left,
        g_desktop.status_window.top,
        rect_width(g_desktop.status_window),
        rect_height(g_desktop.status_window)
    };

    os_gui_draw_desktop_background();
    os_gui_draw_taskbar(g_desktop.start_menu_open == TRUE);

    os_gui_draw_window(app_rect, "Desktop");
    draw_app_list(dc);

    os_gui_draw_window(status_rect, "System");
    draw_status_panel(dc);

    if (g_desktop.start_menu_open) {
        draw_start_menu(dc);
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
    SendMessage(g_main_window, WM_LBUTTONDOWN, 0,
                (LPARAM)(((uint32_t)(uint16_t)mx) | ((uint32_t)(uint16_t)my << 16)));
}

static LRESULT CALLBACK desktop_wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    (void)hWnd;

    switch (msg) {
        case WM_KEYDOWN:
            if (wParam >= KEY_1 && wParam <= KEY_4) {
                g_framework.selected_app = (uint32_t)(wParam - KEY_1);
                return 0;
            }
            if (wParam == KEY_L) return SendMessage(g_main_window, OS_WM_LAUNCH, 0, 0);
            if (wParam == KEY_S) return SendMessage(g_main_window, OS_WM_SUSPEND, 0, 0);
            if (wParam == KEY_R) return SendMessage(g_main_window, OS_WM_RESUME, 0, 0);
            if (wParam == KEY_T) return SendMessage(g_main_window, OS_WM_TERMINATE, 0, 0);
            return 0;

        case WM_LBUTTONDOWN: {
            int32_t mx = (int32_t)(int16_t)(lParam & 0xFFFF);
            int32_t my = (int32_t)(int16_t)((lParam >> 16) & 0xFFFF);

            os_rect_t start_button = {
                g_desktop.start_button.left,
                g_desktop.start_button.top,
                rect_width(g_desktop.start_button),
                rect_height(g_desktop.start_button)
            };
            os_rect_t start_menu = {
                g_desktop.start_menu.left,
                g_desktop.start_menu.top,
                rect_width(g_desktop.start_menu),
                rect_height(g_desktop.start_menu)
            };

            if (os_gui_point_in_rect(mx, my, start_button)) {
                g_desktop.start_menu_open = !g_desktop.start_menu_open;
                return 0;
            }

            if (g_desktop.start_menu_open && os_gui_point_in_rect(mx, my, start_menu)) {
                uint32_t row = (uint32_t)(my - g_desktop.start_menu.top);
                if (row < 24) {
                    framework_launch(g_framework.selected_app);
                } else if (row < 36) {
                    framework_suspend(g_framework.selected_app);
                } else if (row < 48) {
                    framework_resume(g_framework.selected_app);
                } else {
                    framework_terminate(g_framework.selected_app);
                }
                g_desktop.start_menu_open = FALSE;
                return 0;
            }

            g_desktop.start_menu_open = FALSE;
            return 0;
        }

        case OS_WM_LAUNCH:
            framework_launch(g_framework.selected_app);
            return 0;

        case OS_WM_SUSPEND:
            framework_suspend(g_framework.selected_app);
            return 0;

        case OS_WM_RESUME:
            framework_resume(g_framework.selected_app);
            return 0;

        case OS_WM_TERMINATE:
            framework_terminate(g_framework.selected_app);
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(g_main_window, &ps);
            draw_home_screen(dc);
            EndPaint(g_main_window, &ps);
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

int main(void)
{
    MSG msg;
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

    WNDCLASS wc;
    wc.style = 0;
    wc.lpfnWndProc = desktop_wnd_proc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = NULL;
    wc.hIcon = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "Sboy28Desktop";

    if (!RegisterClass(&wc)) {
        return -1;
    }

    g_main_window = CreateWindowEx(0, "Sboy28Desktop", "SBoy28 OS Desktop", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                    0, 0, VGA_WIDTH, VGA_HEIGHT, NULL, NULL, NULL, NULL);

    if (!g_main_window) {
        return -1;
    }

    ShowWindow(g_main_window, SW_SHOW);
    UpdateWindow(g_main_window);

    while (1) {
        kernel_scheduler_tick();
        framework_schedule();
        kernel_refresh_ram_metrics();

        if (is_key_pressed()) {
            uint8_t scancode = read_key();
            if (scancode < 0x80) {
                PostMessage(g_main_window, WM_KEYDOWN, (WPARAM)scancode, 0);
            }
        }

        process_mouse();

        while (GetMessage(&msg, NULL, 0, 0)) {
            if (msg.message == WM_NULL) {
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        SendMessage(g_main_window, WM_PAINT, 0, 0);

        mouse_get_position(&mx, &my);
        mouse_get_buttons(&left, &right, &middle);
        os_gui_draw_cursor(mx, my, left || right || middle);
        vga_render();

        thread_yield();
    }

    return 0;
}
