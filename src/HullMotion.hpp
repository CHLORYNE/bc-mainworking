/*   NAUTITECH - Simulateur de Navigation (Bridge Command fork)
     KYARA HOULE: seakeeping response of a hull (heave, pitch, roll) plus the wave forces used
     by the manoeuvring model (surge, sway, yaw).

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

// HOW IT WORKS (short version)
// 1. The sea surface (FFT chop + swell, via SimulationModel::getWaveHeight) is sampled on a
//    small grid laid along the hull and rotated with the heading.
// 2. A least-squares plane through those samples gives the "equilibrium" the hull would take if
//    it followed the water perfectly: mean height, slope along the keel, slope across the beam.
//    Averaging over the hull is itself physics: waves much shorter than the ship cancel out.
// 3. Each motion is a damped oscillator pulled towards that equilibrium:
//        x'' + 2*zeta*wn*x' + wn^2*x = wn^2*x_eq
//    with the ship's natural period (RollPeriod / PitchPeriod / HeavePeriod). This gives, for
//    free: head seas -> pitch, beam seas -> roll, speed/heading change the ENCOUNTER period,
//    and large motions when that period approaches the natural one (resonance).
// 4. The fore/aft and port/stbd slopes also give Froude-Krylov-like wave forces: the ship
//    slides down wave faces (surge, surfing in following seas) and is yawed when bow and stern
//    see different lateral slopes (quartering seas).

#ifndef __HULLMOTION_HPP_INCLUDED__
#define __HULLMOTION_HPP_INCLUDED__

#include "irrlicht.h"

class SimulationModel;

class HullMotion
{
public:
    struct Params
    {
        irr::f32 length;       // m
        irr::f32 breadth;      // m
        irr::f32 draught;      // m
        irr::f32 heavePeriod;  // s, 0 = estimate
        irr::f32 pitchPeriod;  // s, 0 = estimate
        irr::f32 rollPeriod;   // s, 0 = estimate
        irr::f32 heaveDamping; // zeta, 0 = default
        irr::f32 pitchDamping;
        irr::f32 rollDamping;
        irr::f32 freeboard;    // m, height of the deck edge above the waterline, 0 = estimate
        irr::f32 slamPitchDeg; // KYARA SLAM: bow-down angle that counts as "landing", 0 = default (3 deg)
        int nLong;             // samples along the hull (odd, >= 3)
        int nTrans;            // samples across the hull (odd, >= 3)

        Params() : length(10), breadth(4), draught(1), heavePeriod(0), pitchPeriod(0), rollPeriod(0),
                   heaveDamping(0), pitchDamping(0), rollDamping(0), freeboard(0), slamPitchDeg(0),
                   nLong(5), nTrans(3) {}
    };

    HullMotion();
    void init(const Params& p);
    void reset(); // snap to the sea on the next update (scenario start, teleport)

    // x, z : hull centre (local world coords). hdgDeg : heading. motionScale : instructor
    // setting (1 = realistic) applied to the DISPLAYED pitch and roll only.
    void update(const SimulationModel* model, irr::f32 dt, irr::f32 x, irr::f32 z, irr::f32 hdgDeg, irr::f32 motionScale);

    // Displayed attitude (after motion scale)
    irr::f32 getHeave() const;      // m above tide datum (waterline of the hull)
    irr::f32 getPitchDeg() const;   // + bow up
    irr::f32 getRollDeg() const;    // + heeled to starboard
    irr::f32 getAntiClipLift() const; // m, extra lift so crests never pass over the deck

    // Wave excitation for the manoeuvring model (unscaled, physical)
    irr::f32 getSlopeLong() const;      // mean surface slope along the keel, + higher at the bow
    irr::f32 getSlopeTrans() const;     // mean surface slope across, + higher to starboard
    irr::f32 getSlopeTransFore() const; // same, forward half only
    irr::f32 getSlopeTransAft() const;  // same, aft half only

    //KYARA SLAM: fires once when the bow drops below normal trim (tangage passing below
    //-slamPitchDeg) while still falling - i.e. she has just come down on the water.
    //  severity  : 1 = an ordinary landing at that angle, 2+ = she came down hard
    //  bowSpeed  : downward speed of the bow at that moment, m/s (used to size the splash)
    bool consumeSlam(irr::f32& severity, irr::f32& bowSpeed);
    irr::f32 getSlamPitchAngle() const;   // deg, the trigger angle for this hull

    irr::f32 getHeavePeriod() const;
    irr::f32 getPitchPeriod() const;
    irr::f32 getRollPeriod() const;

private:
    static void integrate(irr::f32& x, irr::f32& v, irr::f32 eq, irr::f32 wn, irr::f32 zeta, irr::f32 dt);

    Params prm;
    bool first;

    // States (physical, unscaled)
    irr::f32 z, zDot;       // heave (m)
    irr::f32 p, pDot;       // pitch (rad, + bow up)
    irr::f32 r, rDot;       // roll  (rad, + heel to starboard)

    // Displayed
    irr::f32 pitchShown, rollShown;
    irr::f32 lift;

    // Last sampled sea
    irr::f32 slopeLong, slopeTrans, slopeTransFore, slopeTransAft;

    //KYARA SLAM
    irr::f32 slamPitchRad;     // trigger angle (bow down)
    irr::f32 slamRefRate;      // rad/s, the pitch rate of an ordinary oscillation at that angle
    irr::f32 prevPitch;
    bool prevPitchValid;
    bool slamPending;
    irr::f32 slamSeverity, slamBowSpeed;
    irr::f32 slamHoldoff;      // s, so one wave gives one impact
    //The tangage passing below normal trim ARMS the landing; the splash and the sound wait until
    //the forefoot actually reaches the water, which is a moment later.
    bool slamArmed;
    irr::f32 slamArmTimer;     // s left before an armed landing is forgotten
    irr::f32 slamPeakRate;     // rad/s, the hardest the bow was falling during the descent
};

#endif
