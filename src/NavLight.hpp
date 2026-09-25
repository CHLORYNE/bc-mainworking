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

#ifndef __NAVLIGHT_HPP_INCLUDED__
#define __NAVLIGHT_HPP_INCLUDED__

#include <string>

#include "irrlicht.h"

class NavLight {

public:
    NavLight(irr::scene::ISceneNode* parent, irr::scene::ISceneManager* smgr, irr::core::vector3df position, irr::video::SColor colour, irr::f32 lightStartAngle, irr::f32 lightEndAngle, irr::f32 lightRange, std::string lightSequence = "", irr::u32 phaseStart = 0);
    ~NavLight();
    void update(irr::f32 scenarioTime, irr::u32 lightLevel);
    irr::core::vector3df getPosition() const;
    void setPosition(irr::core::vector3df position);
    void moveNode(irr::f32 deltaX, irr::f32 deltaY, irr::f32 deltaZ);
    //KYARA FEUX: switch a lamp on or off, and recolour it (the signal line re-uses the same
    //three lamps for red-red, red-white-red and green-white).
    void setEnabled(bool enabled);
    bool isEnabled() const;
    void setColour(irr::video::SColor colour);

private:
    irr::scene::ISceneManager* smgr;
    irr::scene::IMeshSceneNode* lightNode;
    irr::scene::IMeshSceneNode* glowNode;   // KYARA: additive halo around the light
    irr::video::SColor baseColour;          // KYARA: unmodulated colour, for brightness scaling
    irr::video::ITexture* lightTexture;
    irr::f32 startAngle;
    irr::f32 endAngle;
    irr::f32 range;
    std::string sequence;
    irr::f32 charTime; //Time in seconds per character in sequence
    irr::f32 timeOffset;
    bool enabled;                           // KYARA FEUX
    irr::u16 currentAlpha; //Note that this is u16 not u8 so we can indicate an initial implausible value.
    //bool setAlpha(irr::u8 alpha, irr::video::ITexture* tex);
    //irr::f32 lightLevel;
};

#endif