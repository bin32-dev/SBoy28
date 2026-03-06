#include "OS/OS.h"
#include "OS/Grapich/gui.h"
#include "OS/Grapich/windows.h"
#include "common/utils.h"
#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "drivers/rtc.h"
#include "drivers/thread.h"
#include "kernel/power.h"

typedef struct {
    RECT start_menu;
    RECT start_button;
    BOOL start_menu_open;
    const char* active_window_title;
    BOOL active_window_open;
    RECT active_window;
    RECT close_button;
    char console_input[64];
    uint32_t console_input_len;
    char console_output[12][64];
    uint32_t console_output_count;
} desktop_state_t;

static desktop_state_t g_desktop;
static HWND g_main_window;


void os_shutdown(void)
{
    kernel_shutdown();
}

void os_restart(void)
{
    kernel_restart();
}

static int rect_width(RECT rect) { return rect.right - rect.left; }
static int rect_height(RECT rect) { return rect.bottom - rect.top; }

static void close_active_window(void);

static bool str_eq(const char* left, const char* right)
{
    uint32_t i = 0;

    if (!left || !right) {
        return false;
    }

    while (left[i] && right[i]) {
        if (left[i] != right[i]) {
            return false;
        }
        i++;
    }

    return left[i] == right[i];
}

static bool str_starts_with(const char* text, const char* prefix)
{
    uint32_t i = 0;

    if (!text || !prefix) {
        return false;
    }

    while (prefix[i]) {
        if (text[i] != prefix[i]) {
            return false;
        }
        i++;
    }

    return true;
}

static void console_push_line(const char* text)
{
    uint32_t i;

    if (!text) {
        return;
    }

    if (g_desktop.console_output_count >= 12) {
        for (i = 1; i < 12; i++) {
            memcpy(g_desktop.console_output[i - 1], g_desktop.console_output[i], 64);
        }
        g_desktop.console_output_count = 11;
    }

    memset(g_desktop.console_output[g_desktop.console_output_count], 0, 64);
    for (i = 0; i < 63 && text[i]; i++) {
        g_desktop.console_output[g_desktop.console_output_count][i] = text[i];
    }

    g_desktop.console_output_count++;
}

static void console_clear(void)
{
    memset(g_desktop.console_output, 0, sizeof(g_desktop.console_output));
    g_desktop.console_output_count = 0;
}

static void console_execute_command(void)
{
    char command_line[64];
    char response[64];
    uint32_t i;

    memset(command_line, 0, sizeof(command_line));
    for (i = 0; i < g_desktop.console_input_len && i < 63; i++) {
        command_line[i] = g_desktop.console_input[i];
    }

    if (g_desktop.console_input_len > 0) {
        char prompt_line[64];
        memset(prompt_line, 0, sizeof(prompt_line));
        prompt_line[0] = '>';
        prompt_line[1] = ' ';
        for (i = 0; i < g_desktop.console_input_len && i < 61; i++) {
            prompt_line[i + 2] = g_desktop.console_input[i];
        }
        console_push_line(prompt_line);
    }

    if (g_desktop.console_input_len == 0) {
        console_push_line("Type 'help' for commands");
    } else if (str_eq(command_line, "help")) {
        console_push_line("help clear echo time about exit shutdown restart");
    } else if (str_eq(command_line, "clear")) {
        console_clear();
    } else if (str_starts_with(command_line, "echo ")) {
        console_push_line(command_line + 5);
    } else if (str_eq(command_line, "time")) {
        char* now = get_current_time();
        memset(response, 0, sizeof(response));
        response[0] = 'T';
        response[1] = 'i';
        response[2] = 'm';
        response[3] = 'e';
        response[4] = ':';
        response[5] = ' ';
        for (i = 0; i < 8 && now[i]; i++) {
            response[i + 6] = now[i];
        }
        console_push_line(response);
    } else if (str_eq(command_line, "about")) {
        console_push_line("SBoy28 desktop console v1");
    } else if (str_eq(command_line, "shutdown")) {
        console_push_line("Shutting down...");
        os_shutdown();
    } else if (str_eq(command_line, "restart")) {
        console_push_line("Restarting...");
        os_restart();
    } else if (str_eq(command_line, "exit")) {
        close_active_window();
    } else {
        console_push_line("Unknown command. Type help");
    }

    memset(g_desktop.console_input, 0, sizeof(g_desktop.console_input));
    g_desktop.console_input_len = 0;
}

static char scancode_to_ascii(uint8_t scancode)
{
    if (scancode >= KEY_A && scancode <= KEY_L) {
        static const char row1[] = { 'a','s','d','f','g','h','j','k','l' };
        return row1[scancode - KEY_A];
    }
    if (scancode >= KEY_Q && scancode <= KEY_P) {
        static const char row2[] = { 'q','w','e','r','t','y','u','i','o','p' };
        return row2[scancode - KEY_Q];
    }
    if (scancode >= KEY_Z && scancode <= KEY_M) {
        static const char row3[] = { 'z','x','c','v','b','n','m' };
        return row3[scancode - KEY_Z];
    }

    switch (scancode) {
        case KEY_0: return '0';
        case KEY_1: return '1';
        case KEY_2: return '2';
        case KEY_3: return '3';
        case KEY_4: return '4';
        case KEY_5: return '5';
        case KEY_6: return '6';
        case KEY_7: return '7';
        case KEY_8: return '8';
        case KEY_9: return '9';
        case KEY_SPACE: return ' ';
        case KEY_MINUS: return '-';
        case KEY_PERIOD: return '.';
        case KEY_SLASH: return '/';
        default: return '\0';
    }
}

static void set_active_window(const char* title)
{
    g_desktop.active_window_title = title;
    g_desktop.active_window_open = TRUE;
    g_desktop.active_window.left = 0;
    g_desktop.active_window.top = 0;
    g_desktop.active_window.right = VGA_WIDTH;
    g_desktop.active_window.bottom = VGA_HEIGHT - 16;

    g_desktop.close_button.left = g_desktop.active_window.right - 14;
    g_desktop.close_button.top = g_desktop.active_window.top + 2;
    g_desktop.close_button.right = g_desktop.active_window.right - 4;
    g_desktop.close_button.bottom = g_desktop.active_window.top + 10;

    if (str_eq(title, "Console")) {
        console_clear();
        console_push_line("Console ready. Type help");
        memset(g_desktop.console_input, 0, sizeof(g_desktop.console_input));
        g_desktop.console_input_len = 0;
    }
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

    g_desktop.active_window.left = 0;
    g_desktop.active_window.top = 0;
    g_desktop.active_window.right = VGA_WIDTH;
    g_desktop.active_window.bottom = VGA_HEIGHT - 16;

    g_desktop.close_button.left = g_desktop.active_window.right - 14;
    g_desktop.close_button.top = g_desktop.active_window.top + 2;
    g_desktop.close_button.right = g_desktop.active_window.right - 4;
    g_desktop.close_button.bottom = g_desktop.active_window.top + 10;

    memset(g_desktop.console_input, 0, sizeof(g_desktop.console_input));
    g_desktop.console_input_len = 0;
    console_clear();
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

    if (str_eq(g_desktop.active_window_title, "Console")) {
        uint32_t i;
        int32_t y = g_desktop.active_window.top + 14;

        TextOutA(dc, g_desktop.active_window.left + 6, y,
                 "Type commands then press ENTER", 29);
        y += 12;

        for (i = 0; i < g_desktop.console_output_count; i++) {
            TextOutA(dc, g_desktop.active_window.left + 6, y,
                     g_desktop.console_output[i], (int)strlen(g_desktop.console_output[i]));
            y += 10;
        }

        {
            char input_line[64];
            memset(input_line, 0, sizeof(input_line));
            input_line[0] = '>';
            input_line[1] = ' ';
            for (i = 0; i < g_desktop.console_input_len && i < 61; i++) {
                input_line[i + 2] = g_desktop.console_input[i];
            }
            TextOutA(dc, g_desktop.active_window.left + 6, g_desktop.active_window.bottom - 12,
                     input_line, (int)strlen(input_line));
        }
    } else {
        TextOutA(dc, g_desktop.active_window.left + 10, g_desktop.active_window.top + 20,
                 "Window opened from Start menu", 29);
        TextOutA(dc, g_desktop.active_window.left + 10, g_desktop.active_window.top + 36,
                 g_desktop.active_window_title, (int)strlen(g_desktop.active_window_title));
    }
}

static void console_handle_key(uint8_t key)
{
    char ch;

    if (!g_desktop.active_window_open || !str_eq(g_desktop.active_window_title, "Console")) {
        return;
    }

    if (key == KEY_ENTER) {
        console_execute_command();
        return;
    }

    if (key == KEY_BACKSPACE) {
        if (g_desktop.console_input_len > 0) {
            g_desktop.console_input_len--;
            g_desktop.console_input[g_desktop.console_input_len] = '\0';
        }
        return;
    }

    ch = scancode_to_ascii(key);
    if (ch != '\0' && g_desktop.console_input_len < (sizeof(g_desktop.console_input) - 1)) {
        g_desktop.console_input[g_desktop.console_input_len++] = ch;
        g_desktop.console_input[g_desktop.console_input_len] = '\0';
    }
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
        set_active_window("Console");
    } else if (row < 36) {
        set_active_window("Task Manager");
    } else if (row < 48) {
        set_active_window("Settings");
    } else if (row < 60) {
        set_active_window("End Session");
    } else if (row < 72) {
        os_shutdown();
    } else {
        os_restart();
    }
}

static LRESULT CALLBACK desktop_wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    (void)hWnd;

    switch (msg) {
        case WM_KEYDOWN:
            console_handle_key((uint8_t)wParam);
            if (wParam == KEY_C) set_active_window("Console");
            if (wParam == KEY_T) set_active_window("Task Manager");
            if (wParam == KEY_S) set_active_window("Settings");
            if (wParam == KEY_E) set_active_window("End Session");
            if (wParam == KEY_D) os_shutdown();
            if (wParam == KEY_R) os_restart();
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
