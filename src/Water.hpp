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

#ifndef __WATER_HPP_INCLUDED__
#define __WATER_HPP_INCLUDED__

#include "irrlicht.h"
#include "MovingWater.hpp"

class Water
{
public:
    Water();
    virtual ~Water();
    void load(irr::scene::ISceneManager* smgr, irr::scene::ISceneNode* ownShip, irr::f32 weather, irr::u32 disableShaders, bool withReflection, irr::u32 segments, irr::u32 reflectionEveryN = 1);
    void update(irr::f32 tideHeight, irr::core::vector3df viewPosition, irr::u32 lightLevel, irr::f32 weather, irr::f32 windDirection = 225.0f, irr::f32 rainIntensity = 0.0f); // KYARA HOULE: + wind direction, KYARA METEO: + rain
    void setSwellShaderData(const irr::f32* comp20, const irr::f32* fade4); // KYARA HOULE
    irr::f32 getWaveHeight(irr::f32 relPosX, irr::f32 relPosZ) const;
    irr::core::vector2df getLocalNormals(irr::f32 relPosX, irr::f32 relPosZ) const;
    irr::core::vector3df getPosition() const;
    void setVisible(bool visible);
    //No sea drawn inside the own ship's hull: her node's position and rotation, and the outline and
    //bottom measured from her model (see OwnShip::getHullWaterline). on = false: off.
    void setHullMask(irr::core::vector3df nodePosition, irr::core::vector3df nodeRotationDeg, irr::f32 zMin, irr::f32 zMax, irr::f32 centreX, const irr::f32* halfWidths24, const irr::f32* keels24, bool on);

private:
    void setSea(irr::f32 weather, irr::f32 windDirection);
    irr::f32 tileWidth;
    irr::scene::MovingWaterSceneNode* waterNode;
};

#endif