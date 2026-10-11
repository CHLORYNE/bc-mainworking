/*   NAUTITECH - Simulateur de Navigation
     The sonars: a 360 degree search sonar and a fish finder (vertical echo sounder).

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#include "Sonar.hpp"
#include "SimulationModel.hpp"
#include "IniFile.hpp"
#include "Utilities.hpp"
#include <cmath>
#include <algorithm>

const irr::f32 Sonar::TRANSDUCER_DEPTH = 1.5f;

namespace {
    const irr::f32 RANGES_360[Sonar::RANGE_COUNT] = { 100, 200, 300, 500, 800, 1000, 1500, 2000 };
    const irr::f32 RANGES_SOUNDER[Sonar::SOUNDER_RANGE_COUNT] = { 5, 10, 15, 20, 30, 40, 60, 80, 100, 150, 200, 300, 400 };
    const irr::f32 SOUND_SPEED = 1500.0f;      //m/s in sea water
    const irr::f32 VERTICAL_BEAM_360 = 8.0f;   //degrees
    const irr::f32 HORIZONTAL_BEAM_360 = 6.0f;
    const irr::f32 FISH_AREA = 1500.0f;        //schools kept within this distance of the ship
    const irr::f32 SINGLE_AREA = 150.0f;       //single fish (they matter under the ship and close by)

    //Repeatable noise of the seabed: a value 0..1 for every cell of a grid, smoothly interpolated
    inline irr::f32 hash2(irr::s32 x, irr::s32 z, irr::u32 seed)
    {
        irr::u32 h = (irr::u32)x * 374761393u + (irr::u32)z * 668265263u + seed * 2246822519u;
        h = (h ^ (h >> 13)) * 1274126177u;
        h ^= h >> 16;
        return (irr::f32)(h & 0xFFFFFF) / 16777215.0f;
    }

    irr::f32 valueNoise(irr::f32 x, irr::f32 z, irr::f32 cell, irr::u32 seed)
    {
        const irr::f32 fx = x / cell, fz = z / cell;
        const irr::f32 flx = std::floor(fx), flz = std::floor(fz);
        const irr::s32 ix = (irr::s32)flx, iz = (irr::s32)flz;
        irr::f32 tx = fx - flx, tz = fz - flz;
        tx = tx * tx * (3.0f - 2.0f * tx);
        tz = tz * tz * (3.0f - 2.0f * tz);
        const irr::f32 a = hash2(ix, iz, seed), b = hash2(ix + 1, iz, seed);
        const irr::f32 c = hash2(ix, iz + 1, seed), d = hash2(ix + 1, iz + 1, seed);
        return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * tz;
    }

    inline irr::f32 wrapDeg(irr::f32 a)
    {
        a = std::fmod(a, 360.0f);
        if (a < 0) { a += 360.0f; }
        return a;
    }

    inline irr::f32 angleDiff(irr::f32 a, irr::f32 b) //a - b, -180..180
    {
        irr::f32 d = std::fmod(a - b + 540.0f, 360.0f) - 180.0f;
        return d;
    }
}

Sonar::Sonar()
    : model(0), rng(20240917u), loaded(false),
      offX(0), offZ(0), ownX(0), ownZ(0), transducerY(0), tide(0), seaState(0), ownHeading(0), ownSpeed(0),
      simTime(0), lastSimTime(-1), lastWakeTime(-1000), fishOn(true), fishDensity(1.0f), fishCheckTime(0),
      beams(BEAMS * BINS, 0), range360(3), tiltDeg(10.0f), gain360v(5), beamAccumulator(0), lastBeam(0), serial360(0),
      echogram(COLS), head(0), columnsFilled(0), pingSerial(0), autoRange(true), sounderRangeIdx(4), gainSounder(5),
      freq50(true), scrollDiv(1), clutterLevel(1), pingTimer(0), pingsInColumn(0), accumulated(ROWS, 0.0f),
      depthNow(0), pendingRangeChange(false)
{
}

void Sonar::setModel(SimulationModel* m)
{
    model = m;
    loaded = false;
}

//-------------------------------------------------------------------------------------------------
//Settings
//-------------------------------------------------------------------------------------------------
irr::f32 Sonar::range360Value(int i) { return RANGES_360[irr::core::clamp(i, 0, RANGE_COUNT - 1)]; }
irr::f32 Sonar::range360m() const { return range360Value(range360); }
void Sonar::setRangeIndex360(int i) { range360 = irr::core::clamp(i, 0, RANGE_COUNT - 1); clear360(); }
void Sonar::setTilt(irr::f32 deg) { tiltDeg = irr::core::clamp(deg, 0.0f, 60.0f); }
void Sonar::setGain360(int g) { gain360v = irr::core::clamp(g, 0, 10); }
void Sonar::clear360() { std::fill(beams.begin(), beams.end(), (irr::u8)0); serial360++; }

irr::f32 Sonar::sounderRangeValue(int i) { return RANGES_SOUNDER[irr::core::clamp(i, 0, SOUNDER_RANGE_COUNT - 1)]; }
void Sonar::setSounderRangeIndex(int i)
{
    i = irr::core::clamp(i, 0, SOUNDER_RANGE_COUNT - 1);
    if (i != sounderRangeIdx) { pendingRangeChange = true; }
    sounderRangeIdx = i;
}
void Sonar::setSounderGain(int g) { gainSounder = irr::core::clamp(g, 0, 10); }
void Sonar::setScrollSpeed(int pingsPerColumn) { scrollDiv = (pingsPerColumn >= 4) ? 4 : ((pingsPerColumn >= 2) ? 2 : 1); }
void Sonar::setClutter(int c) { clutterLevel = irr::core::clamp(c, 0, 10); }

const Sonar::Column& Sonar::column(int i) const
{
    const irr::u32 oldest = (columnsFilled < (irr::u32)COLS) ? 0 : head;
    return echogram[(oldest + (irr::u32)i) % COLS];
}

irr::f32 Sonar::random01()
{
    return (irr::f32)(rng() & 0xFFFFFF) / 16777215.0f;
}

//-------------------------------------------------------------------------------------------------
//The world
//-------------------------------------------------------------------------------------------------
void Sonar::readWorld()
{
    const irr::core::vector3df own = model->getOwnShipScenePosition();
    offX = model->getPosX() - own.X;
    offZ = model->getPosZ() - own.Z;
    ownX = model->getPosX();
    ownZ = model->getPosZ();
    transducerY = own.Y - TRANSDUCER_DEPTH;
    tide = model->getTideHeight();
    seaState = model->getWeather();
    ownHeading = model->getHeading();
    ownSpeed = model->getSOG();
    simTime = model->getTimeDelta();

    const irr::u32 n = model->getNumberOfOtherShips();
    ships.resize(n);
    for (irr::u32 i = 0; i < n; i++) {
        Ship& s = ships[i];
        const irr::core::vector3df p = model->getOtherShipScenePosition((int)i);
        s.x = p.X + offX;
        s.z = p.Z + offZ;
        s.y = p.Y;
        s.heading = model->getOtherShipHeading((int)i);
        s.length = irr::core::max_(model->getOtherShipLength((int)i), 4.0f);
        s.breadth = irr::core::max_(model->getOtherShipBreadth((int)i), 2.0f);
        s.draft = irr::core::clamp(0.055f * s.length + 0.8f, 1.0f, 12.0f);
    }
}

irr::f32 Sonar::hardness(irr::f32 ax, irr::f32 az) const
{
    //Patches a few hundred metres across, from mud to rock
    const irr::f32 n = 0.65f * valueNoise(ax, az, 180.0f, 11u) + 0.35f * valueNoise(ax, az, 45.0f, 12u);
    return irr::core::clamp((n - 0.2f) * 1.6f, 0.05f, 1.0f);
}

irr::f32 Sonar::seabedY(irr::f32 ax, irr::f32 az) const
{
    const irr::f32 terrain = model->getTerrainHeight(ax - offX, az - offZ);
    //Fine relief the terrain grid cannot hold: rougher on rock than on mud
    const irr::f32 hard = hardness(ax, az);
    const irr::f32 relief = (valueNoise(ax, az, 22.0f, 21u) - 0.5f) * 1.1f + (valueNoise(ax, az, 6.0f, 22u) - 0.5f) * 0.45f;
    return terrain + relief * (0.35f + hard);
}

irr::f32 Sonar::waterDepth(irr::f32 ax, irr::f32 az) const
{
    return tide - model->getTerrainHeight(ax - offX, az - offZ);
}

irr::f32 Sonar::layerDepth(irr::f32 ax, irr::f32 az) const
{
    //Part way down, rising and sinking slowly from place to place
    return waterDepth(ax, az) * (0.4f + 0.2f * valueNoise(ax, az, 500.0f, 31u));
}

void Sonar::loadScenarioObjects()
{
    wrecks.clear();
    fishOn = true;
    fishDensity = 1.0f;
    const std::string file = model->getScenarioFolder() + "sonar.ini";
    if (!Utilities::pathExists(file)) { return; }
    fishOn = IniFile::iniFileTou32(file, "Fish", 1) != 0;
    fishDensity = irr::core::clamp(IniFile::iniFileTof32(file, "FishDensity", 1.0f), 0.0f, 4.0f);
    const irr::u32 n = IniFile::iniFileTou32(file, "Number");
    for (irr::u32 i = 1; i <= n; i++) {
        Wreck w;
        const irr::f32 lat = IniFile::iniFileTof32(file, IniFile::enumerate1("Lat", (irr::s32)i));
        const irr::f32 lon = IniFile::iniFileTof32(file, IniFile::enumerate1("Long", (irr::s32)i));
        w.x = model->longToX(lon);
        w.z = model->latToZ(lat);
        w.length = irr::core::max_(IniFile::iniFileTof32(file, IniFile::enumerate1("Length", (irr::s32)i), 30.0f), 1.0f);
        w.height = irr::core::max_(IniFile::iniFileTof32(file, IniFile::enumerate1("Height", (irr::s32)i), 5.0f), 0.3f);
        w.heading = IniFile::iniFileTof32(file, IniFile::enumerate1("Heading", (irr::s32)i), 0.0f);
        w.name = IniFile::iniFileToWString(file, IniFile::enumerate1("Name", (irr::s32)i));
        wrecks.push_back(w);
    }
}

//-------------------------------------------------------------------------------------------------
//Fish and wakes
//-------------------------------------------------------------------------------------------------
void Sonar::spawnFish(bool school, bool anywhere)
{
    const irr::f32 area = school ? FISH_AREA : SINGLE_AREA;
    for (int attempt = 0; attempt < 6; attempt++) {
        const irr::f32 a = random01() * 360.0f * irr::core::DEGTORAD;
        //At start, anywhere around; later at the edge of the area, so none appears under the ship
        const irr::f32 r = anywhere ? area * std::sqrt(0.02f + 0.98f * random01()) : area * (0.85f + 0.15f * random01());
        const irr::f32 x = ownX + r * std::sin(a);
        const irr::f32 z = ownZ + r * std::cos(a);
        const irr::f32 depth = waterDepth(x, z);
        if (depth < (school ? 8.0f : 4.0f)) { continue; }
        Fish f;
        f.x = x;
        f.z = z;
        f.school = school;
        f.heading = random01() * 360.0f;
        f.speed = 0.1f + 0.4f * random01();
        if (school) {
            f.radius = 6.0f + 20.0f * random01();
            f.thick = irr::core::min_(2.0f + 7.0f * random01(), depth * 0.25f);
            f.strength = 0.55f + 0.45f * random01();
            //Pelagic (mid-water) or close to the bottom
            if (random01() < 0.6f) { f.depth = depth * (0.25f + 0.5f * random01()); }
            else { f.depth = depth - f.thick - 1.0f - 2.0f * random01(); }
        }
        else {
            f.radius = 0.0f;
            f.thick = 0.3f;
            f.strength = 0.25f + 0.4f * random01();
            f.depth = depth * (0.15f + 0.8f * random01());
        }
        f.depth = irr::core::clamp(f.depth, 2.0f, irr::core::max_(2.0f, depth - 0.8f));
        fish.push_back(f);
        return;
    }
}

void Sonar::manageFish(irr::f32 simDt)
{
    //Drift, wandering slowly
    for (size_t i = 0; i < fish.size(); i++) {
        Fish& f = fish[i];
        f.heading = wrapDeg(f.heading + (random01() - 0.5f) * 20.0f * simDt);
        f.x += f.speed * simDt * std::sin(f.heading * irr::core::DEGTORAD);
        f.z += f.speed * simDt * std::cos(f.heading * irr::core::DEGTORAD);
    }
    //Twice a second: stay in the water, leave the area, come in
    fishCheckTime += simDt;
    if (fishCheckTime < 0.5f && !fish.empty()) { return; }
    fishCheckTime = 0;
    const bool first = fish.empty();
    for (size_t i = 0; i < fish.size(); ) {
        Fish& f = fish[i];
        const irr::f32 dx = f.x - ownX, dz = f.z - ownZ;
        const irr::f32 area = f.school ? FISH_AREA : SINGLE_AREA;
        const irr::f32 depth = waterDepth(f.x, f.z);
        if (dx * dx + dz * dz > area * area * 1.5f || depth < 2.5f) {
            fish.erase(fish.begin() + i);
            continue;
        }
        f.depth = irr::core::clamp(f.depth, 1.5f, irr::core::max_(1.5f, depth - f.thick - 0.5f));
        i++;
    }
    if (!fishOn) { fish.clear(); return; }
    int schools = 0, singles = 0;
    for (size_t i = 0; i < fish.size(); i++) { if (fish[i].school) { schools++; } else { singles++; } }
    const int wantSchools = (int)(18 * fishDensity + 0.5f);
    const int wantSingles = (int)(120 * fishDensity + 0.5f);
    for (int k = schools; k < wantSchools; k++) { spawnFish(true, first); }
    for (int k = singles; k < wantSingles; k++) { spawnFish(false, first); }
}

void Sonar::recordWakes()
{
    wakes.resize(1 + ships.size());
    for (size_t i = 0; i < wakes.size(); i++) {
        std::deque<WakePoint>& w = wakes[i];
        while (!w.empty() && (simTime - w.front().t > 90.0f || simTime < w.front().t)) { w.pop_front(); }
    }
    if (simTime - lastWakeTime < 3.0f && simTime >= lastWakeTime) { return; }
    lastWakeTime = simTime;
    //Bubbles at the stern, more with speed
    auto add = [&](size_t index, irr::f32 x, irr::f32 z, irr::f32 heading, irr::f32 length, irr::f32 speed) {
        if (speed < 1.0f) { return; }
        WakePoint p;
        p.x = x - 0.5f * length * std::sin(heading * irr::core::DEGTORAD);
        p.z = z - 0.5f * length * std::cos(heading * irr::core::DEGTORAD);
        p.t = simTime;
        p.strength = irr::core::min_(1.0f, speed / 6.0f);
        wakes[index].push_back(p);
    };
    add(0, ownX, ownZ, ownHeading, model->getOwnShipLengthM(), ownSpeed);
    for (size_t i = 0; i < ships.size(); i++) {
        add(1 + i, ships[i].x, ships[i].z, ships[i].heading, ships[i].length, model->getOtherShipSpeed((int)i));
    }
}

//-------------------------------------------------------------------------------------------------
//Update
//-------------------------------------------------------------------------------------------------
void Sonar::update(irr::f32 dt)
{
    if (!model) { return; }
    dt = irr::core::clamp(dt, 0.0f, 0.5f);
    readWorld();
    if (!loaded) {
        loaded = true;
        loadScenarioObjects();
        fish.clear();
        wakes.clear();
        lastSimTime = simTime;
    }
    irr::f32 simDt = simTime - lastSimTime;
    if (simDt < 0 || simDt > 5.0f) { simDt = 0; }
    lastSimTime = simTime;
    manageFish(simDt);
    recordWakes();

    //360: a whole turn every period (the longer the range, the longer the sound takes to come back)
    const irr::f32 period = irr::core::max_(1.2f, 2.0f * range360m() / SOUND_SPEED * 1.5f);
    beamAccumulator += dt * BEAMS / period;
    int steps = (int)beamAccumulator;
    beamAccumulator -= (irr::f32)steps;
    steps = irr::core::min_(steps, 16);   //a long frame: the turn waits rather than the simulator
    for (int s = 0; s < steps; s++) {
        lastBeam = (lastBeam + 1) % BEAMS;
        computeBeam(lastBeam);
    }
    if (steps > 0) { serial360++; }

    //Fish finder: the next ping when the echo of the last one is back
    const irr::f32 pingInterval = irr::core::max_(0.07f, 2.0f * sounderRange() / SOUND_SPEED + 0.05f);
    pingTimer += dt;
    for (int n = 0; n < 4 && pingTimer >= pingInterval; n++) {
        pingTimer -= pingInterval;
        ping();
    }
    if (pingTimer >= pingInterval) { pingTimer = 0; }   //(a long stall: the pings it missed are lost)
}

//-------------------------------------------------------------------------------------------------
//360 degree search sonar: one beam
//-------------------------------------------------------------------------------------------------
void Sonar::computeBeam(int b)
{
    const irr::f32 bearing = (b + 0.5f) * (360.0f / BEAMS);
    const irr::f32 sinB = std::sin(bearing * irr::core::DEGTORAD), cosB = std::cos(bearing * irr::core::DEGTORAD);
    const irr::f32 range = range360m();
    const irr::f32 binLen = range / BINS;
    const irr::f32 tau = tiltDeg * irr::core::DEGTORAD;
    const irr::f32 halfV = 0.5f * VERTICAL_BEAM_360 * irr::core::DEGTORAD;
    const irr::f32 sinLo = std::sin(tau - halfV), sinHi = std::sin(tau + halfV), cosT = std::cos(tau);
    const irr::f32 surfaceBelowTransducer = -(tide - transducerY);   //depth of the surface below the transducer (negative)

    irr::f32 s[BINS];
    int blockedAt = BINS;
    irr::f32 prevDepth = -1e9f;
    for (int i = 0; i < BINS; i++) {
        const irr::f32 r = (i + 0.5f) * binLen;
        const irr::f32 d = r * cosT;
        const irr::f32 ax = ownX + d * sinB, az = ownZ + d * cosB;
        const irr::f32 zlo = r * sinLo, zhi = r * sinHi;
        const irr::f32 bottom = transducerY - seabedY(ax, az);   //below the transducer
        irr::f32 v = 0.015f;
        if (bottom < surfaceBelowTransducer + 0.2f) {
            //The coast: a hard wall
            v = 1.0f;
            blockedAt = i + 1;
            s[i] = v;
            break;
        }
        if (bottom <= zlo) {
            //Ground rising into the whole beam
            v = 0.75f + 0.25f * hardness(ax, az);
            blockedAt = i + 1;
            s[i] = v;
            break;
        }
        if (bottom < zhi) {
            //The bottom in the beam: stronger the more of the beam it takes, and on rising ground
            const irr::f32 frac = (zhi - bottom) / irr::core::max_(zhi - zlo, 0.01f);
            v = hardness(ax, az) * (0.3f + 0.6f * frac);
            if (prevDepth > -1e8f && bottom < prevDepth - 0.05f * binLen) { v *= 1.35f; }
            v *= 0.8f + 0.4f * random01();
        }
        //The scattering layer (plankton, bait fish) where the beam crosses it
        if (fishOn && bottom > zhi) {
            const irr::f32 layer = transducerY - (tide - layerDepth(ax, az));
            if (layer > zlo - 3.0f && layer < zhi + 3.0f) { v += 0.09f * fishDensity * random01(); }
        }
        //The sea surface in the beam: reverberation, more with the sea
        if (zlo < surfaceBelowTransducer + 1.5f) {
            const irr::f32 surf = (0.04f + seaState / 12.0f * 0.5f) * std::exp(-r / (0.25f * range + 20.0f));
            v += surf * random01();
        }
        prevDepth = bottom;
        s[i] = v;
    }
    for (int i = blockedAt; i < BINS; i++) { s[i] = 0.0f; }   //behind the ground: nothing

    //Targets in the beam: added at their range, if not behind the ground. Returns the last bin it
    //took (-1: not in the beam).
    auto addTarget = [&](irr::f32 tx, irr::f32 tz, irr::f32 radius, irr::f32 zTop, irr::f32 zBottom, irr::f32 strength) -> int {
        const irr::f32 dx = tx - ownX, dz = tz - ownZ;
        const irr::f32 h = std::sqrt(dx * dx + dz * dz);
        if (h > range * 1.05f + radius) { return -1; }
        const irr::f32 tb = std::atan2(dx, dz) * irr::core::RADTODEG;
        const irr::f32 diff = angleDiff(tb, bearing) * irr::core::DEGTORAD;
        const irr::f32 along = h * std::cos(diff), lateral = std::fabs(h * std::sin(diff));
        if (along < -radius) { return -1; }
        const irr::f32 halfW = irr::core::max_(along, 0.0f) * std::tan(0.5f * HORIZONTAL_BEAM_360 * irr::core::DEGTORAD) + radius + 1.0f;
        if (lateral > halfW) { return -1; }
        //Vertically: the target's depths against the beam's at that range
        const irr::f32 zMid = 0.5f * (zTop + zBottom);
        const irr::f32 slant = std::sqrt(along * along + zMid * zMid);
        const irr::f32 lo = slant * sinLo, hi = slant * sinHi;
        if (zBottom < lo || zTop > hi) { return -1; }
        const irr::f32 w = (1.0f - 0.6f * lateral / halfW) * strength;
        const int i0 = irr::core::max_(0, (int)((slant - radius) / binLen));
        const int i1 = irr::core::min_(BINS - 1, (int)((slant + radius) / binLen));
        for (int i = i0; i <= i1 && i < blockedAt; i++) {
            s[i] += w * (0.7f + 0.6f * random01());
        }
        return (i0 < blockedAt) ? irr::core::min_(i1, blockedAt - 1) : -1;
    };
    for (size_t k = 0; k < fish.size(); k++) {
        const Fish& f = fish[k];
        const irr::f32 fy = tide - f.depth;
        const irr::f32 z = transducerY - fy;
        addTarget(f.x, f.z, f.school ? f.radius : 0.5f, z - f.thick, z + f.thick, f.school ? 0.75f * f.strength : 0.45f * f.strength);
    }
    for (size_t k = 0; k < ships.size(); k++) {
        const Ship& sh = ships[k];
        const irr::f32 top = transducerY - sh.y;
        addTarget(sh.x, sh.z, 0.5f * sh.length * 0.8f, top, top + sh.draft, 1.0f);
    }
    for (size_t k = 0; k < wakes.size(); k++) {
        for (size_t j = 0; j < wakes[k].size(); j++) {
            const WakePoint& p = wakes[k][j];
            const irr::f32 age = simTime - p.t;
            const irr::f32 top = transducerY - tide;
            addTarget(p.x, p.z, 4.0f + 0.1f * age, top, top + 2.5f, 0.45f * p.strength * (1.0f - age / 90.0f));
        }
    }
    for (size_t k = 0; k < wrecks.size(); k++) {
        const Wreck& w = wrecks[k];
        const irr::f32 bottom = transducerY - seabedY(w.x, w.z);
        const int last = addTarget(w.x, w.z, 0.4f * w.length, bottom - w.height, bottom, 1.1f);
        if (last >= 0) {
            //Its acoustic shadow on the bottom behind it: longer the lower the beam grazes
            const irr::f32 shadow = w.height * irr::core::max_(range * 0.1f, 1.0f) / irr::core::max_(bottom, 2.0f);
            const int n = irr::core::min_((int)(shadow / binLen) + 2, (int)BINS);
            for (int i = last + 1; i < BINS && i <= last + n; i++) { s[i] *= 0.12f; }
        }
    }

    //Gain, and the speckle of the receiver
    const irr::f32 gain = 0.35f + 0.14f * gain360v;
    for (int i = 0; i < BINS; i++) {
        irr::f32 v = s[i] * gain + 0.035f * gain * random01();
        v = irr::core::clamp(v, 0.0f, 1.0f);
        beams[b * BINS + i] = (irr::u8)(v * 255.0f);
    }
}

//-------------------------------------------------------------------------------------------------
//Fish finder: one ping
//-------------------------------------------------------------------------------------------------
void Sonar::ping()
{
    //The cone under the ship, and the bottom it covers
    const irr::f32 half = (freq50 ? 22.0f : 6.0f) * irr::core::DEGTORAD;
    const irr::f32 dCentre = transducerY - seabedY(ownX, ownZ);
    irr::f32 dMin = dCentre, dMax = dCentre;
    const irr::f32 rho = irr::core::max_(dCentre, 0.5f) * std::tan(half);
    for (int k = 0; k < 8; k++) {
        const irr::f32 a = k * 45.0f * irr::core::DEGTORAD;
        for (int ring = 1; ring <= 2; ring++) {
            const irr::f32 rr = rho * ring * 0.5f;
            const irr::f32 d = transducerY - seabedY(ownX + rr * std::sin(a), ownZ + rr * std::cos(a));
            const irr::f32 slant = std::sqrt(d * d + rr * rr);
            dMin = irr::core::min_(dMin, slant);
            dMax = irr::core::max_(dMax, d);
        }
    }
    const irr::f32 hard = hardness(ownX, ownZ);
    depthNow = (dCentre > 0) ? dCentre + TRANSDUCER_DEPTH : 0.0f;

    //Automatic range: the bottom in the lower part of the picture
    if (autoRange && dCentre > 0) {
        const irr::f32 R = sounderRange();
        if (dMax > 0.92f * R || dMax < 0.35f * R) {
            int i = 0;
            while (i < SOUNDER_RANGE_COUNT - 1 && sounderRangeValue(i) < dMax * 1.3f) { i++; }
            setSounderRangeIndex(i);
        }
    }
    const irr::f32 R = sounderRange();
    const irr::f32 rowLen = R / ROWS;

    std::vector<irr::f32> col(ROWS, 0.0f);
    auto addBand = [&](irr::f32 z0, irr::f32 z1, irr::f32 v, bool speckle) {
        int r0 = (int)(z0 / rowLen), r1 = (int)(z1 / rowLen);
        if (r1 < 0 || r0 >= ROWS) { return; }
        r0 = irr::core::max_(r0, 0);
        r1 = irr::core::min_(r1, ROWS - 1);
        for (int r = r0; r <= r1; r++) { col[r] += speckle ? v * (0.6f + 0.8f * random01()) : v; }
    };

    //The transmission and the surface: the band at the top, deeper with the sea
    addBand(0.0f, 0.5f + 0.08f * R / 10.0f, 0.9f, false);
    const irr::f32 bubbles = 0.3f + seaState * 0.35f + ownSpeed * 0.08f;
    for (int r = 0; r < ROWS; r++) {
        const irr::f32 z = (r + 0.5f) * rowLen;
        if (z > bubbles * 2.5f) { break; }
        col[r] += 0.45f * std::exp(-z / irr::core::max_(bubbles, 0.1f)) * random01();
    }

    //Fish: schools as blobs, single fish at their slant range (arches as the boat goes over them)
    for (size_t k = 0; k < fish.size(); k++) {
        const Fish& f = fish[k];
        const irr::f32 dx = f.x - ownX, dz = f.z - ownZ;
        const irr::f32 h = std::sqrt(dx * dx + dz * dz);
        const irr::f32 z = transducerY - (tide - f.depth);
        if (z <= 0.3f) { continue; }
        const irr::f32 off = irr::core::max_(0.0f, h - f.radius);
        const irr::f32 angle = std::atan2(off, z);
        if (angle > half) { continue; }
        const irr::f32 w = 1.0f - 0.7f * angle / half;
        const irr::f32 slant = std::sqrt(off * off + z * z);
        if (f.school) {
            const irr::f32 thick = f.thick * irr::core::clamp(1.0f - (off / irr::core::max_(z * std::tan(half), 1.0f)) * 0.5f, 0.3f, 1.0f);
            addBand(slant - thick, slant + thick, 0.8f * f.strength * w, true);
        }
        else {
            addBand(slant - 0.25f, slant + 0.25f, 0.9f * f.strength * w, false);
        }
    }

    //The scattering layer: a faint grainy band in mid-water
    if (fishOn && dCentre > 4.0f) {
        const irr::f32 layer = transducerY - (tide - layerDepth(ownX, ownZ));
        const irr::f32 thick = 1.5f + 2.5f * valueNoise(ownX, ownZ, 300.0f, 41u);
        int r0 = (int)((layer - thick) / rowLen), r1 = (int)((layer + thick) / rowLen);
        r0 = irr::core::max_(r0, 0);
        r1 = irr::core::min_(r1, ROWS - 1);
        for (int r = r0; r <= r1; r++) {
            if (random01() < 0.4f) { col[r] += 0.28f * fishDensity * random01(); }
        }
    }

    //Wrecks under the cone: from their top to the bottom
    for (size_t k = 0; k < wrecks.size(); k++) {
        const Wreck& wr = wrecks[k];
        const irr::f32 dx = wr.x - ownX, dz = wr.z - ownZ;
        const irr::f32 h = std::sqrt(dx * dx + dz * dz);
        if (h > 0.5f * wr.length + rho) { continue; }
        const irr::f32 bottom = transducerY - seabedY(wr.x, wr.z);
        const irr::f32 top = bottom - wr.height * (1.0f - irr::core::clamp(h / (0.5f * wr.length + rho), 0.0f, 1.0f) * 0.6f);
        addBand(top, bottom, 1.0f, true);
    }

    //Wakes crossed: bubbles deep under the surface
    for (size_t k = 0; k < wakes.size(); k++) {
        for (size_t j = 0; j < wakes[k].size(); j++) {
            const WakePoint& p = wakes[k][j];
            const irr::f32 dx = p.x - ownX, dz = p.z - ownZ;
            const irr::f32 age = simTime - p.t;
            if (k == 0 && age < 6.0f) { continue; } //(own wake: behind the transducer)
            if (dx * dx + dz * dz > 100.0f) { continue; }
            addBand(0.0f, 1.0f + 3.0f * p.strength, 0.5f * p.strength * (1.0f - age / 90.0f), true);
        }
    }

    //The bottom: its first edge strong, the slope under the cone widens it, the hardness sets the
    //tail (soft mud lets the sound in); a hard bottom sends a second echo at twice the depth
    if (dCentre > 0) {
        const irr::f32 tail = (0.3f + (1.0f - hard) * 0.9f + (freq50 ? 0.8f : 0.2f)) * irr::core::max_(1.0f, R / 25.0f);
        addBand(dMin, dMin + irr::core::max_(rowLen * 2.0f, 0.15f), 1.25f, false);
        addBand(dMin, dMax, 0.85f + 0.3f * hard, true);
        const int r0 = (int)(dMax / rowLen);
        const int n = irr::core::max_(1, (int)(tail / rowLen));
        for (int r = r0; r < ROWS && r < r0 + n * 3; r++) {
            const irr::f32 t = (irr::f32)(r - r0) / n;
            col[r] += (0.75f * hard + 0.25f) * std::exp(-t) * (0.7f + 0.6f * random01());
        }
        if (hard > 0.55f && gainSounder >= 5) {
            addBand(2.0f * dCentre, 2.0f * dCentre + irr::core::max_(rowLen * 2.0f, 0.3f), 0.25f + 0.3f * (hard - 0.55f) + 0.04f * (gainSounder - 5), true);
        }
    }

    //The receiver: speckle, the own engine at speed, gain, clutter
    const irr::f32 gain = 0.35f + 0.13f * gainSounder;
    const irr::f32 engineNoise = (ownSpeed > 3.0f && random01() < 0.1f) ? 0.07f * (ownSpeed / 8.0f) : 0.0f;
    for (int r = 0; r < ROWS; r++) {
        irr::f32 v = col[r] * gain + (0.03f + engineNoise) * gain * random01();
        v -= 0.035f * clutterLevel;
        col[r] = irr::core::clamp(v, 0.0f, 1.0f);
    }

    //Into the column being made (several pings per column when scrolling slowly: the strongest)
    for (int r = 0; r < ROWS; r++) { accumulated[r] = (pingsInColumn == 0) ? col[r] : irr::core::max_(accumulated[r], col[r]); }
    pingsInColumn++;
    if (pingsInColumn < scrollDiv) { return; }
    pingsInColumn = 0;

    Column& c = echogram[head];
    for (int r = 0; r < ROWS; r++) { c.echo[r] = (irr::u8)(accumulated[r] * 255.0f); }
    c.range = R;
    c.bottom = depthNow;
    c.rangeChanged = pendingRangeChange;
    pendingRangeChange = false;
    head = (head + 1) % COLS;
    if (columnsFilled < (irr::u32)COLS) { columnsFilled++; }
    pingSerial++;
}
