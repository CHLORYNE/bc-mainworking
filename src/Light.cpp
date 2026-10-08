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

#include "Light.hpp"
#include <cmath>
#include <iostream>
#include "Constants.hpp"

Light::Light()
{
    //ctor
}

Light::~Light()
{
    //dtor
}

void Light::load(irr::scene::ISceneManager* smgr, irr::f32 sunRise, irr::f32 sunSet, irr::scene::ISceneNode* parent)
{
    this->smgr = smgr;
    this->sunRise = sunRise;
    this->sunSet = sunSet;
    this->parent = parent;

    lightLevel = 0;
    warmth = 0.0f;
    dawn = false;

    ambientColor = irr::video::SColor(255, 64, 64, 64);
    smgr->setAmbientLight(ambientColor);

    //add a directional light
    directionalLight = smgr->addLightSceneNode();
    directionalLight->setLightType(irr::video::ELT_DIRECTIONAL);
    directionalLight->setRotation(irr::core::vector3df(30, 0, 0)); //Light from South, 30 deg above horizon

    //Set non-varying light data
    irr::video::SLight lightData = directionalLight->getLightData();
    lightData.AmbientColor = irr::video::SColor(255, 0, 0, 0);
    lightData.SpecularColor = irr::video::SColor(255, 0, 0, 0);
    lightData.Radius = 50000;
    directionalLight->setLightData(lightData);
}

irr::f32 Light::getWarmth() const
{
    return warmth;
}

bool Light::isDawn() const
{
    return dawn;
}

void Light::update(irr::f32 scenarioTime)
{
    //convert scenario time (in seconds) into hours
    irr::f32 hourTime = std::fmod(scenarioTime, SECONDS_IN_DAY) / SECONDS_IN_HOUR;

    //Light parameters
    irr::s32 lightLow = 50;
    irr::s32 lightHigh = 205;
    irr::s32 lightCos = 45;

    if (hourTime >= 0 && hourTime < (sunRise - 0.5)) { lightLevel = lightLow; }
    if (hourTime >= (sunRise - 0.5) && hourTime < (sunRise + 0.5)) { lightLevel = (lightHigh - lightLow) * (hourTime - (sunRise - 0.5)) + lightLow; }
    if (hourTime >= (sunRise + 0.5) && hourTime < (sunSet - 0.5)) { lightLevel = lightHigh; }
    if (hourTime >= (sunSet - 0.5) && hourTime < (sunSet + 0.5)) { lightLevel = (lightLow - lightHigh) * (hourTime - (sunSet - 0.5)) + lightHigh; }
    if (hourTime >= (sunSet + 0.5) && hourTime <= 24) { lightLevel = lightLow; }

    //Solar elevation: +1 at noon, 0 at sunrise/sunset, -1 at midnight
    irr::f32 solarElevation = cos((2 * PI / 24.0) * (hourTime - 12.0));

    //sinusoidal component
    lightLevel = (irr::s32)lightLevel + lightCos * solarElevation;
    //Cloud cover: up to a third of the daylight gone under a full overcast
    lightLevel = (irr::u32)((irr::f32)lightLevel * (1.0f - 0.33f * overcast));

    // --- KYARA: DAWN vs DUSK -------------------------------------------------------------
    //warmth is derived from |solarElevation|, so it is SYMMETRIC: it cannot tell 06:00 from
    //18:00, which is why dawn used to come out the same orange as sunset. Real dawn is
    //cooler - rose and violet under a still-blue sky, because the morning atmosphere carries
    //far less dust and haze than the afternoon's. So we flag which side of the day we're on
    //and pick a different hue for each.
    dawn = (hourTime < ((sunRise + sunSet) * 0.5f));

    const irr::f32 WARMTH_REACH = 0.45f;
    irr::f32 warm = 1.0f - (fabs(solarElevation) / WARMTH_REACH);
    if (warm < 0.0f) { warm = 0.0f; }
    if (warm > 1.0f) { warm = 1.0f; }

    warm *= (1.0f - overcast); //no low sun under a cloud deck
    warmth = warm; //normalised 0..1, handed to Sky for the horizon glow

    //Evening light is warm and strong; morning light is cool, blue-shifted and gentler.
    irr::f32 tintR, tintG, tintB, tintAmount;
    if (dawn) {
        tintR = 0.82f; tintG = 0.84f; tintB = 1.00f; //cool blue "blue hour"
        tintAmount = 0.40f;
    } else {
        tintR = 1.00f; tintG = 0.62f; tintB = 0.38f; //warm golden hour
        tintAmount = 0.45f;
    }

    warm *= tintAmount;

    irr::f32 r = (irr::f32)lightLevel * ((1.0f - warm) + tintR * warm);
    irr::f32 g = (irr::f32)lightLevel * ((1.0f - warm) + tintG * warm);
    irr::f32 b = (irr::f32)lightLevel * ((1.0f - warm) + tintB * warm);

    if (r > 255.0f) { r = 255.0f; }
    if (g > 255.0f) { g = 255.0f; }
    if (b > 255.0f) { b = 255.0f; }

    ambientColor = irr::video::SColor(255, (irr::u32)r, (irr::u32)g, (irr::u32)b);
    smgr->setAmbientLight(ambientColor);

    //Update the directional light
    irr::video::SLight lightData = directionalLight->getLightData();
    lightData.DiffuseColor = ambientColor;
    directionalLight->setLightData(lightData);
}

irr::video::SColor Light::getLightSColor() const
{
    return ambientColor;
}

irr::u32 Light::getLightLevel() const
{
    return lightLevel;
}
