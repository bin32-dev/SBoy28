#ifndef WINDOWS_H
#define WINDOWS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Win32 API Compatibility Header for Custom Operating System
 *
 * This header provides source-level compatibility with the Microsoft Win32 API,
 * enabling existing Windows GUI applications to compile and run against the
 * custom OS user-space implementation. It replicates the architecture, types,
 * constants, structures, and function signatures from the early Win32 SDK.
 *
 * All types are based on stdint.h for portability across 32/64-bit compilers
 * and platforms. Handles are opaque pointers for type safety.
 */

/* =========================================================
   Primitive Types and Definitions
   ========================================================= */

typedef uint8_t  BYTE;
typedef uint16_t WORD;
typedef uint32_t UINT;
typedef uint32_t DWORD;
typedef int32_t  LONG;
typedef int      BOOL;
typedef uint64_t QWORD;

typedef intptr_t  INT_PTR;
typedef uintptr_t UINT_PTR;
typedef INT_PTR   LONG_PTR;
typedef UINT_PTR  ULONG_PTR;

typedef LONG_PTR  LRESULT;
typedef UINT_PTR  WPARAM;
typedef LONG_PTR  LPARAM;

typedef WORD ATOM;

#define TRUE  1
#define FALSE 0

#ifndef NULL
#define NULL ((void*)0)
#endif

#define CONST const

/* String types (ANSI variants for A functions) */
typedef char CHAR;
typedef CHAR *LPSTR;
typedef const CHAR *LPCSTR;
typedef void *LPVOID;

/* Core handle definitions (opaque for type safety) */
typedef void *PVOID;
typedef PVOID HANDLE;

#define DECLARE_HANDLE(name) struct name##__ { int unused; }; typedef struct name##__ *name

DECLARE_HANDLE(HWND);
DECLARE_HANDLE(HDC);
DECLARE_HANDLE(HINSTANCE);
DECLARE_HANDLE(HBRUSH);
DECLARE_HANDLE(HCURSOR);
DECLARE_HANDLE(HICON);
DECLARE_HANDLE(HMENU);
DECLARE_HANDLE(HGDIOBJ);

/* Calling conventions (for source compatibility across toolchains) */
#if defined(_MSC_VER)
#define WINAPI   __stdcall
#define CALLBACK __stdcall
#define WINAPIV  __cdecl
#elif defined(__GNUC__) && (defined(__i386__) || defined(_M_IX86))
#define WINAPI   __attribute__((stdcall))
#define CALLBACK __attribute__((stdcall))
#define WINAPIV  __attribute__((cdecl))
#else
#define WINAPI
#define CALLBACK
#define WINAPIV
#endif

/* Window procedure type */
typedef LRESULT (CALLBACK *WNDPROC)(HWND, UINT, WPARAM, LPARAM);

/* =========================================================
   Basic Geometric Structures
   ========================================================= */

typedef struct tagPOINT {
    LONG x;
    LONG y;
} POINT, *PPOINT, *LPPOINT;

typedef struct tagRECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT, *PRECT, *LPRECT;

/* =========================================================
   Message and Painting Structures
   ========================================================= */

typedef struct tagMSG {
    HWND   hwnd;
    UINT   message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD  time;
    POINT  pt;
} MSG, *PMSG, *LPMSG;

typedef struct tagPAINTSTRUCT {
    HDC  hdc;
    BOOL fErase;
    RECT rcPaint;
    BOOL fRestore;
    BOOL fIncUpdate;
    BYTE rgbReserved[32];
} PAINTSTRUCT, *PPAINTSTRUCT, *LPPAINTSTRUCT;

/* =========================================================
   Window Class Structures
   ========================================================= */

typedef struct tagWNDCLASSA {
    UINT      style;
    WNDPROC   lpfnWndProc;
    int       cbClsExtra;
    int       cbWndExtra;
    HINSTANCE hInstance;
    HICON     hIcon;
    HCURSOR   hCursor;
    HBRUSH    hbrBackground;
    LPCSTR    lpszMenuName;
    LPCSTR    lpszClassName;
} WNDCLASSA, *PWNDCLASSA, *LPWNDCLASSA;

typedef WNDCLASSA WNDCLASS;

typedef struct tagWNDCLASSEXA {
    UINT      cbSize;
    UINT      style;
    WNDPROC   lpfnWndProc;
    int       cbClsExtra;
    int       cbWndExtra;
    HINSTANCE hInstance;
    HICON     hIcon;
    HCURSOR   hCursor;
    HBRUSH    hbrBackground;
    LPCSTR    lpszMenuName;
    LPCSTR    lpszClassName;
    HICON     hIconSm;
} WNDCLASSEXA, *PWNDCLASSEXA, *LPWNDCLASSEXA;

typedef WNDCLASSEXA WNDCLASSEX;

/* =========================================================
   GDI Color and Helper Types
   ========================================================= */

typedef DWORD COLORREF;

#define RGB(r,g,b)          ((COLORREF)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))
#define GetRValue(rgb)      ((BYTE)(rgb))
#define GetGValue(rgb)      ((BYTE)(((WORD)(rgb)) >> 8))
#define GetBValue(rgb)      ((BYTE)((rgb)>>16))

/* =========================================================
   Window Styles
   ========================================================= */

#define WS_OVERLAPPED       0x00000000L
#define WS_POPUP            0x80000000L
#define WS_CHILD            0x40000000L
#define WS_MINIMIZE         0x20000000L
#define WS_VISIBLE          0x10000000L
#define WS_DISABLED         0x08000000L
#define WS_CLIPSIBLINGS     0x04000000L
#define WS_CLIPCHILDREN     0x02000000L
#define WS_MAXIMIZE         0x01000000L
#define WS_CAPTION          0x00C00000L
#define WS_BORDER           0x00800000L
#define WS_DLGFRAME         0x00400000L
#define WS_VSCROLL          0x00200000L
#define WS_HSCROLL          0x00100000L
#define WS_SYSMENU          0x00080000L
#define WS_THICKFRAME       0x00040000L
#define WS_GROUP            0x00020000L
#define WS_TABSTOP          0x00010000L
#define WS_MINIMIZEBOX      0x00020000L
#define WS_MAXIMIZEBOX      0x00010000L

#define WS_TILED            WS_OVERLAPPED
#define WS_ICONIC           WS_MINIMIZE
#define WS_SIZEBOX          WS_THICKFRAME
#define WS_TILEDWINDOW      WS_OVERLAPPEDWINDOW

#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#define WS_POPUPWINDOW      (WS_POPUP | WS_BORDER | WS_SYSMENU)
#define WS_CHILDWINDOW      (WS_CHILD)

/* Extended Window Styles */
#define WS_EX_DLGMODALFRAME     0x00000001L
#define WS_EX_NOPARENTNOTIFY    0x00000004L
#define WS_EX_TOPMOST           0x00000008L
#define WS_EX_ACCEPTFILES       0x00000010L
#define WS_EX_TRANSPARENT       0x00000020L
#define WS_EX_TOOLWINDOW        0x00000080L
#define WS_EX_WINDOWEDGE        0x00000100L
#define WS_EX_CLIENTEDGE        0x00000200L
#define WS_EX_CONTEXTHELP       0x00000400L
#define WS_EX_RIGHT             0x00001000L
#define WS_EX_RTLREADING        0x00002000L
#define WS_EX_LEFTSCROLLBAR     0x00004000L
#define WS_EX_CONTROLPARENT     0x00010000L
#define WS_EX_STATICEDGE        0x00020000L
#define WS_EX_APPWINDOW         0x00040000L

/* =========================================================
   Window Messages (WM_*)
   ========================================================= */

#define WM_NULL                         0x0000
#define WM_CREATE                       0x0001
#define WM_DESTROY                      0x0002
#define WM_MOVE                         0x0003
#define WM_SIZE                         0x0005
#define WM_ACTIVATE                     0x0006
#define WM_SETFOCUS                     0x0007
#define WM_KILLFOCUS                    0x0008
#define WM_ENABLE                       0x000A
#define WM_SETREDRAW                    0x000B
#define WM_SETTEXT                      0x000C
#define WM_GETTEXT                      0x000D
#define WM_GETTEXTLENGTH                0x000E
#define WM_PAINT                        0x000F
#define WM_CLOSE                        0x0010
#define WM_QUIT                         0x0012
#define WM_ERASEBKGND                   0x0014
#define WM_SYSCOLORCHANGE               0x0015
#define WM_SHOWWINDOW                   0x0018
#define WM_WININICHANGE                 0x001A
#define WM_DEVMODECHANGE                0x001B
#define WM_ACTIVATEAPP                  0x001C
#define WM_FONTCHANGE                   0x001D
#define WM_TIMECHANGE                   0x001E
#define WM_CANCELMODE                   0x001F
#define WM_SETCURSOR                    0x0020
#define WM_MOUSEACTIVATE                0x0021
#define WM_CHILDACTIVATE                0x0022
#define WM_QUEUESYNC                    0x0023
#define WM_GETMINMAXINFO                0x0024
#define WM_PAINTICON                    0x0026
#define WM_ICONERASEBKGND               0x0027
#define WM_NEXTDLGCTL                   0x0028
#define WM_SPOOLERSTATUS                0x002A
#define WM_DRAWITEM                     0x002B
#define WM_MEASUREITEM                  0x002C
#define WM_DELETEITEM                   0x002D
#define WM_VKEYTOITEM                   0x002E
#define WM_CHARTOITEM                   0x002F
#define WM_SETFONT                      0x0030
#define WM_GETFONT                      0x0031
#define WM_SETHOTKEY                    0x0032
#define WM_GETHOTKEY                    0x0033
#define WM_QUERYDRAGICON                0x0037
#define WM_COMPAREITEM                  0x0039
#define WM_GETOBJECT                    0x003D
#define WM_COMPACTING                   0x0041
#define WM_COMMNOTIFY                   0x0044
#define WM_WINDOWPOSCHANGING            0x0046
#define WM_WINDOWPOSCHANGED             0x0047
#define WM_POWER                        0x0048
#define WM_COPYDATA                     0x004A
#define WM_CANCELJOURNAL                0x004B
#define WM_NOTIFY                       0x004E
#define WM_INPUTLANGCHANGEREQUEST       0x0050
#define WM_INPUTLANGCHANGE              0x0051
#define WM_TCARD                        0x0052
#define WM_HELP                         0x0053
#define WM_USERCHANGED                  0x0054
#define WM_NOTIFYFORMAT                 0x0055
#define WM_CONTEXTMENU                  0x007B
#define WM_STYLECHANGING                0x007C
#define WM_STYLECHANGED                 0x007D
#define WM_DISPLAYCHANGE                0x007E
#define WM_GETICON                      0x007F
#define WM_SETICON                      0x0080
#define WM_NCCREATE                     0x0081
#define WM_NCDESTROY                    0x0082
#define WM_NCCALCSIZE                   0x0083
#define WM_NCHITTEST                    0x0084
#define WM_NCPAINT                      0x0085
#define WM_NCACTIVATE                   0x0086
#define WM_GETDLGCODE                   0x0087
#define WM_SYNCPAINT                    0x0088
#define WM_NCMOUSEMOVE                  0x00A0
#define WM_NCLBUTTONDOWN                0x00A1
#define WM_NCLBUTTONUP                  0x00A2
#define WM_NCLBUTTONDBLCLK              0x00A3
#define WM_NCRBUTTONDOWN                0x00A4
#define WM_NCRBUTTONUP                  0x00A5
#define WM_NCRBUTTONDBLCLK              0x00A6
#define WM_NCMBUTTONDOWN                0x00A7
#define WM_NCMBUTTONUP                  0x00A8
#define WM_NCMBUTTONDBLCLK              0x00A9
#define WM_KEYDOWN                      0x0100
#define WM_KEYUP                        0x0101
#define WM_CHAR                         0x0102
#define WM_DEADCHAR                     0x0103
#define WM_SYSKEYDOWN                   0x0104
#define WM_SYSKEYUP                     0x0105
#define WM_SYSCHAR                      0x0106
#define WM_SYSDEADCHAR                  0x0107
#define WM_UNICHAR                      0x0109
#define WM_MOUSEMOVE                    0x0200
#define WM_LBUTTONDOWN                  0x0201
#define WM_LBUTTONUP                    0x0202
#define WM_LBUTTONDBLCLK                0x0203
#define WM_RBUTTONDOWN                  0x0204
#define WM_RBUTTONUP                    0x0205
#define WM_RBUTTONDBLCLK                0x0206
#define WM_MBUTTONDOWN                  0x0207
#define WM_MBUTTONUP                    0x0208
#define WM_MBUTTONDBLCLK                0x0209
#define WM_MOUSEWHEEL                   0x020A
#define WM_PARENTNOTIFY                 0x0210
#define WM_ENTERMENULOOP                0x0211
#define WM_EXITMENULOOP                 0x0212
#define WM_NEXTMENU                     0x0213
#define WM_SIZING                       0x0214
#define WM_CAPTURECHANGED               0x0215
#define WM_MOVING                       0x0216
#define WM_POWERBROADCAST               0x0218
#define WM_DEVICECHANGE                 0x0219
#define WM_MDICREATE                    0x0220
#define WM_MDIDESTROY                   0x0221
#define WM_MDIACTIVATE                  0x0222
#define WM_MDIRESTORE                   0x0223
#define WM_MDINEXT                      0x0224
#define WM_MDIMAXIMIZE                  0x0225
#define WM_MDITILE                      0x0226
#define WM_MDICASCADE                   0x0227
#define WM_MDIICONARRANGE               0x0228
#define WM_MDIGETACTIVE                 0x0229
#define WM_MDISETMENU                   0x0230
#define WM_ENTERSIZEMOVE                0x0231
#define WM_EXITSIZEMOVE                 0x0232
#define WM_DROPFILES                    0x0233
#define WM_MDIREFRESHMENU               0x0234
#define WM_IME_SETCONTEXT               0x0281
#define WM_IME_NOTIFY                   0x0282
#define WM_IME_CONTROL                  0x0283
#define WM_IME_COMPOSITIONFULL          0x0284
#define WM_IME_SELECT                   0x0285
#define WM_IME_CHAR                     0x0286
#define WM_IME_REQUEST                  0x0288
#define WM_IME_KEYDOWN                  0x0290
#define WM_IME_KEYUP                    0x0291
#define WM_MOUSEHOVER                   0x02A1
#define WM_MOUSELEAVE                   0x02A3
#define WM_CUT                          0x0300
#define WM_COPY                         0x0301
#define WM_PASTE                        0x0302
#define WM_CLEAR                        0x0303
#define WM_UNDO                         0x0304
#define WM_RENDERFORMAT                 0x0305
#define WM_RENDERALLFORMATS             0x0306
#define WM_DESTROYCLIPBOARD             0x0307
#define WM_DRAWCLIPBOARD                0x0308
#define WM_PAINTCLIPBOARD               0x0309
#define WM_VSCROLLCLIPBOARD             0x030A
#define WM_SIZECLIPBOARD                0x030B
#define WM_ASKCBFORMATNAME              0x030C
#define WM_CHANGECBCHAIN                0x030D
#define WM_HSCROLLCLIPBOARD             0x030E
#define WM_QUERYNEWPALETTE              0x030F
#define WM_PALETTEISCHANGING            0x0310
#define WM_PALETTECHANGED               0x0311
#define WM_HOTKEY                       0x0312
#define WM_PRINT                        0x0317
#define WM_PRINTCLIENT                  0x0318
#define WM_APPCOMMAND                   0x0319
#define WM_THEMECHANGED                 0x031A
#define WM_HANDHELDFIRST                0x0358
#define WM_HANDHELDLAST                 0x035F
#define WM_AFXFIRST                     0x0360
#define WM_AFXLAST                      0x037F
#define WM_PENWINFIRST                  0x0380
#define WM_PENWINLAST                   0x038F
#define WM_USER                         0x0400
#define WM_APP                          0x8000

/* =========================================================
   ShowWindow() Commands
   ========================================================= */

#define SW_HIDE             0
#define SW_SHOWNORMAL       1
#define SW_NORMAL           1
#define SW_SHOWMINIMIZED    2
#define SW_SHOWMAXIMIZED    3
#define SW_MAXIMIZE         3
#define SW_SHOWNOACTIVATE   4
#define SW_SHOW             5
#define SW_MINIMIZE         6
#define SW_SHOWMINNOACTIVE  7
#define SW_SHOWNA           8
#define SW_RESTORE          9
#define SW_SHOWDEFAULT      10
#define SW_FORCEMINIMIZE    11

/* =========================================================
   SetWindowPos() Flags
   ========================================================= */

#define SWP_NOSIZE          0x0001
#define SWP_NOMOVE          0x0002
#define SWP_NOZORDER        0x0004
#define SWP_NOREDRAW        0x0008
#define SWP_NOACTIVATE      0x0010
#define SWP_FRAMECHANGED    0x0020
#define SWP_SHOWWINDOW      0x0040
#define SWP_HIDEWINDOW      0x0080
#define SWP_NOCOPYBITS      0x0100
#define SWP_NOOWNERZORDER   0x0200
#define SWP_NOSENDCHANGING  0x0400
#define SWP_DRAWFRAME       SWP_FRAMECHANGED
#define SWP_NOREPOSITION    SWP_NOOWNERZORDER
#define SWP_DEFERERASE      0x2000
#define SWP_ASYNCWINDOWPOS  0x4000

/* =========================================================
   Cursor and Icon Resource Identifiers
   ========================================================= */

#define MAKEINTRESOURCEA(i) ((LPCSTR)((ULONG_PTR)((WORD)(i))))
#define MAKEINTRESOURCE    MAKEINTRESOURCEA

/* Standard cursor IDs */
#define IDC_ARROW           MAKEINTRESOURCE(32512)
#define IDC_IBEAM           MAKEINTRESOURCE(32513)
#define IDC_WAIT            MAKEINTRESOURCE(32514)
#define IDC_CROSS           MAKEINTRESOURCE(32515)
#define IDC_UPARROW         MAKEINTRESOURCE(32516)
#define IDC_SIZE            MAKEINTRESOURCE(32640)
#define IDC_ICON            MAKEINTRESOURCE(32641)
#define IDC_SIZENWSE        MAKEINTRESOURCE(32642)
#define IDC_SIZENESW        MAKEINTRESOURCE(32643)
#define IDC_SIZEWE          MAKEINTRESOURCE(32644)
#define IDC_SIZENS          MAKEINTRESOURCE(32645)
#define IDC_SIZEALL         MAKEINTRESOURCE(32646)
#define IDC_NO              MAKEINTRESOURCE(32648)
#define IDC_HAND            MAKEINTRESOURCE(32649)
#define IDC_APPSTARTING     MAKEINTRESOURCE(32650)
#define IDC_HELP            MAKEINTRESOURCE(32651)

/* Standard icon IDs */
#define IDI_APPLICATION     MAKEINTRESOURCE(32512)
#define IDI_HAND            MAKEINTRESOURCE(32513)
#define IDI_QUESTION        MAKEINTRESOURCE(32514)
#define IDI_EXCLAMATION     MAKEINTRESOURCE(32515)
#define IDI_ASTERISK        MAKEINTRESOURCE(32516)
#define IDI_WINLOGO         MAKEINTRESOURCE(32517)

/* =========================================================
   MessageBox() Flags and Return Codes
   ========================================================= */

#define MB_OK                       0x00000000L
#define MB_OKCANCEL                 0x00000001L
#define MB_ABORTRETRYIGNORE         0x00000002L
#define MB_YESNOCANCEL              0x00000003L
#define MB_YESNO                    0x00000004L
#define MB_RETRYCANCEL              0x00000005L
#define MB_CANCELTRYCONTINUE        0x00000006L
#define MB_ICONHAND                 0x00000010L
#define MB_ICONQUESTION             0x00000020L
#define MB_ICONEXCLAMATION          0x00000030L
#define MB_ICONASTERISK             0x00000040L
#define MB_USERICON                 0x00000080L
#define MB_ICONWARNING              MB_ICONEXCLAMATION
#define MB_ICONERROR                MB_ICONHAND
#define MB_ICONINFORMATION          MB_ICONASTERISK
#define MB_ICONSTOP                 MB_ICONHAND

#define MB_DEFBUTTON1               0x00000000L
#define MB_DEFBUTTON2               0x00000100L
#define MB_DEFBUTTON3               0x00000200L
#define MB_DEFBUTTON4               0x00000300L

#define MB_APPLMODAL                0x00000000L
#define MB_SYSTEMMODAL              0x00001000L
#define MB_TASKMODAL                0x00002000L
#define MB_HELP                     0x00004000L
#define MB_NOFOCUS                  0x00008000L
#define MB_SETFOREGROUND            0x00010000L
#define MB_DEFAULT_DESKTOP_ONLY     0x00020000L
#define MB_TOPMOST                  0x00040000L
#define MB_RIGHT                    0x00080000L
#define MB_RTLREADING               0x00100000L

#define IDOK                1
#define IDCANCEL            2
#define IDABORT             3
#define IDRETRY             4
#define IDIGNORE            5
#define IDYES               6
#define IDNO                7
#define IDCLOSE             8
#define IDHELP              9
#define IDTRYAGAIN          10
#define IDCONTINUE          11
#define IDTIMEOUT           32000

/* =========================================================
   Memory Helper Macros (for Win32 source compatibility)
   ========================================================= */

#define ZeroMemory(Destination, Length) memset((Destination), 0, (Length))
#define CopyMemory(Destination, Source, Length) memcpy((Destination), (Source), (Length))

/* =========================================================
   USER Subsystem Function Prototypes
   ========================================================= */

/*
 * Registers a window class for subsequent use in calls to CreateWindow or CreateWindowEx.
 *
 * Parameters:
 *  lpWndClass - Pointer to a WNDCLASS structure that contains the class information.
 *
 * Returns:
 *  An ATOM that uniquely identifies the class, or zero on failure.
 */
ATOM WINAPI RegisterClass(
        CONST WNDCLASS *lpWndClass
);

/*
 * Registers a window class for subsequent use in calls to CreateWindow or CreateWindowEx (extended version).
 *
 * Parameters:
 *  lpWndClassEx - Pointer to a WNDCLASSEX structure that contains the class information.
 *
 * Returns:
 *  An ATOM that uniquely identifies the class, or zero on failure.
 */
ATOM WINAPI RegisterClassEx(
        CONST WNDCLASSEX *lpWndClassEx
);

/*
 * Creates a new top-level or child window.
 *
 * Parameters:
 *  dwExStyle     - Extended window style flags (WS_EX_*).
 *  lpClassName   - Registered class name or atom.
 *  lpWindowName  - Window title text.
 *  dwStyle       - Window style flags (WS_*).
 *  X             - Initial horizontal position.
 *  Y             - Initial vertical position.
 *  nWidth        - Initial width of the window.
 *  nHeight       - Initial height of the window.
 *  hWndParent    - Handle to the parent or owner window.
 *  hMenu         - Handle to a menu or child-window identifier.
 *  hInstance     - Handle to the application instance.
 *  lpParam       - Pointer to optional creation data.
 *
 * Returns:
 *  Handle to the created window or NULL on failure.
 */
HWND WINAPI CreateWindowEx(
        DWORD dwExStyle,
        LPCSTR lpClassName,
        LPCSTR lpWindowName,
        DWORD dwStyle,
        int X,
        int Y,
        int nWidth,
        int nHeight,
        HWND hWndParent,
        HMENU hMenu,
        HINSTANCE hInstance,
        LPVOID lpParam
);

/* Convenience macro for CreateWindow (no extended style) */
#define CreateWindow(lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam) \
    CreateWindowEx(0L, lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam)

/*
 * Destroys the specified window and frees all associated resources.
 *
 * Parameters:
 *  hWnd - Handle to the window to be destroyed.
 *
 * Returns:
 *  Nonzero if the function succeeds, zero otherwise.
 */
BOOL WINAPI DestroyWindow(
        HWND hWnd
);

/*
 * Sets the specified window's show state.
 *
 * Parameters:
 *  hWnd     - Handle to the window.
 *  nCmdShow - Specifies how the window is to be shown (one of the SW_* constants).
 *
 * Returns:
 *  The previous show state of the window.
 */
BOOL WINAPI ShowWindow(
        HWND hWnd,
        int nCmdShow
);

/*
 * Updates the client area of the specified window by sending a WM_PAINT message.
 *
 * Parameters:
 *  hWnd - Handle to the window to be updated.
 *
 * Returns:
 *  Nonzero if the function succeeds, zero otherwise.
 */
BOOL WINAPI UpdateWindow(
        HWND hWnd
);

/*
 * Retrieves a message from the calling thread's message queue.
 *
 * Parameters:
 *  lpMsg          - Pointer to an MSG structure that receives message information.
 *  hWnd           - Handle to the window whose messages are to be retrieved (NULL for all).
 *  wMsgFilterMin  - Minimum message value to retrieve.
 *  wMsgFilterMax  - Maximum message value to retrieve.
 *
 * Returns:
 *  Nonzero if a message other than WM_QUIT was retrieved, zero if WM_QUIT, -1 on error.
 */
BOOL WINAPI GetMessage(
        LPMSG lpMsg,
        HWND hWnd,
        UINT wMsgFilterMin,
        UINT wMsgFilterMax
);

/*
 * Translates virtual-key messages into character messages.
 *
 * Parameters:
 *  lpMsg - Pointer to an MSG structure.
 *
 * Returns:
 *  Nonzero if the message was translated, zero otherwise.
 */
BOOL WINAPI TranslateMessage(
        CONST MSG *lpMsg
);

/*
 * Dispatches a message to a window procedure.
 *
 * Parameters:
 *  lpMsg - Pointer to an MSG structure.
 *
 * Returns:
 *  The value returned by the window procedure.
 */
LRESULT WINAPI DispatchMessage(
        CONST MSG *lpMsg
);

/*
 * Places a message in the message queue associated with the thread that created the specified window.
 *
 * Parameters:
 *  hWnd    - Handle to the window.
 *  Msg     - Message identifier.
 *  wParam  - Additional message information.
 *  lParam  - Additional message information.
 *
 * Returns:
 *  Nonzero if the function succeeds, zero otherwise.
 */
BOOL WINAPI PostMessage(
        HWND hWnd,
        UINT Msg,
        WPARAM wParam,
        LPARAM lParam
);

/*
 * Sends the specified message to a window or windows and waits for the message to be processed.
 *
 * Parameters:
 *  hWnd    - Handle to the window.
 *  Msg     - Message identifier.
 *  wParam  - Additional message information.
 *  lParam  - Additional message information.
 *
 * Returns:
 *  The result of the message processing.
 */
LRESULT WINAPI SendMessage(
        HWND hWnd,
        UINT Msg,
        WPARAM wParam,
        LPARAM lParam
);

/*
 * Posts a WM_QUIT message to the message queue of the calling thread and returns control.
 *
 * Parameters:
 *  nExitCode - Exit code to be used by the PostQuitMessage function.
 */
void WINAPI PostQuitMessage(
        int nExitCode
);

/*
 * Displays a modal dialog box that contains a system icon, a set of buttons, and a brief application-specific message.
 *
 * Parameters:
 *  hWnd      - Handle to the owner window.
 *  lpText    - Message text.
 *  lpCaption - Dialog box title.
 *  uType     - Style of the message box (MB_* constants).
 *
 * Returns:
 *  Identifier of the button clicked by the user (ID* constants).
 */
int WINAPI MessageBox(
        HWND hWnd,
        LPCSTR lpText,
        LPCSTR lpCaption,
        UINT uType
);

/*
 * Copies the text of the specified window's title bar (if it has one) into a buffer.
 *
 * Parameters:
 *  hWnd      - Handle to the window.
 *  lpString  - Buffer to receive the text.
 *  nMaxCount - Maximum number of characters to copy.
 *
 * Returns:
 *  Length of the copied string in characters, excluding the terminating null character.
 */
int WINAPI GetWindowText(
        HWND hWnd,
        LPSTR lpString,
        int nMaxCount
);

/*
 * Changes the text of the specified window's title bar.
 *
 * Parameters:
 *  hWnd     - Handle to the window.
 *  lpString - New title or text.
 *
 * Returns:
 *  Nonzero if the function succeeds, zero otherwise.
 */
BOOL WINAPI SetWindowText(
        HWND hWnd,
        LPCSTR lpString
);

/*
 * Changes the position and dimensions of the specified window.
 *
 * Parameters:
 *  hWnd      - Handle to the window.
 *  X         - New horizontal position.
 *  Y         - New vertical position.
 *  nWidth    - New width.
 *  nHeight   - New height.
 *  bRepaint  - TRUE to repaint the window.
 *
 * Returns:
 *  Nonzero if the function succeeds, zero otherwise.
 */
BOOL WINAPI MoveWindow(
        HWND hWnd,
        int X,
        int Y,
        int nWidth,
        int nHeight,
        BOOL bRepaint
);

/*
 * Changes the size, position, and Z order of a child, pop-up, or top-level window.
 *
 * Parameters:
 *  hWnd            - Handle to the window.
 *  hWndInsertAfter - Handle to the window to precede in Z order.
 *  X               - New horizontal position.
 *  Y               - New vertical position.
 *  cx              - New width.
 *  cy              - New height.
 *  uFlags          - Window positioning flags (SWP_* constants).
 *
 * Returns:
 *  Nonzero if the function succeeds, zero otherwise.
 */
BOOL WINAPI SetWindowPos(
        HWND hWnd,
        HWND hWndInsertAfter,
        int X,
        int Y,
        int cx,
        int cy,
        UINT uFlags
);

/*
 * Retrieves the coordinates of a window's client area.
 *
 * Parameters:
 *  hWnd   - Handle to the window.
 *  lpRect - Pointer to RECT structure to receive coordinates.
 *
 * Returns:
 *  Nonzero if the function succeeds, zero otherwise.
 */
BOOL WINAPI GetClientRect(
        HWND hWnd,
        LPRECT lpRect
);

/*
 * Retrieves the dimensions of the bounding rectangle of the specified window.
 *
 * Parameters:
 *  hWnd   - Handle to the window.
 *  lpRect - Pointer to RECT structure to receive coordinates.
 *
 * Returns:
 *  Nonzero if the function succeeds, zero otherwise.
 */
BOOL WINAPI GetWindowRect(
        HWND hWnd,
        LPRECT lpRect
);

/*
 * Loads the specified cursor resource from a module or the system.
 *
 * Parameters:
 *  hInstance    - Handle to the module (NULL for system cursor).
 *  lpCursorName - Name of the cursor resource or IDC_* identifier.
 *
 * Returns:
 *  Handle to the loaded cursor, or NULL on failure.
 */
HCURSOR WINAPI LoadCursor(
        HINSTANCE hInstance,
        LPCSTR lpCursorName
);

/*
 * Sets the cursor shape.
 *
 * Parameters:
 *  hCursor - Handle to the cursor.
 *
 * Returns:
 *  Handle to the previous cursor, or NULL if there was none.
 */
HCURSOR WINAPI SetCursor(
        HCURSOR hCursor
);

/*
 * Retrieves the position of the cursor in screen coordinates.
 *
 * Parameters:
 *  lpPoint - Pointer to POINT structure that receives the cursor position.
 *
 * Returns:
 *  Nonzero if the function succeeds, zero otherwise.
 */
BOOL WINAPI GetCursorPos(
        LPPOINT lpPoint
);

/*
 * Converts the screen coordinates of a specified point on the screen to client coordinates.
 *
 * Parameters:
 *  hWnd    - Handle to the window.
 *  lpPoint - Pointer to POINT structure containing screen coordinates.
 *
 * Returns:
 *  Nonzero if the function succeeds, zero otherwise.
 */
BOOL WINAPI ScreenToClient(
        HWND hWnd,
        LPPOINT lpPoint
);

/*
 * Converts the client coordinates of a specified point to screen coordinates.
 *
 * Parameters:
 *  hWnd    - Handle to the window.
 *  lpPoint - Pointer to POINT structure containing client coordinates.
 *
 * Returns:
 *  Nonzero if the function succeeds, zero otherwise.
 */
BOOL WINAPI ClientToScreen(
        HWND hWnd,
        LPPOINT lpPoint
);

/*
 * Prepares the specified window for painting and fills a PAINTSTRUCT structure with information about the painting.
 *
 * Parameters:
 *  hWnd      - Handle to the window.
 *  lpPaint   - Pointer to PAINTSTRUCT structure that receives painting information.
 *
 * Returns:
 *  Handle to a display device context for the specified window.
 */
HDC WINAPI BeginPaint(
        HWND hWnd,
        LPPAINTSTRUCT lpPaint
);

/*
 * Marks the end of painting in the specified window and releases the display device context.
 *
 * Parameters:
 *  hWnd      - Handle to the window.
 *  lpPaint   - Pointer to PAINTSTRUCT structure.
 *
 * Returns:
 *  Nonzero if the function succeeds, zero otherwise.
 */
BOOL WINAPI EndPaint(
        HWND hWnd,
        CONST PAINTSTRUCT *lpPaint
);

/*
 * Writes a character string at the specified location using the currently selected font.
 *
 * Parameters:
 *  hdc       - Handle to device context.
 *  x         - X-coordinate of starting position.
 *  y         - Y-coordinate of starting position.
 *  lpString  - Character string.
 *  c         - Length of string.
 *
 * Returns:
 *  Nonzero if the function succeeds, zero otherwise.
 */
BOOL WINAPI TextOutA(
        HDC hdc,
        int x,
        int y,
        LPCSTR lpString,
        int c
);

/*
 * Draws formatted text in the specified rectangle.
 *
 * Parameters:
 *  hdc       - Handle to device context.
 *  lpchText  - Text string.
 *  cchText   - Length of string.
 *  lprc      - Rectangle in which to draw the text.
 *  format    - Formatting options (DT_* constants - basic support assumed).
 *
 * Returns:
 *  Height of the text in logical units.
 */
int WINAPI DrawTextA(
        HDC hdc,
        LPCSTR lpchText,
        int cchText,
        LPRECT lprc,
        UINT format
);

/*
 * Creates a logical brush with the specified solid color.
 *
 * Parameters:
 *  crColor - Color of the brush (RGB macro).
 *
 * Returns:
 *  Handle to the brush, or NULL on failure.
 */
HBRUSH WINAPI CreateSolidBrush(
        COLORREF crColor
);

/*
 * Selects an object into the specified device context.
 *
 * Parameters:
 *  hdc      - Handle to device context.
 *  hgdiobj  - Handle to GDI object.
 *
 * Returns:
 *  Handle to the replaced object.
 */
HGDIOBJ WINAPI SelectObject(
        HDC hdc,
        HGDIOBJ hgdiobj
);

/*
 * Deletes a logical pen, brush, font, bitmap, region, or palette.
 *
 * Parameters:
 *  hObject - Handle to GDI object.
 *
 * Returns:
 *  Nonzero if the function succeeds, zero otherwise.
 */
BOOL WINAPI DeleteObject(
        HGDIOBJ hObject
);

/*
 * Draws a rectangle using the current pen and fills it using the current brush.
 *
 * Parameters:
 *  hdc   - Handle to device context.
 *  left  - X-coordinate of upper-left corner.
 *  top   - Y-coordinate of upper-left corner.
 *  right - X-coordinate of lower-right corner.
 *  bottom- Y-coordinate of lower-right corner.
 *
 * Returns:
 *  Nonzero if the function succeeds, zero otherwise.
 */
BOOL WINAPI Rectangle(
        HDC hdc,
        int left,
        int top,
        int right,
        int bottom
);

/*
 * Fills a rectangle by using the specified brush.
 *
 * Parameters:
 *  hdc   - Handle to device context.
 *  lprc  - Pointer to RECT structure.
 *  hbr   - Handle to brush.
 *
 * Returns:
 *  Nonzero if the function succeeds, zero otherwise.
 */
int WINAPI FillRect(
        HDC hdc,
        CONST RECT *lprc,
        HBRUSH hbr
);

/*
 * Retrieves a handle to a device context for the client area of the specified window.
 *
 * Parameters:
 *  hWnd - Handle to the window.
 *
 * Returns:
 *  Handle to the device context, or NULL on failure.
 */
HDC WINAPI GetDC(
        HWND hWnd
);

/*
 * Releases a device context obtained by GetDC.
 *
 * Parameters:
 *  hWnd - Handle to the window.
 *  hDC  - Handle to the device context.
 *
 * Returns:
 *  Nonzero if the function succeeds, zero otherwise.
 */
int WINAPI ReleaseDC(
        HWND hWnd,
        HDC hDC
);

/*
 * Calls the default window procedure to provide default processing for any window messages that an application does not process.
 *
 * Parameters:
 *  hWnd    - Handle to the window.
 *  Msg     - Message identifier.
 *  wParam  - Additional message information.
 *  lParam  - Additional message information.
 *
 * Returns:
 *  Result of the message processing.
 */
LRESULT WINAPI DefWindowProc(
        HWND hWnd,
        UINT Msg,
        WPARAM wParam,
        LPARAM lParam
);

#ifdef __cplusplus
}
#endif

#endif /* WINDOWS_H */