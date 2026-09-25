/*   NAUTITECH - Simulateur de Navigation (Bridge Command fork)
     KYARA SLAM: spray thrown up when the bow lands. See Splash.hpp.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#include "Splash.hpp"

#include <cmath>

namespace
{
    // ---------------------------------------------------------------------------------------
    // UNITS - read this before changing any speed here.
    // Irrlicht advances particles with  pos += vector * timeDiff  where timeDiff is in
    // MILLISECONDS. So an emitter direction of (0, 0.004, 0) is 4 m/s upward, not 0.004 m/s.
    // Everything below is written in m/s and divided by 1000 at the point of use. The gravity
    // affector works the same way: its vector is the terminal velocity the particle is pulled
    // towards, again per millisecond.
    // ---------------------------------------------------------------------------------------

    // TUNABLES
    const irr::f32 BURST_BASE = 0.12f;        // s of emission for an ordinary landing
    const irr::f32 BURST_PER_SEVERITY = 0.08f;
    const irr::f32 BURST_MAX = 0.40f;

    const irr::u32 SHEET_PPS_BASE = 900;      // per side, at severity 1
    const irr::u32 SHEET_PPS_MAX = 5000;
    const irr::u32 MIST_PPS_BASE = 700;
    const irr::u32 MIST_PPS_MAX = 4000;

    const irr::f32 SHEET_SPEED = 5.5f;        // m/s of throw at severity 1
    const irr::f32 SHEET_SPEED_MAX = 16.0f;
    const irr::f32 MIST_SPEED = 2.2f;         // m/s

    const irr::f32 FALL_SPEED = 4.5f;         // m/s the droplets settle to
    const irr::u32 FALL_TIME = 700;           // ms to reach it
    const irr::f32 MIST_FALL_SPEED = 1.1f;    // fine mist hangs in the air far longer
    const irr::u32 MIST_FALL_TIME = 1400;

    const irr::f32 DROP_SIZE = 0.030f;        // sheet droplet size as a fraction of the beam
    const irr::f32 MIST_SIZE = 0.016f;
    const irr::f32 STREAK = 3.2f;             // droplets are drawn this many times taller than wide

    const irr::u32 SHEET_LIFE_MIN = 450, SHEET_LIFE_MAX = 1100; // ms
    const irr::u32 MIST_LIFE_MIN = 900, MIST_LIFE_MAX = 2200;

    const irr::f32 MIN_SEVERITY = 0.5f;       // below this a landing throws no visible spray

    const irr::u32 TEX_W = 16, TEX_H = 48;    // streak texture
    const irr::u32 MIST_TEX = 32;             // round soft texture
}

Splash::Splash()
    : smgr(0), streakTexture(0), mistTexture(0), burstRemaining(0), loaded(false)
{
    for (int i = 0; i < 3; i++) { systems[i] = 0; }
}

Splash::~Splash()
{
}

//A soft elongated droplet. Real spray is not round: it is torn into streaks by its own speed, and
//that is most of what makes a particle sheet read as water rather than as bubbles.
irr::video::ITexture* Splash::makeStreakTexture(irr::video::IVideoDriver* driver)
{
    irr::video::IImage* image = driver->createImage(irr::video::ECF_A8R8G8B8,
        irr::core::dimension2d<irr::u32>(TEX_W, TEX_H));
    if (!image) { return 0; }
    const irr::f32 cx = (irr::f32)TEX_W * 0.5f;
    const irr::f32 cy = (irr::f32)TEX_H * 0.5f;
    for (irr::u32 y = 0; y < TEX_H; y++) {
        for (irr::u32 x = 0; x < TEX_W; x++) {
            const irr::f32 dx = ((irr::f32)x + 0.5f - cx) / cx;
            const irr::f32 dy = ((irr::f32)y + 0.5f - cy) / cy;
            //Teardrop: fat at the bottom, drawn out towards the top
            const irr::f32 taper = 0.35f + 0.65f * (0.5f * (dy + 1.0f));
            irr::f32 r = sqrtf((dx / taper) * (dx / taper) + dy * dy);
            if (r > 1.0f) { r = 1.0f; }
            irr::f32 a = (1.0f - r);
            a = a * a * (1.6f - 0.6f * a);          // soft edge, denser core
            if (a > 1.0f) { a = 1.0f; }
            image->setPixel(x, y, irr::video::SColor((irr::u32)(a * 235.0f), 255, 255, 255));
        }
    }
    irr::video::ITexture* t = driver->addTexture("nautitech_splash_streak", image);
    image->drop();
    return t;
}

//Fine mist: a plain soft dot, but small and used in large numbers.
irr::video::ITexture* Splash::makeMistTexture(irr::video::IVideoDriver* driver)
{
    irr::video::IImage* image = driver->createImage(irr::video::ECF_A8R8G8B8,
        irr::core::dimension2d<irr::u32>(MIST_TEX, MIST_TEX));
    if (!image) { return 0; }
    const irr::f32 c = (irr::f32)MIST_TEX * 0.5f;
    for (irr::u32 y = 0; y < MIST_TEX; y++) {
        for (irr::u32 x = 0; x < MIST_TEX; x++) {
            const irr::f32 dx = ((irr::f32)x + 0.5f - c) / c;
            const irr::f32 dy = ((irr::f32)y + 0.5f - c) / c;
            irr::f32 r = sqrtf(dx * dx + dy * dy);
            if (r > 1.0f) { r = 1.0f; }
            const irr::f32 a = (1.0f - r) * (1.0f - r) * 0.75f;
            image->setPixel(x, y, irr::video::SColor((irr::u32)(a * 255.0f), 255, 255, 255));
        }
    }
    irr::video::ITexture* t = driver->addTexture("nautitech_splash_mist", image);
    image->drop();
    return t;
}

void Splash::load(irr::scene::ISceneManager* sceneManager, irr::IrrlichtDevice* dev)
{
    smgr = sceneManager;
    if (!smgr || !dev) { return; }

    irr::video::IVideoDriver* driver = dev->getVideoDriver();
    streakTexture = makeStreakTexture(driver);
    mistTexture = makeMistTexture(driver);

    //Three systems: a sheet thrown out each side of the bow, and a cloud of fine mist over the
    //stem. One system holds one emitter, and the two sheets fly in opposite directions, so they
    //cannot share.
    for (int i = 0; i < 3; i++) {
        systems[i] = smgr->addParticleSystemSceneNode(false);
        if (!systems[i]) { continue; }
        systems[i]->setEmitter(0); // idle until the first landing
        systems[i]->setMaterialFlag(irr::video::EMF_LIGHTING, false);
        systems[i]->setMaterialFlag(irr::video::EMF_ZWRITE_ENABLE, false);
        systems[i]->setMaterialFlag(irr::video::EMF_FOG_ENABLE, true);
        systems[i]->setMaterialTexture(0, (i == 2) ? mistTexture : streakTexture);
        systems[i]->setMaterialType(irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL);

        const bool mist = (i == 2);
        irr::scene::IParticleAffector* gravity = systems[i]->createGravityAffector(
            irr::core::vector3df(0.0f, (mist ? -MIST_FALL_SPEED : -FALL_SPEED) * 0.001f, 0.0f),
            mist ? MIST_FALL_TIME : FALL_TIME);
        if (gravity) { systems[i]->addAffector(gravity); gravity->drop(); }

        irr::scene::IParticleAffector* fade = systems[i]->createFadeOutParticleAffector(
            irr::video::SColor(0, 255, 255, 255), mist ? 900 : 500);
        if (fade) { systems[i]->addAffector(fade); fade->drop(); }
    }

    loaded = true;
}

void Splash::trigger(irr::core::vector3df position, irr::f32 severity, irr::f32 bowSpeed,
    irr::f32 headingDeg, irr::f32 shipLength, irr::f32 shipBreadth)
{
    if (!loaded) { return; }
    if (severity < MIN_SEVERITY) { return; }

    if (severity > 4.0f) { severity = 4.0f; }
    if (shipBreadth < 1.0f) { shipBreadth = 1.0f; }
    if (shipLength < 2.0f) { shipLength = 2.0f; }

    const irr::f32 hr = headingDeg * irr::core::DEGTORAD;
    const irr::f32 sh = sinf(hr), ch = cosf(hr);
    //Ship axes: ahead = (sin h, cos h), starboard = (cos h, -sin h)
    const irr::core::vector3df ahead(sh, 0.0f, ch);
    const irr::core::vector3df stbd(ch, 0.0f, -sh);
    const irr::core::vector3df up(0.0f, 1.0f, 0.0f);

    irr::f32 speed = SHEET_SPEED * severity + 0.35f * bowSpeed;
    if (speed > SHEET_SPEED_MAX) { speed = SHEET_SPEED_MAX; }

    const irr::f32 dropSize = DROP_SIZE * shipBreadth * (0.8f + 0.2f * severity);
    const irr::f32 mistSize = MIST_SIZE * shipBreadth;

    irr::u32 sheetPPS = (irr::u32)(SHEET_PPS_BASE * severity * severity);
    if (sheetPPS > SHEET_PPS_MAX) { sheetPPS = SHEET_PPS_MAX; }
    irr::u32 mistPPS = (irr::u32)(MIST_PPS_BASE * severity * severity);
    if (mistPPS > MIST_PPS_MAX) { mistPPS = MIST_PPS_MAX; }

    //White water is not paint: slightly blue-grey at its dullest, never fully opaque.
    const irr::video::SColor darkest(200, 205, 218, 226);
    const irr::video::SColor brightest(255, 255, 255, 255);

    for (int i = 0; i < 3; i++) {
        if (!systems[i]) { continue; }
        const bool mist = (i == 2);
        const irr::f32 side = (i == 0) ? -1.0f : 1.0f;   // 0 = port sheet, 1 = starboard sheet

        //Where it comes from: each sheet off its own shoulder, the mist over the stem.
        irr::core::vector3df origin = position;
        if (!mist) { origin += stbd * (side * 0.42f * shipBreadth); }

        systems[i]->setPosition(origin);

        //A flat box along the hull, so the spray leaves as a sheet and not from a single point.
        const irr::f32 alongExt = mist ? 0.05f * shipLength : 0.045f * shipLength;
        const irr::f32 acrossExt = mist ? 0.45f * shipBreadth : 0.16f * shipBreadth;
        const irr::f32 ex = fabsf(acrossExt * ch) + fabsf(alongExt * sh);
        const irr::f32 ez = fabsf(acrossExt * sh) + fabsf(alongExt * ch);
        const irr::core::aabbox3df box(-ex, -0.15f, -ez, ex, 0.15f, ez);

        //Direction: the sheets go outboard, up, and a little aft (she is running into them);
        //the mist just lifts.
        irr::core::vector3df dir;
        if (mist) {
            dir = up * MIST_SPEED;
        }
        else {
            dir = (stbd * (side * 0.80f) + up * 0.62f - ahead * 0.30f);
            dir.normalize();
            dir *= speed;
        }
        dir *= 0.001f; // m/s -> Irrlicht's per-millisecond units

        const irr::f32 size = mist ? mistSize : dropSize;
        //Streaked, not round: tall and narrow, like a droplet torn by its own speed.
        const irr::f32 wMin = size * 0.55f, wMax = size * 1.25f;
        const irr::f32 hMin = mist ? wMin : wMin * STREAK;
        const irr::f32 hMax = mist ? wMax : wMax * STREAK;

        irr::scene::IParticleEmitter* emitter = systems[i]->createBoxEmitter(
            box,
            dir,
            (mist ? mistPPS : sheetPPS) / 2, (mist ? mistPPS : sheetPPS),
            darkest, brightest,
            mist ? MIST_LIFE_MIN : SHEET_LIFE_MIN,
            mist ? MIST_LIFE_MAX : SHEET_LIFE_MAX,
            mist ? 45 : (18 + (irr::s32)(6.0f * severity)),   // cone spread, degrees
            irr::core::dimension2df(wMin, hMin),
            irr::core::dimension2df(wMax, hMax));

        if (emitter) {
            systems[i]->setEmitter(emitter);
            emitter->drop();
        }
    }

    irr::f32 burst = BURST_BASE + BURST_PER_SEVERITY * severity;
    if (burst > BURST_MAX) { burst = BURST_MAX; }
    burstRemaining = burst;
}

void Splash::update(irr::f32 deltaTime)
{
    if (!loaded) { return; }

    if (burstRemaining > 0.0f) {
        burstRemaining -= deltaTime;
        if (burstRemaining <= 0.0f) {
            //Stop emitting, but leave what is already in the air to fall and fade.
            for (int i = 0; i < 3; i++) {
                if (systems[i]) { systems[i]->setEmitter(0); }
            }
            burstRemaining = 0.0f;
        }
    }
}

void Splash::moveNode(irr::f32 deltaX, irr::f32 deltaY, irr::f32 deltaZ)
{
    for (int i = 0; i < 3; i++) {
        if (!systems[i]) { continue; }
        const irr::core::vector3df p = systems[i]->getPosition();
        systems[i]->setPosition(irr::core::vector3df(p.X + deltaX, p.Y + deltaY, p.Z + deltaZ));
    }
}