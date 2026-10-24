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

#ifndef __LIGHT_HPP_INCLUDED__
#define __LIGHT_HPP_INCLUDED__

#include "irrlicht.h"

class Light
{
    public:
        Light();
        virtual ~Light();
        void load(irr::scene::ISceneManager* smgr, irr::f32 sunRise, irr::f32 sunSet, irr::scene::ISceneNode* parent);
        void update(irr::f32 scenarioTime);
        irr::video::SColor getLightSColor() const;
        irr::u32 getLightLevel() const;
        irr::f32 getWarmth() const; //0..1, peaks when the sun is on the horizon
        bool isDawn() const;        //true in the morning half of the day, false in the evening
        //Cloud cover 0..1: an overcast day is dimmer and has no warm low sun
        void setOvercast(irr::f32 cover) { overcast = cover < 0 ? 0 : (cover > 1 ? 1 : cover); }

    private:
        irr::u32 lightLevel;
        irr::video::SColor ambientColor;
        irr::scene::ISceneManager* smgr;
        irr::f32 sunRise;
        irr::f32 sunSet;
        irr::scene::ISceneNode* parent;
        irr::scene::ILightSceneNode* directionalLight;
        irr::f32 warmth;
        bool dawn;
        irr::f32 overcast = 0;
};

#endif // __LIGHT_HPP_INCLUDED__
