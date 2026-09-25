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

#include "NavLight.hpp"
#include "Angles.hpp"

#include <iostream>
#include <cmath> //For fmod(), exp(), pow()
#include <cstdlib> //For rand()

     //using namespace irr;

NavLight::NavLight(irr::scene::ISceneNode* parent, irr::scene::ISceneManager* smgr, irr::core::vector3df position, irr::video::SColor colour, irr::f32 lightStartAngle, irr::f32 lightEndAngle, irr::f32 lightRange, std::string lightSequence, irr::u32 phaseStart) {

    //Store the scene manager, so we can find the active camera
    this->smgr = smgr;
    // KYARA CHANGE NAV LIGHT SIZE 
    irr::f32 lightSize = 0.3;
    if (parent && parent->getScale().X > 0) {
        lightSize /= parent->getScale().X; //Assume scale in all directions is the same
    }

    // KYARA: core light - small, saturated, additive
    baseColour = colour;

    lightNode = smgr->addSphereSceneNode(lightSize, 16, parent, -1, position);
    smgr->getMeshManipulator()->setVertexColors(lightNode->getMesh(), colour);

    lightNode->setMaterialType(irr::video::EMT_TRANSPARENT_ADD_COLOR);
    lightNode->setMaterialFlag(irr::video::EMF_LIGHTING, false);
    // KYARA FOG FIX: fixed-function fog BLENDS the fragment towards the fog colour,
    // then this additive material ADDS the result to the scene. So a distant red
    // light in grey fog gets mixed towards grey and the grey is added on top -
    // which is exactly why the lights wash out to white.
    // Haze does not add its colour to a light, it absorbs the light. So fog is
    // disabled here and applied in update() as a scalar dimming of the RGB, which
    // preserves the hue: a red light in fog stays red, it just gets fainter.
    lightNode->setMaterialFlag(irr::video::EMF_FOG_ENABLE, false);
    lightNode->setMaterialFlag(irr::video::EMF_ZWRITE_ENABLE, false);

    // KYARA: halo - child node, so it inherits visibility + scale automatically
    glowNode = smgr->addSphereSceneNode(lightSize * 3.0f, 12, lightNode, -1, irr::core::vector3df(0, 0, 0));
    smgr->getMeshManipulator()->setVertexColors(glowNode->getMesh(),
        irr::video::SColor(255, colour.getRed() / 4, colour.getGreen() / 4, colour.getBlue() / 4));

    glowNode->setMaterialType(irr::video::EMT_TRANSPARENT_ADD_COLOR);
    glowNode->setMaterialFlag(irr::video::EMF_LIGHTING, false);
    glowNode->setMaterialFlag(irr::video::EMF_FOG_ENABLE, false); // KYARA FOG FIX: as above
    glowNode->setMaterialFlag(irr::video::EMF_ZWRITE_ENABLE, false);

    //Fix angles if start is negative
    while (lightStartAngle < 0) {
        lightStartAngle += 360;
        lightEndAngle += 360;
    }

    //store extra information
    startAngle = lightStartAngle;
    endAngle = lightEndAngle;
    range = lightRange;

    //initialise light sequence information
    charTime = 0.25; //where each character represents 0.25s of time
    sequence = lightSequence;
    if (phaseStart == 0) {
        timeOffset = 60.0 * ((irr::f32)std::rand() / RAND_MAX); //Random, 0-60s
    }
    else {
        timeOffset = (phaseStart - 1) * charTime;
    }

    //set initial alpha to implausible value
    currentAlpha = -1;
    enabled = true; // KYARA FEUX
}

NavLight::~NavLight() {
    //TODO: Understand why NavLights are being created and destroyed during model set-up
}

irr::core::vector3df NavLight::getPosition() const
{
    lightNode->updateAbsolutePosition();//ToDo: This may be needed, but seems odd that it's required
    return lightNode->getAbsolutePosition();
}

void NavLight::setPosition(irr::core::vector3df position)
{
    lightNode->setPosition(position);
}

//KYARA FEUX
void NavLight::setEnabled(bool e) {
    enabled = e;
    if (!enabled && lightNode) { lightNode->setVisible(false); }
}

bool NavLight::isEnabled() const {
    return enabled;
}

void NavLight::setColour(irr::video::SColor colour) {
    baseColour = colour;
    currentAlpha = -1; //force the vertex colours to be rewritten on the next update
}

void NavLight::update(irr::f32 scenarioTime, irr::u32 lightLevel) {

    //KYARA FEUX: a lamp that is switched off is simply not there
    if (!enabled) {
        if (lightNode) { lightNode->setVisible(false); }
        return;
    }

    //find light position
    lightNode->updateAbsolutePosition();
    irr::core::vector3df lightPosition = lightNode->getAbsolutePosition();

    //Find the active camera
    irr::scene::ICameraSceneNode* camera = smgr->getActiveCamera();
    if (camera == 0) {
        return;
    }
    camera->updateAbsolutePosition();
    irr::core::vector3df viewPosition = camera->getAbsolutePosition();

    //find the HFOV
    irr::f32 hFOV = 2 * atan(tan(camera->getFOV() / 2) * camera->getAspectRatio());
    irr::f32 zoom = hFOV / (irr::core::PI / 2.0);

    irr::f32 lightDistance = lightPosition.getDistanceFrom(viewPosition);

    // KYARA: perspective-correct light sizing.
    // scale grows as d^SIZE_POWER (<1), so on-screen size shrinks as d^(SIZE_POWER-1).
    const irr::f32 REF_DIST = 200.0f;  // metres
    const irr::f32 SIZE_AT_REF = 0.9f;    // overall size knob
    const irr::f32 SIZE_POWER = 0.62f;   // 1.0 = old behaviour, 0.5 = strong shrink with range

    irr::f32 dClamped = irr::core::max_(lightDistance, 5.0f);
    irr::f32 zoomFactor = std::pow(zoom, 0.25f);
    irr::f32 s = SIZE_AT_REF * std::pow(dClamped / REF_DIST, SIZE_POWER) * REF_DIST * 0.0035f * zoomFactor;

    lightNode->setScale(irr::core::vector3df(s, s, s));

    //set light visibility depending on range
    if (lightDistance > range) {
        lightNode->setVisible(false);
    }
    else {
        lightNode->setVisible(true);
    }

    //set light visibility depending on angle
    irr::f32 relativeAngleDeg = (viewPosition - lightPosition).getHorizontalAngle().Y;
    irr::f32 parentAngleDeg = lightNode->getParent()->getRotation().Y;
    irr::f32 localRelativeAngleDeg = relativeAngleDeg - parentAngleDeg;
    if (!Angles::isAngleBetween(localRelativeAngleDeg, startAngle, endAngle)) {
        lightNode->setVisible(false);
    }

    //set light visibility depending on light sequence
    std::string::size_type sequenceLength = sequence.length();
    if (sequenceLength > 0) {
        irr::f32 timeInSequence = std::fmod(((scenarioTime + timeOffset) / charTime), sequenceLength);
        irr::u32 positionInSequence = timeInSequence;
        if (positionInSequence >= sequenceLength) { positionInSequence = sequenceLength - 1; }
        if (sequence[positionInSequence] == 'D' || sequence[positionInSequence] == 'd') {
            lightNode->setVisible(false);
        }
    }

    // ================= KYARA: fog / haze =================
    // Tunables
    const irr::f32 FOG_SOFTNESS = 0.5f; // <1 = light survives further into the fog (0.5 = very persistent)
    const irr::f32 FOG_FLOOR = 0.04f; // never quite reach zero - a light in fog stays a faint smudge
    const irr::f32 HAZE_BLOOM = 2.2f;  // how much the halo swells in thick fog (0 = no swell)
    const irr::f32 HAZE_LIFT = 0.45f; // how much brighter the halo gets relative to the core, in fog

    // Read the scene's actual fog settings, so the lights match the rest of the world
    irr::video::SColor fogColour;
    irr::video::E_FOG_TYPE fogType = irr::video::EFT_FOG_LINEAR;
    irr::f32 fogStart = 0.0f, fogEnd = 0.0f, fogDensity = 0.0f;
    bool pixelFog = false, rangeFog = false;
    smgr->getVideoDriver()->getFog(fogColour, fogType, fogStart, fogEnd, fogDensity, pixelFog, rangeFog);

    // fogTransmission = 1.0 in clear air, 0.0 when fully obscured
    irr::f32 fogTransmission = 1.0f;
    if (fogType == irr::video::EFT_FOG_LINEAR) {
        if (fogEnd > fogStart) {
            fogTransmission = (fogEnd - lightDistance) / (fogEnd - fogStart);
        }
    }
    else if (fogType == irr::video::EFT_FOG_EXP) {
        fogTransmission = std::exp(-fogDensity * lightDistance);
    }
    else { // EFT_FOG_EXP2
        irr::f32 fd = fogDensity * lightDistance;
        fogTransmission = std::exp(-(fd * fd));
    }
    if (fogTransmission < 0.0f) { fogTransmission = 0.0f; }
    if (fogTransmission > 1.0f) { fogTransmission = 1.0f; }

    // Soften the falloff - raw fog kills small lights far too abruptly
    irr::f32 bFog = std::pow(fogTransmission, FOG_SOFTNESS);
    bFog = FOG_FLOOR + (1.0f - FOG_FLOOR) * bFog;

    // Haze bloom: as the fog thickens, the halo grows and takes a larger share of
    // the light's energy. That scattering is what makes a light look "hazy" rather
    // than merely dim - and because it is all scalar, the colour never shifts.
    irr::f32 haze = 1.0f - fogTransmission;                 // 0 = clear, 1 = socked in
    irr::f32 glowScale = 1.0f + HAZE_BLOOM * haze;
    glowNode->setScale(irr::core::vector3df(glowScale, glowScale, glowScale));
    // =====================================================

    // KYARA: additive lights ignore alpha, so modulate RGB intensity instead.

   // KYARA: floor 0.15 -> 0.5 so lights stay punchy in daylight, not crushed to ~17%
    irr::f32 bAmbient = 0.8f + 0.8f * ((irr::f32)(255 - lightLevel) / 255.0f);

    // range term - fades as the light approaches its nominal range
    irr::f32 bRange = 1.0f;
    if (range > 0.0f) {
        irr::f32 rangeFrac = lightDistance / range;
        if (rangeFrac > 1.0f) { rangeFrac = 1.0f; }
        bRange = 1.0f - (rangeFrac * rangeFrac);
        bRange = 0.25f + 0.75f * bRange;   // floor: still faintly visible at the horizon
    }

    irr::f32 b = bAmbient * bRange * bFog;   // KYARA: fog folded in as pure dimming

    // Halo share of the light: 0.25 in clear air, rising towards 0.70 in thick fog
    irr::f32 haloShare = 0.25f + HAZE_LIFT * haze;

    // Quantise into 32 buckets - setVertexColors is slow, so only touch the mesh on change
    irr::u16 bQuant = (irr::u16)(b * 32.0f);
    if (bQuant != currentAlpha) {

        irr::f32 bq = (irr::f32)bQuant / 32.0f;

        irr::video::SColor core(255,
            (irr::u32)(baseColour.getRed() * bq),
            (irr::u32)(baseColour.getGreen() * bq),
            (irr::u32)(baseColour.getBlue() * bq));

        // Halo is dimmer than the core, but always the SAME hue - never blended
        // towards the fog colour, which is what used to grey the lights out.
        irr::video::SColor halo(255,
            (irr::u32)(baseColour.getRed() * bq * haloShare),
            (irr::u32)(baseColour.getGreen() * bq * haloShare),
            (irr::u32)(baseColour.getBlue() * bq * haloShare));

        smgr->getMeshManipulator()->setVertexColors(lightNode->getMesh(), core);
        smgr->getMeshManipulator()->setVertexColors(glowNode->getMesh(), halo);
        currentAlpha = bQuant;
    }
}

void NavLight::moveNode(irr::f32 deltaX, irr::f32 deltaY, irr::f32 deltaZ)
{
    irr::core::vector3df currentPos = lightNode->getPosition();
    irr::f32 newPosX = currentPos.X + deltaX;
    irr::f32 newPosY = currentPos.Y + deltaY;
    irr::f32 newPosZ = currentPos.Z + deltaZ;

    lightNode->setPosition(irr::core::vector3df(newPosX, newPosY, newPosZ));
}