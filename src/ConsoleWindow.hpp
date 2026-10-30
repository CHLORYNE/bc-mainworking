/*   NAUTITECH - Simulateur de Navigation
     A second, native window for the instrument console, so it can sit on another screen while the
     main window shows the full bridge view.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#ifndef __CONSOLE_WINDOW_HPP_INCLUDED__
#define __CONSOLE_WINDOW_HPP_INCLUDED__

//The window shares the simulator's OpenGL context: the console is drawn into it with the same
//driver, textures and fonts, by switching the context's drawable for one beginScene/endScene pass
//per frame (driver->beginScene(..., videoData())). Nothing is copied between windows.
//
//Windows: an owned top-level window that never takes activation (WS_EX_NOACTIVATE), so clicking
//it leaves the keyboard with the simulator. Its messages are dispatched by Irrlicht's own message
//loop (device->run()); this class only queues them.
//Linux/X11: a window on its own X connection, so Irrlicht's event loop never sees its events.

#include "irrlicht.h"
#include <vector>
#include <string>

class ConsoleWindow
{
public:
    ConsoleWindow();
    ~ConsoleWindow();

    //Opens the window with a client area of w x h at screen position (x, y) (top-left of the frame).
    //borderless: no frame or title bar, the client area at (x, y) - for a screen of its own.
    bool open(irr::IrrlichtDevice* device, const wchar_t* title, irr::s32 x, irr::s32 y, irr::u32 w, irr::u32 h, bool borderless = false);
    void close();
    bool isOpen() const;
    //Opens a file with the program the system uses for it (the debrief report in the browser).
    //Windows only; elsewhere nothing happens.
    static void openWithSystem(const std::string& path);

    //Mouse input since the last call, in client coordinates (left and right buttons, moves, wheel).
    //closeRequested: the user clicked the window's close button.
    void poll(std::vector<irr::SEvent>& events, bool& closeRequested);

    irr::core::dimension2du getClientSize() const;
    //Keeps the window in front of the simulator while the simulator is in use (a full-screen bridge
    //view, above all on a Surround / Eyefinity canvas, would otherwise cover it), and lets other
    //programs go over it when the simulator is in the background. Called every frame.
    void keepAboveSimulator();
    //Frame position on the desktop and client size, for saving and restoring the placement. In full
    //screen: the window's place before it went full screen.
    bool getPlacement(irr::s32& x, irr::s32& y, irr::u32& w, irr::u32& h) const;
    //Full screen on the screen the window is on (no frame), and back to the window it was.
    void setFullScreen(bool on);
    //New size of the client area, the window's corner staying where it is (not in full screen)
    void resizeClient(irr::u32 w, irr::u32 h);
    //Brings a framed window wholly inside the work area of its screen (title bar reachable), smaller if
    //it is taller or wider than the screen. Done on opening and after resizeClient().
    void keepOnScreen();
    //The usable part of the screen the window is on (without the taskbar), or 0 x 0 if unknown
    irr::core::dimension2du workArea() const;
    //Starts moving the window with the mouse, as a drag of its title bar would (button held down)
    void beginMove(irr::s32 clientX, irr::s32 clientY);
    bool isFullScreen() const { return fullScreen; }

    //Whether the simulator can draw into the window now: not while the bridge view is drawn by a
    //processor's built-in graphics and this window is on a screen of another card (AMD's OpenGL driver
    //stops the simulator on the first picture copied that way; the other way round, a graphics card
    //drawing for a screen on the motherboard, works). The window then says so, until it is moved back.
    //Call before each beginScene() into it. Always true off Windows.
    bool canDraw();
    //Whether a card is a processor's built-in graphics ("AMD Radeon(TM) RX Vega 11 Graphics",
    //"Intel(R) UHD Graphics 630"), from its name
    static bool isBuiltInGraphics(const std::string& card);
    //Whether a window on a screen of 'card' can be drawn by the card of the bridge view's screen
    static bool canDrawAcross(const std::string& viewCard, const std::string& card);
    //bc5.ini other_card_windows=1: draw on screens of another card anyway
    static void setAllowOtherCard(bool allow) { allowOtherCard = allow; }
    //The graphics card a screen (an HMONITOR) is plugged into ("AMD Radeon RX 580"), empty if unknown
    static std::string graphicsCardOf(void* monitor);
    //Internal: the message shown instead of the picture (called from the window procedure).
    void paintOtherCardNotice(void* dc);

    //Pass to driver->beginScene() to draw into this window.
    const irr::video::SExposedVideoData& videoData() const { return windowData; }
    //Pass to driver->beginScene() to draw into the simulator window again. (Needed on X11, where
    //Irrlicht does not switch back by itself when the drawable changed but the display did not.)
    const irr::video::SExposedVideoData& mainVideoData() const { return mainData; }

    //Internal: queue an event / note a resize (called from the window procedure).
    void queueEvent(const irr::SEvent& event);
    void setClientSize(irr::u32 w, irr::u32 h);
    void requestClose() { closePending = true; }

private:
    irr::video::SExposedVideoData windowData;
    irr::video::SExposedVideoData mainData;
    std::vector<irr::SEvent> pending;
    irr::core::dimension2du clientSize;
    bool closePending;
    bool opened;
    bool borderless;
    bool fullScreen = false;
    irr::s32 windowedX = 0, windowedY = 0;   //placement before going full screen
    irr::u32 windowedW = 0, windowedH = 0;
    static bool allowOtherCard;
    void* lastMonitor = 0;          //screens of this window and of the simulator window at the last check
    void* lastMainMonitor = 0;
    bool otherCard = false;         //on a screen of another card: not drawn
    std::string cardName, mainCardName;

#ifdef _WIN32
    void* hwnd;
    void* hdc;
    unsigned long lastZCheck = 0;   //GetTickCount() of the last look at the stacking order
#else
    void* display;          //this window's own X connection
    unsigned long window;
    unsigned long deleteAtom;
    void pumpX11();
#endif
};

#endif
