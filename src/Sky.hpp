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

#ifndef __SKY_HPP_INCLUDED__
#define __SKY_HPP_INCLUDED__

#include "irrlicht.h"

class Sky
{
public:
    Sky();
    virtual ~Sky();
    void load(irr::scene::ISceneManager* smgr);
    void update(irr::u32 lightLevel, irr::f32 warmth, bool isDawn,
        irr::f32 visibilityRangeNm, irr::video::SColor fogColour,
        bool stormMode = false);   // KYARA: swap to the overcast dome

    // NAUTITECH HERO BOLT: one big descending bolt in the 3D sky (occluded by terrain,
    // never over the radar or inside the wheelhouse). triggerHeroBolt sets where/how big;
    // updateHeroBolt advances the top-down reveal + fade each frame.
    void triggerHeroBolt(irr::core::vector3df worldTop, irr::f32 fullHeight, irr::f32 width);
    void updateHeroBolt(irr::f32 deltaTime);

    // NAUTITECH TENDER LIGHTNING: several lightning pictures parked at fixed positions around
    // the sky, flashed one at a time on a timer. Same additive-billboard idea as the hero bolt,
    // so they're occluded by terrain, never hit the radar, and never draw inside the wheelhouse.
    static const int NUM_TENDERS = 6;                 // number of tender pictures (media/tender1..N.png)
    void triggerTender(int index, irr::core::vector3df cameraPos); // flash tender[index] at its home
    void updateTenders(irr::f32 deltaTime);           // advance flash/fade for all tenders
    int getNumTenders() const { return NUM_TENDERS; }

private:
    irr::scene::ISceneNode* dayDome;   // Solid, lit - the base sky (as stock Bridge Command)
    irr::scene::ISceneNode* glowDome;  // Additive horizon glow for sunrise/sunset
    irr::scene::ISceneNode* hazeDome;  // Additive haze, driven by the visibility range
    irr::video::ITexture* skyTexFair;    // partly-cloudy
    irr::video::ITexture* skyTexStorm;   // overcast
    bool skyIsStorm;                     // current state, so we only swap on change

    // NAUTITECH hero bolt
    irr::scene::IBillboardSceneNode* heroBolt;
    irr::video::ITexture* heroBoltTex;
    bool heroBoltActive;
    irr::f32 heroBoltAge;         // seconds since triggered
    irr::f32 heroBoltFullHeight;  // world units
    irr::f32 heroBoltWidth;       // world units
    irr::core::vector3df heroBoltTop; // fixed top anchor while it descends

    // NAUTITECH tender lightning (fixed sky positions, flashed in turn)
    irr::scene::IBillboardSceneNode* tender[NUM_TENDERS];
    irr::video::ITexture* tenderTex[NUM_TENDERS];
    bool  tenderActive[NUM_TENDERS];
    irr::f32 tenderAge[NUM_TENDERS];       // seconds since this tender was fired
    irr::f32 tenderBearingDeg[NUM_TENDERS]; // fixed bearing around the ship (deg)
};

#endif // __SKY_HPP_INCLUDED__
