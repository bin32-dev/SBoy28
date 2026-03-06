#include "OS/OS.h"
#include "OS/Grapich/gui.h"
#include "OS/Grapich/windows.h"
#include "common/utils.h"
#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "drivers/rtc.h"
#include "drivers/thread.h"

typedef struct {
    RECT desktop_window;
    RECT info_window;
    RECT start_menu;
    RECT start_button;
    BOOL start_menu_open;
    const char* selected_option;
} desktop_state_t;

static desktop_state_t g_desktop;
static HWND g_main_window;

static int rect_width(RECT rect) { return rect.right - rect.left; }
static int rect_height(RECT rect) { return rect.bottom - rect.top; }

static void desktop_init(void)
{
    g_desktop.desktop_window.left = 10;
    g_desktop.desktop_window.top = 18;
    g_desktop.desktop_window.right = VGA_WIDTH - 10;
    g_desktop.desktop_window.bottom = VGA_HEIGHT - 62;

    g_desktop.info_window.left = 10;
    g_desktop.info_window.top = VGA_HEIGHT - 60;
    g_desktop.info_window.right = VGA_WIDTH - 10;
    g_desktop.info_window.bottom = VGA_HEIGHT - 20;

    g_desktop.start_menu.left = 4;
    g_desktop.start_menu.top = VGA_HEIGHT - 108;
    g_desktop.start_menu.right = 150;
    g_desktop.start_menu.bottom = VGA_HEIGHT - 16;

    g_desktop.start_button.left = 4;
    g_desktop.start_button.top = VGA_HEIGHT - 14;
    g_desktop.start_button.right = 50;
    g_desktop.start_button.bottom = VGA_HEIGHT - 2;

    g_desktop.start_menu_open = FALSE;
    g_desktop.selected_option = "None";
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

static void draw_desktop(HDC dc)
{
    os_rect_t desktop_rect = {
        g_desktop.desktop_window.left,
        g_desktop.desktop_window.top,
        rect_width(g_desktop.desktop_window),
        rect_height(g_desktop.desktop_window)
    };

    os_rect_t info_rect = {
        g_desktop.info_window.left,
        g_desktop.info_window.top,
        rect_width(g_desktop.info_window),
        rect_height(g_desktop.info_window)
    };

    os_gui_draw_desktop_background();
    os_gui_draw_taskbar(g_desktop.start_menu_open == TRUE);

    os_gui_draw_window(desktop_rect, "Desktop");
    TextOutA(dc, g_desktop.desktop_window.left + 10, g_desktop.desktop_window.top + 20,
             "Simple SBoy28 Desktop", 20);
    TextOutA(dc, g_desktop.desktop_window.left + 10, g_desktop.desktop_window.top + 36,
             "Click Start for options.", 24);

    os_gui_draw_window(info_rect, "Task Bar Options");
    TextOutA(dc, g_desktop.info_window.left + 8, g_desktop.info_window.top + 16,
             "Console | Task Manager | Settings | End Session | Shut Down | Restart", 68);

    TextOutA(dc, g_desktop.info_window.left + 8, g_desktop.info_window.top + 28,
             "Selected:", 9);
    TextOutA(dc, g_desktop.info_window.left + 70, g_desktop.info_window.top + 28,
             g_desktop.selected_option, (int)strlen(g_desktop.selected_option));

    draw_clock(dc);

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

static void choose_menu_option(uint32_t row)
{
    if (row < 24) {
        g_desktop.selected_option = "Console";
    } else if (row < 36) {
        g_desktop.selected_option = "Task Manager";
    } else if (row < 48) {
        g_desktop.selected_option = "Settings";
    } else if (row < 60) {
        g_desktop.selected_option = "End Session";
    } else if (row < 72) {
        g_desktop.selected_option = "Shut Down";
    } else {
        g_desktop.selected_option = "Restart";
    }
}

static LRESULT CALLBACK desktop_wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    (void)hWnd;

    switch (msg) {
        case WM_KEYDOWN:
            if (wParam == KEY_C) g_desktop.selected_option = "Console";
            if (wParam == KEY_T) g_desktop.selected_option = "Task Manager";
            if (wParam == KEY_S) g_desktop.selected_option = "Settings";
            if (wParam == KEY_E) g_desktop.selected_option = "End Session";
            if (wParam == KEY_D) g_desktop.selected_option = "Shut Down";
            if (wParam == KEY_R) g_desktop.selected_option = "Restart";
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
    int32_t mx = 0;
    int32_t my = 0;
    bool left = false;
    bool right = false;
    bool middle = false;

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
