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

#ifndef __RAIN_HPP_INCLUDED__
#define __RAIN_HPP_INCLUDED__

#include "irrlicht.h"
#include <vector>

class Rain
{

public:

    Rain();
    ~Rain();
    void load(irr::scene::ISceneManager* smgr, irr::scene::ISceneNode* parent, irr::IrrlichtDevice* dev);
    void update(irr::f32 scenarioTime);
    void setIntensity(irr::f32 intensity);

    //KYARA: wind speed in knots. Drives the slant of the streaks and their sideways drift.
    //Optional - if never called the rain simply falls vertically, as before.
    void setWind(irr::f32 windSpeedKts);

    //KYARA: wind direction in degrees. This is the METEOROLOGICAL convention - the direction
    //the wind is blowing FROM, same as everywhere else in the sim. Call it alongside setWind()
    //from wherever the weather is updated. If never called, the rain leans north, which is
    //harmless but wrong, so do wire it up.
    void setWindDirection(irr::f32 windDirDeg);

private:

    irr::f32 rainIntensity;
    irr::f32 windSpeedKts;
    irr::f32 windDirDeg;             //KYARA: direction wind blows FROM, degrees
    irr::scene::ISceneManager* smgr; //KYARA: kept so update() can find the active camera
    irr::scene::ISceneNode* parent;
    irr::scene::ISceneNode* rainNode1;
    irr::scene::ISceneNode* rainNode2;
    irr::scene::ISceneNode* rainNode3; //KYARA: third layer, for parallax depth
    std::vector<irr::video::ITexture*> rainTextures;
    void applyTextures();
    void setupNode(irr::scene::ISceneNode* node);

    //Texture offsets, moved on by speed x time step each frame. Working them out as speed x the
    //whole scenario time made the streaks race whenever the rain or the wind changed (a preset
    //coming in, a squall, gusts), since the change was multiplied by every second gone by.
    irr::f32 lastScenarioTime;
    bool timeKnown;
    irr::f32 scrollY1, scrollY2, scrollY3, scrollDrift;

};

#endif
