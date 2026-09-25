/* FIRE FEATURE: a burning-ship effect with an intensity that drains under water.
   S1 realism: wind-leaned smoke, steam-on-knockdown, flickering night glow. */
#ifndef __FIRE_HPP_INCLUDED__
#define __FIRE_HPP_INCLUDED__

#include "irrlicht.h"

class Fire
{
public:
    Fire();
    ~Fire();

    // Attach fire + smoke particle systems to 'parent' at a LOCAL offset (so they ride
    // with the ship). 'radius' is the logical hit-test radius, in world units.
    void ignite(irr::scene::ISceneManager* smgr,
        irr::video::IVideoDriver* driver,
        irr::scene::ISceneNode* parent,
        irr::core::vector3df localOffset,
        irr::f32 radius,
        irr::f32 hullHalfLen = 0.0f,      // FIRE (escalation): full-spread half-extents, world m,
        irr::f32 hullHalfBreadth = 0.0f); // hull-aligned. 0 => derive from radius.
    void     setEscalation(irr::f32 e01); // 0 = fresh central seat, 1 = whole-deck inferno
    irr::f32 getEscalation() const;
    // FIRE FEATURE: windDirectionDeg is the direction the wind blows FROM (model
    // convention, 0..360); windSpeedKts is model->getWindSpeed(). Both drive the plume lean.
    void update(irr::f32 deltaTime, irr::f32 windDirectionDeg, irr::f32 windSpeedKts);
    void applyWater(irr::f32 deltaTime);  // called on frames the stream is hitting

    bool isActive() const;                // intensity > 0
    bool isExtinguished() const;          // intensity == 0
    irr::core::vector3df getWorldPosition() const;
    irr::f32 getRadius() const;
    void remove();

private:
    irr::core::vector3df baseOffset;   // FIRE: ignite offset, so smoke can ride the flame top
    irr::scene::IParticleSystemSceneNode* firePS;
    irr::scene::IParticleSystemSceneNode* smokePS;
    irr::scene::IParticleEmitter* fireEmitter;
    irr::scene::IParticleEmitter* smokeEmitter;

    // FIRE FEATURE: horizontal wind push on the smoke, re-set every frame from live wind.
    irr::scene::IParticleGravityAffector* smokeWindAffector;
    // FIRE FEATURE: dynamic point light so the casualty glows on the sea/hulls at night.
    irr::scene::ILightSceneNode* fireLight;
    irr::f32 lightFlickerPhase;

    // FIRE FEATURE: smoky (normal) vs steam (while being hosed) start colours.
    irr::video::SColor smokeColMin, smokeColMax;
    irr::video::SColor steamColMin, steamColMax;

    irr::f32 intensity;        // 1.0 = full blaze, 0.0 = out
    irr::f32 extinguishRate;   // intensity lost per second while hit  (TUNE)
    irr::f32 reigniteRate;     // intensity regained/sec when NOT hit  (0 = never reignites)
    bool     beingHitThisFrame;
    irr::scene::IParticleBoxEmitter* fireBox;   // typed view of fireEmitter, to grow the seat
    irr::scene::IParticleBoxEmitter* smokeBox;
    irr::f32 escalation;                        // 0..1 spread across the hull
    irr::f32 vis;                               // stored visual scale
    irr::f32 seatHalfX, seatHalfZ;              // central seat half-extents (world m)
    irr::f32 hullHalfX, hullHalfZ;              // whole-hull half-extents (world m)
    irr::u32 fireBaseMinPPS, fireBaseMaxPPS;   // full-intensity emission rates
    irr::u32 smokeBaseMinPPS, smokeBaseMaxPPS;
    irr::f32 radius;
};

#endif