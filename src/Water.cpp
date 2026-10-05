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

     //NOTE: This uses a modified version of Irrlicht for the water surface scene node, which bases the waves
     //on the absolute position, so you can tile multiple water nodes seamlessly.

#include <vector>
#include <cmath>
#include <iostream>

#include "Water.hpp"
#include "Utilities.hpp"

//using namespace irr;

Water::Water()
{

}

Water::~Water()
{
    //dtor
}

void Water::load(irr::scene::ISceneManager* smgr, irr::scene::ISceneNode* ownShip, irr::f32 weather, irr::u32 disableShaders, bool withReflection, irr::u32 segments, irr::u32 reflectionEveryN)
{

    irr::video::IVideoDriver* driver = smgr->getVideoDriver();

    //Set tile width
    //FIXME: Hardcoded or defined in multiple places
    tileWidth = 100; //Width in metres - Note this is used in Simulation model normalisation as 1000, so visible jumps in water are minimised


    waterNode = new irr::scene::MovingWaterSceneNode(smgr->getRootSceneNode(), smgr, ownShip, 0, disableShaders, withReflection, segments, reflectionEveryN);

    //waterNode->setPosition(irr::core::vector3df(0,-0.25f,0));

    //KYARA EAU - LOOK SWITCH. With shaders on, water.bmp REPLACES the ripple normal map
    //(media/waterbump.png, set in MovingWater's constructor) and Water_ps.glsl reads the photo as
    //ripple directions. Physically "wrong", but it gives the broken, far-reaching sun sparkle
    //and wavy texture chosen for the simulator - so it is kept ON deliberately.
    //   true  = photo ripples (chosen look)
    //   false = true normal map (smoother water, needs GLITTER_STRENGTH > 0 in Water_ps.glsl
    //           and CHOP_NORMAL_WEIGHT ~0.35 in the vertex shaders to look good)
    const bool USE_PHOTO_RIPPLES = true;
    if (disableShaders || USE_PHOTO_RIPPLES) {
        waterNode->setMaterialTexture(0, driver->getTexture("media/water.bmp"));
    }

    //The scenario's sea from the start: the loading screens already draw the water, and with the
    //grid's default (rough) sea the ship's first physics step would feel steep slopes.
    setSea(weather, 225.0f);
}

void Water::update(irr::f32 tideHeight, irr::core::vector3df viewPosition, irr::u32 lightLevel, irr::f32 weather, irr::f32 windDirection, irr::f32 rainIntensity)
{
    //Round these to nearest tileWidth
    irr::f32 xPos = tileWidth * Utilities::round(viewPosition.X / tileWidth);
    irr::f32 yPos = tideHeight;
    irr::f32 zPos = tileWidth * Utilities::round(viewPosition.Z / tileWidth);

    //std::cout << "xPos: " << xPos << " yPos: " << yPos << " zPos: " << zPos << std::endl;

    waterNode->setPosition(irr::core::vector3df(xPos, yPos, zPos));

    setSea(weather, windDirection);

    // KYARA METEO: how stormy the sea should LOOK (0..1), sent to the water shader as 'gloom'.
    //   Sea state: starts at weather 1.5, full at 4.0 (the "Mauvais temps" preset).
    //   Rain: full at 7/10 - under a rain sky the sea goes grey even when it is calm.
    // Whichever is stronger wins. Tune the four numbers here; the colours are in the shaders.
    irr::f32 seaGloom = (weather - 1.5f) / (4.0f - 1.5f);
    irr::f32 rainGloom = rainIntensity / 7.0f;
    irr::f32 gloom = (seaGloom > rainGloom) ? seaGloom : rainGloom;
    if (gloom < 0.0f) { gloom = 0.0f; }
    if (gloom > 1.0f) { gloom = 1.0f; }
    waterNode->setGloom(gloom);
}

void Water::setSea(irr::f32 weather, irr::f32 windDirection)
{
    //scale with weather
    //waterNode->setVerticalScale(sqrt(weather));
    // KYARA HOULE: the FFT wind vector used to be (k, k) - i.e. the chop always ran at 45 deg,
    // whatever the wind. It now follows the wind, so the chop and the swell travel the same way.
    // Same magnitude as before (|(k,k)| = k*sqrt(2)), so the chop height is unchanged.
    // Quantised to 10 deg: every change re-seeds the FFT spectrum, so we don't want that happening
    // on every step while the instructor drags the wind slider.
    // X is negated because MovingWater mirrors the FFT grid in X when it fills the mesh.
    const irr::f32 k = (weather + 0.25f) / 12.0f * 32.0f * 1.41421356f;
    const irr::f32 dirQ = 10.0f * Utilities::round(windDirection / 10.0f);
    const irr::f32 prop = (dirQ + 180.0f) * irr::core::DEGTORAD; // waves travel downwind
    waterNode->resetParameters((weather + 0.25) * 0.000025f, vector2(-k * sinf(prop), k * cosf(prop)), weather + 0.25);
}

void Water::setHullMask(irr::core::vector3df nodePosition, irr::core::vector3df nodeRotationDeg, irr::f32 zMin, irr::f32 zMax, irr::f32 centreX, const irr::f32* halfWidths24, const irr::f32* keels24, bool on)
{
    irr::core::matrix4 r;
    r.setRotationDegrees(nodeRotationDeg);
    //Her fore-and-aft axis on the water (the node's local Z, levelled)
    irr::f32 fx = r[8], fz = r[10];
    const irr::f32 len = sqrtf(fx * fx + fz * fz);
    if (len > 1e-6f) { fx /= len; fz /= len; } else { fx = 0.0f; fz = 1.0f; }
    const irr::f32 a[4] = { nodePosition.X, nodePosition.Z, fx, fz };
    const irr::f32 b[4] = { zMin, zMax, centreX, on ? 1.0f : 0.0f };
    //World height of a point in the node's frame: nodeY + r[1]*x + r[5]*y + r[9]*z
    const irr::f32 rowY[4] = { r[1], r[5], r[9], nodePosition.Y };
    waterNode->setHullMask(a, b, halfWidths24, keels24, rowY);
}

void Water::setSwellShaderData(const irr::f32* comp20, const irr::f32* fade4)
{
    waterNode->setSwellShaderData(comp20, fade4);
}

irr::f32 Water::getWaveHeight(irr::f32 relPosX, irr::f32 relPosZ) const
{
    return waterNode->getWaveHeight(relPosX, relPosZ);
}

irr::core::vector2df Water::getLocalNormals(irr::f32 relPosX, irr::f32 relPosZ) const
{
    return waterNode->getLocalNormals(relPosX, relPosZ);
}


irr::core::vector3df Water::getPosition() const
{
    return waterNode->getPosition();
}

void Water::setVisible(bool visible)
{
    waterNode->setVisible(visible);
}