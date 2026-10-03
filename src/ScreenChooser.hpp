/*   NAUTITECH - Simulateur de Navigation
     The desk's screens, which of the simulator's windows are already open on each, and a small window
     asking which screen to open on (for a program started directly on a desk with several screens).
     Windows only: elsewhere no screens are listed and nothing is asked.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#ifndef __SCREEN_CHOOSER_HPP_INCLUDED__
#define __SCREEN_CHOOSER_HPP_INCLUDED__

#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <cstring>
#include <cwchar>
#endif

namespace ScreenChooser {

//Window classes of the programs' borderless windows, so that each program can see which screens are taken.
const char* const SimulatorWindowClass = "NautitechSimulatorWindow";
const char* const RepeaterWindowClass = "NautitechRepeaterWindow";

struct Screen {
    int left, top, right, bottom;   //desktop coordinates
    bool primary;
    std::wstring inUse;             //the simulator's windows already open on it, e.g. "Simulateur, Instruments"
};

const int Cancelled = -2;

#ifdef _WIN32

namespace detail {

    typedef std::vector<std::pair<HMONITOR, Screen> > ScreenList;

    inline BOOL CALLBACK addMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM data)
    {
        ScreenList* list = reinterpret_cast<ScreenList*>(data);
        Screen s;
        s.left = s.top = s.right = s.bottom = 0;
        s.primary = false;
        MONITORINFO mi;
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfo(monitor, &mi)) {
            s.left = mi.rcMonitor.left;
            s.top = mi.rcMonitor.top;
            s.right = mi.rcMonitor.right;
            s.bottom = mi.rcMonitor.bottom;
            s.primary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
        }
        list->push_back(std::make_pair(monitor, s));
        return TRUE;
    }

    struct WindowScan {
        ScreenList* screens;
        bool french;
    };

    inline BOOL CALLBACK checkWindow(HWND hwnd, LPARAM data)
    {
        WindowScan* scan = reinterpret_cast<WindowScan*>(data);
        if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) { return TRUE; }
        DWORD process = 0;
        GetWindowThreadProcessId(hwnd, &process);
        if (process == GetCurrentProcessId()) { return TRUE; }
        char windowClass[64] = "";
        GetClassNameA(hwnd, windowClass, sizeof(windowClass));
        wchar_t title[128] = L"";
        GetWindowTextW(hwnd, title, 128);
        const wchar_t* simulator = scan->french ? L"Simulateur" : L"Simulator";
        const wchar_t* repeater = scan->french ? L"R\u00E9p\u00E9titeur radar" : L"Radar repeater";
        std::wstring what;
        if (strcmp(windowClass, SimulatorWindowClass) == 0) { what = simulator; }
        else if (strcmp(windowClass, RepeaterWindowClass) == 0) { what = repeater; }
        else if (strcmp(windowClass, "NautitechConsoleWindow") == 0) { what = L"Instruments"; }
        else if (strcmp(windowClass, "CIrrDeviceWin32") == 0) { //a window of their own (not borderless)
            if (wcscmp(title, L"NAUTITECH") == 0) { what = simulator; }
            else if (wcsncmp(title, L"NAUTITECH - R", 13) == 0) { what = repeater; }
        }
        if (what.empty()) { return TRUE; }
        const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        for (size_t i = 0; i < scan->screens->size(); i++) {
            Screen& s = (*scan->screens)[i].second;
            if ((*scan->screens)[i].first != monitor || s.inUse.find(what) != std::wstring::npos) { continue; }
            if (!s.inUse.empty()) { s.inUse += L", "; }
            s.inUse += what;
        }
        return TRUE;
    }

    //The chooser window
    struct Chooser {
        std::vector<Screen> screens;
        std::vector<RECT> tiles;
        RECT openButton, cancelButton;
        std::wstring title, subtitle;
        bool french;
        int selected, hover, result;
        bool done, tracking;
        HFONT titleFont, textFont, smallFont, numberFont;
        int textH, smallH, numberH;
    };

    const COLORREF background = RGB(14, 28, 50);
    const COLORREF edge = RGB(64, 110, 170);
    const COLORREF text = RGB(244, 247, 251);
    const COLORREF textDim = RGB(178, 192, 208);
    const COLORREF textFaint = RGB(130, 146, 166);
    const COLORREF inUseText = RGB(255, 196, 92);
    const int Width = 760, Height = 480;
    const int HoverOpen = 100, HoverCancel = 101;

    inline void roundBox(HDC dc, const RECT& r, int radius, COLORREF fill, COLORREF line)
    {
        HBRUSH brush = CreateSolidBrush(fill);
        HPEN pen = CreatePen(PS_SOLID, 1, line);
        HGDIOBJ oldBrush = SelectObject(dc, brush);
        HGDIOBJ oldPen = SelectObject(dc, pen);
        RoundRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(brush);
        DeleteObject(pen);
    }

    inline void drawText(HDC dc, HFONT font, const std::wstring& s, RECT r, COLORREF colour, UINT flags)
    {
        SelectObject(dc, font);
        SetTextColor(dc, colour);
        DrawTextW(dc, s.c_str(), -1, &r, flags | DT_NOPREFIX | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    inline int fontHeight(HDC dc, HFONT font)
    {
        TEXTMETRICW tm;
        SelectObject(dc, font);
        GetTextMetricsW(dc, &tm);
        return tm.tmHeight;
    }

    inline HFONT makeFont(int height, int weight)
    {
        return CreateFontW(-height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    }

    inline bool inside(const RECT& r, int x, int y)
    {
        return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
    }

    //The desk to scale, the buttons below it.
    inline void layout(Chooser& c)
    {
        c.openButton.right = Width - 36;
        c.openButton.left = c.openButton.right - 250;
        c.openButton.bottom = Height - 30;
        c.openButton.top = c.openButton.bottom - 44;
        c.cancelButton = c.openButton;
        c.cancelButton.right = c.openButton.left - 12;
        c.cancelButton.left = c.cancelButton.right - 130;

        const int areaL = 36, areaT = 112, areaR = Width - 36, areaB = Height - 100;
        c.tiles.clear();
        if (c.screens.empty()) { return; }
        int deskL = c.screens[0].left, deskT = c.screens[0].top, deskR = c.screens[0].right, deskB = c.screens[0].bottom;
        for (size_t i = 1; i < c.screens.size(); i++) {
            if (c.screens[i].left < deskL) { deskL = c.screens[i].left; }
            if (c.screens[i].top < deskT) { deskT = c.screens[i].top; }
            if (c.screens[i].right > deskR) { deskR = c.screens[i].right; }
            if (c.screens[i].bottom > deskB) { deskB = c.screens[i].bottom; }
        }
        const double dw = (deskR - deskL) > 0 ? (double)(deskR - deskL) : 1.0;
        const double dh = (deskB - deskT) > 0 ? (double)(deskB - deskT) : 1.0;
        double scale = (areaR - areaL) / dw;
        if ((areaB - areaT) / dh < scale) { scale = (areaB - areaT) / dh; }
        scale *= 0.96;
        const double ox = (areaL + areaR) * 0.5 - dw * scale * 0.5, oy = (areaT + areaB) * 0.5 - dh * scale * 0.5;
        for (size_t i = 0; i < c.screens.size(); i++) {
            RECT r;
            r.left = (LONG)(ox + (c.screens[i].left - deskL) * scale) + 5;
            r.top = (LONG)(oy + (c.screens[i].top - deskT) * scale) + 5;
            r.right = (LONG)(ox + (c.screens[i].right - deskL) * scale) - 5;
            r.bottom = (LONG)(oy + (c.screens[i].bottom - deskT) * scale) - 5;
            c.tiles.push_back(r);
        }
    }

    inline void paint(HDC dc, Chooser& c)
    {
        SetBkMode(dc, TRANSPARENT);
        if (!c.numberH) {
            c.numberH = fontHeight(dc, c.numberFont);
            c.textH = fontHeight(dc, c.textFont);
            c.smallH = fontHeight(dc, c.smallFont);
        }
        const RECT all = { 0, 0, Width, Height };
        roundBox(dc, all, 18, background, edge);

        RECT r = { 36, 26, Width - 36, 60 };
        drawText(dc, c.titleFont, c.title, r, text, DT_LEFT | DT_VCENTER);
        r.top = 62; r.bottom = 86;
        drawText(dc, c.textFont, c.subtitle, r, textDim, DT_LEFT | DT_VCENTER);

        for (size_t i = 0; i < c.tiles.size(); i++) {
            const RECT& t = c.tiles[i];
            const bool sel = ((int)i == c.selected), hov = ((int)i == c.hover);
            if (sel) {
                RECT glow = { t.left - 4, t.top - 4, t.right + 4, t.bottom + 4 };
                roundBox(dc, glow, 18, background, RGB(70, 130, 210));
                roundBox(dc, t, 14, RGB(32, 112, 212), RGB(200, 230, 255));
            }
            else {
                roundBox(dc, t, 14, hov ? RGB(40, 64, 98) : RGB(28, 46, 72), hov ? RGB(120, 190, 255) : RGB(70, 104, 150));
            }
            //Number, resolution, then main screen and what is already open there, centred as a block.
            struct Line { std::wstring s; HFONT font; int h; COLORREF colour; };
            std::vector<Line> lines;
            Line number = { std::to_wstring(i + 1), c.numberFont, c.numberH, text };
            lines.push_back(number);
            const Screen& s = c.screens[i];
            Line size = { std::to_wstring(s.right - s.left) + L" \u00D7 " + std::to_wstring(s.bottom - s.top), c.smallFont, c.smallH, sel ? RGB(214, 232, 252) : textDim };
            lines.push_back(size);
            if (s.primary) {
                Line primary = { c.french ? L"\u00C9cran principal" : L"Main screen", c.smallFont, c.smallH, sel ? RGB(214, 232, 252) : textDim };
                lines.push_back(primary);
            }
            if (!s.inUse.empty()) {
                Line used = { (c.french ? L"Ouvert : " : L"Open: ") + s.inUse, c.smallFont, c.smallH, inUseText };
                lines.push_back(used);
            }
            int total = 0;
            for (size_t l = 0; l < lines.size(); l++) { total += lines[l].h; }
            while (lines.size() > 1 && total > (t.bottom - t.top) - 8) { total -= lines.back().h; lines.pop_back(); }
            int y = (t.top + t.bottom) / 2 - total / 2;
            for (size_t l = 0; l < lines.size(); l++) {
                RECT lr = { t.left + 6, y, t.right - 6, y + lines[l].h };
                drawText(dc, lines[l].font, lines[l].s, lr, lines[l].colour, DT_CENTER | DT_VCENTER);
                y += lines[l].h;
            }
        }

        //Key hints: as many as fit before the buttons.
        RECT hint = { 36, c.openButton.top, c.cancelButton.left - 12, c.openButton.bottom };
        const wchar_t* hintsFr[3] = { L"1 \u00E0 9 : choisir", L"Entr\u00E9e : ouvrir", L"\u00C9chap : annuler" };
        const wchar_t* hintsEn[3] = { L"1 to 9: choose", L"Enter: open", L"Esc: cancel" };
        std::wstring hints;
        SelectObject(dc, c.smallFont);
        for (int h = 0; h < 3; h++) {
            const std::wstring longer = (hints.empty() ? std::wstring() : hints + L"  \u00B7  ") + (c.french ? hintsFr[h] : hintsEn[h]);
            SIZE extent;
            if (!GetTextExtentPoint32W(dc, longer.c_str(), (int)longer.size(), &extent) || extent.cx > hint.right - hint.left) { break; }
            hints = longer;
        }
        drawText(dc, c.smallFont, hints, hint, textFaint, DT_LEFT | DT_VCENTER);
        const bool hovCancel = (c.hover == HoverCancel), hovOpen = (c.hover == HoverOpen);
        roundBox(dc, c.cancelButton, 44, hovCancel ? RGB(36, 58, 88) : RGB(24, 40, 64), hovCancel ? RGB(120, 190, 255) : RGB(90, 120, 160));
        drawText(dc, c.textFont, c.french ? L"Annuler" : L"Cancel", c.cancelButton, text, DT_CENTER | DT_VCENTER);
        roundBox(dc, c.openButton, 44, hovOpen ? RGB(56, 140, 236) : RGB(32, 112, 212), RGB(150, 200, 255));
        const std::wstring open = (c.french ? L"Ouvrir sur l'\u00E9cran " : L"Open on screen ") + std::to_wstring(c.selected + 1);
        drawText(dc, c.textFont, open, c.openButton, RGB(255, 255, 255), DT_CENTER | DT_VCENTER);
    }

    inline void finish(Chooser& c, int result)
    {
        c.result = result;
        c.done = true;
    }

    inline LRESULT CALLBACK chooserProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (msg == WM_NCCREATE) {
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
        Chooser* c = reinterpret_cast<Chooser*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (!c) { return DefWindowProcW(hwnd, msg, wParam, lParam); }
        const int x = (short)LOWORD(lParam), y = (short)HIWORD(lParam);
        const int count = (int)c->screens.size();
        switch (msg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bitmap = CreateCompatibleBitmap(dc, Width, Height);
            HGDIOBJ oldBitmap = SelectObject(mem, bitmap);
            paint(mem, *c);
            BitBlt(dc, 0, 0, Width, Height, mem, 0, 0, SRCCOPY);
            SelectObject(mem, oldBitmap);
            DeleteObject(bitmap);
            DeleteDC(mem);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (!c->tracking) {
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
                c->tracking = TrackMouseEvent(&tme) != FALSE;
            }
            int hover = -1;
            for (int i = 0; i < count; i++) { if (inside(c->tiles[i], x, y)) { hover = i; } }
            if (inside(c->openButton, x, y)) { hover = HoverOpen; }
            if (inside(c->cancelButton, x, y)) { hover = HoverCancel; }
            if (hover != c->hover) { c->hover = hover; InvalidateRect(hwnd, NULL, FALSE); }
            return 0;
        }
        case WM_MOUSELEAVE:
            c->tracking = false;
            if (c->hover != -1) { c->hover = -1; InvalidateRect(hwnd, NULL, FALSE); }
            return 0;
        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT) {
                SetCursor(LoadCursor(NULL, c->hover != -1 ? IDC_HAND : IDC_ARROW));
                return TRUE;
            }
            break;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
            for (int i = 0; i < count; i++) {
                if (inside(c->tiles[i], x, y)) {
                    c->selected = i;
                    if (msg == WM_LBUTTONDBLCLK) { finish(*c, i); }
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }
            if (inside(c->openButton, x, y)) { finish(*c, c->selected); }
            if (inside(c->cancelButton, x, y)) { finish(*c, Cancelled); }
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) { finish(*c, Cancelled); }
            else if (wParam == VK_RETURN || wParam == VK_SPACE) { finish(*c, c->selected); }
            else if (wParam >= '1' && wParam <= '9' && (int)(wParam - '1') < count) { c->selected = (int)(wParam - '1'); }
            else if (wParam >= VK_NUMPAD1 && wParam <= VK_NUMPAD9 && (int)(wParam - VK_NUMPAD1) < count) { c->selected = (int)(wParam - VK_NUMPAD1); }
            else if ((wParam == VK_LEFT || wParam == VK_UP) && count > 0) { c->selected = (c->selected + count - 1) % count; }
            else if ((wParam == VK_RIGHT || wParam == VK_DOWN || wParam == VK_TAB) && count > 0) { c->selected = (c->selected + 1) % count; }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        case WM_CLOSE:
            finish(*c, Cancelled);
            return 0;
        default:
            break;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

} // namespace detail

//The screens in EnumDisplayMonitors order: screen N here is "-monitor N" and monitor=N in the ini files.
inline std::vector<Screen> listScreens(bool french)
{
    detail::ScreenList found;
    EnumDisplayMonitors(0, 0, detail::addMonitor, (LPARAM)&found);
    detail::WindowScan scan = { &found, french };
    EnumWindows(detail::checkWindow, (LPARAM)&scan);
    std::vector<Screen> screens;
    for (size_t i = 0; i < found.size(); i++) { screens.push_back(found[i].second); }
    return screens;
}

//Asks which screen to open on, in a small window on the screen under the mouse pointer. Preselected:
//that screen if none of the simulator's windows is open there, else the first free one. Returns the
//screen index (0 for the first), or Cancelled.
inline int ask(const std::wstring& title, const std::wstring& subtitle, bool french)
{
    detail::Chooser c;
    c.screens = listScreens(french);
    if (c.screens.empty()) { return Cancelled; }
    c.title = title;
    c.subtitle = subtitle;
    c.french = french;
    c.hover = -1;
    c.result = Cancelled;
    c.done = false;
    c.tracking = false;
    c.textH = c.smallH = c.numberH = 0;

    POINT cursor = { 0, 0 };
    GetCursorPos(&cursor);
    int under = 0;
    for (size_t i = 0; i < c.screens.size(); i++) {
        const Screen& s = c.screens[i];
        if (cursor.x >= s.left && cursor.x < s.right && cursor.y >= s.top && cursor.y < s.bottom) { under = (int)i; }
    }
    c.selected = under;
    if (!c.screens[under].inUse.empty()) {
        for (size_t i = 0; i < c.screens.size(); i++) {
            if (c.screens[i].inUse.empty()) { c.selected = (int)i; break; }
        }
    }
    detail::layout(c);

    c.titleFont = detail::makeFont(24, FW_SEMIBOLD);
    c.textFont = detail::makeFont(16, FW_NORMAL);
    c.smallFont = detail::makeFont(13, FW_NORMAL);
    c.numberFont = detail::makeFont(38, FW_LIGHT);

    HINSTANCE instance = GetModuleHandleW(NULL);
    const wchar_t* className = L"NautitechScreenChooser";
    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    if (!GetClassInfoExW(instance, className, &wc)) {
        ZeroMemory(&wc, sizeof(wc));
        wc.cbSize = sizeof(wc);
        wc.style = CS_DBLCLKS | CS_DROPSHADOW;
        wc.lpfnWndProc = detail::chooserProc;
        wc.hInstance = instance;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = className;
        RegisterClassExW(&wc);
    }

    //Centred on the screen under the mouse pointer.
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY), &mi);
    const int x = (mi.rcWork.left + mi.rcWork.right - detail::Width) / 2;
    const int y = (mi.rcWork.top + mi.rcWork.bottom - detail::Height) / 2;
    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_APPWINDOW, className, title.c_str(), WS_POPUP,
        x, y, detail::Width, detail::Height, NULL, NULL, instance, &c);
    if (!hwnd) {
        DeleteObject(c.titleFont); DeleteObject(c.textFont); DeleteObject(c.smallFont); DeleteObject(c.numberFont);
        return c.selected; //no window: open where it would have been preselected
    }
    SetWindowRgn(hwnd, CreateRoundRectRgn(0, 0, detail::Width + 1, detail::Height + 1, 18, 18), FALSE);
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    MSG msg;
    while (!c.done) {
        const BOOL got = GetMessageW(&msg, NULL, 0, 0);
        if (got == 0 || got == -1) { break; } //WM_QUIT or error: as if cancelled
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    DestroyWindow(hwnd);
    DeleteObject(c.titleFont);
    DeleteObject(c.textFont);
    DeleteObject(c.smallFont);
    DeleteObject(c.numberFont);
    return c.result;
}

#else

inline std::vector<Screen> listScreens(bool) { return std::vector<Screen>(); }
inline int ask(const std::wstring&, const std::wstring&, bool) { return Cancelled; }

#endif

} // namespace ScreenChooser

#endif
