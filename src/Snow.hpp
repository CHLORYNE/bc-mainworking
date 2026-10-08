/*   NAUTITECH - Simulateur de Navigation
     Falling snow round the bridge.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#ifndef __SNOW_HPP_INCLUDED__
#define __SNOW_HPP_INCLUDED__

#include "irrlicht.h"

//Made like the rain (Rain.cpp): three shells round the camera, near, middle and far, each
//showing a picture of flakes that scrolls down slowly and drifts with the wind. The pictures are
//drawn here (soft round flakes, more of them the heavier the snow), so no media file is needed.
class Snow
{
public:
    Snow();
    void load(irr::scene::ISceneManager* smgr);
    //intensity 0..1, wind in knots, the direction it blows FROM in degrees, scenario time in seconds
    void update(irr::f32 intensity, irr::f32 windSpeedKts, irr::f32 windDirDeg, irr::f32 scenarioTime);

private:
    static const int LEVELS = 4;                //flake pictures, light to heavy
    irr::scene::ISceneManager* smgr;
    irr::scene::ISceneNode* layer[3];           //near, middle, far
    irr::video::ITexture* picture[LEVELS];
    int shownLevel;
};

#endif
