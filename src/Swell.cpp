/*   NAUTITECH - Simulateur de Navigation (Bridge Command fork)
     KYARA HOULE: analytic long-wave swell field. See Swell.hpp for the why.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#include "Swell.hpp"

#include <cmath>

namespace
{
    // ---------------------------------------------------------------------------------------
    // TUNABLES - everything that shapes the houle is here.
    // ---------------------------------------------------------------------------------------

    // Probable significant wave height (m) for each Beaufort force 0..12 (WMO sea-state table).
    // The weather slider IS the Beaufort force (the tab shows "F5" etc.).
    const irr::f32 HS_BY_FORCE[13] = { 0.0f, 0.1f, 0.2f, 0.6f, 1.0f, 2.0f, 3.0f, 4.0f, 5.5f, 7.0f, 9.0f, 11.5f, 14.0f };

    // Fraction of the sea-state height carried by the long analytic waves. The FFT chop
    // (already scaled by weather) supplies the short-wave rest. Raise for a bigger houle.
    const irr::f32 SWELL_SHARE = 0.60f;

    // Global height trim, for calibration against what instructors expect to see.
    const irr::f32 SWELL_HEIGHT_SCALE = 1.0f;

    // Peak period of a fully-developed sea: Tp ~ 5 * sqrt(Hs)  (Pierson-Moskowitz).
    const irr::f32 TP_PER_SQRT_HS = 5.0f;
    const irr::f32 TP_MIN = 3.5f;   // s
    const irr::f32 TP_MAX = 17.0f;  // s

    // Seconds for the sea to follow a change of weather/wind (first-order lag).
    const irr::f32 SWELL_RESPONSE_TIME = 20.0f;

    // Swell fades out between these distances from the centre of the rendered sea patch
    // (the animated patch is 21 x 100 m tiles, i.e. +/- 1050 m).
    const irr::f32 FADE_START = 750.0f;
    const irr::f32 FADE_END = 1000.0f;

    // Component shape: frequency multiplier (x peak), relative amplitude, direction offset (deg,
    // multiplied by 'spread') and a fixed phase. Non-commensurate frequencies give realistic
    // wave groups ("sets" of bigger waves) instead of a metronome.
    const irr::f32 COMP_FREQ[Swell::NCOMP] = { 0.70f, 0.86f, 1.00f, 1.15f, 1.35f };
    const irr::f32 COMP_AMP[Swell::NCOMP] = { 0.50f, 0.85f, 1.00f, 0.78f, 0.48f };
    const irr::f32 COMP_DIR[Swell::NCOMP] = { -18.0f, 9.0f, 0.0f, -9.0f, 22.0f };
    const irr::f64 COMP_PHASE0[Swell::NCOMP] = { 0.3, 2.1, 4.4, 1.2, 5.5 };

    const irr::f32 G = 9.81f;
    const irr::f64 TWO_PI = 6.283185307179586;

    inline irr::f64 wrapTwoPi(irr::f64 a)
    {
        a = fmod(a, TWO_PI);
        if (a < 0) { a += TWO_PI; }
        return a;
    }

    inline irr::f64 wrapPi(irr::f64 a)
    {
        a = wrapTwoPi(a);
        if (a > TWO_PI * 0.5) { a -= TWO_PI; }
        return a;
    }

    inline irr::f32 clampf(irr::f32 v, irr::f32 lo, irr::f32 hi)
    {
        return v < lo ? lo : (v > hi ? hi : v);
    }
}

Swell::Swell()
    : enabled(true), initialised(false),
      hsSea(0), tp(TP_MIN), dirX(0), dirZ(1), spread(1.0f),
      anchorX(0), anchorZ(0), fadeCx(0), fadeCz(0)
{
    for (int i = 0; i < NCOMP; i++) {
        kx[i] = 0; kz[i] = 0; amp[i] = 0; omega[i] = 0;
        phase[i] = COMP_PHASE0[i];
    }
}

void Swell::setEnabled(bool e) { enabled = e; }
bool Swell::isEnabled() const { return enabled; }

void Swell::computeTargets(irr::f32 weather, irr::f32 windDirDeg, irr::f32 windSpeedKts,
                           irr::f32& hsT, irr::f32& tpT, irr::f32& dxT, irr::f32& dzT, irr::f32& spreadT) const
{
    // Height from the Beaufort force (interpolated for fractional slider values)
    irr::f32 w = clampf(weather, 0.0f, 12.0f);
    int i0 = (int)floorf(w);
    if (i0 > 11) { i0 = 11; }
    const irr::f32 t = w - (irr::f32)i0;
    hsT = HS_BY_FORCE[i0] * (1.0f - t) + HS_BY_FORCE[i0 + 1] * t;

    // Period from height (fully developed sea) ...
    tpT = TP_PER_SQRT_HS * sqrtf(hsT > 0.02f ? hsT : 0.02f);

    // ... adjusted by the wind: compare the actual wind with the wind that would be needed to
    // raise this sea (Pierson-Moskowitz: Hs = 0.21 U^2 / g).
    const irr::f32 uDeveloped = sqrtf(G * (hsT > 0.02f ? hsT : 0.02f) / 0.21f); // m/s
    const irr::f32 uWind = windSpeedKts * 0.514444f;                            // m/s
    const irr::f32 r = uWind / uDeveloped;
    if (r < 1.0f) {
        // Wind weaker than the sea: decaying swell - longer period, longer crests.
        tpT *= 1.0f + 0.35f * (1.0f - r);
        spreadT = 0.5f + 0.5f * r;
    } else {
        // Wind stronger than the sea: young, steeper, more confused wind sea.
        tpT *= (1.0f - 0.08f * (r - 1.0f) > 0.85f) ? (1.0f - 0.08f * (r - 1.0f)) : 0.85f;
        spreadT = (1.0f + 0.15f * (r - 1.0f) < 1.3f) ? (1.0f + 0.15f * (r - 1.0f)) : 1.3f;
    }
    tpT = clampf(tpT, TP_MIN, TP_MAX);

    // Waves come FROM the wind direction. Heading convention: X = sin, Z = cos.
    const irr::f32 a = windDirDeg * irr::core::DEGTORAD;
    dxT = sinf(a);
    dzT = cosf(a);
}

void Swell::update(irr::f32 dt, irr::f32 weather, irr::f32 windDirDeg, irr::f32 windSpeedKts,
                   irr::f32 ancX, irr::f32 ancZ)
{
    anchorX = ancX;
    anchorZ = ancZ;

    irr::f32 hsT, tpT, dxT, dzT, spreadT;
    computeTargets(weather, windDirDeg, windSpeedKts, hsT, tpT, dxT, dzT, spreadT);

    if (!initialised) {
        // Scenario start: the sea is already there, no build-up.
        hsSea = hsT; tp = tpT; dirX = dxT; dirZ = dzT; spread = spreadT;
        initialised = true;
        rebuildComponents(false);
        return;
    }

    if (dt <= 0.0f) {
        return; // paused: frozen sea state, frozen phases
    }

    const irr::f32 f = dt / (SWELL_RESPONSE_TIME + dt);
    hsSea += (hsT - hsSea) * f;
    tp += (tpT - tp) * f;
    spread += (spreadT - spread) * f;
    dirX += (dxT - dirX) * f;
    dirZ += (dzT - dirZ) * f;
    const irr::f32 len = sqrtf(dirX * dirX + dirZ * dirZ);
    if (len > 1e-4f) { dirX /= len; dirZ /= len; } else { dirX = dxT; dirZ = dzT; }

    rebuildComponents(true);

    // Advance phases. Accumulating omega*dt (instead of computing omega*t) is what lets the
    // period change live without the whole sea jumping.
    for (int i = 0; i < NCOMP; i++) {
        phase[i] = wrapTwoPi(phase[i] + (irr::f64)omega[i] * dt);
    }
}

void Swell::rebuildComponents(bool keepContinuousAtAnchor)
{
    // Direction waves travel TOWARDS = opposite of 'from'
    const irr::f32 baseProp = atan2f(-dirX, -dirZ); // radians, heading convention (X = sin)
    const irr::f32 wp = (irr::f32)(TWO_PI / (tp > 0.1f ? tp : 0.1f));

    irr::f32 sumSq = 0;
    for (int i = 0; i < NCOMP; i++) { sumSq += COMP_AMP[i] * COMP_AMP[i]; }
    // Hs = 4 * sigma, sigma^2 = sum(a^2)/2  ->  a_i = w_i * Hs / (4 * sqrt(sum(w^2)/2))
    const irr::f32 hsSwell = enabled ? hsSea * SWELL_SHARE * SWELL_HEIGHT_SCALE : 0.0f;
    const irr::f32 norm = (sumSq > 0) ? hsSwell / (4.0f * sqrtf(sumSq * 0.5f)) : 0.0f;

    for (int i = 0; i < NCOMP; i++) {
        const irr::f32 w = wp * COMP_FREQ[i];
        const irr::f32 k = w * w / G; // deep water dispersion
        const irr::f32 th = baseProp + spread * COMP_DIR[i] * irr::core::DEGTORAD;
        const irr::f32 nkx = k * sinf(th);
        const irr::f32 nkz = k * cosf(th);

        if (keepContinuousAtAnchor) {
            // Keep the local phase at the anchor unchanged: phi += dk . anchor
            phase[i] = wrapTwoPi(phase[i] + (irr::f64)(nkx - kx[i]) * anchorX + (irr::f64)(nkz - kz[i]) * anchorZ);
        }
        kx[i] = nkx;
        kz[i] = nkz;
        omega[i] = w;
        amp[i] = COMP_AMP[i] * norm;
    }
}

void Swell::shiftOrigin(irr::f32 dx, irr::f32 dz)
{
    // eta(x) = a cos(k.x - phi). After x' = x + d, the same physical sea needs phi' = phi + k.d
    for (int i = 0; i < NCOMP; i++) {
        phase[i] = wrapTwoPi(phase[i] + (irr::f64)kx[i] * dx + (irr::f64)kz[i] * dz);
    }
    anchorX += dx;
    anchorZ += dz;
    fadeCx += dx;
    fadeCz += dz;
}

void Swell::setFadeCentre(irr::f32 x, irr::f32 z)
{
    fadeCx = x;
    fadeCz = z;
}

irr::f32 Swell::fadeAt(irr::f32 x, irr::f32 z) const
{
    const irr::f32 ddx = x - fadeCx, ddz = z - fadeCz;
    const irr::f32 r = sqrtf(ddx * ddx + ddz * ddz);
    if (r <= FADE_START) { return 1.0f; }
    if (r >= FADE_END) { return 0.0f; }
    const irr::f32 t = (r - FADE_START) / (FADE_END - FADE_START);
    return 1.0f - t * t * (3.0f - 2.0f * t); // smoothstep, identical to the GLSL
}

irr::f32 Swell::getHeight(irr::f32 x, irr::f32 z) const
{
    if (!enabled) { return 0.0f; }
    const irr::f32 fade = fadeAt(x, z);
    if (fade <= 0.0f) { return 0.0f; }
    irr::f32 h = 0;
    for (int i = 0; i < NCOMP; i++) {
        h += amp[i] * cosf((irr::f32)((irr::f64)kx[i] * x + (irr::f64)kz[i] * z - phase[i]));
    }
    return h * fade;
}

void Swell::getShaderData(irr::f32 comp[NCOMP * 4], irr::f32 fade[4]) const
{
    for (int i = 0; i < NCOMP; i++) {
        comp[i * 4 + 0] = kx[i];
        comp[i * 4 + 1] = kz[i];
        comp[i * 4 + 2] = enabled ? amp[i] : 0.0f;
        comp[i * 4 + 3] = (irr::f32)phase[i];
    }
    fade[0] = fadeCx;
    fade[1] = fadeCz;
    fade[2] = FADE_START;
    fade[3] = FADE_END;
}

irr::f32 Swell::getSignificantHeight() const { return enabled ? hsSea * SWELL_SHARE * SWELL_HEIGHT_SCALE : 0.0f; }
irr::f32 Swell::getSeaStateHeight() const { return hsSea; }
irr::f32 Swell::getPeakPeriod() const { return tp; }
irr::f32 Swell::getPeakWavelength() const { return (irr::f32)(G * tp * tp / TWO_PI); }

irr::f32 Swell::getDirectionFrom() const
{
    irr::f32 d = atan2f(dirX, dirZ) * irr::core::RADTODEG;
    if (d < 0) { d += 360.0f; }
    return d;
}

void Swell::getNetworkState(irr::f64 offsetX, irr::f64 offsetZ, irr::f32 out[NET_FIELDS]) const
{
    out[0] = hsSea;
    out[1] = tp;
    out[2] = getDirectionFrom();
    out[3] = spread;
    // local: cos(k.xl - phi); absolute xa = xl + off  ->  phiAbs = phi + k.off
    for (int i = 0; i < NCOMP; i++) {
        out[4 + i] = (irr::f32)wrapTwoPi(phase[i] + (irr::f64)kx[i] * offsetX + (irr::f64)kz[i] * offsetZ);
    }
}

void Swell::applyNetworkState(const irr::f32 in[NET_FIELDS], irr::f64 offsetX, irr::f64 offsetZ)
{
    hsSea = in[0];
    tp = in[1];
    const irr::f32 a = in[2] * irr::core::DEGTORAD;
    dirX = sinf(a);
    dirZ = cosf(a);
    spread = in[3];
    initialised = true;
    rebuildComponents(false);

    for (int i = 0; i < NCOMP; i++) {
        const irr::f64 target = wrapTwoPi((irr::f64)in[4 + i] - (irr::f64)kx[i] * offsetX - (irr::f64)kz[i] * offsetZ);
        const irr::f64 err = wrapPi(target - phase[i]);
        if (fabs(err) > 0.5) {
            phase[i] = target;                        // far off (first packet, big lag): snap
        } else {
            phase[i] = wrapTwoPi(phase[i] + 0.3 * err); // close: ease in, no visible jitter
        }
    }
}
