/*
 * Épingle - keep any window on top with a global keyboard shortcut.
 *
 *   - Default shortcut: Alt + ² (the key under Échap), configurable.
 *   - A pinned window is surrounded by an animated blue/green glow.
 *   - No network code, no admin rights, settings in HKCU only.
 *
 * Built with MSVC and the Windows SDK (see build.bat).
 */
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#define WINVER       0x0A00
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <shellscalingapi.h>
#include <string.h>
#include "render.h"

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

/* ------------------------------------------------------------------ */
#define APP_NAME      L"Épingle"
#define HOTKEY_ID     1
#define TIMER_ID      1
#define FRAME_MS      33          /* ~30 images/s, only while a window is pinned */
#define WM_TRAY       (WM_APP + 1)
#define MAX_PINS      16
#define FADE_IN_MS    160
#define FLASH_MS      700
#define FADE_OUT_MS   260
#define CYCLE_MS      2400        /* time for the colours to travel one wave */
#define GLOW_PX       5           /* glow thickness at 100 % scaling */

#define ID_CHANGE     101
#define ID_RELEASE    102
#define ID_STARTUP    103
#define ID_ABOUT      104
#define ID_QUIT       105

#define DEFAULT_SCAN  0x29        /* physical key under Esc: "²" on AZERTY */

typedef struct {
    HWND    hwnd;
    HBITMAP bmp;
    unsigned int *bits;
    int     bw, bh;
    int     shown;
} Strip;

typedef struct {
    int       used;
    HWND      target;
    int       leaving;
    ULONGLONG t0, tLeave, lastDraw;
    unsigned  alphaAtLeave;
    Strip     s[4];
} Pin;

static HINSTANCE     g_inst;
static HWND          g_main, g_cap;
static HICON         g_icon;
static UINT          g_msgTaskbar;
static HDC           g_memdc;
static HWINEVENTHOOK g_hook;
static int           g_timer;
static Pin           g_pins[MAX_PINS];
static NOTIFYICONDATAW g_nid;
static int           g_trayOk;

static UINT  g_mods, g_vk, g_scan;   /* current shortcut */
static int   g_hkOk;                 /* shortcut registered? */
static WCHAR g_hkText[96];

static const WCHAR REG_APP[] = L"Software\\Epingle";
static const WCHAR REG_RUN[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const WCHAR RUN_NAME[] = L"Epingle";

/* ------------------------------------------------------------------ */
/* tiny string helpers                                                 */
static int wlen(const WCHAR *s) { int n = 0; while (s[n]) n++; return n; }

static void wcopy(WCHAR *d, const WCHAR *s, int cap)
{
    int i = 0;
    while (s[i] && i < cap - 1) { d[i] = s[i]; i++; }
    d[i] = 0;
}

static void wcat(WCHAR *d, const WCHAR *s, int cap)
{
    int n = wlen(d);
    if (n < cap) wcopy(d + n, s, cap - n);
}

static void wcat_uint(WCHAR *d, unsigned v, int cap)
{
    WCHAR tmp[12]; int i = 11; tmp[i] = 0;
    do { tmp[--i] = (WCHAR)(L'0' + v % 10); v /= 10; } while (v && i > 0);
    wcat(d, tmp + i, cap);
}

static int wequal(const WCHAR *a, const WCHAR *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static int wcontains(const WCHAR *hay, const WCHAR *needle)
{
    int n = wlen(needle);
    for (; *hay; hay++) {
        int i = 0;
        while (i < n && hay[i] == needle[i]) i++;
        if (i == n) return 1;
    }
    return 0;
}

static int scale(int v, UINT dpi) { return (int)(((long long)v * dpi + 48) / 96); }

static UINT dpi_of_monitor(HMONITOR mon)
{
    UINT dx = 96, dy = 96;
    if (GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dx, &dy) != 0 || dx == 0) dx = 96;
    return dx;
}

/* ------------------------------------------------------------------ */
/* settings (HKEY_CURRENT_USER\Software\Epingle)                       */
static int reg_get_dword(HKEY k, const WCHAR *name, DWORD *out)
{
    DWORD type = 0, v = 0, cb = sizeof v;
    if (RegQueryValueExW(k, name, 0, &type, (BYTE *)&v, &cb) == 0 && type == REG_DWORD) {
        *out = v;
        return 1;
    }
    return 0;
}

static void default_shortcut(UINT *mods, UINT *vk, UINT *scan)
{
    *mods = MOD_ALT;
    *scan = DEFAULT_SCAN;
    *vk = MapVirtualKeyW(DEFAULT_SCAN, MAPVK_VSC_TO_VK);
    if (!*vk) *vk = VK_OEM_7;
}

static void load_settings(void)
{
    HKEY k;
    default_shortcut(&g_mods, &g_vk, &g_scan);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_APP, 0, KEY_READ, &k) == 0) {
        DWORD m, v, s;
        if (reg_get_dword(k, L"Modifiers", &m) && reg_get_dword(k, L"VirtualKey", &v) && v) {
            g_mods = m & (MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN);
            g_vk = v;
            g_scan = reg_get_dword(k, L"ScanCode", &s) ? s : MapVirtualKeyW(v, MAPVK_VK_TO_VSC);
        }
        RegCloseKey(k);
    }
}

static void save_settings(void)
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_APP, 0, 0, 0, KEY_WRITE, 0, &k, 0) == 0) {
        DWORD m = g_mods, v = g_vk, s = g_scan;
        RegSetValueExW(k, L"Modifiers", 0, REG_DWORD, (const BYTE *)&m, sizeof m);
        RegSetValueExW(k, L"VirtualKey", 0, REG_DWORD, (const BYTE *)&v, sizeof v);
        RegSetValueExW(k, L"ScanCode", 0, REG_DWORD, (const BYTE *)&s, sizeof s);
        RegCloseKey(k);
    }
}

static int startup_enabled(void)
{
    HKEY k; int on = 0;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_RUN, 0, KEY_QUERY_VALUE, &k) == 0) {
        DWORD type = 0, cb = 0;
        if (RegQueryValueExW(k, RUN_NAME, 0, &type, 0, &cb) == 0) on = 1;
        RegCloseKey(k);
    }
    return on;
}

static void startup_set(int on)
{
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REG_RUN, 0, KEY_SET_VALUE, &k) != 0) return;
    if (on) {
        WCHAR exe[520], cmd[560];
        DWORD n = GetModuleFileNameW(0, exe, 520);
        if (n > 0 && n < 520) {
            cmd[0] = 0;
            wcat(cmd, L"\"", 560);
            wcat(cmd, exe, 560);
            wcat(cmd, L"\" --startup", 560);
            RegSetValueExW(k, RUN_NAME, 0, REG_SZ, (const BYTE *)cmd, (DWORD)(wlen(cmd) + 1) * 2);
        }
    } else {
        RegDeleteValueW(k, RUN_NAME);
    }
    RegCloseKey(k);
}

/* ------------------------------------------------------------------ */
/* shortcut                                                            */
static void shortcut_text(UINT mods, UINT vk, UINT scan, WCHAR *out, int cap)
{
    WCHAR name[48];
    out[0] = 0;
    if (mods & MOD_CONTROL) wcat(out, L"Ctrl + ", cap);
    if (mods & MOD_ALT)     wcat(out, L"Alt + ", cap);
    if (mods & MOD_SHIFT)   wcat(out, L"Maj + ", cap);
    if (mods & MOD_WIN)     wcat(out, L"Win + ", cap);
    if (!vk) return;
    if (!scan) scan = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    name[0] = 0;
    LONG lp = (LONG)(((scan & 0xFF) << 16) | ((scan & 0x100) ? (1 << 24) : 0));
    if (!scan || GetKeyNameTextW(lp, name, 48) <= 0) {
        wcopy(name, L"touche ", 48);
        wcat_uint(name, vk, 48);
    }
    wcat(out, name, cap);
}

static int shortcut_register(UINT mods, UINT vk)
{
    return RegisterHotKey(g_main, HOTKEY_ID, mods | MOD_NOREPEAT, vk) != 0;
}

/* ------------------------------------------------------------------ */
/* tray icon                                                           */
static void tray_set_tip(void)
{
    WCHAR tip[128];
    wcopy(tip, APP_NAME L" — ", 128);
    if (g_hkOk) wcat(tip, g_hkText, 128);
    else        wcat(tip, L"raccourci indisponible", 128);
    wcopy(g_nid.szTip, tip, 128);
}

static void tray_add(void)
{
    memset(&g_nid, 0, sizeof g_nid);
    g_nid.cbSize = sizeof g_nid;
    g_nid.hWnd = g_main;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY;
    g_nid.hIcon = g_icon;
    tray_set_tip();
    g_trayOk = Shell_NotifyIconW(NIM_ADD, &g_nid) != 0;
}

static void tray_refresh_tip(void)
{
    tray_set_tip();
    if (g_trayOk) {
        g_nid.uFlags = NIF_TIP;
        Shell_NotifyIconW(NIM_MODIFY, &g_nid);
        g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    }
}

static void tray_notify(const WCHAR *title, const WCHAR *text, DWORD flags)
{
    NOTIFYICONDATAW n;
    if (!g_trayOk) return;
    memcpy(&n, &g_nid, sizeof n);
    n.uFlags = NIF_INFO;
    wcopy(n.szInfoTitle, title, 64);
    wcopy(n.szInfo, text, 256);
    n.dwInfoFlags = flags;
    Shell_NotifyIconW(NIM_MODIFY, &n);
}

/* ------------------------------------------------------------------ */
/* the glow: 4 click-through layered strips around each pinned window  */
static LRESULT WINAPI strip_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_NCHITTEST) return HTTRANSPARENT;
    return DefWindowProcW(h, m, w, l);
}

static void strip_hide(Strip *s)
{
    if (s->shown) { ShowWindow(s->hwnd, SW_HIDE); s->shown = 0; }
}

static int strip_ensure(Strip *s, int w, int h)
{
    BITMAPINFO bi;
    void *bits = 0;
    if (s->bmp && s->bw == w && s->bh == h) return 1;
    if (s->bmp) { DeleteObject(s->bmp); s->bmp = 0; s->bits = 0; }
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;        /* top-down */
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    s->bmp = CreateDIBSection(g_memdc, &bi, DIB_RGB_COLORS, &bits, 0, 0);
    if (!s->bmp || !bits) { s->bmp = 0; return 0; }
    s->bits = (unsigned int *)bits;
    s->bw = w; s->bh = h;
    return 1;
}

static void pin_hide(Pin *p) { for (int k = 0; k < 4; k++) strip_hide(&p->s[k]); }

static void anim_stop_if_idle(void);

static void pin_free(Pin *p)
{
    for (int k = 0; k < 4; k++) {
        if (p->s[k].hwnd) DestroyWindow(p->s[k].hwnd);
        if (p->s[k].bmp) DeleteObject(p->s[k].bmp);
    }
    memset(p, 0, sizeof *p);
    anim_stop_if_idle();
}

static Pin *pin_find(HWND target)
{
    for (int i = 0; i < MAX_PINS; i++)
        if (g_pins[i].used && g_pins[i].target == target) return &g_pins[i];
    return 0;
}

static Pin *pin_alloc(HWND target)
{
    for (int i = 0; i < MAX_PINS; i++) {
        Pin *p = &g_pins[i];
        if (p->used) continue;
        memset(p, 0, sizeof *p);
        for (int k = 0; k < 4; k++) {
            p->s[k].hwnd = CreateWindowExW(
                WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
                L"EpingleGlow", L"", WS_POPUP, 0, 0, 1, 1, 0, 0, g_inst, 0);
            if (!p->s[k].hwnd) { p->used = 1; pin_free(p); return 0; }
        }
        p->used = 1;
        p->target = target;
        return p;
    }
    return 0;
}

static int active_count(void)
{
    int n = 0;
    for (int i = 0; i < MAX_PINS; i++) if (g_pins[i].used && !g_pins[i].leaving) n++;
    return n;
}

static unsigned pin_alpha(Pin *p, ULONGLONG now)
{
    if (p->leaving) {
        ULONGLONG e = now - p->tLeave;
        if (e >= FADE_OUT_MS) return 0;
        return (unsigned)(p->alphaAtLeave * (FADE_OUT_MS - e) / FADE_OUT_MS);
    } else {
        ULONGLONG e = now - p->t0;
        if (e >= FADE_IN_MS) return 255;
        return (unsigned)(1 + 254 * e / FADE_IN_MS);
    }
}

static void pin_start_leaving(Pin *p, ULONGLONG now)
{
    p->alphaAtLeave = pin_alpha(p, now);
    p->leaving = 1;
    p->tLeave = now;
}

static void pin_update(Pin *p, ULONGLONG now)
{
    HWND t = p->target;
    if (!IsWindow(t)) { pin_free(p); return; }

    /* pinned state removed by someone else (the app itself, another tool...) */
    if (!p->leaving && !(GetWindowLongW(t, GWL_EXSTYLE) & WS_EX_TOPMOST))
        pin_start_leaving(p, now);

    unsigned alpha = pin_alpha(p, now);
    if (p->leaving && alpha == 0) { pin_free(p); return; }

    DWORD cloaked = 0;
    DwmGetWindowAttribute(t, DWMWA_CLOAKED, &cloaked, sizeof cloaked);
    if (!IsWindowVisible(t) || IsIconic(t) || cloaked) { pin_hide(p); return; }

    RECT r;
    if (DwmGetWindowAttribute(t, DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof r) != 0)
        GetWindowRect(t, &r);
    if (r.right - r.left < 4 || r.bottom - r.top < 4) { pin_hide(p); return; }

    HMONITOR mon = MonitorFromWindow(t, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    memset(&mi, 0, sizeof mi);
    mi.cbSize = sizeof mi;
    GetMonitorInfoW(mon, &mi);
    int T = scale(GLOW_PX, dpi_of_monitor(mon));
    if (T < 3) T = 3;

    /* maximized or fullscreen: the outside is off-screen, so draw inside */
    int inside = IsZoomed(t) ||
        (r.left <= mi.rcMonitor.left && r.top <= mi.rcMonitor.top &&
         r.right >= mi.rcMonitor.right && r.bottom >= mi.rcMonitor.bottom);

    RRect tg = { r.left, r.top, r.right, r.bottom };
    int W = r.right - r.left, H = r.bottom - r.top;
    int geo[4][4];   /* x, y, w, h  for top, right, bottom, left */
    if (!inside) {
        int g0[4] = { r.left - T, r.top - T, W + 2 * T, T };
        int g1[4] = { r.right, r.top, T, H };
        int g2[4] = { r.left - T, r.bottom, W + 2 * T, T };
        int g3[4] = { r.left - T, r.top, T, H };
        memcpy(geo[0], g0, sizeof g0); memcpy(geo[1], g1, sizeof g1);
        memcpy(geo[2], g2, sizeof g2); memcpy(geo[3], g3, sizeof g3);
    } else {
        int g0[4] = { r.left, r.top, W, T };
        int g1[4] = { r.right - T, r.top + T, T, H - 2 * T };
        int g2[4] = { r.left, r.bottom - T, W, T };
        int g3[4] = { r.left, r.top + T, T, H - 2 * T };
        memcpy(geo[0], g0, sizeof g0); memcpy(geo[1], g1, sizeof g1);
        memcpy(geo[2], g2, sizeof g2); memcpy(geo[3], g3, sizeof g3);
    }

    unsigned phase = (unsigned)((now % CYCLE_MS) * 256 / CYCLE_MS);
    unsigned whiten = 0;
    if (!p->leaving) {
        ULONGLONG e = now - p->t0;
        if (e < FLASH_MS) whiten = (unsigned)(210 * (FLASH_MS - e) / FLASH_MS);
    }

    BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)alpha, AC_SRC_ALPHA };
    HGDIOBJ old = 0;
    for (int k = 0; k < 4; k++) {
        Strip *s = &p->s[k];
        int x = geo[k][0], y = geo[k][1], w = geo[k][2], h = geo[k][3];
        if (w <= 0 || h <= 0 || !strip_ensure(s, w, h)) { strip_hide(s); continue; }
        rg_render(s->bits, x, y, w, h, tg, T, inside, phase, whiten);
        HGDIOBJ prev = SelectObject(g_memdc, s->bmp);
        if (!old) old = prev;
        POINT dst = { x, y }, src = { 0, 0 };
        SIZE sz = { w, h };
        UpdateLayeredWindow(s->hwnd, 0, &dst, &sz, g_memdc, &src, 0, &bf, ULW_ALPHA);
        if (!s->shown) { ShowWindow(s->hwnd, SW_SHOWNA); s->shown = 1; }
    }
    if (old) SelectObject(g_memdc, old);

    /* keep the glow just above its window in the z-order */
    HWND above = GetWindow(t, GW_HWNDPREV);
    int ok = 0;
    for (int k = 0; k < 4; k++) if (above == p->s[k].hwnd) ok = 1;
    if (!ok) {
        HWND after = above ? above : HWND_TOPMOST;
        for (int k = 0; k < 4; k++) {
            SetWindowPos(p->s[k].hwnd, after, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOSENDCHANGING);
            after = p->s[k].hwnd;
        }
    }
    p->lastDraw = now;
}

static void tick_all(void)
{
    ULONGLONG now = GetTickCount64();
    for (int i = 0; i < MAX_PINS; i++) if (g_pins[i].used) pin_update(&g_pins[i], now);
}

/* immediate redraw when a pinned window moves or resizes */
static void WINAPI on_win_event(HWINEVENTHOOK h, DWORD ev, HWND hwnd, LONG idObject,
                                LONG idChild, DWORD thread, DWORD time)
{
    (void)h; (void)ev; (void)thread; (void)time;
    if (idObject != OBJID_WINDOW || idChild != 0 || !hwnd) return;
    Pin *p = pin_find(hwnd);
    if (!p) return;
    ULONGLONG now = GetTickCount64();
    if (now - p->lastDraw >= 8) pin_update(p, now);
}

static void anim_start(void)
{
    if (!g_timer) g_timer = SetTimer(g_main, TIMER_ID, FRAME_MS, 0) != 0;
    if (!g_hook)
        g_hook = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, 0,
                                 on_win_event, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
}

static void anim_stop_if_idle(void)
{
    for (int i = 0; i < MAX_PINS; i++) if (g_pins[i].used) return;
    if (g_timer) { KillTimer(g_main, TIMER_ID); g_timer = 0; }
    if (g_hook)  { UnhookWinEvent(g_hook); g_hook = 0; }
}

/* ------------------------------------------------------------------ */
/* pin / unpin                                                         */
static int is_excluded(HWND w)
{
    static const WCHAR *const shell[] = {
        L"Progman", L"WorkerW", L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd",
        L"Windows.UI.Core.CoreWindow", L"NotifyIconOverflowWindow", L"TopLevelWindowForOverflowXamlIsland",
    };
    WCHAR cls[96];
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    if (pid == GetCurrentProcessId()) return 1;
    cls[0] = 0;
    GetClassNameW(w, cls, 96);
    for (unsigned i = 0; i < sizeof shell / sizeof shell[0]; i++)
        if (wequal(cls, shell[i])) return 1;
    return 0;
}

static void toggle_foreground(void)
{
    HWND fg = GetForegroundWindow();
    if (!fg) return;
    HWND w = GetAncestor(fg, GA_ROOT);
    if (!w) w = fg;
    if (is_excluded(w)) return;

    ULONGLONG now = GetTickCount64();
    Pin *p = pin_find(w);

    if (p && !p->leaving) {                       /* unpin */
        SetWindowPos(w, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        pin_start_leaving(p, now);
        pin_update(p, now);
        return;
    }

    SetWindowPos(w, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    if (!(GetWindowLongW(w, GWL_EXSTYLE) & WS_EX_TOPMOST)) {
        tray_notify(L"Impossible d'épingler cette fenêtre",
                    L"Elle appartient à un programme lancé en administrateur. "
                    L"Épingle tourne sans droits élevés et Windows l'empêche de la modifier.",
                    NIIF_WARNING);
        return;
    }
    if (!p) p = pin_alloc(w);
    if (!p) {
        SetWindowPos(w, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        tray_notify(APP_NAME, L"Trop de fenêtres épinglées (16 au maximum).", NIIF_WARNING);
        return;
    }
    p->leaving = 0;
    p->t0 = now;
    anim_start();
    pin_update(p, now);
}

static void release_all(int immediate)
{
    ULONGLONG now = GetTickCount64();
    for (int i = 0; i < MAX_PINS; i++) {
        Pin *p = &g_pins[i];
        if (!p->used) continue;
        if (!p->leaving && IsWindow(p->target))
            SetWindowPos(p->target, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        if (immediate) pin_free(p);
        else if (!p->leaving) pin_start_leaving(p, now);
    }
}

/* ------------------------------------------------------------------ */
/* "change shortcut" window                                            */
static HFONT g_fTitle, g_fBig, g_fSmall;
static UINT  g_capDpi;
static WCHAR g_capBig[96];
static WCHAR g_capStatus[200];
static int   g_capErr;

static HFONT make_font(int pt, int weight, UINT dpi)
{
    return CreateFontW(-(int)(((long long)pt * dpi + 36) / 72), 0, 0, 0, weight, 0, 0, 0,
                       DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
}

static void cap_close(void)
{
    if (!g_hkOk) g_hkOk = shortcut_register(g_mods, g_vk);   /* restore if cancelled */
    tray_refresh_tip();
    if (g_cap) DestroyWindow(g_cap);
}

static void cap_open(void)
{
    if (g_cap) { SetForegroundWindow(g_cap); return; }

    POINT pt; GetCursorPos(&pt);
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi; memset(&mi, 0, sizeof mi); mi.cbSize = sizeof mi;
    GetMonitorInfoW(mon, &mi);
    g_capDpi = dpi_of_monitor(mon);
    int w = scale(500, g_capDpi), h = scale(260, g_capDpi);
    int x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - w) / 2;
    int y = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - h) / 2;

    g_fTitle = make_font(12, FW_SEMIBOLD, g_capDpi);
    g_fBig   = make_font(22, FW_SEMIBOLD, g_capDpi);
    g_fSmall = make_font(9,  FW_NORMAL,   g_capDpi);
    wcopy(g_capBig, g_hkOk ? g_hkText : L"…", 96);
    g_capStatus[0] = 0;
    g_capErr = 0;

    /* free the current shortcut so it can be pressed (or replaced) here */
    if (g_hkOk) { UnregisterHotKey(g_main, HOTKEY_ID); g_hkOk = 0; }

    g_cap = CreateWindowExW(WS_EX_TOPMOST, L"EpingleCapture", APP_NAME L" — raccourci",
                            WS_POPUP | WS_CAPTION | WS_SYSMENU, x, y, w, h, 0, 0, g_inst, 0);
    if (!g_cap) { cap_close(); return; }
    BOOL dark = 1;
    DwmSetWindowAttribute(g_cap, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof dark);
    ShowWindow(g_cap, SW_SHOW);
    SetForegroundWindow(g_cap);
}

static UINT held_mods(void)
{
    UINT m = 0;
    if (GetKeyState(VK_CONTROL) < 0) m |= MOD_CONTROL;
    if (GetKeyState(VK_MENU) < 0)    m |= MOD_ALT;
    if (GetKeyState(VK_SHIFT) < 0)   m |= MOD_SHIFT;
    if (GetKeyState(VK_LWIN) < 0 || GetKeyState(VK_RWIN) < 0) m |= MOD_WIN;
    return m;
}

static int is_modifier_key(UINT vk)
{
    return vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU || vk == VK_LWIN || vk == VK_RWIN ||
           (vk >= VK_LSHIFT && vk <= VK_RMENU) || vk == VK_CAPITAL || vk == VK_NUMLOCK;
}

static void cap_key(UINT vk, LPARAM lp)
{
    if (is_modifier_key(vk)) {
        UINT m = held_mods();
        if (m) { shortcut_text(m, 0, 0, g_capBig, 96); wcat(g_capBig, L"…", 96); }
        else   wcopy(g_capBig, g_hkText, 96);
        InvalidateRect(g_cap, 0, 0);
        return;
    }

    UINT m = held_mods();
    UINT scan = (UINT)((lp >> 16) & 0xFF) | (((lp >> 24) & 1) ? 0x100u : 0u);

    if (!m && vk == VK_ESCAPE) { cap_close(); return; }
    if (m == MOD_ALT && vk == VK_F1 + 3) { cap_close(); return; }   /* Alt + F4 closes, never captured */
    if (!m && (vk == VK_DELETE || vk == VK_BACK)) {
        default_shortcut(&m, &vk, &scan);
    } else if (!m && !(vk >= VK_F1 && vk <= VK_F24) && vk != VK_PAUSE && vk != VK_SCROLL) {
        shortcut_text(0, vk, scan, g_capBig, 96);
        wcopy(g_capStatus, L"Ajoutez au moins Ctrl, Alt, Maj ou Win (seules les touches F1 à F24 "
                           L"peuvent être utilisées seules).", 200);
        g_capErr = 1;
        InvalidateRect(g_cap, 0, 0);
        return;
    }

    if (shortcut_register(m, vk)) {
        g_mods = m; g_vk = vk; g_scan = scan; g_hkOk = 1;
        save_settings();
        shortcut_text(g_mods, g_vk, g_scan, g_hkText, 96);
        cap_close();
        WCHAR msg[200];
        wcopy(msg, g_hkText, 200);
        wcat(msg, L" épingle désormais la fenêtre active.", 200);
        tray_notify(L"Raccourci enregistré", msg, NIIF_INFO);
    } else {
        shortcut_text(m, vk, scan, g_capBig, 96);
        wcopy(g_capStatus, L"Ce raccourci est déjà pris par Windows ou une autre application. "
                           L"Essayez-en un autre.", 200);
        g_capErr = 1;
        InvalidateRect(g_cap, 0, 0);
    }
}

static void cap_paint(HWND h)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(h, &ps);
    RECT rc; GetClientRect(h, &rc);
    UINT d = g_capDpi;

    HBRUSH bg = CreateSolidBrush(RGB(0x1F, 0x21, 0x27));
    FillRect(dc, &rc, bg);
    DeleteObject(bg);

    /* blue -> green accent bar, same colours as the glow */
    int barH = scale(4, d), n = 48;
    for (int i = 0; i < n; i++) {
        RECT b = { rc.left + (rc.right - rc.left) * i / n, rc.bottom - barH,
                   rc.left + (rc.right - rc.left) * (i + 1) / n, rc.bottom };
        unsigned c = rg_lut[(unsigned)(i * 128 / n) & 255];
        HBRUSH br = CreateSolidBrush(RGB((c >> 16) & 255, (c >> 8) & 255, c & 255));
        FillRect(dc, &b, br);
        DeleteObject(br);
    }

    SetBkMode(dc, TRANSPARENT);
    int L = scale(28, d), R = rc.right - scale(28, d);

    RECT r1 = { L, scale(18, d), R, scale(48, d) };
    SelectObject(dc, g_fTitle);
    SetTextColor(dc, RGB(0xEB, 0xEE, 0xF5));
    DrawTextW(dc, L"Appuyez sur la nouvelle combinaison de touches", -1, &r1,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    RECT r2 = { L, scale(56, d), R, scale(112, d) };
    SelectObject(dc, g_fBig);
    SetTextColor(dc, RGB(0x4C, 0xC8, 0xFF));
    DrawTextW(dc, g_capBig, -1, &r2, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    RECT r3 = { L, scale(120, d), R, scale(162, d) };
    SelectObject(dc, g_fSmall);
    SetTextColor(dc, g_capErr ? RGB(0xFF, 0xAE, 0x57) : RGB(0x9A, 0xA0, 0xAE));
    DrawTextW(dc, g_capErr ? g_capStatus : L"Exemples : Alt + ²,  Ctrl + Maj + Espace,  Win + Alt + T",
              -1, &r3, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);

    RECT r4 = { L, scale(166, d), R, scale(192, d) };
    SetTextColor(dc, RGB(0x7A, 0x80, 0x8E));
    DrawTextW(dc, L"Échap : annuler     ·     Suppr : revenir à Alt + ²", -1, &r4,
              DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    EndPaint(h, &ps);
}

static LRESULT WINAPI cap_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        cap_key((UINT)w, l);
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (is_modifier_key((UINT)w)) cap_key((UINT)w, l);
        return 0;                       /* also stops Alt from opening the window menu */
    case WM_CHAR:
    case WM_SYSCHAR:
        return 0;                       /* no beep */
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        cap_paint(h);
        return 0;
    case WM_CLOSE:
        cap_close();
        return 0;
    case WM_DESTROY:
        if (g_fTitle) DeleteObject(g_fTitle);
        if (g_fBig)   DeleteObject(g_fBig);
        if (g_fSmall) DeleteObject(g_fSmall);
        g_fTitle = g_fBig = g_fSmall = 0;
        g_cap = 0;
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

/* ------------------------------------------------------------------ */
/* main window (hidden): hotkey, timer, tray menu                      */
static void show_about(void)
{
    MessageBoxW(0,
        L"Épingle 1.0\n\n"
        L"Garde une fenêtre au premier plan avec un raccourci clavier.\n\n"
        L"•  Aucun accès réseau : le programme n'utilise aucune bibliothèque réseau.\n"
        L"•  Aucune donnée collectée, aucune télémétrie.\n"
        L"•  Fonctionne avec les droits d'un utilisateur standard.\n"
        L"•  Réglages enregistrés dans HKEY_CURRENT_USER\\Software\\Epingle.",
        APP_NAME, MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND | MB_TOPMOST);
}

static void show_menu(void)
{
    HMENU m = CreatePopupMenu();
    WCHAR line[160];
    if (!m) return;

    wcopy(line, L"Épingler la fenêtre active :  ", 160);
    wcat(line, g_hkOk ? g_hkText : L"(raccourci indisponible)", 160);
    AppendMenuW(m, MF_STRING | MF_GRAYED, 0, line);
    AppendMenuW(m, MF_SEPARATOR, 0, 0);
    AppendMenuW(m, MF_STRING, ID_CHANGE, L"Modifier le raccourci…");

    int n = active_count();
    wcopy(line, L"Libérer toutes les fenêtres", 160);
    if (n) { wcat(line, L" (", 160); wcat_uint(line, (unsigned)n, 160); wcat(line, L")", 160); }
    AppendMenuW(m, MF_STRING | (n ? 0 : MF_GRAYED), ID_RELEASE, line);

    AppendMenuW(m, MF_STRING | (startup_enabled() ? MF_CHECKED : 0), ID_STARTUP,
                L"Lancer au démarrage de Windows");
    AppendMenuW(m, MF_SEPARATOR, 0, 0);
    AppendMenuW(m, MF_STRING, ID_ABOUT, L"À propos");
    AppendMenuW(m, MF_STRING, ID_QUIT, L"Quitter");

    POINT pt; GetCursorPos(&pt);
    SetForegroundWindow(g_main);
    int cmd = TrackPopupMenu(m, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, g_main, 0);
    PostMessageW(g_main, WM_NULL, 0, 0);
    DestroyMenu(m);

    switch (cmd) {
    case ID_CHANGE:  cap_open(); break;
    case ID_RELEASE: release_all(0); break;
    case ID_STARTUP: startup_set(!startup_enabled()); break;
    case ID_ABOUT:   show_about(); break;
    case ID_QUIT:
        if (g_cap) DestroyWindow(g_cap);
        DestroyWindow(g_main);
        break;
    }
}

static LRESULT WINAPI main_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_HOTKEY:
        if (w == HOTKEY_ID) toggle_foreground();
        return 0;
    case WM_TIMER:
        if (w == TIMER_ID) tick_all();
        return 0;
    case WM_TRAY:
        if ((UINT)l == WM_RBUTTONUP || (UINT)l == WM_LBUTTONUP) show_menu();
        return 0;
    case WM_DESTROY:
        release_all(1);
        if (g_trayOk) Shell_NotifyIconW(NIM_DELETE, &g_nid);
        if (g_hkOk) UnregisterHotKey(h, HOTKEY_ID);
        PostQuitMessage(0);
        return 0;
    }
    if (g_msgTaskbar && m == g_msgTaskbar) {   /* Explorer restarted */
        tray_add();
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static void register_class(const WCHAR *name, WNDPROC proc)
{
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = proc;
    wc.hInstance = g_inst;
    wc.hCursor = LoadCursorW(0, IDC_ARROW);
    wc.hIcon = g_icon;
    wc.hIconSm = g_icon;
    wc.lpszClassName = name;
    RegisterClassExW(&wc);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmdLine, int nShow)
{
    MSG msg;
    (void)hPrev; (void)nShow;
    g_inst = hInst;

    CreateMutexW(0, 0, L"Local\\Epingle.SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(0, L"Épingle est déjà lancé.\n\nSon icône se trouve dans la zone de notification, près de l'horloge.",
                    APP_NAME, MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
        return 0;
    }

    rg_build_lut();
    g_icon = (HICON)LoadImageW(g_inst, MAKEINTRESOURCEW(1), IMAGE_ICON,
                               GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    register_class(L"EpingleMain", main_proc);
    register_class(L"EpingleGlow", strip_proc);
    register_class(L"EpingleCapture", cap_proc);

    g_main = CreateWindowExW(WS_EX_TOOLWINDOW, L"EpingleMain", APP_NAME, WS_POPUP,
                             0, 0, 0, 0, 0, 0, g_inst, 0);
    if (!g_main) return 1;
    g_memdc = CreateCompatibleDC(0);
    g_msgTaskbar = RegisterWindowMessageW(L"TaskbarCreated");

    load_settings();
    shortcut_text(g_mods, g_vk, g_scan, g_hkText, 96);
    g_hkOk = shortcut_register(g_mods, g_vk);
    tray_add();

    int silent = cmdLine && wcontains(cmdLine, L"--startup");
    WCHAR msgText[256];
    if (!g_hkOk) {
        wcopy(msgText, g_hkText, 256);
        wcat(msgText, L" est déjà utilisé par une autre application. "
                      L"Cliquez sur l'icône d'Épingle puis « Modifier le raccourci… ».", 256);
        tray_notify(L"Raccourci indisponible", msgText, NIIF_WARNING);
    } else if (!silent) {
        wcopy(msgText, g_hkText, 256);
        wcat(msgText, L" épingle la fenêtre active au premier plan. Refaites-le pour la libérer.", 256);
        tray_notify(L"Épingle est actif", msgText, NIIF_INFO);
    }

    while (GetMessageW(&msg, 0, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (g_memdc) DeleteDC(g_memdc);
    return (int)msg.wParam;
}
