#include "OS/Grapich/windows.h"
#include "OS/Grapich/gui.h"
#include "drivers/vga.h"
#include "ui/colors.h"
#include "kernel/kheap.h"

#include <common/utils.h>

#define OS_MAX_WINDOWS 16
#define OS_MAX_CLASSES 16

typedef struct {
    int in_use;
    UINT atom;
    WNDPROC wnd_proc;
    char class_name[64];
} os_window_class_t;

typedef struct {
    int in_use;
    int visible;
    RECT bounds;
    DWORD style;
    DWORD ex_style;
    os_window_class_t* window_class;
    char title[128];
} os_window_t;

typedef struct {
    COLORREF color;
} os_brush_t;

typedef struct {
    UINT msg;
    WPARAM w_param;
    LPARAM l_param;
    HWND hwnd;
} os_message_t;

static os_window_class_t g_classes[OS_MAX_CLASSES];
static os_window_t g_windows[OS_MAX_WINDOWS];
static os_message_t g_pending_message;
static BOOL g_has_pending_message;
static BOOL g_quit_requested;
static int g_quit_code;
static UINT g_next_atom = 1;

static int strcmp(char name[64], LPCSTR name1);

void strncpy(char name[64], LPCSTR name1, size_t i);


static os_window_class_t* find_class_by_name(LPCSTR name)
{
    if (!name) {
        return NULL;
    }

    for (int i = 0; i < OS_MAX_CLASSES; i++) {
        if (g_classes[i].in_use && strcmp(g_classes[i].class_name, name) == 0) {
            return &g_classes[i];
        }
    }

    return NULL;
}

static int strcmp(char name[64], LPCSTR name1)
{
    int i = 0;

    while (i < 64 && name[i] && name1[i]) {
        if (name[i] != name1[i]) {
            return (unsigned char)name[i] - (unsigned char)name1[i];
        }
        i++;
    }

    if (i == 64)
    {
        return 0;
    }

    return (unsigned char)name[i] - (unsigned char)name1[i];
}

static BOOL is_valid_window(os_window_t* window)
{
    if (!window) {
        return FALSE;
    }

    return window->in_use ? TRUE : FALSE;
}

static void draw_window(os_window_t* window)
{
    if (!is_valid_window(window) || !window->visible) {
        return;
    }

    os_rect_t rect = {
        window->bounds.left,
        window->bounds.top,
        window->bounds.right - window->bounds.left,
        window->bounds.bottom - window->bounds.top
    };

    os_gui_draw_window(rect, window->title);
}

static void redraw_desktop(void)
{
    for (int i = 0; i < OS_MAX_WINDOWS; i++) {
        draw_window(&g_windows[i]);
    }
}

ATOM WINAPI RegisterClass(CONST WNDCLASS* lpWndClass)
{
    if (!lpWndClass || !lpWndClass->lpszClassName || !lpWndClass->lpfnWndProc) {
        return 0;
    }

    if (find_class_by_name(lpWndClass->lpszClassName)) {
        return 0;
    }

    for (int i = 0; i < OS_MAX_CLASSES; i++) {
        if (!g_classes[i].in_use) {
            g_classes[i].in_use = 1;
            g_classes[i].atom = g_next_atom++;
            g_classes[i].wnd_proc = lpWndClass->lpfnWndProc;
            strncpy(g_classes[i].class_name, lpWndClass->lpszClassName, sizeof(g_classes[i].class_name) - 1);
            g_classes[i].class_name[sizeof(g_classes[i].class_name) - 1] = '\0';
            return (ATOM)g_classes[i].atom;
        }
    }

    return 0;
}

void strncpy(char name[64], LPCSTR name1, size_t n)
{
    size_t i = 0;

    // Copy characters until n or null terminator
    while (i < n && i < 64 && name1[i]) {
        name[i] = name1[i];
        i++;
    }

    // Pad with null characters if needed
    while (i < n && i < 64) {
        name[i] = '\0';
        i++;
    }

    // Ensure the last byte is null (buffer safety)
    if (i == 64) {
        name[63] = '\0';
    }
}

ATOM WINAPI RegisterClassEx(CONST WNDCLASSEX* lpWndClassEx)
{
    if (!lpWndClassEx) {
        return 0;
    }

    WNDCLASS compat;
    compat.style = lpWndClassEx->style;
    compat.lpfnWndProc = lpWndClassEx->lpfnWndProc;
    compat.cbClsExtra = lpWndClassEx->cbClsExtra;
    compat.cbWndExtra = lpWndClassEx->cbWndExtra;
    compat.hInstance = lpWndClassEx->hInstance;
    compat.hIcon = lpWndClassEx->hIcon;
    compat.hCursor = lpWndClassEx->hCursor;
    compat.hbrBackground = lpWndClassEx->hbrBackground;
    compat.lpszMenuName = lpWndClassEx->lpszMenuName;
    compat.lpszClassName = lpWndClassEx->lpszClassName;
    return RegisterClass(&compat);
}

HWND WINAPI CreateWindowEx(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle,
                           int X, int Y, int nWidth, int nHeight, HWND hWndParent,
                           HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
    (void)hWndParent;
    (void)hMenu;
    (void)hInstance;
    (void)lpParam;

    os_window_class_t* klass = find_class_by_name(lpClassName);
    if (!klass) {
        return NULL;
    }

    for (int i = 0; i < OS_MAX_WINDOWS; i++) {
        if (!g_windows[i].in_use) {
            g_windows[i].in_use = 1;
            g_windows[i].visible = (dwStyle & WS_VISIBLE) ? 1 : 0;
            g_windows[i].style = dwStyle;
            g_windows[i].ex_style = dwExStyle;
            g_windows[i].window_class = klass;
            g_windows[i].bounds.left = X;
            g_windows[i].bounds.top = Y;
            g_windows[i].bounds.right = X + nWidth;
            g_windows[i].bounds.bottom = Y + nHeight;

            if (lpWindowName) {
                strncpy(g_windows[i].title, lpWindowName, sizeof(g_windows[i].title) - 1);
                g_windows[i].title[sizeof(g_windows[i].title) - 1] = '\0';
            } else {
                g_windows[i].title[0] = '\0';
            }

            if (g_windows[i].visible) {
                draw_window(&g_windows[i]);
            }

            return (HWND)&g_windows[i];
        }
    }

    return NULL;
}

BOOL WINAPI DestroyWindow(HWND hWnd)
{
    os_window_t* window = (os_window_t*)hWnd;
    if (!is_valid_window(window)) {
        return FALSE;
    }

    memset(window, 0, sizeof(*window));
    redraw_desktop();
    return TRUE;
}

BOOL WINAPI ShowWindow(HWND hWnd, int nCmdShow)
{
    os_window_t* window = (os_window_t*)hWnd;
    if (!is_valid_window(window)) {
        return FALSE;
    }

    window->visible = (nCmdShow == SW_HIDE) ? 0 : 1;
    redraw_desktop();
    return TRUE;
}

BOOL WINAPI UpdateWindow(HWND hWnd)
{
    os_window_t* window = (os_window_t*)hWnd;
    if (!is_valid_window(window)) {
        return FALSE;
    }

    draw_window(window);
    return TRUE;
}

BOOL WINAPI GetMessage(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax)
{
    (void)hWnd;
    (void)wMsgFilterMin;
    (void)wMsgFilterMax;

    if (!lpMsg) {
        return FALSE;
    }

    if (g_quit_requested) {
        lpMsg->message = WM_QUIT;
        lpMsg->wParam = (WPARAM)g_quit_code;
        lpMsg->lParam = 0;
        lpMsg->hwnd = NULL;
        return FALSE;
    }

    if (g_has_pending_message) {
        lpMsg->message = g_pending_message.msg;
        lpMsg->wParam = g_pending_message.w_param;
        lpMsg->lParam = g_pending_message.l_param;
        lpMsg->hwnd = g_pending_message.hwnd;
        g_has_pending_message = FALSE;
        return TRUE;
    }

    lpMsg->message = WM_NULL;
    lpMsg->wParam = 0;
    lpMsg->lParam = 0;
    lpMsg->hwnd = NULL;
    return TRUE;
}

BOOL WINAPI TranslateMessage(CONST MSG* lpMsg)
{
    (void)lpMsg;
    return TRUE;
}

LRESULT WINAPI DispatchMessage(CONST MSG* lpMsg)
{
    if (!lpMsg || !lpMsg->hwnd) {
        return 0;
    }

    os_window_t* window = (os_window_t*)lpMsg->hwnd;
    if (!is_valid_window(window) || !window->window_class || !window->window_class->wnd_proc) {
        return 0;
    }

    return window->window_class->wnd_proc(lpMsg->hwnd, lpMsg->message, lpMsg->wParam, lpMsg->lParam);
}

BOOL WINAPI PostMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    g_pending_message.hwnd = hWnd;
    g_pending_message.msg = Msg;
    g_pending_message.w_param = wParam;
    g_pending_message.l_param = lParam;
    g_has_pending_message = TRUE;
    return TRUE;
}

LRESULT WINAPI SendMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    MSG msg;
    msg.hwnd = hWnd;
    msg.message = Msg;
    msg.wParam = wParam;
    msg.lParam = lParam;
    msg.time = 0;
    msg.pt.x = 0;
    msg.pt.y = 0;
    return DispatchMessage(&msg);
}

void WINAPI PostQuitMessage(int nExitCode)
{
    g_quit_requested = TRUE;
    g_quit_code = nExitCode;
}

int WINAPI MessageBox(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
    (void)hWnd;
    (void)uType;

    if (lpCaption) {
        vga_draw_string(4, 4, lpCaption, WHITE);
    }
    if (lpText) {
        vga_draw_string(4, 16, lpText, YELLOW);
    }
    return IDOK;
}

int WINAPI GetWindowText(HWND hWnd, LPSTR lpString, int nMaxCount)
{
    os_window_t* window = (os_window_t*)hWnd;
    if (!is_valid_window(window) || !lpString || nMaxCount <= 0) {
        return 0;
    }

    strncpy(lpString, window->title, (size_t)nMaxCount - 1);
    lpString[nMaxCount - 1] = '\0';
    return (int)strlen(lpString);
}

BOOL WINAPI SetWindowText(HWND hWnd, LPCSTR lpString)
{
    os_window_t* window = (os_window_t*)hWnd;
    if (!is_valid_window(window)) {
        return FALSE;
    }

    if (!lpString) {
        window->title[0] = '\0';
    } else {
        strncpy(window->title, lpString, sizeof(window->title) - 1);
        window->title[sizeof(window->title) - 1] = '\0';
    }

    draw_window(window);
    return TRUE;
}

BOOL WINAPI MoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint)
{
    os_window_t* window = (os_window_t*)hWnd;
    if (!is_valid_window(window)) {
        return FALSE;
    }

    window->bounds.left = X;
    window->bounds.top = Y;
    window->bounds.right = X + nWidth;
    window->bounds.bottom = Y + nHeight;

    if (bRepaint) {
        redraw_desktop();
    }

    return TRUE;
}

BOOL WINAPI SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags)
{
    (void)hWndInsertAfter;

    os_window_t* window = (os_window_t*)hWnd;
    if (!is_valid_window(window)) {
        return FALSE;
    }

    if (!(uFlags & SWP_NOMOVE)) {
        window->bounds.left = X;
        window->bounds.top = Y;
    }

    if (!(uFlags & SWP_NOSIZE)) {
        window->bounds.right = window->bounds.left + cx;
        window->bounds.bottom = window->bounds.top + cy;
    }

    if (!(uFlags & SWP_NOREDRAW)) {
        redraw_desktop();
    }

    return TRUE;
}

BOOL WINAPI GetClientRect(HWND hWnd, LPRECT lpRect)
{
    os_window_t* window = (os_window_t*)hWnd;
    if (!is_valid_window(window) || !lpRect) {
        return FALSE;
    }

    lpRect->left = 0;
    lpRect->top = 0;
    lpRect->right = window->bounds.right - window->bounds.left;
    lpRect->bottom = window->bounds.bottom - window->bounds.top;
    return TRUE;
}

BOOL WINAPI GetWindowRect(HWND hWnd, LPRECT lpRect)
{
    os_window_t* window = (os_window_t*)hWnd;
    if (!is_valid_window(window) || !lpRect) {
        return FALSE;
    }

    *lpRect = window->bounds;
    return TRUE;
}

HCURSOR WINAPI LoadCursor(HINSTANCE hInstance, LPCSTR lpCursorName)
{
    (void)hInstance;
    return (HCURSOR)lpCursorName;
}

HCURSOR WINAPI SetCursor(HCURSOR hCursor)
{
    return hCursor;
}

BOOL WINAPI GetCursorPos(LPPOINT lpPoint)
{
    if (!lpPoint) {
        return FALSE;
    }

    lpPoint->x = 0;
    lpPoint->y = 0;
    return TRUE;
}

BOOL WINAPI ScreenToClient(HWND hWnd, LPPOINT lpPoint)
{
    os_window_t* window = (os_window_t*)hWnd;
    if (!is_valid_window(window) || !lpPoint) {
        return FALSE;
    }

    lpPoint->x -= window->bounds.left;
    lpPoint->y -= window->bounds.top;
    return TRUE;
}

BOOL WINAPI ClientToScreen(HWND hWnd, LPPOINT lpPoint)
{
    os_window_t* window = (os_window_t*)hWnd;
    if (!is_valid_window(window) || !lpPoint) {
        return FALSE;
    }

    lpPoint->x += window->bounds.left;
    lpPoint->y += window->bounds.top;
    return TRUE;
}

HDC WINAPI BeginPaint(HWND hWnd, LPPAINTSTRUCT lpPaint)
{
    if (lpPaint) {
        memset(lpPaint, 0, sizeof(*lpPaint));
        GetClientRect(hWnd, &lpPaint->rcPaint);
    }

    return (HDC)hWnd;
}

BOOL WINAPI EndPaint(HWND hWnd, CONST PAINTSTRUCT* lpPaint)
{
    (void)hWnd;
    (void)lpPaint;
    return TRUE;
}

BOOL WINAPI TextOutA(HDC hdc, int x, int y, LPCSTR lpString, int c)
{
    (void)hdc;

    if (!lpString || c <= 0) {
        return FALSE;
    }

    char buffer[256];
    int count = (c < (int)sizeof(buffer) - 1) ? c : (int)sizeof(buffer) - 1;
    memcpy(buffer, lpString, (size_t)count);
    buffer[count] = '\0';
    vga_draw_string((uint32_t)x, (uint32_t)y, buffer, WHITE);
    return TRUE;
}

int WINAPI DrawTextA(HDC hdc, LPCSTR lpchText, int cchText, LPRECT lprc, UINT format)
{
    (void)format;
    if (!lprc) {
        return 0;
    }

    if (cchText < 0 && lpchText) {
        cchText = (int)strlen(lpchText);
    }

    if (lpchText && cchText > 0) {
        TextOutA(hdc, lprc->left, lprc->top, lpchText, cchText);
    }

    return 8;
}

HBRUSH WINAPI CreateSolidBrush(COLORREF crColor)
{
    os_brush_t* brush = (os_brush_t*)kmalloc(sizeof(os_brush_t));
    if (!brush) {
        return NULL;
    }

    brush->color = crColor;
    return (HBRUSH)brush;
}



HGDIOBJ WINAPI SelectObject(HDC hdc, HGDIOBJ hgdiobj)
{
    (void)hdc;
    return hgdiobj;
}

BOOL WINAPI DeleteObject(HGDIOBJ hObject)
{
    if (!hObject) {
        return FALSE;
    }

    kfree(hObject);
    return TRUE;
}

BOOL WINAPI Rectangle(HDC hdc, int left, int top, int right, int bottom)
{
    (void)hdc;
    os_rect_t rect = { left, top, right - left, bottom - top };
    os_gui_draw_rect_outline(rect, WHITE);
    return TRUE;
}

int WINAPI FillRect(HDC hdc, CONST RECT* lprc, HBRUSH hbr)
{
    (void)hdc;

    if (!lprc || !hbr) {
        return 0;
    }

    os_brush_t* brush = (os_brush_t*)hbr;
    os_rect_t rect = {
        lprc->left,
        lprc->top,
        lprc->right - lprc->left,
        lprc->bottom - lprc->top
    };
    os_gui_fill_rect(rect, (uint8_t)(brush->color & 0xFFu));
    return 1;
}

HDC WINAPI GetDC(HWND hWnd)
{
    return (HDC)hWnd;
}

int WINAPI ReleaseDC(HWND hWnd, HDC hDC)
{
    (void)hWnd;
    (void)hDC;
    return 1;
}

LRESULT WINAPI DefWindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    (void)hWnd;
    (void)wParam;
    (void)lParam;

    if (Msg == WM_CLOSE) {
        return 0;
    }

    return 0;
}
