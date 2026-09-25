/*   NAUTITECH - Simulateur de Navigation (Bridge Command fork)
     KYARA SLAM: the sheet of spray thrown up when the bow comes down on the water.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

     // Three particle systems, held in WORLD space (not parented to the ship), so the spray stays
     // where it was thrown instead of being dragged along by the hull: a sheet off each shoulder,
     // thrown outboard and aft the way water actually leaves a bow, plus a cloud of fine mist over
     // the stem. Each landing is a short burst whose size, speed and density come from how hard she
     // came down; the droplets then live out their own life under gravity while the ship moves on.
     //
     // The droplet texture is generated in code, so there is no asset to ship or to lose.

#ifndef __SPLASH_HPP_INCLUDED__
#define __SPLASH_HPP_INCLUDED__

#include "irrlicht.h"

class Splash
{
public:
    Splash();
    ~Splash();

    void load(irr::scene::ISceneManager* smgr, irr::IrrlichtDevice* dev);

    // Call every frame.
    void update(irr::f32 deltaTime);

    // position  : where the bow meets the water, in world coordinates
    // severity  : 1 = an ordinary landing, 2+ = she came down hard
    // bowSpeed  : downward speed of the bow, m/s
    // headingDeg: ship's heading, so the spray is thrown out along the hull
    // shipLength / shipBreadth: to size the sheet of spray to the vessel
    void trigger(irr::core::vector3df position, irr::f32 severity, irr::f32 bowSpeed,
        irr::f32 headingDeg, irr::f32 shipLength, irr::f32 shipBreadth);

    // The world gets re-centred periodically (SimulationModel "Normalise"): move with it.
    void moveNode(irr::f32 deltaX, irr::f32 deltaY, irr::f32 deltaZ);

private:
    irr::video::ITexture* makeStreakTexture(irr::video::IVideoDriver* driver);
    irr::video::ITexture* makeMistTexture(irr::video::IVideoDriver* driver);

    irr::scene::ISceneManager* smgr;
    //[0] port sheet, [1] starboard sheet, [2] fine mist over the stem. One system can hold only
    //one emitter, and the two sheets fly in opposite directions, so they cannot share one.
    irr::scene::IParticleSystemSceneNode* systems[3];
    irr::video::ITexture* streakTexture;
    irr::video::ITexture* mistTexture;
    irr::f32 burstRemaining; // s of emission left in the current splash
    bool loaded;
};

#endif