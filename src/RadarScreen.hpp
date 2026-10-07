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

#ifndef __RADARSCREEN_HPP_INCLUDED__
#define __RADARSCREEN_HPP_INCLUDED__

#include "irrlicht.h"

     //kyara: the radar picture (a circle inscribed in a square bitmap) is rendered onto a plane that
     //fills its viewport. In practice the plane comes out very slightly TALLER than the visible
     //viewport, so the top and bottom of the scope were being sliced off flat (~11 px on a 500 px
     //radius). Rather than chase the exact camera/viewport rounding, we deliberately draw the picture
     //a few percent smaller than the viewport, leaving a thin surround-coloured margin all round so
     //the circle can never be clipped.
     //IMPORTANT: RadarScreen (texture scaling) and GUIMain (overlay/label geometry, reported pixel
     //radius) must use the SAME value, or the range rings and bearing labels drift off the picture.
static const irr::f32 RADAR_FIT_MARGIN = 1.045f;

class RadarScreen
{
public:
    RadarScreen();
    virtual ~RadarScreen();

    void load(irr::scene::ISceneManager* smgr, irr::scene::ISceneNode* parent, irr::core::vector3df offset, irr::f32 size, irr::f32 tilt);
    void setRadarDisplayRadius(irr::u32 radiusPx);
    void update(irr::video::IImage* radarImage);
    irr::scene::ISceneNode* getSceneNode() const;
    void setDisplayOffset(irr::f32 x, irr::f32 y);
    void rescale(irr::f32 factor); //the ship was resized live: keep the screen where it was on her

private:
    irr::video::IVideoDriver* driver;
    irr::scene::IMeshSceneNode* radarScreen;
    irr::scene::ISceneNode* parent;
    irr::core::vector3df offset;
    irr::u32 radarRadiusPx;
    irr::f32 tilt;
    irr::f32 displayOffsetX = 0, displayOffsetY = 0;
};

#endif