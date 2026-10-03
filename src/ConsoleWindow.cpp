/*   NAUTITECH - Simulateur de Navigation
     Second window for the instrument console. See ConsoleWindow.hpp.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#include "ConsoleWindow.hpp"

#include <string>

namespace {

irr::SEvent makeMouseEvent(irr::EMOUSE_INPUT_EVENT type, int x, int y, float wheel, irr::u32 buttons)
{
    irr::SEvent e;
    e.EventType = irr::EET_MOUSE_INPUT_EVENT;
    e.MouseInput.Event = type;
    e.MouseInput.X = x;
    e.MouseInput.Y = y;
    e.MouseInput.Wheel = wheel;
    e.MouseInput.Shift = false;
    e.MouseInput.Control = false;
    e.MouseInput.ButtonStates = buttons;
    return e;
}

} // namespace

void ConsoleWindow::queueEvent(const irr::SEvent& event)
{
    pending.push_back(event);
}

void ConsoleWindow::setClientSize(irr::u32 w, irr::u32 h)
{
    clientSize = irr::core::dimension2du(w, h);
}

bool ConsoleWindow::isOpen() const
{
    return opened;
}

irr::core::dimension2du ConsoleWindow::getClientSize() const
{
    return clientSize;
}

//=================================================================================================
#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>

namespace {

const wchar_t* kClassName = L"NautitechConsoleWindow";

irr::u32 buttonStates(WPARAM wParam)
{
    irr::u32 s = 0;
    if (wParam & MK_LBUTTON) { s |= irr::EMBSM_LEFT; }
    if (wParam & MK_RBUTTON) { s |= irr::EMBSM_RIGHT; }
    if (wParam & MK_MBUTTON) { s |= irr::EMBSM_MIDDLE; }
    return s;
}

LRESULT CALLBACK consoleWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    ConsoleWindow* self = reinterpret_cast<ConsoleWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    if (!self) { return DefWindowProcW(hWnd, msg, wParam, lParam); }

    const int x = GET_X_LPARAM(lParam);
    const int y = GET_Y_LPARAM(lParam);
    switch (msg) {
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;   //clicks never take the keyboard away from the simulator
    case WM_ERASEBKGND:
        return 1;               //everything is drawn by OpenGL
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_SIZE:
        self->setClientSize(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_CLOSE:
        self->requestClose();   //the simulator re-attaches the console, then closes the window
        return 0;
    case WM_LBUTTONDOWN:
        SetCapture(hWnd);
        self->queueEvent(makeMouseEvent(irr::EMIE_LMOUSE_PRESSED_DOWN, x, y, 0, buttonStates(wParam)));
        return 0;
    case WM_LBUTTONUP:
        ReleaseCapture();
        self->queueEvent(makeMouseEvent(irr::EMIE_LMOUSE_LEFT_UP, x, y, 0, buttonStates(wParam)));
        return 0;
    case WM_RBUTTONDOWN:
        self->queueEvent(makeMouseEvent(irr::EMIE_RMOUSE_PRESSED_DOWN, x, y, 0, buttonStates(wParam)));
        return 0;
    case WM_RBUTTONUP:
        self->queueEvent(makeMouseEvent(irr::EMIE_RMOUSE_LEFT_UP, x, y, 0, buttonStates(wParam)));
        return 0;
    case WM_MOUSEMOVE:
        self->queueEvent(makeMouseEvent(irr::EMIE_MOUSE_MOVED, x, y, 0, buttonStates(wParam)));
        return 0;
    case WM_MOUSEWHEEL: {
        POINT p = { x, y };   //screen coordinates for this message
        ScreenToClient(hWnd, &p);
        const float wheel = (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
        self->queueEvent(makeMouseEvent(irr::EMIE_MOUSE_WHEEL, p.x, p.y, wheel, buttonStates(GET_KEYSTATE_WPARAM(wParam))));
        return 0;
    }
    default:
        break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

} // namespace

ConsoleWindow::ConsoleWindow()
    : closePending(false), opened(false), borderless(false), hwnd(0), hdc(0)
{
}

ConsoleWindow::~ConsoleWindow()
{
    close();
}

bool ConsoleWindow::open(irr::IrrlichtDevice* device, const wchar_t* title, irr::s32 x, irr::s32 y, irr::u32 w, irr::u32 h, bool noFrame)
{
    if (opened) { return true; }
    if (!device || device->getVideoDriver()->getDriverType() != irr::video::EDT_OPENGL) { return false; }

    mainData = device->getVideoDriver()->getExposedVideoData();
    HWND mainHwnd = (HWND)mainData.OpenGLWin32.HWnd;
    HDC mainDc = (HDC)mainData.OpenGLWin32.HDc;
    if (!mainHwnd || !mainDc || !mainData.OpenGLWin32.HRc) { return false; }

    HINSTANCE instance = GetModuleHandleW(0);
    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    if (!GetClassInfoExW(instance, kClassName, &wc)) {
        ZeroMemory(&wc, sizeof(wc));
        wc.cbSize = sizeof(wc);
        wc.style = CS_OWNDC;                 //one DC for the window's lifetime, holding the pixel format
        wc.lpfnWndProc = consoleWndProc;
        wc.hInstance = instance;
        wc.hCursor = LoadCursor(0, IDC_ARROW);
        wc.hIcon = (HICON)GetClassLongPtrW(mainHwnd, GCLP_HICON);
        wc.hIconSm = (HICON)GetClassLongPtrW(mainHwnd, GCLP_HICONSM);
        wc.hbrBackground = 0;
        wc.lpszClassName = kClassName;
        if (!RegisterClassExW(&wc)) { return false; }
    }

    const DWORD style = noFrame ? WS_POPUP : WS_OVERLAPPEDWINDOW;
    const DWORD exStyle = WS_EX_NOACTIVATE;
    RECT frame = { 0, 0, (LONG)w, (LONG)h };
    if (!noFrame) { AdjustWindowRectEx(&frame, style, FALSE, exStyle); }
    const int frameW = frame.right - frame.left;
    const int frameH = frame.bottom - frame.top;

    //A saved position on a screen that is no longer connected: start on the simulator's screen.
    RECT wanted = { x, y, x + frameW, y + frameH };
    if (!MonitorFromRect(&wanted, MONITOR_DEFAULTTONULL)) {
        MONITORINFO mi;
        mi.cbSize = sizeof(mi);
        GetMonitorInfoW(MonitorFromWindow(mainHwnd, MONITOR_DEFAULTTOPRIMARY), &mi);
        x = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - frameW) / 2;
        y = mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) - frameH) / 2;
    }

    //Owned by the simulator window: stays above it, closes with it, no extra taskbar entry.
    HWND window = CreateWindowExW(exStyle, kClassName, title, style, x, y, frameW, frameH, mainHwnd, 0, instance, this);
    if (!window) { return false; }

    //Same pixel format as the simulator window, so its OpenGL context can draw here too.
    HDC dc = GetDC(window);
    const int format = GetPixelFormat(mainDc);
    PIXELFORMATDESCRIPTOR pfd;
    ZeroMemory(&pfd, sizeof(pfd));
    if (format == 0 || !DescribePixelFormat(mainDc, format, sizeof(pfd), &pfd) || !SetPixelFormat(dc, format, &pfd)) {
        ReleaseDC(window, dc);
        DestroyWindow(window);
        return false;
    }

    hwnd = window;
    hdc = dc;
    borderless = noFrame;
    windowData = irr::video::SExposedVideoData();
    windowData.OpenGLWin32.HWnd = window;
    windowData.OpenGLWin32.HDc = dc;
    windowData.OpenGLWin32.HRc = mainData.OpenGLWin32.HRc;
    clientSize = irr::core::dimension2du(w, h);
    pending.clear();
    closePending = false;
    opened = true;
    ShowWindow(window, SW_SHOWNOACTIVATE);
    return true;
}

void ConsoleWindow::close()
{
    if (!opened) { return; }
    HWND window = (HWND)hwnd;
    SetWindowLongPtrW(window, GWLP_USERDATA, 0);
    ReleaseDC(window, (HDC)hdc);
    DestroyWindow(window);
    hwnd = 0;
    hdc = 0;
    opened = false;
    pending.clear();
    closePending = false;
}

void ConsoleWindow::poll(std::vector<irr::SEvent>& events, bool& closeRequested)
{
    //Messages were already dispatched to the window procedure by device->run().
    events.swap(pending);
    pending.clear();
    closeRequested = closePending;
    closePending = false;
}

bool ConsoleWindow::getPlacement(irr::s32& x, irr::s32& y, irr::u32& w, irr::u32& h) const
{
    if (!opened) { return false; }
    WINDOWPLACEMENT wp;
    wp.length = sizeof(wp);
    if (!GetWindowPlacement((HWND)hwnd, &wp)) { return false; }
    x = wp.rcNormalPosition.left;   //restored (not minimised / maximised) frame position
    y = wp.rcNormalPosition.top;
    RECT frame = { 0, 0, 0, 0 };
    if (!borderless) { AdjustWindowRectEx(&frame, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE); }
    const int fw = (wp.rcNormalPosition.right - wp.rcNormalPosition.left) - (frame.right - frame.left);
    const int fh = (wp.rcNormalPosition.bottom - wp.rcNormalPosition.top) - (frame.bottom - frame.top);
    w = fw > 0 ? (irr::u32)fw : clientSize.Width;
    h = fh > 0 ? (irr::u32)fh : clientSize.Height;
    return true;
}

//=================================================================================================
#else // X11

#include <X11/Xlib.h>
#include <X11/Xutil.h>

ConsoleWindow::ConsoleWindow()
    : closePending(false), opened(false), borderless(false), display(0), window(0), deleteAtom(0)
{
}

ConsoleWindow::~ConsoleWindow()
{
    close();
}

bool ConsoleWindow::open(irr::IrrlichtDevice* device, const wchar_t* title, irr::s32 x, irr::s32 y, irr::u32 w, irr::u32 h, bool noFrame)
{
    if (opened) { return true; }
    if (!device || device->getVideoDriver()->getDriverType() != irr::video::EDT_OPENGL) { return false; }

    mainData = device->getVideoDriver()->getExposedVideoData();
    Display* mainDisplay = (Display*)mainData.OpenGLLinux.X11Display;
    Window mainWindow = (Window)mainData.OpenGLLinux.X11Window;
    if (!mainDisplay || !mainWindow || !mainData.OpenGLLinux.X11Context) { return false; }

    XWindowAttributes wa;
    if (!XGetWindowAttributes(mainDisplay, mainWindow, &wa)) { return false; }

    //Own connection: Irrlicht's event loop reads every event on its connection, whatever the window.
    Display* dpy = XOpenDisplay(DisplayString(mainDisplay));
    if (!dpy) { return false; }

    XVisualInfo tmpl;
    tmpl.visualid = XVisualIDFromVisual(wa.visual);
    int count = 0;
    XVisualInfo* vi = XGetVisualInfo(dpy, VisualIDMask, &tmpl, &count);
    if (!vi || count < 1) {
        if (vi) { XFree(vi); }
        XCloseDisplay(dpy);
        return false;
    }
    Window root = RootWindow(dpy, vi->screen);
    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(dpy, root, vi->visual, AllocNone);
    swa.border_pixel = 0;
    swa.background_pixmap = None;
    swa.event_mask = ButtonPressMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask | ExposureMask;
    Window win = XCreateWindow(dpy, root, x, y, w, h, 0, vi->depth, InputOutput, vi->visual,
        CWColormap | CWBorderPixel | CWEventMask | CWBackPixmap, &swa);
    XFree(vi);
    if (!win) {
        XCloseDisplay(dpy);
        return false;
    }

    std::string name;
    for (const wchar_t* c = title; c && *c; ++c) { name += (*c < 128) ? (char)*c : '?'; }
    XStoreName(dpy, win, name.c_str());

    XWMHints hints;
    hints.flags = InputHint;
    hints.input = False;   //clicks never take the keyboard away from the simulator
    XSetWMHints(dpy, win, &hints);

    XSizeHints sizeHints;
    sizeHints.flags = USPosition | USSize;
    sizeHints.x = x;
    sizeHints.y = y;
    sizeHints.width = (int)w;
    sizeHints.height = (int)h;
    XSetWMNormalHints(dpy, win, &sizeHints);

    Atom del = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &del, 1);

    if (noFrame) {
        //No decorations (Motif hints, honoured by the usual window managers).
        struct { unsigned long flags, functions, decorations; long inputMode; unsigned long status; } motif = { 2, 0, 0, 0, 0 };
        Atom motifAtom = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
        XChangeProperty(dpy, win, motifAtom, motifAtom, 32, PropModeReplace, (unsigned char*)&motif, 5);
    }

    XMapWindow(dpy, win);
    XSync(dpy, False);   //the window must exist on the server before the main connection draws to it

    display = dpy;
    window = win;
    deleteAtom = del;
    borderless = noFrame;
    windowData = mainData;
    windowData.OpenGLLinux.X11Window = win;
    windowData.OpenGLLinux.GLXWindow = win;
    clientSize = irr::core::dimension2du(w, h);
    pending.clear();
    closePending = false;
    opened = true;
    return true;
}

void ConsoleWindow::close()
{
    if (!opened) { return; }
    Display* dpy = (Display*)display;
    XDestroyWindow(dpy, (Window)window);
    XSync(dpy, False);
    XCloseDisplay(dpy);
    display = 0;
    window = 0;
    opened = false;
    pending.clear();
    closePending = false;
}

void ConsoleWindow::pumpX11()
{
    Display* dpy = (Display*)display;
    while (XPending(dpy) > 0) {
        XEvent e;
        XNextEvent(dpy, &e);
        switch (e.type) {
        case ButtonPress:
        case ButtonRelease: {
            const bool down = (e.type == ButtonPress);
            const int bx = e.xbutton.x, by = e.xbutton.y;
            if (e.xbutton.button == Button1) {
                queueEvent(makeMouseEvent(down ? irr::EMIE_LMOUSE_PRESSED_DOWN : irr::EMIE_LMOUSE_LEFT_UP, bx, by, 0, down ? irr::EMBSM_LEFT : 0));
            }
            else if (e.xbutton.button == Button3) {
                queueEvent(makeMouseEvent(down ? irr::EMIE_RMOUSE_PRESSED_DOWN : irr::EMIE_RMOUSE_LEFT_UP, bx, by, 0, down ? irr::EMBSM_RIGHT : 0));
            }
            else if (down && (e.xbutton.button == Button4 || e.xbutton.button == Button5)) {
                queueEvent(makeMouseEvent(irr::EMIE_MOUSE_WHEEL, bx, by, e.xbutton.button == Button4 ? 1.0f : -1.0f, 0));
            }
            break;
        }
        case MotionNotify: {
            irr::u32 states = 0;
            if (e.xmotion.state & Button1Mask) { states |= irr::EMBSM_LEFT; }
            if (e.xmotion.state & Button3Mask) { states |= irr::EMBSM_RIGHT; }
            queueEvent(makeMouseEvent(irr::EMIE_MOUSE_MOVED, e.xmotion.x, e.xmotion.y, 0, states));
            break;
        }
        case ConfigureNotify:
            setClientSize((irr::u32)e.xconfigure.width, (irr::u32)e.xconfigure.height);
            break;
        case ClientMessage:
            if ((unsigned long)e.xclient.data.l[0] == deleteAtom) { requestClose(); }
            break;
        default:
            break;
        }
    }
}

void ConsoleWindow::poll(std::vector<irr::SEvent>& events, bool& closeRequested)
{
    if (opened) { pumpX11(); }
    events.swap(pending);
    pending.clear();
    closeRequested = closePending;
    closePending = false;
}

bool ConsoleWindow::getPlacement(irr::s32& x, irr::s32& y, irr::u32& w, irr::u32& h) const
{
    if (!opened) { return false; }
    Display* dpy = (Display*)display;
    Window child;
    int rx = 0, ry = 0;
    XTranslateCoordinates(dpy, (Window)window, DefaultRootWindow(dpy), 0, 0, &rx, &ry, &child);
    x = rx;
    y = ry;
    w = clientSize.Width;
    h = clientSize.Height;
    return true;
}

#endif
