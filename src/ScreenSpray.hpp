/*   NAUTITECH - Simulateur de Navigation (Bridge Command fork)
     KYARA SLAM: water thrown onto the wheelhouse glass.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

     // Yes, this really happens: on a small craft driven into a head sea, the sheet thrown up by the
     // bow comes back over the wheelhouse and the forward windows stream with water.
     //
     // HOW IT IS DRAWN, and why it is not a screen overlay
     // The first version drew droplets in 2D over the whole picture, which put water on the deckhead,
     // the console and the radar as well as the windows. Instead, the drops now live on a PANE: a flat
     // quad placed just inside the windscreen, as a child of the ship's scene node. That means
     //   * the wheelhouse structure in front of it hides the drops through the normal depth test, so
     //     water appears only where there is actually an opening,
     //   * the drops stay put on the glass when you look around, pitch, roll or zoom,
     //   * they are correctly sized and perspective-correct, because they are real geometry.
     // The pane's position and size come from the vessel's boat.ini (WindscreenX/Y/Z, WindscreenWidth,
     // WindscreenHeight, WindscreenTilt), in the same model units as RadarScreenX/Y/Z.

#ifndef __SCREENSPRAY_HPP_INCLUDED__
#define __SCREENSPRAY_HPP_INCLUDED__

#include "irrlicht.h"
#include <vector>

class SprayPaneSceneNode;

class ScreenSpray
{
public:
    ScreenSpray();

    // parent: the own ship's scene node. position/width/height/tiltDeg are in the ship model's
    // own units, exactly like RadarScreenX/Y/Z and RadarScreenSize.
    void load(irr::scene::ISceneManager* smgr, irr::IrrlichtDevice* dev, irr::scene::ISceneNode* parent,
        irr::core::vector3df position, irr::f32 width, irr::f32 height, irr::f32 tiltDeg);

    // bc5.ini screen_spray: 0 = off, 1 = normal, 2 = test (water after every landing, however
    // gentle - to check the effect without having to find a head sea first).
    void setMode(int mode);
    int getMode() const;

    // severity: same figure as the slam sound.
    void trigger(irr::f32 severity);
    void update(irr::f32 deltaTime);

    void clear();
    bool isWet() const;

private:
    struct Drop
    {
        irr::f32 x, y;        // 0..1 across the pane, 0 = top
        irr::f32 size;        // as a fraction of the pane height
        irr::f32 alpha;       // 0..1
        irr::f32 speed;       // pane fractions per second, downwards
        irr::f32 drift;
        irr::f32 age, life;   // seconds
        bool running;         // drawn with the tail it left behind
    };

    irr::f32 randf();

    SprayPaneSceneNode* pane;
    std::vector<Drop> drops;
    int mode;              // 0 off, 1 normal, 2 test
    irr::f32 minSeverity;
    bool loaded;
    irr::u32 seed;
};

#endif