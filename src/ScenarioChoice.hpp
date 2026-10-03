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

#ifndef __SCENARIOCHOICE_HPP_INCLUDED__
#define __SCENARIOCHOICE_HPP_INCLUDED__

#include "irrlicht.h"
#include "Lang.hpp"
#include "OperatingModeEnum.hpp"
#include <string>

//Start screen: the exercises in a list, with a quick view of the selected one (chart with the ships and
//their routes, own ship, conditions, traffic, incident, description), and how this station runs
//(exercise, secondary display, multiplayer).
class ScenarioChoice
{
public:
    //fontName: bc5.ini font. french: French wording for the screen.
    ScenarioChoice(irr::IrrlichtDevice* device, Lang* language, const std::string& fontName, bool french);
    void chooseScenario(std::string& scenarioName, std::string& hostname, irr::u32& udpPort, OperatingMode::Mode& mode, std::string scenarioPath);

private:
    irr::IrrlichtDevice* device;
    irr::gui::IGUIEnvironment* gui;
    Lang* language;
    std::string fontName;
    bool french;
};

#endif
