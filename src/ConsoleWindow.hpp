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

class ConsoleWindow
{
public:
    ConsoleWindow();
    ~ConsoleWindow();

    //Opens the window with a client area of w x h at screen position (x, y) (top-left of the frame).
    bool open(irr::IrrlichtDevice* device, const wchar_t* title, irr::s32 x, irr::s32 y, irr::u32 w, irr::u32 h);
    void close();
    bool isOpen() const;

    //Mouse input since the last call, in client coordinates (left and right buttons, moves, wheel).
    //closeRequested: the user clicked the window's close button.
    void poll(std::vector<irr::SEvent>& events, bool& closeRequested);

    irr::core::dimension2du getClientSize() const;
    //Frame position on the desktop and client size, for saving and restoring the placement.
    bool getPlacement(irr::s32& x, irr::s32& y, irr::u32& w, irr::u32& h) const;

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

#ifdef _WIN32
    void* hwnd;
    void* hdc;
#else
    void* display;          //this window's own X connection
    unsigned long window;
    unsigned long deleteAtom;
    void pumpX11();
#endif
};

#endif
