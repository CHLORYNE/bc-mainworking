/*   Bridge Command 5.0 Ship Simulator
     Copyright (C) 2014 James Packer

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation

     This program is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY Or FITNESS For A PARTICULAR PURPOSE.  See the
     GNU General Public License For more details.

     You should have received a copy of the GNU General Public License along
     with this program; if not, write to the Free Software Foundation, Inc.,
     51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. */

#ifndef __STARTUPEVENTRECEIVER_HPP_INCLUDED__
#define __STARTUPEVENTRECEIVER_HPP_INCLUDED__

#include "irrlicht.h"

//The scenario screen's keyboard: what it does with a key, before the GUI gets it.
class StartupScreen
{
public:
    virtual ~StartupScreen() {}
    //True when the key has been used (the focused control does not get it).
    virtual bool onKey(const irr::SEvent::SKeyInput& key) = 0;
    //Mouse input over the screen's own areas (the list, the chart), whichever control has the focus;
    //true when used.
    virtual bool onMouse(const irr::SEvent::SMouseInput& mouse) = 0;
};

//Device-level receiver while the scenario is chosen: keys and mouse go to the screen first (list moves,
//Enter to start, Escape to quit, wheel and drag on the list and chart), everything else to the GUI.
class StartupEventReceiver : public irr::IEventReceiver
{
public:
    StartupEventReceiver(StartupScreen* screen, irr::IrrlichtDevice* dev);
    bool OnEvent(const irr::SEvent& event);

private:
    StartupScreen* screen;
    irr::IrrlichtDevice* device;
};

#endif
