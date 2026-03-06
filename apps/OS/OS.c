#include <kernel/power.h>
#include "OS/OS.h"
#include "OS/Grapich/gui.h"
#include "OS/Grapich/windows.h"
#include "common/utils.h"
#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "drivers/rtc.h"
#include "drivers/thread.h"

typedef struct {
    RECT start_menu;
    RECT start_button;
    BOOL start_menu_open;

    const char* active_window_title;
    BOOL active_window_open;
    RECT active_window;
    RECT close_button;
} desktop_state_t;

static desktop_state_t g_desktop;
static HWND g_main_window;

static int rect_width(RECT rect) { return rect.right - rect.left; }
static int rect_height(RECT rect) { return rect.bottom - rect.top; }

static void set_active_window(const char* title)
{
    g_desktop.active_window_title = title;
    g_desktop.active_window_open = TRUE;
}

static void close_active_window(void)
{
    g_desktop.active_window_open = FALSE;
    g_desktop.active_window_title = "None";
}

static void desktop_init(void)
{
    g_desktop.start_menu.left = 4;
    g_desktop.start_menu.top = VGA_HEIGHT - 108;
    g_desktop.start_menu.right = 150;
    g_desktop.start_menu.bottom = VGA_HEIGHT - 16;

    g_desktop.start_button.left = 4;
    g_desktop.start_button.top = VGA_HEIGHT - 14;
    g_desktop.start_button.right = 50;
    g_desktop.start_button.bottom = VGA_HEIGHT - 2;

    g_desktop.start_menu_open = FALSE;
    g_desktop.active_window_title = "None";
    g_desktop.active_window_open = FALSE;

    g_desktop.active_window.left = (VGA_WIDTH / 2) - 100;
    g_desktop.active_window.top = 26;
    g_desktop.active_window.right = g_desktop.active_window.left + 200;
    g_desktop.active_window.bottom = g_desktop.active_window.top + 110;

    g_desktop.close_button.left = g_desktop.active_window.right - 14;
    g_desktop.close_button.top = g_desktop.active_window.top + 2;
    g_desktop.close_button.right = g_desktop.active_window.right - 4;
    g_desktop.close_button.bottom = g_desktop.active_window.top + 10;
}

static void draw_clock(HDC dc)
{
    char* now = get_current_time();
    TextOutA(dc, VGA_WIDTH - 64, VGA_HEIGHT - 12, now, 8);
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

    TextOutA(dc, g_desktop.start_menu.left + 8, g_desktop.start_menu.top + 16, "Console", 7);
    TextOutA(dc, g_desktop.start_menu.left + 8, g_desktop.start_menu.top + 28, "Task Manager", 12);
    TextOutA(dc, g_desktop.start_menu.left + 8, g_desktop.start_menu.top + 40, "Settings", 8);
    TextOutA(dc, g_desktop.start_menu.left + 8, g_desktop.start_menu.top + 52, "End Session", 11);
    TextOutA(dc, g_desktop.start_menu.left + 8, g_desktop.start_menu.top + 64, "Shut Down", 9);
    TextOutA(dc, g_desktop.start_menu.left + 8, g_desktop.start_menu.top + 76, "Restart", 7);
}

static void draw_active_window(HDC dc)
{
    os_rect_t app_rect = {
            g_desktop.active_window.left,
            g_desktop.active_window.top,
            rect_width(g_desktop.active_window),
            rect_height(g_desktop.active_window)
    };

    os_rect_t close_rect = {
            g_desktop.close_button.left,
            g_desktop.close_button.top,
            rect_width(g_desktop.close_button),
            rect_height(g_desktop.close_button)
    };

    os_gui_draw_window(app_rect, g_desktop.active_window_title);
    os_gui_fill_rect(close_rect, BRIGHT_RED);
    TextOutA(dc, g_desktop.close_button.left + 2, g_desktop.close_button.top + 1, "X", 1);

    TextOutA(dc, g_desktop.active_window.left + 10, g_desktop.active_window.top + 20,
             "Window opened from Start menu", 29);

    TextOutA(dc, g_desktop.active_window.left + 10, g_desktop.active_window.top + 36,
             g_desktop.active_window_title, (int)strlen(g_desktop.active_window_title));
}

static void draw_desktop(HDC dc)
{
    os_gui_draw_desktop_background();
    os_gui_draw_taskbar(g_desktop.start_menu_open == TRUE);

    draw_clock(dc);

    if (g_desktop.active_window_open) {
        draw_active_window(dc);
    }

    if (g_desktop.start_menu_open) {
        draw_start_menu(dc);
    }
}

static void process_mouse(void)
{
    int32_t mx, my;
    if (!mouse_consume_left_click()) return;

    mouse_get_position(&mx, &my);
    SendMessage(g_main_window, WM_LBUTTONDOWN, 0,
                (LPARAM)(((uint32_t)(uint16_t)mx) | ((uint32_t)(uint16_t)my << 16)));
}

static void show_shutdown_message(const char* msg)
{
    os_rect_t msg_box = {VGA_WIDTH/2 - 60, VGA_HEIGHT/2 - 20, 140, 20};
    os_gui_draw_window(msg_box, msg);
    vga_render();
    Sleep(2);
}


static void choose_menu_option(uint32_t row)
{
    if (row < 24) {
        set_active_window("Console");
    } else if (row < 36) {
        set_active_window("Task Manager");
    } else if (row < 48) {
        set_active_window("Settings");
    } else if (row < 60) {
        set_active_window("End Session");
    } else if (row < 72) {
        show_shutdown_message("Shutting Down...");
        kernel_shutdown();
    } else {
        show_shutdown_message("Restarting...");
        kernel_restart();
    }
}

static LRESULT CALLBACK desktop_wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    (void)hWnd;

    switch (msg) {
        case WM_KEYDOWN:
            if (wParam == KEY_C) set_active_window("Console");
            if (wParam == KEY_T) set_active_window("Task Manager");
            if (wParam == KEY_S) set_active_window("Settings");
            if (wParam == KEY_E) set_active_window("End Session");
            if (wParam == KEY_D) set_active_window("Shut Down");
            if (wParam == KEY_R) set_active_window("Restart");
            if (wParam == KEY_ESC) close_active_window();
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

            if (g_desktop.active_window_open) {
                os_rect_t close_button = {
                        g_desktop.close_button.left,
                        g_desktop.close_button.top,
                        rect_width(g_desktop.close_button),
                        rect_height(g_desktop.close_button)
                };
                if (os_gui_point_in_rect(mx, my, close_button)) {
                    close_active_window();
                    g_desktop.start_menu_open = FALSE;
                    return 0;
                }
            }

            if (g_desktop.start_menu_open && os_gui_point_in_rect(mx, my, start_menu)) {
                choose_menu_option((uint32_t)(my - g_desktop.start_menu.top));
                g_desktop.start_menu_open = FALSE;
                return 0;
            }

            g_desktop.start_menu_open = FALSE;
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(g_main_window, &ps);
            draw_desktop(dc);
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
    int32_t mx = 0, my = 0;
    bool left = false, right = false, middle = false;

    desktop_init();

    WNDCLASS wc = {0};
    wc.lpfnWndProc = desktop_wnd_proc;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "Sboy28Desktop";

    if (!RegisterClass(&wc)) return -1;

    g_main_window = CreateWindowEx(0, "Sboy28Desktop", "SBoy28 OS Desktop",
                                   WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                   0, 0, VGA_WIDTH, VGA_HEIGHT,
                                   NULL, NULL, NULL, NULL);

    if (!g_main_window) return -1;

    ShowWindow(g_main_window, SW_SHOW);
    UpdateWindow(g_main_window);

    while (1) {
        if (is_key_pressed()) {
            uint8_t scancode = read_key();
            if (scancode < 0x80) {
                PostMessage(g_main_window, WM_KEYDOWN, (WPARAM)scancode, 0);
            }
        }

        process_mouse();

        while (GetMessage(&msg, NULL, 0, 0)) {
            if (msg.message == WM_NULL) break;
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