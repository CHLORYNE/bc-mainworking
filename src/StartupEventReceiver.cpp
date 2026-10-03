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

#include "StartupEventReceiver.hpp"

StartupEventReceiver::StartupEventReceiver(StartupScreen* screen, irr::IrrlichtDevice* dev)
    : screen(screen), device(dev)
{
}

bool StartupEventReceiver::OnEvent(const irr::SEvent& event)
{
    if (event.EventType == irr::EET_KEY_INPUT_EVENT) {
        if (event.KeyInput.Key == irr::KEY_F4 && event.KeyInput.PressedDown) {
            device->closeDevice(); //Shutdown
            return true;
        }
        if (screen && screen->onKey(event.KeyInput)) { return true; }
    }
    if (event.EventType == irr::EET_MOUSE_INPUT_EVENT && screen && screen->onMouse(event.MouseInput)) {
        return true;
    }
    return false;
}
