/* FIRE FEATURE: the water cannon carried by the fireboat. */
#ifndef __FIREMONITOR_HPP_INCLUDED__
#define __FIREMONITOR_HPP_INCLUDED__

#include "irrlicht.h"

class FireMonitor
{
public:
    FireMonitor();
    ~FireMonitor();

    void mount(irr::scene::ISceneManager* smgr,
        irr::video::IVideoDriver* driver,
        irr::scene::ISceneNode* fireBoatNode,
        irr::core::vector3df nozzleOffset);

    void setFiring(bool firing);
    bool isFiring() const;
    void setAimPoint(irr::core::vector3df worldAim);
    void update(irr::f32 deltaTime);

    irr::core::vector3df getNozzleWorldPos() const;
    irr::core::vector3df getAimPoint() const;
    bool isMounted() const;

private:
    irr::scene::IParticleSystemSceneNode* waterPS;
    irr::scene::IParticleEmitter* waterEmitter;
    // FIRE FEATURE: soft spray/mist cloud at the point of impact, so the jet reads as water
    // breaking up on the target rather than a stream that just stops in mid-air.
    irr::scene::IParticleSystemSceneNode* mistPS;
    irr::scene::IParticleEmitter* mistEmitter;
    irr::scene::ISceneNode* nozzleNode;    // anchor on the boat = base of the pedestal
    // FIRE FEATURE: pro-look monitor, built from primitives (cylinders/sphere/cone)
    irr::scene::ISceneNode* monitorBase;   // vertical pedestal (cylinder)
    irr::scene::ISceneNode* monitorSwivel; // swivel ball (sphere)
    irr::scene::ISceneNode* barrelPivot;   // holds barrel + nozzle, rotates to aim
    irr::f32 barrelLen;
    irr::f32 pedestalH;
    bool firing, mounted;
    irr::core::vector3df aimPoint;
    irr::u32 baseMinPPS, baseMaxPPS;
    irr::f32 jetSpeedScale;
};

#endif