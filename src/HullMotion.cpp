/*   NAUTITECH - Simulateur de Navigation (Bridge Command fork)
     KYARA HOULE: seakeeping response of a hull. See HullMotion.hpp.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#include "HullMotion.hpp"
#include "SimulationModel.hpp"

#include <cmath>

namespace
{
    // ---------------------------------------------------------------------------------------
    // TUNABLES
    // ---------------------------------------------------------------------------------------
    const irr::f32 DEFAULT_HEAVE_DAMPING = 0.30f;
    const irr::f32 DEFAULT_PITCH_DAMPING = 0.30f;
    const irr::f32 DEFAULT_ROLL_DAMPING = 0.08f;  // roll is lightly damped on real ships -> resonance
    const irr::f32 ROLL_EXCITATION = 0.80f;       // "effective wave slope" factor for roll
    const irr::f32 MAX_PITCH_DEG = 30.0f;
    const irr::f32 MAX_ROLL_DEG = 45.0f;
    const irr::f32 SAMPLE_SPAN = 0.45f;           // samples from -0.45 to +0.45 of L and B
    const irr::f32 CLIP_MARGIN = 0.05f;           // m kept between deck edge and crest
    const irr::f32 LIFT_RISE_TIME = 0.12f;        // s, anti-clip lift comes in fast...
    const irr::f32 LIFT_FALL_TIME = 1.50f;        // s, ...and goes away gently (no bounce)
    const irr::f32 MAX_SUBSTEP = 0.01f;           // s, integration step

    //KYARA SLAM
    const irr::f32 SLAM_PITCH_DEG = 3.0f;         // default bow-down angle that counts as a landing
    const irr::f32 SLAM_MIN_SEVERITY = 0.35f;     // below this it is just ordinary pitching
    const irr::f32 SLAM_EVENT_GAP = 0.45f;        // s, so one wave gives one impact
    const irr::f32 SLAM_WHIP = 0.012f;            // rad/s of bow-up kick per unit of severity
    const irr::f32 FOREFOOT_STATION = 0.45f;      // fraction of the length forward where the bow lands

    inline irr::f32 clampf(irr::f32 v, irr::f32 lo, irr::f32 hi) { return v < lo ? lo : (v > hi ? hi : v); }
}

HullMotion::HullMotion()
    : first(true), z(0), zDot(0), p(0), pDot(0), r(0), rDot(0),
      pitchShown(0), rollShown(0), lift(0),
      slopeLong(0), slopeTrans(0), slopeTransFore(0), slopeTransAft(0),
      slamPitchRad(0.05f), slamRefRate(0.05f), prevPitch(0), prevPitchValid(false),
      slamPending(false), slamSeverity(0), slamBowSpeed(0), slamHoldoff(0)
{
}

void HullMotion::init(const Params& in)
{
    prm = in;
    if (prm.length < 1.0f) { prm.length = 1.0f; }
    if (prm.breadth < 0.5f) { prm.breadth = 0.5f; }
    if (prm.draught < 0.2f) { prm.draught = 0.2f; }
    if (prm.nLong < 3) { prm.nLong = 3; }
    if (prm.nTrans < 3) { prm.nTrans = 3; }
    if ((prm.nLong % 2) == 0) { prm.nLong++; }
    if ((prm.nTrans % 2) == 0) { prm.nTrans++; }

    // Natural period estimates, used only when the boat.ini gives none.
    if (prm.heavePeriod <= 0) {
        prm.heavePeriod = clampf(2.5f * sqrtf(prm.draught), 1.5f, 10.0f);
    }
    if (prm.pitchPeriod <= 0) {
        prm.pitchPeriod = clampf(0.6f * sqrtf(prm.length), 1.5f, 10.0f);
    }
    if (prm.rollPeriod <= 0) {
        // T = 2 * C * B / sqrt(GM), C ~ 0.4, GM guessed from the beam
        const irr::f32 gm = (0.07f * prm.breadth > 0.3f) ? 0.07f * prm.breadth : 0.3f;
        prm.rollPeriod = clampf(0.8f * prm.breadth / sqrtf(gm), 3.0f, 25.0f);
    }
    if (prm.heaveDamping <= 0) { prm.heaveDamping = DEFAULT_HEAVE_DAMPING; }
    if (prm.pitchDamping <= 0) { prm.pitchDamping = DEFAULT_PITCH_DAMPING; }
    if (prm.rollDamping <= 0) { prm.rollDamping = DEFAULT_ROLL_DAMPING; }
    if (prm.freeboard <= 0) {
        prm.freeboard = clampf(0.12f * prm.length, 0.8f, 6.0f);
    }
    //KYARA SLAM: the bow "lands" when the tangage drops below this angle while still falling.
    //slamRefRate is the pitch rate an ordinary oscillation of that amplitude would have, so
    //severity comes out around 1 for a normal landing and 2+ when she really comes down.
    if (prm.slamPitchDeg <= 0) { prm.slamPitchDeg = SLAM_PITCH_DEG; }
    slamPitchRad = prm.slamPitchDeg * irr::core::DEGTORAD;
    slamRefRate = slamPitchRad * (6.2831853f / prm.pitchPeriod);
    if (slamRefRate < 0.005f) { slamRefRate = 0.005f; }

    first = true;
}

void HullMotion::reset()
{
    first = true;
    prevPitchValid = false;
    slamPending = false;
}

bool HullMotion::consumeSlam(irr::f32& severity, irr::f32& bowSpeed)
{
    if (!slamPending) { return false; }
    severity = slamSeverity;
    bowSpeed = slamBowSpeed;
    slamPending = false;
    return true;
}

irr::f32 HullMotion::getSlamPitchAngle() const { return prm.slamPitchDeg; }

void HullMotion::integrate(irr::f32& x, irr::f32& v, irr::f32 eq, irr::f32 wn, irr::f32 zeta, irr::f32 dt)
{
    // Semi-implicit Euler, sub-stepped so a slow frame can't make it unstable.
    int n = (int)ceilf(dt / MAX_SUBSTEP);
    if (n < 1) { n = 1; }
    if (n > 20) { n = 20; }
    const irr::f32 h = dt / (irr::f32)n;
    for (int i = 0; i < n; i++) {
        const irr::f32 a = wn * wn * (eq - x) - 2.0f * zeta * wn * v;
        v += a * h;
        x += v * h;
    }
}

void HullMotion::update(const SimulationModel* model, irr::f32 dt, irr::f32 x, irr::f32 zPos, irr::f32 hdgDeg, irr::f32 motionScale)
{
    if (!model) { return; }

    // Heading convention (same as the position integration):
    //   ahead     (X,Z) = ( sin h,  cos h)
    //   starboard (X,Z) = ( cos h, -sin h)
    const irr::f32 hr = hdgDeg * irr::core::DEGTORAD;
    const irr::f32 sh = sinf(hr), ch = cosf(hr);

    const int nL = prm.nLong, nT = prm.nTrans;
    const irr::f32 halfL = SAMPLE_SPAN * prm.length;
    const irr::f32 halfB = SAMPLE_SPAN * prm.breadth;

    // Sample the sea and keep the samples for the anti-clip test below.
    // (max grid 9 x 5 - more than enough for a hull)
    irr::f32 hs[9 * 5];
    irr::f32 xs[9 * 5];
    irr::f32 ys[9 * 5];
    const int nLc = nL > 9 ? 9 : nL;
    const int nTc = nT > 5 ? 5 : nT;

    irr::f32 sumH = 0, sumXH = 0, sumXX = 0, sumYH = 0, sumYY = 0;
    irr::f32 sumYHf = 0, sumYYf = 0, sumYHa = 0, sumYYa = 0;
    int count = 0;
    for (int i = 0; i < nLc; i++) {
        const irr::f32 xl = -halfL + 2.0f * halfL * (irr::f32)i / (irr::f32)(nLc - 1);
        for (int j = 0; j < nTc; j++) {
            const irr::f32 yt = -halfB + 2.0f * halfB * (irr::f32)j / (irr::f32)(nTc - 1);
            const irr::f32 wx = x + xl * sh + yt * ch;
            const irr::f32 wz = zPos + xl * ch - yt * sh;
            const irr::f32 h = model->getWaveHeight(wx, wz);
            hs[count] = h; xs[count] = xl; ys[count] = yt;
            count++;
            sumH += h;
            sumXH += xl * h; sumXX += xl * xl;
            sumYH += yt * h; sumYY += yt * yt;
            if (xl > 0.001f) { sumYHf += yt * h; sumYYf += yt * yt; }
            if (xl < -0.001f) { sumYHa += yt * h; sumYYa += yt * yt; }
        }
    }

    const irr::f32 heaveEq = sumH / (irr::f32)count;
    slopeLong = (sumXX > 0) ? sumXH / sumXX : 0.0f;
    slopeTrans = (sumYY > 0) ? sumYH / sumYY : 0.0f;
    slopeTransFore = (sumYYf > 0) ? sumYHf / sumYYf : 0.0f;
    slopeTransAft = (sumYYa > 0) ? sumYHa / sumYYa : 0.0f;

    // Equilibrium attitude: bow up when the water is higher at the bow; heel to PORT when the
    // water is higher to starboard (the starboard side is pushed up).
    const irr::f32 pitchEq = atanf(slopeLong);
    const irr::f32 rollEq = -ROLL_EXCITATION * atanf(slopeTrans);

    if (first) {
        z = heaveEq; zDot = 0;
        p = pitchEq; pDot = 0;
        r = rollEq; rDot = 0;
        lift = 0;
        first = false;
    } else if (dt > 0.0f) {
        const irr::f32 TWO_PI = 6.2831853f;
        integrate(z, zDot, heaveEq, TWO_PI / prm.heavePeriod, prm.heaveDamping, dt);
        integrate(p, pDot, pitchEq, TWO_PI / prm.pitchPeriod, prm.pitchDamping, dt);
        integrate(r, rDot, rollEq, TWO_PI / prm.rollPeriod, prm.rollDamping, dt);

        const irr::f32 pMax = MAX_PITCH_DEG * irr::core::DEGTORAD;
        const irr::f32 rMax = MAX_ROLL_DEG * irr::core::DEGTORAD;
        if (p > pMax) { p = pMax; if (pDot > 0) pDot = 0; }
        if (p < -pMax) { p = -pMax; if (pDot < 0) pDot = 0; }
        if (r > rMax) { r = rMax; if (rDot > 0) rDot = 0; }
        if (r < -rMax) { r = -rMax; if (rDot < 0) rDot = 0; }
    }

    // Instructor motion scale: displayed attitude only. Heave stays physical, otherwise a
    // reduced heave would let the swell crests swallow the hull.
    const irr::f32 ms = clampf(motionScale, 0.0f, 2.0f);
    pitchShown = p * ms;
    rollShown = r * ms;

    // Anti-clip: find how far any crest rises above the deck edge at that sample, for the pose
    // actually shown. Only this excess is lifted, and smoothly - the old version lifted the
    // whole waterline up to the highest crest, which killed most of the real motion.
    irr::f32 target = 0.0f;
    const irr::f32 sp = sinf(pitchShown), sr = sinf(rollShown);
    for (int k = 0; k < count; k++) {
        const irr::f32 deck = z + prm.freeboard + xs[k] * sp - ys[k] * sr;
        const irr::f32 excess = hs[k] - deck + CLIP_MARGIN;
        if (excess > target) { target = excess; }
    }
    if (dt > 0.0f) {
        const irr::f32 tau = (target > lift) ? LIFT_RISE_TIME : LIFT_FALL_TIME;
        lift += (target - lift) * (dt / (tau + dt));
    } else {
        lift = target;
    }

    //KYARA SLAM ------------------------------------------------------------------------------
    //One trigger, one sound: the moment the tangage passes below normal trim (bow down past
    //slamPitchDeg) while it is still falling, she has just come down on the water. How hard is
    //read from the pitch RATE at that instant, which is also what sizes the splash.
    if (slamHoldoff > 0.0f) { slamHoldoff -= dt; }

    if (prevPitchValid && dt > 0.0f) {
        if (prevPitch > -slamPitchRad && p <= -slamPitchRad && pDot < 0.0f && slamHoldoff <= 0.0f) {
            const irr::f32 severity = (-pDot) / slamRefRate;
            if (severity > SLAM_MIN_SEVERITY) {
                slamSeverity = severity;
                //Downward speed of the bow itself: the pitch rate about midships, plus whatever
                //the whole hull is doing in heave.
                slamBowSpeed = (-pDot) * FOREFOOT_STATION * prm.length - zDot;
                if (slamBowSpeed < 0.0f) { slamBowSpeed = 0.0f; }
                slamPending = true;
                slamHoldoff = SLAM_EVENT_GAP;
                //Whipping: a hard landing kicks the bow back up and the hull rings with it.
                if (severity > 1.0f) {
                    const irr::f32 r = (severity > 3.0f) ? 3.0f : severity;
                    pDot += SLAM_WHIP * r;
                }
            }
        }
    }
    prevPitch = p;
    prevPitchValid = true;
}

irr::f32 HullMotion::getHeave() const { return z; }
irr::f32 HullMotion::getPitchDeg() const { return pitchShown * irr::core::RADTODEG; }
irr::f32 HullMotion::getRollDeg() const { return rollShown * irr::core::RADTODEG; }
irr::f32 HullMotion::getAntiClipLift() const { return lift; }
irr::f32 HullMotion::getSlopeLong() const { return slopeLong; }
irr::f32 HullMotion::getSlopeTrans() const { return slopeTrans; }
irr::f32 HullMotion::getSlopeTransFore() const { return slopeTransFore; }
irr::f32 HullMotion::getSlopeTransAft() const { return slopeTransAft; }
irr::f32 HullMotion::getHeavePeriod() const { return prm.heavePeriod; }
irr::f32 HullMotion::getPitchPeriod() const { return prm.pitchPeriod; }
irr::f32 HullMotion::getRollPeriod() const { return prm.rollPeriod; }
