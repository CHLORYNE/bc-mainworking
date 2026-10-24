/*   NAUTITECH - Simulateur de Navigation
     Menus on the middle screen of a multi-screen canvas.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#ifndef __CENTRESCREEN_HPP_INCLUDED__
#define __CENTRESCREEN_HPP_INCLUDED__

#include "irrlicht.h"
#include <cmath>

//With NVIDIA Surround or AMD Eyefinity the operating system sees the screens of the bridge as one
//very wide screen, so a full-screen menu would spread over all of them. The launcher menu and the
//exercise choice are drawn instead into a picture the size of ONE screen, shown on the middle one;
//the side screens stay black. The 3D view itself is not affected.
//
//Use: a View draws the frames (beginScene / endScene in place of the driver's), the menus lay
//themselves out for contentSize() instead of the screen size, and a MouseShift receiver moves the
//mouse into the picture's coordinates.
namespace centre
{
    struct State
    {
        bool active = false;                 //menus are being drawn on the middle screen
        irr::core::rect<irr::s32> area;      //where, in the window
        int forcedScreens = 0;               //bc5.ini menu_screens: 0 = guess from the window's shape
    };

    inline State& state()
    {
        static State s;
        return s;
    }

    //How many screens side by side a window this size covers. A single wide screen (21:9 is about
    //2.4, 32:9 is 3.6 but then the setting says 1) is one screen.
    inline int screensAcross(const irr::core::dimension2du& size)
    {
        if (state().forcedScreens > 0) { return state().forcedScreens; }
        if (size.Height == 0) { return 1; }
        const irr::f32 aspect = (irr::f32)size.Width / (irr::f32)size.Height;
        if (aspect < 2.6f) { return 1; }
        return irr::core::max_(2, (int)std::floor(aspect / (16.0f / 9.0f) + 0.5f));
    }

    //The middle screen of the window (with an even number of screens, one screen's width in the middle).
    inline irr::core::rect<irr::s32> middleArea(const irr::core::dimension2du& size, int screens)
    {
        if (screens <= 1) { return irr::core::rect<irr::s32>(0, 0, (irr::s32)size.Width, (irr::s32)size.Height); }
        const irr::s32 w = (irr::s32)size.Width / screens;
        const irr::s32 x = ((irr::s32)size.Width - w) / 2;
        return irr::core::rect<irr::s32>(x, 0, x + w, (irr::s32)size.Height);
    }

    //The size the menus lay themselves out for: the middle screen, or the whole window.
    inline irr::core::dimension2du contentSize(irr::video::IVideoDriver* driver)
    {
        if (state().active) {
            return irr::core::dimension2du((irr::u32)state().area.getWidth(), (irr::u32)state().area.getHeight());
        }
        return driver->getScreenSize();
    }

    class View
    {
    public:
        explicit View(irr::IrrlichtDevice* device) : driver(device->getVideoDriver()), target(0) {}
        ~View()
        {
            release();
            state().active = false;
        }

        //Follows the window: called by beginScene, and once before the first layout.
        void refresh()
        {
            const irr::core::dimension2du size = driver->getScreenSize();
            const int screens = screensAcross(size);
            if (screens <= 1 || !driver->queryFeature(irr::video::EVDF_RENDER_TO_TARGET)) {
                release();
                state().active = false;
                return;
            }
            const irr::core::rect<irr::s32> area = middleArea(size, screens);
            const irr::core::dimension2du want((irr::u32)area.getWidth(), (irr::u32)area.getHeight());
            if (!target || target->getSize() != want) {
                release();
                target = driver->addRenderTargetTexture(want, "centre-screen-menu", irr::video::ECF_A8R8G8B8);
            }
            state().active = (target != 0);
            state().area = area;
        }

        void beginScene(irr::video::SColor clear)
        {
            refresh();
            driver->beginScene(irr::video::ECBF_COLOR | irr::video::ECBF_DEPTH, target ? irr::video::SColor(255, 0, 0, 0) : clear);
            if (target) { driver->setRenderTarget(target, irr::video::ECBF_COLOR | irr::video::ECBF_DEPTH, clear); }
        }

        void endScene()
        {
            if (target) {
                driver->setRenderTarget((irr::video::ITexture*)0, 0);
                driver->draw2DImage(target, state().area.UpperLeftCorner);
            }
            driver->endScene();
        }

    private:
        void release()
        {
            if (target) {
                driver->removeTexture(target);
                target = 0;
            }
        }

        irr::video::IVideoDriver* driver;
        irr::video::ITexture* target;
    };

    //Mouse events in the menus' coordinates: put in front of the screen's own receiver.
    class MouseShift : public irr::IEventReceiver
    {
    public:
        MouseShift(irr::gui::IGUIEnvironment* gui, irr::IEventReceiver* inner) : gui(gui), inner(inner) {}

        virtual bool OnEvent(const irr::SEvent& event)
        {
            if (event.EventType != irr::EET_MOUSE_INPUT_EVENT || !state().active) {
                return inner ? inner->OnEvent(event) : false;
            }
            irr::SEvent moved = event;
            moved.MouseInput.X -= state().area.UpperLeftCorner.X;
            moved.MouseInput.Y -= state().area.UpperLeftCorner.Y;
            if (inner && inner->OnEvent(moved)) { return true; }
            if (gui) { gui->postEventFromUser(moved); }
            return true; //the window's own coordinates must not reach the menus as well
        }

    private:
        irr::gui::IGUIEnvironment* gui;
        irr::IEventReceiver* inner;
    };
}

#endif
