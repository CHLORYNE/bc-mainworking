/*   NAUTITECH - Simulateur de Navigation (Bridge Command fork)
     KYARA HOULE: analytic long-wave swell field, driven live by the weather tab.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

// WHY THIS EXISTS
// The FFT ocean (FFTWave / MovingWater) is a 100 m tile that repeats. A real houle has a
// wavelength of 60-300 m, so it cannot live inside that tile. This class adds a small set of
// long directional waves ON TOP of the FFT chop:
//   * height and period come from the weather (Beaufort force 0-12),
//   * direction comes from the wind direction (waves come FROM the wind),
//   * wind speed sets the "age" of the sea: little wind under a big sea = long, clean swell;
//     strong wind = shorter, more confused wind sea.
// The SAME numbers are used by the physics (getHeight) and by the water vertex shader
// (getShaderData), so what the trainee sees is what the hull rides on.
//
// Everything changes smoothly (SWELL_RESPONSE_TIME), so moving the weather slider makes the sea
// build up or die down over ~a minute instead of jumping.
//
// Coordinates: simulator-local world X/Z (the ones ship nodes use). When SimulationModel
// re-centres the world (the "Normalise" block), call shiftOrigin() so the waves don't jump.

#ifndef __SWELL_HPP_INCLUDED__
#define __SWELL_HPP_INCLUDED__

#include "irrlicht.h"

class Swell
{
public:
    static const int NCOMP = 5;      // number of long-wave components (keep in sync with the shader)
    static const int NET_FIELDS = 9; // floats exchanged with secondaries

    Swell();

    // false = swell forced flat (e.g. shaders disabled, so it could not be SEEN - physics then
    // stays consistent with the picture).
    void setEnabled(bool enabled);
    bool isEnabled() const;

    // Call once per frame, BEFORE ships are updated.
    //   weather      : Beaufort-like sea state 0..12 (the weather slider)
    //   windDirDeg   : direction the wind blows FROM (deg true)
    //   windSpeedKts : true wind speed, knots
    //   anchorX/Z    : own-ship position. When the period changes, the wave field is kept
    //                  continuous at this point (where the trainee is looking).
    void update(irr::f32 dt, irr::f32 weather, irr::f32 windDirDeg, irr::f32 windSpeedKts,
                irr::f32 anchorX, irr::f32 anchorZ);

    // World re-centred by (dx, dz) (same values passed to every moveNode()).
    void shiftOrigin(irr::f32 dx, irr::f32 dz);

    // Centre of the rendered animated sea patch (Water node position). The swell fades to zero
    // near the edge of that patch so it meets the flat far-sea mesh without a step.
    void setFadeCentre(irr::f32 x, irr::f32 z);

    // Swell elevation (m, NOT including tide or FFT chop) at a local world position.
    irr::f32 getHeight(irr::f32 x, irr::f32 z) const;

    // For the water shader: comp[i*4+0..3] = (kx, kz, amplitude, phase), fade = (cx, cz, r0, r1)
    void getShaderData(irr::f32 comp[NCOMP * 4], irr::f32 fade[4]) const;

    // Smoothed sea description (for GUI / physics)
    irr::f32 getSignificantHeight() const;  // Hs of the swell part (m)
    irr::f32 getSeaStateHeight() const;     // Hs of the whole sea for this weather (m), smoothed
    irr::f32 getPeakPeriod() const;         // s
    irr::f32 getPeakWavelength() const;     // m
    irr::f32 getDirectionFrom() const;      // deg true, waves come FROM

    // Network sync (primary -> secondary). Phases are exchanged in ABSOLUTE world coordinates,
    // because each machine normalises its world independently. offsetX/Z = the model's
    // offsetPosition (absolute = local + offset).
    void getNetworkState(irr::f64 offsetX, irr::f64 offsetZ, irr::f32 out[NET_FIELDS]) const;
    void applyNetworkState(const irr::f32 in[NET_FIELDS], irr::f64 offsetX, irr::f64 offsetZ);

private:
    void computeTargets(irr::f32 weather, irr::f32 windDirDeg, irr::f32 windSpeedKts,
                        irr::f32& hsSea, irr::f32& tp, irr::f32& dirX, irr::f32& dirZ, irr::f32& spread) const;
    void rebuildComponents(bool keepContinuousAtAnchor);
    irr::f32 fadeAt(irr::f32 x, irr::f32 z) const;

    bool enabled;
    bool initialised;

    // Smoothed state
    irr::f32 hsSea;        // whole-sea Hs (m)
    irr::f32 tp;           // peak period (s)
    irr::f32 dirX, dirZ;   // unit vector, direction waves come FROM (smoothed as a vector: no 359/0 wrap)
    irr::f32 spread;       // directional spreading multiplier

    // Components
    irr::f32 kx[NCOMP], kz[NCOMP], amp[NCOMP], omega[NCOMP];
    irr::f64 phase[NCOMP]; // radians, wrapped to [0, 2pi)

    irr::f32 anchorX, anchorZ;
    irr::f32 fadeCx, fadeCz;
};

#endif
