/*
 * win.h - minimal Win32 declarations used by Epingle.
 *
 * Epingle is built without the Windows SDK and without the C runtime,
 * so only the handful of APIs it really needs are declared here.
 * Reading this file is a quick way to audit everything the program can do:
 * there is no networking API anywhere (no ws2_32, wininet, winhttp...).
 */
#pragma once

typedef unsigned char      BYTE;
typedef unsigned short     WORD;
typedef unsigned long      DWORD;   /* 32-bit on Windows (LLP64) */
typedef int                BOOL;
typedef unsigned int       UINT;
typedef long               LONG;
typedef short              SHORT;
typedef long long          LONG_PTR;
typedef unsigned long long ULONG_PTR;
typedef unsigned long long UINT_PTR;
typedef unsigned long long SIZE_T;
typedef unsigned long long ULONGLONG;
typedef UINT_PTR           WPARAM;
typedef LONG_PTR           LPARAM;
typedef LONG_PTR           LRESULT;
typedef long               HRESULT;
typedef unsigned short     WCHAR;   /* = wchar_t on Windows */
typedef DWORD              COLORREF;
typedef WORD               ATOM;

typedef void *HANDLE;
typedef void *HWND;
typedef void *HINSTANCE;
typedef void *HMODULE;
typedef void *HICON;
typedef void *HCURSOR;
typedef void *HBRUSH;
typedef void *HMENU;
typedef void *HDC;
typedef void *HBITMAP;
typedef void *HGDIOBJ;
typedef void *HFONT;
typedef void *HKEY;
typedef void *HMONITOR;
typedef void *HWINEVENTHOOK;

#define WINAPI __stdcall
#define IMP    __declspec(dllimport)

typedef LRESULT (WINAPI *WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef void    (WINAPI *WINEVENTPROC)(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD);

typedef struct { LONG x, y; } POINT;
typedef struct { LONG cx, cy; } SIZE;
typedef struct { LONG left, top, right, bottom; } RECT;

typedef struct {
    UINT      cbSize;
    UINT      style;
    WNDPROC   lpfnWndProc;
    int       cbClsExtra;
    int       cbWndExtra;
    HINSTANCE hInstance;
    HICON     hIcon;
    HCURSOR   hCursor;
    HBRUSH    hbrBackground;
    const WCHAR *lpszMenuName;
    const WCHAR *lpszClassName;
    HICON     hIconSm;
} WNDCLASSEXW;

typedef struct {
    HWND   hwnd;
    UINT   message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD  time;
    POINT  pt;
    DWORD  lPrivate;
} MSG;

typedef struct {
    HDC  hdc;
    BOOL fErase;
    RECT rcPaint;
    BOOL fRestore;
    BOOL fIncUpdate;
    BYTE rgbReserved[32];
} PAINTSTRUCT;

typedef struct {
    BYTE BlendOp;
    BYTE BlendFlags;
    BYTE SourceConstantAlpha;
    BYTE AlphaFormat;
} BLENDFUNCTION;

typedef struct {
    DWORD biSize;
    LONG  biWidth;
    LONG  biHeight;
    WORD  biPlanes;
    WORD  biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG  biXPelsPerMeter;
    LONG  biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
} BITMAPINFOHEADER;

typedef struct {
    BITMAPINFOHEADER bmiHeader;
    DWORD            bmiColors[1];
} BITMAPINFO;

typedef struct {
    DWORD cbSize;
    RECT  rcMonitor;
    RECT  rcWork;
    DWORD dwFlags;
} MONITORINFO;

typedef struct {
    DWORD Data1;
    WORD  Data2;
    WORD  Data3;
    BYTE  Data4[8];
} GUID;

typedef struct {
    DWORD cbSize;
    HWND  hWnd;
    UINT  uID;
    UINT  uFlags;
    UINT  uCallbackMessage;
    HICON hIcon;
    WCHAR szTip[128];
    DWORD dwState;
    DWORD dwStateMask;
    WCHAR szInfo[256];
    union { UINT uTimeout; UINT uVersion; } u;
    WCHAR szInfoTitle[64];
    DWORD dwInfoFlags;
    GUID  guidItem;
    HICON hBalloonIcon;
} NOTIFYICONDATAW;

/* ---- kernel32 ---- */
IMP HMODULE   WINAPI GetModuleHandleW(const WCHAR *);
IMP HANDLE    WINAPI CreateMutexW(void *, BOOL, const WCHAR *);
IMP DWORD     WINAPI GetLastError(void);
IMP void      WINAPI ExitProcess(UINT);
IMP DWORD     WINAPI GetModuleFileNameW(HMODULE, WCHAR *, DWORD);
IMP WCHAR *   WINAPI GetCommandLineW(void);
IMP ULONGLONG WINAPI GetTickCount64(void);
IMP DWORD     WINAPI GetCurrentProcessId(void);

/* ---- user32 ---- */
IMP ATOM    WINAPI RegisterClassExW(const WNDCLASSEXW *);
IMP HWND    WINAPI CreateWindowExW(DWORD, const WCHAR *, const WCHAR *, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, void *);
IMP LRESULT WINAPI DefWindowProcW(HWND, UINT, WPARAM, LPARAM);
IMP BOOL    WINAPI GetMessageW(MSG *, HWND, UINT, UINT);
IMP BOOL    WINAPI TranslateMessage(const MSG *);
IMP LRESULT WINAPI DispatchMessageW(const MSG *);
IMP void    WINAPI PostQuitMessage(int);
IMP BOOL    WINAPI PostMessageW(HWND, UINT, WPARAM, LPARAM);
IMP BOOL    WINAPI DestroyWindow(HWND);
IMP BOOL    WINAPI ShowWindow(HWND, int);
IMP BOOL    WINAPI RegisterHotKey(HWND, int, UINT, UINT);
IMP BOOL    WINAPI UnregisterHotKey(HWND, int);
IMP HWND    WINAPI GetForegroundWindow(void);
IMP BOOL    WINAPI SetForegroundWindow(HWND);
IMP HWND    WINAPI GetAncestor(HWND, UINT);
IMP HWND    WINAPI GetWindow(HWND, UINT);
IMP BOOL    WINAPI SetWindowPos(HWND, HWND, int, int, int, int, UINT);
IMP LONG    WINAPI GetWindowLongW(HWND, int);
IMP DWORD   WINAPI GetWindowThreadProcessId(HWND, DWORD *);
IMP BOOL    WINAPI IsWindow(HWND);
IMP BOOL    WINAPI IsWindowVisible(HWND);
IMP BOOL    WINAPI IsIconic(HWND);
IMP BOOL    WINAPI IsZoomed(HWND);
IMP BOOL    WINAPI GetWindowRect(HWND, RECT *);
IMP BOOL    WINAPI GetClientRect(HWND, RECT *);
IMP int     WINAPI GetClassNameW(HWND, WCHAR *, int);
IMP UINT_PTR WINAPI SetTimer(HWND, UINT_PTR, UINT, void *);
IMP BOOL    WINAPI KillTimer(HWND, UINT_PTR);
IMP BOOL    WINAPI UpdateLayeredWindow(HWND, HDC, POINT *, SIZE *, HDC, POINT *, COLORREF, BLENDFUNCTION *, DWORD);
IMP HDC     WINAPI BeginPaint(HWND, PAINTSTRUCT *);
IMP BOOL    WINAPI EndPaint(HWND, const PAINTSTRUCT *);
IMP BOOL    WINAPI InvalidateRect(HWND, const RECT *, BOOL);
IMP int     WINAPI FillRect(HDC, const RECT *, HBRUSH);
IMP int     WINAPI DrawTextW(HDC, const WCHAR *, int, RECT *, UINT);
IMP HCURSOR WINAPI LoadCursorW(HINSTANCE, const WCHAR *);
IMP HANDLE  WINAPI LoadImageW(HINSTANCE, const WCHAR *, UINT, int, int, UINT);
IMP int     WINAPI GetSystemMetrics(int);
IMP HMENU   WINAPI CreatePopupMenu(void);
IMP BOOL    WINAPI AppendMenuW(HMENU, UINT, UINT_PTR, const WCHAR *);
IMP BOOL    WINAPI TrackPopupMenu(HMENU, UINT, int, int, int, HWND, const RECT *);
IMP BOOL    WINAPI DestroyMenu(HMENU);
IMP BOOL    WINAPI GetCursorPos(POINT *);
IMP SHORT   WINAPI GetKeyState(int);
IMP UINT    WINAPI MapVirtualKeyW(UINT, UINT);
IMP int     WINAPI GetKeyNameTextW(LONG, WCHAR *, int);
IMP int     WINAPI MessageBoxW(HWND, const WCHAR *, const WCHAR *, UINT);
IMP UINT    WINAPI RegisterWindowMessageW(const WCHAR *);
IMP HMONITOR WINAPI MonitorFromWindow(HWND, DWORD);
IMP HMONITOR WINAPI MonitorFromPoint(POINT, DWORD);
IMP BOOL    WINAPI GetMonitorInfoW(HMONITOR, MONITORINFO *);
IMP HWINEVENTHOOK WINAPI SetWinEventHook(DWORD, DWORD, HMODULE, WINEVENTPROC, DWORD, DWORD, DWORD);
IMP BOOL    WINAPI UnhookWinEvent(HWINEVENTHOOK);

/* ---- gdi32 ---- */
IMP HDC     WINAPI CreateCompatibleDC(HDC);
IMP BOOL    WINAPI DeleteDC(HDC);
IMP HBITMAP WINAPI CreateDIBSection(HDC, const BITMAPINFO *, UINT, void **, HANDLE, DWORD);
IMP HGDIOBJ WINAPI SelectObject(HDC, HGDIOBJ);
IMP BOOL    WINAPI DeleteObject(HGDIOBJ);
IMP HBRUSH  WINAPI CreateSolidBrush(COLORREF);
IMP HFONT   WINAPI CreateFontW(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, const WCHAR *);
IMP int     WINAPI SetBkMode(HDC, int);
IMP COLORREF WINAPI SetTextColor(HDC, COLORREF);

/* ---- shell32 ---- */
IMP BOOL    WINAPI Shell_NotifyIconW(DWORD, NOTIFYICONDATAW *);

/* ---- advapi32 (HKEY_CURRENT_USER only) ---- */
IMP LONG WINAPI RegOpenKeyExW(HKEY, const WCHAR *, DWORD, DWORD, HKEY *);
IMP LONG WINAPI RegCreateKeyExW(HKEY, const WCHAR *, DWORD, WCHAR *, DWORD, DWORD, void *, HKEY *, DWORD *);
IMP LONG WINAPI RegQueryValueExW(HKEY, const WCHAR *, DWORD *, DWORD *, BYTE *, DWORD *);
IMP LONG WINAPI RegSetValueExW(HKEY, const WCHAR *, DWORD, DWORD, const BYTE *, DWORD);
IMP LONG WINAPI RegDeleteValueW(HKEY, const WCHAR *);
IMP LONG WINAPI RegCloseKey(HKEY);

/* ---- dwmapi ---- */
IMP HRESULT WINAPI DwmGetWindowAttribute(HWND, DWORD, void *, DWORD);
IMP HRESULT WINAPI DwmSetWindowAttribute(HWND, DWORD, const void *, DWORD);

/* ---- shcore ---- */
IMP HRESULT WINAPI GetDpiForMonitor(HMONITOR, int, UINT *, UINT *);

/* ---- constants ---- */
#define MAKEINTRESOURCEW(i) ((const WCHAR *)(ULONG_PTR)(WORD)(i))
#define RGB(r, g, b) ((COLORREF)((BYTE)(r) | ((WORD)(BYTE)(g) << 8) | ((DWORD)(BYTE)(b) << 16)))
#define HKEY_CURRENT_USER ((HKEY)(LONG_PTR)(LONG)0x80000001)

#define WM_NULL          0x0000
#define WM_DESTROY       0x0002
#define WM_PAINT         0x000F
#define WM_CLOSE         0x0010
#define WM_ERASEBKGND    0x0014
#define WM_NCHITTEST     0x0084
#define WM_KEYDOWN       0x0100
#define WM_KEYUP         0x0101
#define WM_CHAR          0x0102
#define WM_SYSKEYDOWN    0x0104
#define WM_SYSKEYUP      0x0105
#define WM_SYSCHAR       0x0106
#define WM_COMMAND       0x0111
#define WM_TIMER         0x0113
#define WM_LBUTTONUP     0x0202
#define WM_RBUTTONUP     0x0205
#define WM_HOTKEY        0x0312
#define WM_APP           0x8000
#define HTTRANSPARENT    (-1)

#define MOD_ALT      0x0001
#define MOD_CONTROL  0x0002
#define MOD_SHIFT    0x0004
#define MOD_WIN      0x0008
#define MOD_NOREPEAT 0x4000

#define VK_BACK     0x08
#define VK_SHIFT    0x10
#define VK_CONTROL  0x11
#define VK_MENU     0x12
#define VK_PAUSE    0x13
#define VK_CAPITAL  0x14
#define VK_ESCAPE   0x1B
#define VK_DELETE   0x2E
#define VK_LWIN     0x5B
#define VK_RWIN     0x5C
#define VK_F1       0x70
#define VK_F24      0x87
#define VK_NUMLOCK  0x90
#define VK_SCROLL   0x91
#define VK_LSHIFT   0xA0
#define VK_RMENU    0xA5
#define VK_OEM_7    0xDE

#define MAPVK_VK_TO_VSC 0
#define MAPVK_VSC_TO_VK 1

#define GA_ROOT      2
#define GW_HWNDPREV  3
#define GWL_EXSTYLE  (-20)

#define HWND_TOPMOST   ((HWND)(LONG_PTR)-1)
#define HWND_NOTOPMOST ((HWND)(LONG_PTR)-2)

#define SWP_NOSIZE         0x0001
#define SWP_NOMOVE         0x0002
#define SWP_NOACTIVATE     0x0010
#define SWP_NOOWNERZORDER  0x0200
#define SWP_NOSENDCHANGING 0x0400

#define WS_POPUP          0x80000000L
#define WS_CAPTION        0x00C00000L
#define WS_SYSMENU        0x00080000L
#define WS_EX_TOPMOST     0x00000008L
#define WS_EX_TRANSPARENT 0x00000020L
#define WS_EX_TOOLWINDOW  0x00000080L
#define WS_EX_LAYERED     0x00080000L
#define WS_EX_NOACTIVATE  0x08000000L

#define SW_HIDE   0
#define SW_SHOW   5
#define SW_SHOWNA 8

#define ULW_ALPHA     0x02
#define AC_SRC_OVER   0x00
#define AC_SRC_ALPHA  0x01
#define BI_RGB        0
#define DIB_RGB_COLORS 0

#define MF_STRING    0x0000
#define MF_GRAYED    0x0001
#define MF_CHECKED   0x0008
#define MF_SEPARATOR 0x0800
#define TPM_RIGHTBUTTON 0x0002
#define TPM_NONOTIFY    0x0080
#define TPM_RETURNCMD   0x0100

#define NIM_ADD     0
#define NIM_MODIFY  1
#define NIM_DELETE  2
#define NIF_MESSAGE 0x01
#define NIF_ICON    0x02
#define NIF_TIP     0x04
#define NIF_INFO    0x10
#define NIIF_INFO    0x01
#define NIIF_WARNING 0x02

#define IMAGE_ICON   1
#define SM_CXSMICON  49
#define SM_CYSMICON  50
#define IDC_ARROW    MAKEINTRESOURCEW(32512)

#define MB_OK            0x00000000L
#define MB_ICONWARNING   0x00000030L
#define MB_ICONINFORMATION 0x00000040L
#define MB_SETFOREGROUND 0x00010000L
#define MB_TOPMOST       0x00040000L

#define DT_LEFT       0x0000
#define DT_CENTER     0x0001
#define DT_VCENTER    0x0004
#define DT_WORDBREAK  0x0010
#define DT_SINGLELINE 0x0020
#define DT_NOPREFIX   0x0800
#define BK_TRANSPARENT 1

#define FW_NORMAL   400
#define FW_SEMIBOLD 600
#define DEFAULT_CHARSET   1
#define CLEARTYPE_QUALITY 5

#define KEY_QUERY_VALUE 0x0001
#define KEY_SET_VALUE   0x0002
#define KEY_READ        0x20019
#define KEY_WRITE       0x20006
#define REG_SZ    1
#define REG_DWORD 4
#define ERROR_ALREADY_EXISTS 183

#define DWMWA_EXTENDED_FRAME_BOUNDS     9
#define DWMWA_CLOAKED                   14
#define DWMWA_USE_IMMERSIVE_DARK_MODE   20
#define MONITOR_DEFAULTTONEAREST 2
#define MDT_EFFECTIVE_DPI 0

#define EVENT_OBJECT_LOCATIONCHANGE 0x800B
#define WINEVENT_OUTOFCONTEXT   0x0000
#define WINEVENT_SKIPOWNPROCESS 0x0002
#define OBJID_WINDOW 0
