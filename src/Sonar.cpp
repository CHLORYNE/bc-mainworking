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

    //TVG: the receiver's gain at a fraction f (0..1) of the range, for a setting 0..10 (5: 1 everywhere)
    inline irr::f32 tvgFactor(int tvg, irr::f32 f)
    {
        return std::exp((tvg - 5) * 0.25f * (f - 0.3f));
    }
    //Echo level: the shown strength of an echo v (0..1), for a setting 0..10 (5: v itself). Above 5 the
    //weak echoes are lifted (v^0.5 at 10), below 5 they are pushed down (v^2 at 0); 0 and 1 stay.
    inline irr::f32 echoLevel(int level, irr::f32 v)
    {
        v = irr::core::clamp(v, 0.0f, 1.0f);
        if (level == 5 || v <= 0.0f) { return v; }
        return std::pow(v, std::pow(2.0f, (5 - level) / 5.0f));
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
      heaveComp(true), heave(0), stillDepth(0), autoRangeTimer(0),
      simTime(0), lastSimTime(-1), lastWakeTime(-1000), fishOn(true), fishDensity(1.0f), fishCheckTime(0),
      beams(BEAMS * BINS, 0), range360(3), tiltDeg(10.0f), gain360v(5), serial360(0),
      txStarted(false), txTime(0), txRange(500.0f), txTilt(10.0f), frontBin(0),
      targetEcho(BEAMS * BINS, 0.0f), shadowMask(BEAMS * BINS, 0),
      echogram(COLS), head(0), columnsFilled(0), pingSerial(0), autoRange(true), sounderRangeIdx(4), gainSounder(5),
      scrollDiv(1), clutterLevel(1), pingTimer(0), pingsInColumn(0),
      depthNow(0), pendingRangeChange(false)
{
    accumulated[0].assign(ROWS, 0.0f);
    accumulated[1].assign(ROWS, 0.0f);
    for (int b = 0; b < BEAMS; b++) { beamBlocked[b] = BINS; beamPrevDepth[b] = -1e9f; beamTail[b] = 0.0f; }
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
//A new range clears the picture and starts a transmission at once; a new tilt is used from the next one
void Sonar::setRangeIndex360(int i) { range360 = irr::core::clamp(i, 0, RANGE_COUNT - 1); clear360(); txStarted = false; }
void Sonar::setTilt(irr::f32 deg) { tiltDeg = irr::core::clamp(deg, -5.0f, 90.0f); }
void Sonar::setGain360(int g) { gain360v = irr::core::clamp(g, 0, 10); }
void Sonar::setTvg360(int t) { tvg360v = irr::core::clamp(t, 0, 10); }
void Sonar::setLevel360(int l) { level360v = irr::core::clamp(l, 0, 10); }
void Sonar::clear360() { std::fill(beams.begin(), beams.end(), (irr::u8)0); serial360++; }

irr::f32 Sonar::sounderRangeValue(int i) { return RANGES_SOUNDER[irr::core::clamp(i, 0, SOUNDER_RANGE_COUNT - 1)]; }
void Sonar::setSounderRangeIndex(int i)
{
    i = irr::core::clamp(i, 0, SOUNDER_RANGE_COUNT - 1);
    if (i != sounderRangeIdx) { pendingRangeChange = true; }
    sounderRangeIdx = i;
}
void Sonar::setSounderGain(int g) { gainSounder = irr::core::clamp(g, 0, 10); }
void Sonar::setSounderTvg(int t) { tvgSounderv = irr::core::clamp(t, 0, 10); }
void Sonar::setSounderLevel(int l) { levelSounderv = irr::core::clamp(l, 0, 10); }
void Sonar::setFishAlarmBand(irr::f32 minDepth, irr::f32 maxDepth)
{
    alarmMin = irr::core::clamp(minDepth, 0.0f, 1000.0f);
    alarmMax = irr::core::clamp(maxDepth, alarmMin + 1.0f, 1200.0f);
}

irr::f32 Sonar::listenBearingNow() const
{
    if (listenMode == LISTEN_MARKER && listenMarkerOn) { return listenMarkerBearing; }
    if (listenMode == LISTEN_SCAN) { return std::fmod(ownHeading + scanBearing + 360.0f, 360.0f); }
    return ownHeading;
}

//The loudspeaker's echo from cell (b, i), at a shown strength 'level': its kind (the target that
//sent most of it, else the ground) and its pitch, moved by the closing speed as a real sonar's audio
//is (the received frequency shifted by 2 v / c of the transmitted one, heard on the audio tone)
void Sonar::addEchoSound(int b, int i, irr::f32 level)
{
    const irr::f32 bearing = (b + 0.5f) * (360.0f / BEAMS) * irr::core::DEGTORAD;
    const irr::f32 ux = std::sin(bearing), uz = std::cos(bearing);   //towards the echo
    const irr::f32 ownVx = ownSpeed * std::sin(ownCourse * irr::core::DEGTORAD);
    const irr::f32 ownVz = ownSpeed * std::cos(ownCourse * irr::core::DEGTORAD);
    int kind = SOUND_GROUND;
    irr::f32 vx = 0, vz = 0;
    const int cell = b * BINS + i;
    const int owner = targetOwner.empty() ? -1 : targetOwner[cell];
    if (owner >= 0 && owner < (int)txTargets.size() && targetOwnerEcho[cell] > 0.1f) {
        kind = txTargets[owner].kind;
        vx = txTargets[owner].vx;
        vz = txTargets[owner].vz;
    }
    //Closing speed: positive when the echo's source and the ship come together
    const irr::f32 closing = -((vx - ownVx) * ux + (vz - ownVz) * uz);
    const irr::f32 TX_FREQUENCY = 26000.0f;   //a search sonar's (Hz)
    const irr::f32 AUDIO_TONE = 1030.0f;      //the loudspeaker's ping (Sound)
    const irr::f32 shift = 2.0f * closing / SOUND_SPEED * TX_FREQUENCY;
    AudioEvent e;
    e.gain = level;
    e.pitch = irr::core::clamp((AUDIO_TONE + shift) / AUDIO_TONE, 0.75f, 1.3f);
    e.kind = kind;
    audioEvents.push_back(e);
}
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
    heave = model->getOwnShipHeave();
    stillDepth = model->getDepth();
    transducerY = (heaveComp ? own.Y - heave : own.Y) - TRANSDUCER_DEPTH;
    tide = model->getTideHeight();
    seaState = model->getWeather();
    ownHeading = model->getHeading();
    ownSpeed = model->getSOG();
    ownCourse = model->getCOG();
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
        s.speed = model->getOtherShipSpeed((int)i);
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
    const irr::f32 relief = (valueNoise(ax, az, 22.0f, 21u) - 0.5f) * 0.5f + (valueNoise(ax, az, 6.0f, 22u) - 0.5f) * 0.2f;
    return terrain + relief * (0.35f + hard) / 1.35f;
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

    //360: the echoes of the transmission come back from the ship outwards; the next one goes when
    //they are all in (plus the time the sonar takes between two)
    if (!txStarted) { startTransmission(); }
    txTime += dt;
    const irr::f32 echoTime = 2.0f * txRange / SOUND_SPEED;
    int want = irr::core::min_((int)BINS, (int)(txTime / echoTime * BINS) + 1);
    want = irr::core::min_(want, frontBin + 24);   //a long frame: the front waits rather than the simulator
    for (int i = frontBin; i < want; i++) {
        for (int b = 0; b < BEAMS; b++) { computeCell(b, i); }
    }
    //The loudspeaker: the one echo of this transmission, as the front reaches it, as loud as it shows
    //(gain, TVG and echo level included); a target too weak on the picture is not heard
    if (echoPending && echoBin < want) {
        echoPending = false;
        const irr::f32 level = beams[echoBeam * BINS + echoBin] / 255.0f;
        if (level > 0.3f) { addEchoSound(echoBeam, echoBin, level); }
    }
    audioClock += dt;
    alarmBeepTimer = irr::core::max_(0.0f, alarmBeepTimer - dt);
    if (want > frontBin) { serial360++; }
    frontBin = irr::core::max_(frontBin, want);
    if (frontBin >= BINS && txTime >= echoTime + 0.25f) { startTransmission(); }

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
//360 degree sonar: a transmission
//-------------------------------------------------------------------------------------------------
void Sonar::startTransmission()
{
    //The ping, in the loudspeaker (at most every AUDIO_PING_INTERVAL); a scanning listening beam moves
    //on by its width at each ping heard
    const irr::f32 AUDIO_PING_INTERVAL = 2.5f;
    txAudible = audioClock >= AUDIO_PING_INTERVAL;
    echoPending = false;
    if (txAudible) {
        audioClock = 0.0f;
        AudioEvent e;
        e.gain = 1.0f;
        e.pitch = 1.0f;
        e.kind = SOUND_PING;
        audioEvents.push_back(e);
        if (listenMode == LISTEN_SCAN) { scanBearing = std::fmod(scanBearing + listenBeamWidth(), 360.0f); }
    }
    txStarted = true;
    txTime = 0;
    frontBin = 0;
    txRange = range360m();
    txTilt = tiltDeg;
    std::fill(targetEcho.begin(), targetEcho.end(), 0.0f);
    std::fill(shadowMask.begin(), shadowMask.end(), (irr::u8)0);
    for (int b = 0; b < BEAMS; b++) { beamBlocked[b] = BINS; beamPrevDepth[b] = -1e9f; beamTail[b] = 0.0f; }

    //The targets, laid into every beam they are in: round echoes, strongest at their centre
    struct Target { irr::f32 h, bearing, radius, zTop, zBottom, strength; bool wreck; irr::f32 wreckBottom, wreckHeight; };
    std::vector<Target> targets;
    txTargets.clear();
    targetOwner.assign(BEAMS * BINS, (irr::s16)-1);
    targetOwnerEcho.assign(BEAMS * BINS, 0.0f);
    //(kind: the loudspeaker's sound for it; vx, vz: its velocity, m/s, for the Doppler)
    auto add = [&](irr::f32 tx, irr::f32 tz, irr::f32 radius, irr::f32 zTop, irr::f32 zBottom, irr::f32 strength, bool wreck,
        int kind = SOUND_SCHOOL, irr::f32 vx = 0, irr::f32 vz = 0) {
        const irr::f32 dx = tx - ownX, dz = tz - ownZ;
        const irr::f32 h = std::sqrt(dx * dx + dz * dz);
        if (h > txRange * 1.05f + radius || strength <= 0.0f) { return; }
        TxTarget tt = { kind, tx, tz, vx, vz };
        txTargets.push_back(tt);
        Target t;
        t.h = h;
        t.bearing = std::atan2(dx, dz) * irr::core::RADTODEG;
        t.radius = radius;
        t.zTop = zTop;
        t.zBottom = zBottom;
        t.strength = strength;
        t.wreck = wreck;
        t.wreckBottom = zBottom;
        t.wreckHeight = zBottom - zTop;
        targets.push_back(t);
    };
    for (size_t k = 0; k < fish.size(); k++) {
        const Fish& f = fish[k];
        const irr::f32 z = transducerY - (tide - f.depth);
        add(f.x, f.z, f.school ? f.radius : 0.6f, z - f.thick, z + f.thick, f.school ? 0.8f * f.strength : 0.4f * f.strength, false,
            SOUND_SCHOOL, f.speed * std::sin(f.heading * irr::core::DEGTORAD), f.speed * std::cos(f.heading * irr::core::DEGTORAD));
    }
    for (size_t k = 0; k < ships.size(); k++) {
        const Ship& sh = ships[k];
        const irr::f32 top = transducerY - sh.y;
        add(sh.x, sh.z, 0.4f * sh.length, top, top + sh.draft, 1.0f, false,
            SOUND_HULL, sh.speed * std::sin(sh.heading * irr::core::DEGTORAD), sh.speed * std::cos(sh.heading * irr::core::DEGTORAD));
    }
    for (size_t k = 0; k < wakes.size(); k++) {
        for (size_t j = 0; j < wakes[k].size(); j++) {
            const WakePoint& p = wakes[k][j];
            const irr::f32 age = simTime - p.t;
            const irr::f32 top = transducerY - tide;
            add(p.x, p.z, 4.0f + 0.1f * age, top, top + 2.5f, 0.45f * p.strength * (1.0f - age / 90.0f), false);
        }
    }
    for (size_t k = 0; k < wrecks.size(); k++) {
        const Wreck& w = wrecks[k];
        const irr::f32 bottom = transducerY - seabedY(w.x, w.z);
        add(w.x, w.z, 0.4f * w.length, bottom - w.height, bottom, 1.1f, true, SOUND_HULL);
    }

    const irr::f32 binLen = txRange / BINS;
    const irr::f32 tau = txTilt * irr::core::DEGTORAD;
    const irr::f32 halfV = 0.5f * VERTICAL_BEAM_360 * irr::core::DEGTORAD;
    const irr::f32 sinLo = std::sin(tau - halfV), sinHi = std::sin(tau + halfV);
    const irr::f32 tanH = std::tan(0.5f * HORIZONTAL_BEAM_360 * irr::core::DEGTORAD);
    for (size_t k = 0; k < targets.size(); k++) {
        const Target& t = targets[k];
        for (int b = 0; b < BEAMS; b++) {
            const irr::f32 bearing = (b + 0.5f) * (360.0f / BEAMS);
            const irr::f32 diff = angleDiff(t.bearing, bearing) * irr::core::DEGTORAD;
            if (std::fabs(diff) > 1.6f) { continue; }
            const irr::f32 along = t.h * std::cos(diff), lateral = std::fabs(t.h * std::sin(diff));
            if (along < -t.radius) { continue; }
            const irr::f32 halfW = irr::core::max_(along, 0.0f) * tanH + t.radius + 0.5f;
            if (lateral > halfW) { continue; }
            //Vertically: how much of the target the beam takes at its range
            const irr::f32 zMid = 0.5f * (t.zTop + t.zBottom);
            const irr::f32 slant = std::sqrt(along * along + zMid * zMid);
            const irr::f32 lo = slant * sinLo, hi = slant * sinHi;
            const irr::f32 overlap = irr::core::min_(hi, t.zBottom) - irr::core::max_(lo, t.zTop);
            if (overlap <= 0.0f) { continue; }
            const irr::f32 vfrac = irr::core::min_(1.0f, overlap / irr::core::max_(t.zBottom - t.zTop, 0.5f) + 0.3f);
            const irr::f32 lw = 1.0f - (lateral / halfW) * (lateral / halfW);
            const irr::f32 rad = irr::core::max_(t.radius, binLen);
            const int i0 = irr::core::max_(0, (int)((slant - rad) / binLen));
            const int i1 = irr::core::min_(BINS - 1, (int)((slant + rad) / binLen));
            for (int i = i0; i <= i1; i++) {
                const irr::f32 u = ((i + 0.5f) * binLen - slant) / rad;
                const irr::f32 rw = irr::core::max_(0.0f, 1.0f - u * u);
                const irr::f32 e = t.strength * lw * rw * vfrac * (0.8f + 0.4f * random01());
                targetEcho[b * BINS + i] += e;
                if (e > targetOwnerEcho[b * BINS + i]) { targetOwnerEcho[b * BINS + i] = e; targetOwner[b * BINS + i] = (irr::s16)k; }
            }
            if (t.wreck) {
                //Its acoustic shadow on the bottom behind it: longer the lower the beam grazes
                const irr::f32 shadow = t.wreckHeight * slant / irr::core::max_(t.wreckBottom, 2.0f);
                const int n = irr::core::min_((int)(shadow / binLen) + 2, (int)BINS);
                for (int i = i1 + 1; i < BINS && i <= i1 + n; i++) { shadowMask[b * BINS + i] = 1; }
            }
        }
    }

    //The echo the loudspeaker will play: the strongest target in the listening beam (not hidden behind
    //a wreck), past the transmission round the ship
    if (txAudible) {
        const irr::f32 listen = listenBearingNow();
        irr::f32 best = 0.05f;
        for (int b = 0; b < BEAMS; b++) {
            const irr::f32 bearing = (b + 0.5f) * (360.0f / BEAMS);
            if (std::fabs(angleDiff(bearing, listen)) > 0.5f * listenBeamWidth()) { continue; }
            for (int i = BINS / 30; i < BINS; i++) {
                const int cell = b * BINS + i;
                if (!shadowMask[cell] && targetEcho[cell] > best) { best = targetEcho[cell]; echoBeam = b; echoBin = i; echoPending = true; }
            }
        }
    }
}

//One bin of one beam: the seabed (or the coast) in the beam, the water, the surface, the targets
void Sonar::computeCell(int b, int i)
{
    const irr::f32 gain = 0.35f + 0.14f * gain360v;
    if (i >= beamBlocked[b]) {
        //Behind the ground: the bottom echo fades out over some metres (the beam's soft edges, the
        //length of the pulse), then only the receiver's noise
        const irr::f32 tailBins = irr::core::max_(3.0f, BINS * 0.05f);
        const irr::f32 fade = beamTail[b] * std::exp(-(irr::f32)(i - beamBlocked[b] + 1) / tailBins);
        const irr::f32 v = (fade * (0.8f + 0.4f * random01()) * gain + 0.03f * gain * random01()) * tvgFactor(tvg360v, (i + 0.5f) / BINS);
        beams[b * BINS + i] = (irr::u8)(echoLevel(level360v, v) * 255.0f);
        return;
    }
    const irr::f32 bearing = (b + 0.5f) * (360.0f / BEAMS);
    const irr::f32 binLen = txRange / BINS;
    const irr::f32 tau = txTilt * irr::core::DEGTORAD;
    const irr::f32 halfV = 0.5f * VERTICAL_BEAM_360 * irr::core::DEGTORAD;
    const irr::f32 r = (i + 0.5f) * binLen;
    const irr::f32 d = r * std::cos(tau);
    const irr::f32 ax = ownX + d * std::sin(bearing * irr::core::DEGTORAD), az = ownZ + d * std::cos(bearing * irr::core::DEGTORAD);
    const irr::f32 zlo = r * std::sin(tau - halfV), zhi = r * std::sin(tau + halfV);
    const irr::f32 surface = -(tide - transducerY);   //the surface, below the transducer (negative)
    const irr::f32 bottom = transducerY - seabedY(ax, az);

    irr::f32 v = 0.012f;
    if (bottom < surface + 0.2f) {
        //The coast: a hard wall, then nothing
        v = 1.0f;
        beamBlocked[b] = i + 1;
        beamTail[b] = 0.5f;
    }
    else if (bottom <= zlo) {
        //Ground rising into the whole beam
        v = 0.85f * (0.6f + 0.3f * (0.45f + 0.55f * hardness(ax, az)));
        beamBlocked[b] = i + 1;
        beamTail[b] = 0.7f * v;
    }
    else if (bottom < zhi) {
        //The bottom in the beam: stronger the more of the beam it takes, and on rising ground
        const irr::f32 frac = (zhi - bottom) / irr::core::max_(zhi - zlo, 0.01f);
        v = 0.85f * (0.45f + 0.55f * hardness(ax, az)) * (0.35f + 0.6f * frac);
        if (beamPrevDepth[b] > -1e8f && bottom < beamPrevDepth[b] - 0.05f * binLen) { v *= 1.3f; }
        v *= 0.88f + 0.24f * random01();
    }
    else if (fishOn) {
        //The scattering layer (plankton, bait fish) where the beam crosses it
        const irr::f32 layer = transducerY - (tide - layerDepth(ax, az));
        if (layer > zlo - 3.0f && layer < zhi + 3.0f) { v += 0.08f * fishDensity * random01(); }
    }
    beamPrevDepth[b] = bottom;
    //The sea surface in the beam: reverberation, more with the sea
    if (zlo < surface + 1.5f) {
        const irr::f32 surf = (0.05f + seaState / 12.0f * 0.5f) * std::exp(-r / (0.25f * txRange + 20.0f));
        v += surf * random01();
    }
    v += targetEcho[b * BINS + i];
    if (shadowMask[b * BINS + i]) { v *= 0.12f; }
    v = (v * gain + 0.03f * gain * random01()) * tvgFactor(tvg360v, (i + 0.5f) / BINS);
    beams[b * BINS + i] = (irr::u8)(echoLevel(level360v, v) * 255.0f);
}

//-------------------------------------------------------------------------------------------------
//Fish finder: one ping, at both frequencies
//-------------------------------------------------------------------------------------------------
void Sonar::ping()
{
    const irr::f32 dCentre = transducerY - seabedY(ownX, ownZ);
    //The reading: depth under the ship as the bridge echo sounder gives it (still water), plus the
    //heave when it is not compensated
    depthNow = (dCentre > 0) ? irr::core::max_(0.1f, stillDepth + (heaveComp ? 0.0f : heave)) : 0.0f;

    //Automatic range: the bottom (under the wide cone) in the lower part of the picture, changed
    //only once it has stayed out for a moment (not at every wave)
    if (autoRange && dCentre > 0) {
        irr::f32 dMax = dCentre;
        const irr::f32 rho = dCentre * std::tan(22.0f * irr::core::DEGTORAD);
        for (int k = 0; k < 8; k++) {
            const irr::f32 a = k * 45.0f * irr::core::DEGTORAD;
            dMax = irr::core::max_(dMax, transducerY - seabedY(ownX + rho * std::sin(a), ownZ + rho * std::cos(a)));
        }
        const irr::f32 R = sounderRange();
        if (dMax > 0.92f * R || dMax < 0.35f * R) {
            autoRangeTimer += 2.0f * R / SOUND_SPEED + 0.05f;
            if (autoRangeTimer > 1.5f || dMax > R) {
                int i = 0;
                while (i < SOUNDER_RANGE_COUNT - 1 && sounderRangeValue(i) < dMax * 1.3f) { i++; }
                setSounderRangeIndex(i);
                autoRangeTimer = 0;
            }
        }
        else {
            autoRangeTimer = 0;
        }
    }
    const irr::f32 R = sounderRange();

    std::vector<irr::f32> col(ROWS);
    irr::s16 bottomRow[2] = { -1, -1 };
    bool fishInBand = false;
    for (int f = 0; f < 2; f++) {
        computeColumn(f, R, col, bottomRow[f]);
        //Fish alarm: an echo in the band, clear of the bottom (a metre above its first edge)
        if (fishAlarmOn) {
            const irr::f32 rowLen = R / ROWS;
            int r0 = (int)(alarmMin / rowLen), r1 = (int)(alarmMax / rowLen);   //(the echogram's depth scale)
            if (bottomRow[f] >= 0) { r1 = irr::core::min_(r1, bottomRow[f] - 1 - (int)(1.0f / rowLen)); }
            r0 = irr::core::max_(r0, 6);   //(not the transmission's line at the top)
            r1 = irr::core::min_(r1, ROWS - 1);
            for (int r = r0; r <= r1 && !fishInBand; r++) { fishInBand = col[r] > 0.45f; }
        }
        //Into the column being made (several pings per column when scrolling slowly: the strongest)
        for (int r = 0; r < ROWS; r++) { accumulated[f][r] = (pingsInColumn == 0) ? col[r] : irr::core::max_(accumulated[f][r], col[r]); }
    }
    fishAlarmActive = fishInBand;
    if (fishInBand && alarmBeepTimer <= 0.0f) {
        AudioEvent e;
        e.gain = 1.0f;
        e.pitch = 1.0f;
        e.kind = SOUND_ALARM;
        audioEvents.push_back(e);
        alarmBeepTimer = 1.5f;   //beeping while the fish stay in the band
    }
    pingsInColumn++;
    if (pingsInColumn < scrollDiv) { return; }
    pingsInColumn = 0;

    Column& c = echogram[head];
    for (int f = 0; f < 2; f++) {
        for (int r = 0; r < ROWS; r++) { c.echo[f][r] = (irr::u8)(accumulated[f][r] * 255.0f); }
        c.bottomRow[f] = bottomRow[f];
    }
    c.range = R;
    c.bottom = depthNow;
    c.rangeChanged = pendingRangeChange;
    pendingRangeChange = false;
    head = (head + 1) % COLS;
    if (columnsFilled < (irr::u32)COLS) { columnsFilled++; }
    pingSerial++;
}

void Sonar::computeColumn(int f, irr::f32 R, std::vector<irr::f32>& col, irr::s16& bottomRow)
{
    const bool low = (f == F50);
    //Cone half angle; sound absorbed on the way (much more at 200 kHz); noise (more at 50 kHz)
    const irr::f32 half = (low ? 22.0f : 6.0f) * irr::core::DEGTORAD;
    const irr::f32 absorbLength = low ? 1500.0f : 220.0f;
    const irr::f32 noise = low ? 0.04f : 0.022f;
    const irr::f32 rowLen = R / ROWS;
    std::fill(col.begin(), col.end(), 0.0f);
    auto atten = [&](irr::f32 z) { return std::exp(-irr::core::max_(z, 0.0f) / absorbLength); };
    auto addBand = [&](irr::f32 z0, irr::f32 z1, irr::f32 v, bool speckle) {
        int r0 = (int)(z0 / rowLen), r1 = (int)(z1 / rowLen);
        if (r1 < 0 || r0 >= ROWS) { return; }
        r0 = irr::core::max_(r0, 0);
        r1 = irr::core::min_(r1, ROWS - 1);
        for (int r = r0; r <= r1; r++) { col[r] += speckle ? v * (0.6f + 0.8f * random01()) : v; }
    };

    //The cone under the ship, and the bottom it covers
    const irr::f32 dCentre = transducerY - seabedY(ownX, ownZ);
    irr::f32 dMin = dCentre, dMax = dCentre;
    const irr::f32 rho = irr::core::max_(dCentre, 0.5f) * std::tan(half);
    for (int k = 0; k < 8; k++) {
        const irr::f32 a = (k * 45.0f + (low ? 0.0f : 22.5f)) * irr::core::DEGTORAD;
        for (int ring = 1; ring <= 2; ring++) {
            const irr::f32 rr = rho * ring * 0.5f;
            const irr::f32 d = transducerY - seabedY(ownX + rr * std::sin(a), ownZ + rr * std::cos(a));
            dMin = irr::core::min_(dMin, std::sqrt(d * d + rr * rr));
            dMax = irr::core::max_(dMax, d);
        }
    }
    const irr::f32 hard = hardness(ownX, ownZ);

    //The zero line (the transmission: a thin line at the top, whatever the range) and the surface
    //below it: bubbles, deeper with the sea and the speed, more at 50 kHz
    addBand(0.0f, rowLen * 5.0f, 0.95f, false);
    const irr::f32 bubbles = irr::core::min_((0.3f + seaState * 0.15f + ownSpeed * 0.03f) * (low ? 1.0f : 0.7f), 2.5f);
    for (int r = 0; r < ROWS; r++) {
        const irr::f32 z = (r + 0.5f) * rowLen;
        if (z > bubbles * 2.5f) { break; }
        col[r] += (low ? 0.45f : 0.35f) * std::exp(-z / irr::core::max_(bubbles, 0.1f)) * random01();
    }

    //Fish: schools as blobs, single fish at their slant range (arches as the boat goes over them)
    for (size_t k = 0; k < fish.size(); k++) {
        const Fish& fi = fish[k];
        const irr::f32 dx = fi.x - ownX, dz = fi.z - ownZ;
        const irr::f32 h = std::sqrt(dx * dx + dz * dz);
        const irr::f32 z = transducerY - (tide - fi.depth);
        if (z <= 0.3f) { continue; }
        const irr::f32 off = irr::core::max_(0.0f, h - fi.radius);
        const irr::f32 angle = std::atan2(off, z);
        if (angle > half) { continue; }
        const irr::f32 w = (1.0f - 0.7f * angle / half) * atten(z);
        const irr::f32 slant = std::sqrt(off * off + z * z);
        if (fi.school) {
            const irr::f32 thick = fi.thick * irr::core::clamp(1.0f - (off / irr::core::max_(z * std::tan(half), 1.0f)) * 0.5f, 0.3f, 1.0f);
            addBand(slant - thick, slant + thick, 0.8f * fi.strength * w, true);
        }
        else {
            //(a fish reflects more at 200 kHz, and shows a finer mark)
            const irr::f32 t = low ? 0.25f : 0.15f;
            addBand(slant - t, slant + t, (low ? 0.85f : 1.0f) * fi.strength * w, false);
        }
    }

    //The scattering layer: a faint grainy band in mid-water (clearer at 200 kHz)
    if (fishOn && dCentre > 4.0f) {
        const irr::f32 layer = transducerY - (tide - layerDepth(ownX, ownZ));
        const irr::f32 thick = 1.5f + 2.5f * valueNoise(ownX, ownZ, 300.0f, 41u);
        int r0 = (int)((layer - thick) / rowLen), r1 = (int)((layer + thick) / rowLen);
        r0 = irr::core::max_(r0, 0);
        r1 = irr::core::min_(r1, ROWS - 1);
        for (int r = r0; r <= r1; r++) {
            if (random01() < 0.4f) { col[r] += (low ? 0.24f : 0.32f) * fishDensity * atten(layer) * random01(); }
        }
    }

    //Wrecks under the cone: from their top to the bottom
    for (size_t k = 0; k < wrecks.size(); k++) {
        const Wreck& wr = wrecks[k];
        const irr::f32 dx = wr.x - ownX, dz = wr.z - ownZ;
        const irr::f32 h = std::sqrt(dx * dx + dz * dz);
        if (h > 0.5f * wr.length + rho) { continue; }
        const irr::f32 bottom = transducerY - seabedY(wr.x, wr.z);
        //(an uneven outline: masts, superstructure, broken parts)
        const irr::f32 rough = 0.55f + 0.45f * valueNoise(ownX, ownZ, 3.0f, 51u);
        const irr::f32 top = bottom - wr.height * rough * (1.0f - irr::core::clamp(h / (0.5f * wr.length + rho), 0.0f, 1.0f) * 0.6f);
        addBand(top, bottom, 1.0f * atten(top), true);
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
    //tail (soft mud lets the sound in, more at 50 kHz); a hard bottom sends a second echo at twice
    //the depth
    if (dCentre > 0) {
        const irr::f32 a = atten(dCentre);
        const irr::f32 tail = (0.3f + (1.0f - hard) * 0.9f + (low ? 0.8f : 0.2f)) * irr::core::max_(1.0f, R / 25.0f);
        addBand(dMin, dMin + irr::core::max_(rowLen * 2.0f, 0.15f), 1.25f * a + 0.2f, false);
        addBand(dMin, dMax, (0.85f + 0.3f * hard) * a, true);
        const int r0 = (int)(dMax / rowLen);
        const int n = irr::core::max_(1, (int)(tail / rowLen));
        for (int r = r0; r < ROWS && r < r0 + n * 3; r++) {
            const irr::f32 t = (irr::f32)(r - r0) / n;
            col[r] += (0.75f * hard + 0.25f) * a * std::exp(-t) * (0.7f + 0.6f * random01());
        }
        if (hard > 0.55f && gainSounder >= 5) {
            addBand(2.0f * dCentre, 2.0f * dCentre + irr::core::max_(rowLen * 2.0f, 0.3f),
                (0.25f + 0.3f * (hard - 0.55f) + 0.04f * (gainSounder - 5)) * a * a, true);
        }
        const int br = (int)(dMin / rowLen);
        bottomRow = (br >= 0 && br < ROWS) ? (irr::s16)br : (irr::s16)-1;
    }
    else {
        bottomRow = -1;
    }

    //The receiver: speckle, the own engine at speed (more on the low frequency), gain, clutter
    const irr::f32 gain = 0.35f + 0.13f * gainSounder;
    const irr::f32 engineNoise = (ownSpeed > 3.0f && random01() < 0.1f) ? (low ? 0.07f : 0.04f) * (ownSpeed / 8.0f) : 0.0f;
    for (int r = 0; r < ROWS; r++) {
        irr::f32 v = (col[r] * gain + (noise + engineNoise) * gain * random01()) * tvgFactor(tvgSounderv, (r + 0.5f) / ROWS);
        v -= 0.035f * clutterLevel;
        col[r] = echoLevel(levelSounderv, v);
    }
}
