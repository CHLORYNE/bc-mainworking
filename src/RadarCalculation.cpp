#include "RadarCalculation.hpp"

#include "Terrain.hpp"
#include "OwnShip.hpp"
#include "Buoys.hpp"
#include "OtherShips.hpp"
#include "RadarData.hpp"
#include "Angles.hpp"
#include "Constants.hpp"
#include "IniFile.hpp"
#include "NumberToImage.hpp"
#include "Utilities.hpp"

#include <iostream>
#include <cmath>
#include <cstdlib> //For rand()
#include <algorithm> //For sort()

#ifdef WITH_PROFILING
#include "iprof.hpp"
#else
#define IPROF(a) //intentionally empty placeholder
#endif

////using namespace irr;

namespace {
    //Land surface reflectivity pattern, see landReflectivity().

    //Value of a lattice point, 0..1, from a 64-bit integer hash (splitmix64 finaliser). Pure integer
    //arithmetic, so a given patch of land gives the same echo on every machine and every run.
    irr::f32 latticeValue(int64_t ix, int64_t iz, uint64_t seed)
    {
        uint64_t h = (uint64_t)ix * 0x9E3779B97F4A7C15ULL;
        h ^= ((uint64_t)iz + seed) * 0xC2B2AE3D27D4EB4FULL;
        h ^= h >> 30; h *= 0xBF58476D1CE4E5B9ULL;
        h ^= h >> 27; h *= 0x94D049BB133111EBULL;
        h ^= h >> 31;
        return (irr::f32)(h >> 40) * (1.0f / 16777216.0f);
    }

    //Smoothly interpolated value noise, 0..1, with features about cellM metres across.
    irr::f32 valueNoise(double x, double z, double cellM, uint64_t seed)
    {
        const double fx = x / cellM;
        const double fz = z / cellM;
        const double floorX = std::floor(fx);
        const double floorZ = std::floor(fz);
        const int64_t ix = (int64_t)floorX;
        const int64_t iz = (int64_t)floorZ;
        irr::f32 tx = (irr::f32)(fx - floorX);
        irr::f32 tz = (irr::f32)(fz - floorZ);
        tx = tx * tx * (3.0f - 2.0f * tx);
        tz = tz * tz * (3.0f - 2.0f * tz);
        const irr::f32 v00 = latticeValue(ix, iz, seed);
        const irr::f32 v10 = latticeValue(ix + 1, iz, seed);
        const irr::f32 v01 = latticeValue(ix, iz + 1, seed);
        const irr::f32 v11 = latticeValue(ix + 1, iz + 1, seed);
        const irr::f32 a = v00 + (v10 - v00) * tx;
        const irr::f32 b = v01 + (v11 - v01) * tx;
        return a + (b - a) * tz;
    }

    //Reflectivity of the ground surface at absolute world position (x, z) in metres, as a factor on
    //the diffuse land return (1 = the old uniform value).
    //Real ground cover is patchy - bare sand, scrub, rock, walls, buildings - so the return from land
    //that does not face the radar varies by tens of dB from place to place. The heightmap has no such
    //detail, so this stands in for it: three octaves of value noise (45 m grain, 170 m patches, 650 m
    //areas), mapped to a log-normal-like spread. It is fixed to the geography, so the pattern stays on
    //the land as the ship moves instead of crawling. Tuned so that at gain 50 the inside of a landmass
    //is nearly solid within 1 Nm, mottled at 3-6 Nm and patchy at 12 Nm, as on a real X-band display.
    irr::f32 landReflectivity(double x, double z)
    {
        const irr::f32 LAND_TEXTURE_CONTRAST = 33.3f; //natural-log spread per unit of noise (std of the noise is about 0.13)
        const irr::f32 LAND_TEXTURE_BIAS = -3.26f;    //natural log of the median reflectivity
        const irr::f32 n = 0.45f * valueNoise(x, z, 45.0, 0x51ED2701ULL)
                         + 0.35f * valueNoise(x, z, 170.0, 0x7A3C9B15ULL)
                         + 0.20f * valueNoise(x, z, 650.0, 0x2F6E4D83ULL);
        return std::exp(LAND_TEXTURE_CONTRAST * (n - 0.5f) + LAND_TEXTURE_BIAS);
    }
}

RadarCalculation::RadarCalculation() : rangeResolution(128), angularResolution(360)
{

    //Initial values for controls, all 0-100:
    radarGain = 50;
    radarRainClutterReduction = 0;
    radarSeaClutterReduction = 0;

    currentRadarColourChoice = 0;

    eblBrg[0] = 0;     eblBrg[1] = 0;
    vrmRangeNm[0] = 0; vrmRangeNm[1] = 0;
    activeEBL = 0;
    activeVRM = 0;
    guardAlarmMode = 0;
    guardAlarmTimer = 0;
    guardZoneNeedsPrime = false;

    CursorRangeNm = 0;
    CursorBrg = 0;
    cursorRangeXNm = 0;
    cursorRangeYNm = 0;

    radarCursorsLastUpdated = clock();

    radarOn = true;

    //Radar modes: North up (false, false). Course up (true, true). Head up (true, false)
    headUp = false;
    stabilised = false;
    rangeRingBrightness = 0;   //kyara: anneaux visibles par défaut
    trueVectors = true;
    vectorLengthMinutes = 6;
    useRealHeading = false; //kyara: défaut = cap ARPA estimé (comportement radar réel)
    echoStretchLevel = 0; //kyara: échostretch désactivé au départ
    offsetXFraction = 0.0f; offsetYFraction = 0.0f; //kyara
    arpaMode = 0;
    showArpaOnBuoys = true; //Buoys shown as ARPA targets by default
    showBuoyTrails = false;
    showShipTrails = false;
    showOwnShipTrails = false;
    showMMSI = true;
    //Default marker colours: ships white, buoys cyan (so they're distinguishable).
    //These indices must match the combo box item order in GUIMain.
    shipContactColour = paletteIndexToColour(5); //White
    buoyContactColour = paletteIndexToColour(1); //Cyan
    // What's selected in the GUI arpa list
    arpaListSelection = -1;

    //Hard coded in GUI and here for 10 parallel index lines
    for (irr::u32 i = 0; i < 10; i++) {
        piBearings.push_back(0.0);
        piRanges.push_back(0.0);
    }

    landTexture = true;

    radarScreenStale = true;
    radarRadiusPx = 10; //Set to an arbitrary value initially, will be set later.

    currentScanAngle = 0;
    currentScanLine = 0;
}

RadarCalculation::~RadarCalculation()
{
    //dtor
}

void RadarCalculation::load(std::string radarConfigFile, irr::IrrlichtDevice* dev)
{
    device = dev;

    // radarConfigFile (the per-boat radar.ini) is intentionally ignored now.
    (void)radarConfigFile;

    // --- Resolution stays in bc5.ini so you can still tune radar CPU cost globally ---
    std::string userFolder = Utilities::getUserDir();
    std::string iniFilename = "bc5.ini";
    if (Utilities::pathExists(userFolder + iniFilename)) {
        iniFilename = userFolder + iniFilename;
    }
    rangeResolution = IniFile::iniFileTou32(iniFilename, "RADAR_RangeRes", 256);
    angularResolution = IniFile::iniFileTou32(iniFilename, "RADAR_AngularRes", 360);
    irr::u32 rangeResolution_max = IniFile::iniFileTou32(iniFilename, "RADAR_RangeRes_Max");
    irr::u32 angularResolution_max = IniFile::iniFileTou32(iniFilename, "RADAR_AngularRes_Max");
    if (rangeResolution < 1) { rangeResolution = 256; }
    if (angularResolution < 1) { angularResolution = 360; }
    if (rangeResolution_max > 0 && rangeResolution > rangeResolution_max) { rangeResolution = rangeResolution_max; }
    if (angularResolution_max > 0 && angularResolution > angularResolution_max) { angularResolution = angularResolution_max; }
    landTexture = (IniFile::iniFileTou32(iniFilename, "RADAR_LandTexture", 1) != 0);

    // --- Radar ranges (Nm), formerly RadarRange(1..N) ---
    radarRangeNm.clear();
    radarRangeNm.push_back(0.1f);  radarRangeNm.push_back(0.2f);  radarRangeNm.push_back(0.5f);
    radarRangeNm.push_back(1.0f);  radarRangeNm.push_back(1.5f);  radarRangeNm.push_back(3.0f);
    radarRangeNm.push_back(6.0f);  radarRangeNm.push_back(12.0f); radarRangeNm.push_back(24.0f);
    radarRangeNm.push_back(36.0f); radarRangeNm.push_back(48.0f);
    std::sort(radarRangeNm.begin(), radarRangeNm.end());
    radarRangeIndex = radarRangeNm.size() / 2; // start ~3 Nm

    // --- Detection parameters (formerly radar_height / radar_noise / etc.) ---
    radarScannerHeight = 5.5f;
    radarNoiseLevel = 0.0000000000006f; //kyara: was 5e-12, tuned for 360x128. Scaled down ~8x for 720x512.
    radarSeaClutter = 0.000000001f;
    radarRainClutter = 0.00001f;
    rangeSensitivity = 5.0f;

    // --- Colour palettes (still cycled by the "Colour" button) ---
    // Surround is DEEP GREEN for every palette (see Step 2).
//RADAR BOX BACKGROUND
    const irr::video::SColor deepGreenSurround(255, 0, 0, 0); // black radar surround
    //----------------------------------------------

    radarBackgroundColours.clear();
    radarForegroundColours.clear();
    radarSurroundColours.clear();

    // helper: foreground (blip) RGB, then background (scope interior) RGB
    auto addPalette = [&](irr::u32 fR, irr::u32 fG, irr::u32 fB,
        irr::u32 bR, irr::u32 bG, irr::u32 bB) {
            radarForegroundColours.push_back(irr::video::SColor(255, fR, fG, fB));
            radarBackgroundColours.push_back(irr::video::SColor(255, bR, bG, bB));
            radarSurroundColours.push_back(deepGreenSurround);
        };

    addPalette(255, 200, 0, 0, 0, 0); // 1: amber on black
    addPalette(255, 120, 0, 0, 0, 0); // 2: orange on black
    addPalette(255, 38, 28, 9, 40, 77); // 3: red on navy
    addPalette(40, 186, 51, 0, 0, 0); // 4: green on black

    // --- YOUR NEW PALETTES ---
    // 5: Green foreground (0,255,0) on Blue background (0,0,255)
    addPalette(0, 242, 40, 0, 0, 154);

    // 6: Red foreground (255,0,0) on Blue background (0,0,255)
    addPalette(176, 255, 0, 0, 0, 154);

    currentRadarColourChoice = 0;

    //initialise scanArray size (angularResolution x rangeResolution points per scan)
    scanArray.resize(angularResolution, std::vector<irr::f32>(rangeResolution, 0.0));
    scanArrayAmplified.resize(angularResolution, std::vector<irr::f32>(rangeResolution, 0.0));
    scanArrayToPlot.resize(angularResolution, std::vector<irr::f32>(rangeResolution, 0.0));
    scanArrayToPlotPrevious.resize(angularResolution, std::vector<irr::f32>(rangeResolution, 0.0));
    toReplot.resize(angularResolution);

    //initialise arrays
    for (irr::u32 i = 0; i < angularResolution; i++) {
        for (irr::u32 j = 0; j < rangeResolution; j++) {
            scanArray[i][j] = 0.0;
            scanArrayAmplified[i][j] = 0.0;
            scanArrayToPlot[i][j] = 0.0;
            scanArrayToPlotPrevious[i][j] = -1.0;
        }
    }

    scanAngleStep = 360.0f / (irr::f32)angularResolution;
}

void RadarCalculation::decreaseRange()
{
    if (radarRangeIndex > 0) {
        radarRangeIndex--;
    }
}

void RadarCalculation::increaseRange()
{
    if (radarRangeIndex < radarRangeNm.size() - 1) {
        radarRangeIndex++;
    }
}

irr::f32 RadarCalculation::getRangeNm() const
{
    return radarRangeNm.at(radarRangeIndex); //Assume that radarRangeIndex is in bounds
}

void RadarCalculation::setGain(irr::f32 value)
{
    radarGain = value;
}

void RadarCalculation::setClutter(irr::f32 value)
{
    radarSeaClutterReduction = value;
}

void RadarCalculation::setRainClutter(irr::f32 value)
{
    radarRainClutterReduction = value;
}

irr::f32 RadarCalculation::getGain() const
{
    return radarGain;
}

irr::f32 RadarCalculation::getClutter() const
{
    return radarSeaClutterReduction;
}

irr::f32 RadarCalculation::getRainClutter() const
{
    return radarRainClutterReduction;
}

void RadarCalculation::increaseClutter(irr::f32 value)
{
    radarSeaClutterReduction += value;
    if (radarSeaClutterReduction > 100) {
        radarSeaClutterReduction = 100;
    }
}

void RadarCalculation::decreaseClutter(irr::f32 value)
{
    radarSeaClutterReduction -= value;
    if (radarSeaClutterReduction < 0) {
        radarSeaClutterReduction = 0;
    }
}

void RadarCalculation::increaseRainClutter(irr::f32 value)
{
    radarRainClutterReduction += value;
    if (radarRainClutterReduction > 100) {
        radarRainClutterReduction = 100;
    }
}

void RadarCalculation::decreaseRainClutter(irr::f32 value)
{
    radarRainClutterReduction -= value;
    if (radarRainClutterReduction < 0) {
        radarRainClutterReduction = 0;
    }
}

void RadarCalculation::increaseGain(irr::f32 value)
{
    radarGain += value;
    if (radarGain > 100) {
        radarGain = 100;
    }
}

void RadarCalculation::decreaseGain(irr::f32 value)
{
    radarGain -= value;
    if (radarGain < 0) {
        radarGain = 0;
    }
}

irr::f32 RadarCalculation::getEBLRangeNm() const
{
    return vrmRangeNm[activeVRM];
}

irr::f32 RadarCalculation::getVRMRangeNm(irr::u32 index) const
{
    return vrmRangeNm[(index < 2) ? index : 0];
}

void RadarCalculation::selectNextEBL()
{
    activeEBL = 1 - activeEBL;
}

void RadarCalculation::selectNextVRM()
{
    activeVRM = 1 - activeVRM;
}

irr::u32 RadarCalculation::getActiveEBL() const
{
    return activeEBL;
}

irr::u32 RadarCalculation::getActiveVRM() const
{
    return activeVRM;
}

void RadarCalculation::cycleGuardAlarmMode()
{
    guardAlarmMode = (guardAlarmMode + 1) % 3;
    guardAlarmTimer = 0;
    guardZoneNeedsPrime = true; //don't alarm for contacts already inside when switching on
}

int RadarCalculation::getGuardAlarmMode() const
{
    return guardAlarmMode;
}

bool RadarCalculation::isGuardAlarmSounding() const
{
    return guardAlarmTimer > 0;
}

void RadarCalculation::updateGuardZoneAlarm(irr::f32 deltaTime)
{
    //kyara: the alarm now SUSTAINS (keeps the looping sound going) for as long as the alarm
    //condition is true, instead of firing once for a fixed 5 s on the entry/exit edge.
    //Each frame the condition holds we re-arm guardAlarmTimer, so isGuardAlarmSounding() stays
    //true and the (already looping) radar-alarm sound channel keeps playing until it clears:
    //  Mode 1 (IN):  a contact is inside the zone      -> alarm loops until it LEAVES.
    //  Mode 2 (OUT): a contact that had been inside     -> alarm loops until it gets BACK INSIDE.
    //The per-contact latch (guardAlarmLatched) holds the alarm between frames. Because it latches
    //on the in/out transition and only clears on the opposite transition, the alarm does NOT stop
    //if the contact later crosses the *other* VRM ring - e.g. in OUT mode, a target that left via
    //VRM2 keeps the alarm sounding (VRM2 doesn't silence an alarm that started from VRM1).
    const irr::f32 ALARM_SUSTAIN = 0.5f; //seconds; refreshed every frame the condition holds.
    //Also smooths one-frame ARPA jitter so the loop doesn't stutter.

    if (guardAlarmTimer > 0) { guardAlarmTimer -= deltaTime; }

    if (guardAlarmMode == 0) {
        guardAlarmTimer = 0;
        //Alarm off: drop every latch so a later re-arm starts clean.
        for (unsigned int i = 0; i < arpaContacts.size(); i++) {
            arpaContacts.at(i).guardAlarmLatched = false;
        }
        return;
    }

    irr::f32 rMin = std::min(vrmRangeNm[0], vrmRangeNm[1]);
    irr::f32 rMax = std::max(vrmRangeNm[0], vrmRangeNm[1]);
    if (rMax <= 0) { return; } //no zone set up yet

    //Sector swept clockwise from EBL1 to EBL2. Equal bearings = full 360 ring.
    irr::f32 sweep = eblBrg[1] - eblBrg[0];
    while (sweep < 0) { sweep += 360; }
    while (sweep >= 360) { sweep -= 360; }
    if (sweep == 0) { sweep = 360; }

    bool alarmConditionActive = false; //true if ANY contact currently warrants the alarm this frame

    for (unsigned int i = 0; i < arpaContacts.size(); i++) {
        ARPAContact& thisContact = arpaContacts.at(i);
        if (thisContact.estimate.range <= 0 || thisContact.estimate.lost) {
            //A lost/invalid contact can't hold the alarm: treat as outside and un-latch it.
            thisContact.wasInGuardZone = false;
            thisContact.guardAlarmLatched = false;
            continue;
        }
        irr::f32 rel = thisContact.estimate.bearing - eblBrg[0];
        while (rel < 0) { rel += 360; }
        while (rel >= 360) { rel -= 360; }
        bool inZone = (thisContact.estimate.range >= rMin) && (thisContact.estimate.range <= rMax) && (rel <= sweep);

        if (guardZoneNeedsPrime) {
            //First frame after arming / mode change: just capture the current in/out state
            //without alarming, so contacts already inside (IN) or already outside (OUT) don't
            //trigger immediately - they only alarm on a fresh transition after this frame.
            thisContact.guardAlarmLatched = false;
        }
        else if (guardAlarmMode == 1) {          //ALARM IN
            if (inZone && !thisContact.wasInGuardZone) {
                thisContact.guardAlarmLatched = true;   //entered -> start (and keep) sounding
            }
            else if (!inZone) {
                thisContact.guardAlarmLatched = false;  //left    -> stop
            }
            //still inside: latch unchanged -> keeps looping
        }
        else if (guardAlarmMode == 2) {          //ALARM OUT
            if (!inZone && thisContact.wasInGuardZone) {
                thisContact.guardAlarmLatched = true;   //left    -> start (and keep) sounding
            }
            else if (inZone) {
                thisContact.guardAlarmLatched = false;  //returned-> stop
            }
            //still outside: latch unchanged -> keeps looping
        }

        if (thisContact.guardAlarmLatched) { alarmConditionActive = true; }
        thisContact.wasInGuardZone = inZone;
    }

    guardZoneNeedsPrime = false;

    //Re-arm the sustain timer on every frame the condition holds. When it clears, we simply stop
    //re-arming and the timer decays to 0 over ALARM_SUSTAIN, giving a short, stutter-free tail.
    if (alarmConditionActive) { guardAlarmTimer = ALARM_SUSTAIN; }
}


void RadarCalculation::setEBLBrg(irr::u32 index, irr::f32 brg)
{
    if (index < 2) { eblBrg[index] = brg; }
}

void RadarCalculation::setVRMRange(irr::u32 index, irr::f32 rangeNm)
{
    if (index < 2 && rangeNm >= 0) { vrmRangeNm[index] = rangeNm; }
}

void RadarCalculation::setActiveEBL(irr::u32 index)
{
    activeEBL = (index < 2) ? index : 0;
}

void RadarCalculation::setActiveVRM(irr::u32 index)
{
    activeVRM = (index < 2) ? index : 0;
}

void RadarCalculation::setGuardAlarmMode(int mode)
{
    mode = mode % 3;
    if (mode != guardAlarmMode) {
        guardAlarmMode = mode;
        guardAlarmTimer = 0;
        guardZoneNeedsPrime = true; //don't alarm for contacts already inside on mode change
    }
}
irr::f32 RadarCalculation::getCursorBrg() const
{
    return CursorBrg;
}

irr::f32 RadarCalculation::getCursorRangeNm() const
{
    return CursorRangeNm;
}

irr::f32 RadarCalculation::getEBLBrg() const
{
    return eblBrg[activeEBL];
}

irr::f32 RadarCalculation::getEBLBrg(irr::u32 index) const
{
    return eblBrg[(index < 2) ? index : 0];
}

void RadarCalculation::setPIData(irr::s32 PIid, irr::f32 PIbearing, irr::f32 PIrange)
{
    if (PIid >= 0 && PIid < (irr::s32)piBearings.size() && PIid < (irr::s32)piRanges.size()) {
        piBearings.at(PIid) = PIbearing;
        piRanges.at(PIid) = PIrange;
    }
}

irr::f32 RadarCalculation::getPIbearing(irr::s32 PIid) const
{
    if (PIid >= 0 && PIid < (irr::s32)piBearings.size()) {
        return piBearings.at(PIid);
    }
    else {
        return 0;
    }
}

irr::f32 RadarCalculation::getPIrange(irr::s32 PIid) const
{
    if (PIid >= 0 && PIid < (irr::s32)piRanges.size()) {
        return piRanges.at(PIid);
    }
    else {
        return 0;
    }
}

void RadarCalculation::increaseCursorRangeXNm()
{
    //Only trigger this if there's been enough time since the last update.
    clock_t clockNow = clock();
    float elapsed = (float)(clockNow - radarCursorsLastUpdated) / CLOCKS_PER_SEC;
    if (elapsed > 0.03) {
        radarCursorsLastUpdated = clockNow;
        irr::f32 oldCursorRangeXNm = cursorRangeXNm;
        cursorRangeXNm += getRangeNm() / 100;

        // Limit: 
        irr::f32 testCursorRangeNm = pow(pow(cursorRangeXNm, 2) + pow(cursorRangeYNm, 2), 0.5);
        if (testCursorRangeNm > getRangeNm()) {
            irr::f32 testCursorBrgRad = std::atan2(oldCursorRangeXNm, cursorRangeYNm);
            cursorRangeXNm = getRangeNm() * sin(testCursorBrgRad);
            cursorRangeYNm = getRangeNm() * cos(testCursorBrgRad);
        }
    }
}

void RadarCalculation::decreaseCursorRangeXNm()
{
    //Only trigger this if there's been enough time since the last update.
    clock_t clockNow = clock();
    float elapsed = (float)(clockNow - radarCursorsLastUpdated) / CLOCKS_PER_SEC;
    if (elapsed > 0.03) {
        radarCursorsLastUpdated = clockNow;
        irr::f32 oldCursorRangeXNm = cursorRangeXNm;
        cursorRangeXNm -= getRangeNm() / 100;
        // Limit: 
        irr::f32 testCursorRangeNm = pow(pow(cursorRangeXNm, 2) + pow(cursorRangeYNm, 2), 0.5);
        if (testCursorRangeNm > getRangeNm()) {
            irr::f32 testCursorBrgRad = std::atan2(oldCursorRangeXNm, cursorRangeYNm);
            cursorRangeXNm = getRangeNm() * sin(testCursorBrgRad);
            cursorRangeYNm = getRangeNm() * cos(testCursorBrgRad);
        }
    }
}

void RadarCalculation::increaseCursorRangeYNm()
{
    //Only trigger this if there's been enough time since the last update.
    clock_t clockNow = clock();
    float elapsed = (float)(clockNow - radarCursorsLastUpdated) / CLOCKS_PER_SEC;
    if (elapsed > 0.03) {
        radarCursorsLastUpdated = clockNow;
        irr::f32 oldCursorRangeYNm = cursorRangeYNm;
        cursorRangeYNm += getRangeNm() / 100;
        // Limit: 
        irr::f32 testCursorRangeNm = pow(pow(cursorRangeXNm, 2) + pow(cursorRangeYNm, 2), 0.5);
        if (testCursorRangeNm > getRangeNm()) {
            irr::f32 testCursorBrgRad = std::atan2(cursorRangeXNm, oldCursorRangeYNm);
            cursorRangeXNm = getRangeNm() * sin(testCursorBrgRad);
            cursorRangeYNm = getRangeNm() * cos(testCursorBrgRad);
        }
    }
}

void RadarCalculation::decreaseCursorRangeYNm()
{
    //Only trigger this if there's been enough time since the last update.
    clock_t clockNow = clock();
    float elapsed = (float)(clockNow - radarCursorsLastUpdated) / CLOCKS_PER_SEC;
    if (elapsed > 0.03) {
        radarCursorsLastUpdated = clockNow;
        irr::f32 oldCursorRangeYNm = cursorRangeYNm;
        cursorRangeYNm -= getRangeNm() / 100;
        // Limit: 
        irr::f32 testCursorRangeNm = pow(pow(cursorRangeXNm, 2) + pow(cursorRangeYNm, 2), 0.5);
        if (testCursorRangeNm > getRangeNm()) {
            irr::f32 testCursorBrgRad = std::atan2(cursorRangeXNm, oldCursorRangeYNm);
            cursorRangeXNm = getRangeNm() * sin(testCursorBrgRad);
            cursorRangeYNm = getRangeNm() * cos(testCursorBrgRad);
        }
    }
}

void RadarCalculation::increaseEBLRange()
{
    //Only trigger this if there's been enough time since the last update.
    clock_t clockNow = clock();
    float elapsed = (float)(clockNow - radarCursorsLastUpdated) / CLOCKS_PER_SEC;
    if (elapsed > 0.03) {
        radarCursorsLastUpdated = clockNow;

        vrmRangeNm[activeVRM] += getRangeNm() / 100;

    }
}

void RadarCalculation::decreaseEBLRange()
{
    //Only trigger this if there's been enough time since the last update.
    clock_t clockNow = clock();
    float elapsed = (float)(clockNow - radarCursorsLastUpdated) / CLOCKS_PER_SEC;
    if (elapsed > 0.03) {
        radarCursorsLastUpdated = clockNow;

        vrmRangeNm[activeVRM] -= getRangeNm() / 100;
        if (vrmRangeNm[activeVRM] < 0) {
            vrmRangeNm[activeVRM] = 0;
        }
    }
}

void RadarCalculation::increaseEBLBrg()
{
    //Only trigger this if there's been enough time since the last update.
    clock_t clockNow = clock();
    float elapsed = (float)(clockNow - radarCursorsLastUpdated) / CLOCKS_PER_SEC;
    if (elapsed > 0.03) {
        radarCursorsLastUpdated = clockNow;

        eblBrg[activeEBL]++;
        while (eblBrg[activeEBL] >= 360) {
            eblBrg[activeEBL] -= 360;
        }
    }
}

void RadarCalculation::decreaseEBLBrg()
{
    //Only trigger this if there's been enough time since the last update.
    clock_t clockNow = clock();
    float elapsed = (float)(clockNow - radarCursorsLastUpdated) / CLOCKS_PER_SEC;
    if (elapsed > 0.03) {
        radarCursorsLastUpdated = clockNow;

        eblBrg[activeEBL]--;
        while (eblBrg[activeEBL] < 0) {
            eblBrg[activeEBL] += 360;
        }
    }
}

void RadarCalculation::setNorthUp()
{
    //Radar modes: North up (false, false). Course up (true, true). Head up (true, false)
    headUp = false;
    stabilised = false;
    radarScreenStale = true;
}

void RadarCalculation::setCourseUp()
{
    //Radar modes: North up (false, false). Course up (true, true). Head up (true, false)
    headUp = true;
    stabilised = true;
    radarScreenStale = true;
}

void RadarCalculation::setHeadUp()
{
    //Radar modes: North up (false, false). Course up (true, true). Head up (true, false)
    headUp = true;
    stabilised = false;
    radarScreenStale = true;
}
bool RadarCalculation::getStabilised() const
{
    return stabilised;
}

void RadarCalculation::cycleRangeRingBrightness()
{
    rangeRingBrightness = (rangeRingBrightness + 1) % 3; // clair -> faible -> off -> clair
}

int RadarCalculation::getRangeRingBrightness() const
{
    return rangeRingBrightness;
}
bool RadarCalculation::getHeadUp() const//Head or course up
{
    return headUp;
}

void RadarCalculation::toggleRadarOn()
{
    radarOn = !radarOn;

    if (!radarOn) {
        //Reset array to empty
        for (irr::u32 i = 0; i < angularResolution; i++) {
            for (irr::u32 j = 0; j < rangeResolution; j++) {
                scanArrayToPlot[i][j] = 0.0;
            }
        }
        radarScreenStale = true;
    }
}

bool RadarCalculation::isRadarOn() const
{
    return radarOn;
}
//buoy arpa 



void RadarCalculation::setArpaOnBuoys(bool on)
{
    showArpaOnBuoys = on;
}

bool RadarCalculation::getArpaOnBuoys() const
{
    return showArpaOnBuoys;
}

int RadarCalculation::getArpaMode() const
{
    return arpaMode;
}


void RadarCalculation::setArpaMode(int mode)
{
    // 0: Off/Manual, 1: MARPA, 2: ARPA
    arpaMode = mode;
    if (arpaMode < 1) {
        // Clear arpa scans:
        // Remove all ARPA (not manual) contacts from arpaContacts. Clear arpaTracks, but reset estimate.displayID to 0 for manual contacts, so it is regenerated 
        for (int i = arpaContacts.size() - 1; i >= 0; i--) {
            // Iterate from end to start, as we may be removing items
            if (arpaContacts.at(i).contactType == CONTACT_MANUAL) {
                arpaContacts.at(i).estimate.displayID = 0;
            }
            else {
                // Remove it
                arpaContacts.erase(arpaContacts.begin() + i);
            }
        }
        arpaTracks.clear(); // This will be regenerated as we have set display ID to 0
    }
}

void RadarCalculation::setRadarARPARel()
{
    trueVectors = false;
}

void RadarCalculation::setRadarARPATrue()
{
    trueVectors = true;
}

void RadarCalculation::setArpaListSelection(irr::s32 selection)
{
    arpaListSelection = selection;
}

irr::s32 RadarCalculation::getArpaListSelection() const
{
    return arpaListSelection;
}

void RadarCalculation::setRadarARPAVectors(irr::f32 vectorMinutes)
{
    vectorLengthMinutes = vectorMinutes;
}

void RadarCalculation::setRadarDisplayRadius(irr::u32 radiusPx)
{
    if (radarRadiusPx != radiusPx) { //If changed
        radarRadiusPx = radiusPx;
        radarScreenStale = true;
    }
}

irr::u32 RadarCalculation::getARPATracksSize() const
{
    return arpaTracks.size();
}

int RadarCalculation::getARPAContactIDFromTrackIndex(irr::u32 trackIndex) const
{
    if (trackIndex >= 0 && trackIndex < arpaTracks.size()) {
        return arpaTracks.at(trackIndex);
    }
    else {
        // Not found
        return -1;
    }
}

ARPAContact RadarCalculation::getARPAContactFromTrackIndex(irr::u32 trackIndex) const
{

    int contactID = getARPAContactIDFromTrackIndex(trackIndex);

    if (contactID >= 0 && contactID < arpaContacts.size()) {
        return arpaContacts.at(contactID);
    }
    else {
        ARPAContact emptyContact;
        return emptyContact;
    }
}

void RadarCalculation::changeRadarColourChoice()
{
    currentRadarColourChoice++;
    if (currentRadarColourChoice >= radarBackgroundColours.size()) {
        //Assume radarBackgroundColours and radarForegroundColours are the same size
        //We check size of both before using currentRadarColourChoice
        currentRadarColourChoice = 0;
    }
    radarScreenStale = true;
}

void RadarCalculation::update(irr::video::IImage* radarImage, irr::video::IImage* radarImageOverlaid, irr::core::vector3d<int64_t> offsetPosition, const Terrain& terrain, const OwnShip& ownShip, const Buoys& buoys, const OtherShips& otherShips, irr::f32 weather, irr::f32 rain, irr::f32 tideHeight, irr::f32 deltaTime, uint64_t absoluteTime, irr::core::vector2di mouseRelPosition, bool isMouseDown)
{

#ifdef WITH_PROFILING
    IPROF_FUNC;
#endif

    {
        IPROF("Set up");

        //Reset screen if needed
        if (radarScreenStale) {
            radarImage->fill(getRadarSurroundColour());
            //kyara: intérieur du scope en couleur de fond, pour le croissant non balayé en décentrage
            {
                const irr::s32 w = radarImage->getDimension().Width;
                const irr::s32 h = radarImage->getDimension().Height;
                const irr::f32 cc = (irr::f32)radarRadiusPx;
                const irr::f32 rr2 = cc * cc;
                const irr::video::SColor bg = getRadarBackgroundColour();
                for (irr::s32 yy = 0; yy < h; yy++)
                    for (irr::s32 xx = 0; xx < w; xx++) {
                        irr::f32 dx = xx - cc, dy = yy - cc;
                        if (dx * dx + dy * dy <= rr2) radarImage->setPixel(xx, yy, bg);
                    }
            }
            //Reset 'previous' array so it will all get re-drawn
            for (irr::u32 i = 0; i < angularResolution; i++) {
                toReplot[i] = true;
                for (irr::u32 j = 0; j < rangeResolution; j++) {
                    scanArrayToPlotPrevious[i][j] = -1.0;
                }
            }
            radarScreenStale = false;
        }

    } {
        IPROF("Mouse cursor");

        //Find position of mouse cursor for radar cursor
        if (isMouseDown) {
            irr::f32 mouseCursorRangeXNm = (irr::f32)mouseRelPosition.X / (irr::f32)radarRadiusPx * radarRangeNm.at(radarRangeIndex);//Nm
            irr::f32 mouseCursorRangeYNm = -1.0 * (irr::f32)mouseRelPosition.Y / (irr::f32)radarRadiusPx * radarRangeNm.at(radarRangeIndex);//Nm
            irr::f32 mouseCursorRange = pow(pow(mouseCursorRangeXNm, 2) + pow(mouseCursorRangeYNm, 2), 0.5);

            //Check if in range
            if (mouseCursorRange <= radarRangeNm.at(radarRangeIndex)) {
                // Store
                cursorRangeXNm = mouseCursorRangeXNm;
                cursorRangeYNm = mouseCursorRangeYNm;
            }
        }

        // Always update the CursorRangeNm and CursorBrg from the current cursorRangeXNm and cursorRangeYNm 
        CursorBrg = irr::core::RADTODEG * std::atan2(cursorRangeXNm, cursorRangeYNm);
        if (headUp) {
            // Adjust angle if needed
            CursorBrg += ownShip.getHeading();
        }
        CursorBrg = Angles::normaliseAngle(CursorBrg);
        CursorRangeNm = pow(pow(cursorRangeXNm, 2) + pow(cursorRangeYNm, 2), 0.5);

        lastAbsoluteTime = absoluteTime;
        ownCogDeg = ownShip.getCOG();
        ownSogMps = ownShip.getSOG();
        handleScopeEvents(ownShip.getHeading());

    } {
        IPROF("Scan");
        scan(offsetPosition, terrain, ownShip, buoys, otherShips, weather, rain, tideHeight, deltaTime, absoluteTime); // scan into scanArray
    } {
        IPROF("Update ARPA");
        updateARPA(offsetPosition, ownShip, absoluteTime); //From data in arpaContacts, updated in scan()
        updateGuardZoneAlarm(deltaTime); //kyara: guard zone IN/OUT alarm check
        //A click while ARPA was off switched MARPA on: the echo is taken once it has an estimate
        if (pendingAcquireTries > 0) {
            pendingAcquireTries--;
            const int c = contactNearPoint(pendingAcquireXNm, pendingAcquireYNm, false);
            if (c >= 0) {
                pendingAcquireTries = 0;
                arpaContacts.at(c).estimate.stationary = false;
                pendingSelectContact = c;
            }
        }
        if (showCoastline) {
            updateCoastline(offsetPosition, terrain, ownShip, tideHeight, absoluteTime);
        }
    } {
        IPROF("Render");

        // Calculate current absolute position for rendering
        irr::core::vector3df position = ownShip.getPosition();
        irr::core::vector3d<int64_t> absolutePosition = offsetPosition;
        absolutePosition.X += position.X;
        absolutePosition.Y += position.Y;
        absolutePosition.Z += position.Z;

        // Record own ship's position history for its trail, at the same cadence as ARPA scans
        const irr::u32 OWN_SHIP_SECONDS_BETWEEN_SCANS = 2;
        size_t ownScansSize = ownShipScans.size();
        if (ownScansSize == 0 || absoluteTime > OWN_SHIP_SECONDS_BETWEEN_SCANS + ownShipScans.at(ownScansSize - 1).timeStamp) {
            ARPAScan ownScan;
            ownScan.timeStamp = absoluteTime;
            ownScan.x = absolutePosition.X;
            ownScan.z = absolutePosition.Z;
            //rangeNm/bearingDeg unused for own ship trail - true position is always known

            //TRAIL TIME 
            while (ownShipScans.size() >= 650) {
                ownShipScans.erase(ownShipScans.begin());
            }
            ownShipScans.push_back(ownScan);
        }

        render(radarImage, radarImageOverlaid, ownShip.getHeading(), ownShip.getSOG(), absolutePosition);
    }

}

void RadarCalculation::scan(irr::core::vector3d<int64_t> offsetPosition, const Terrain& terrain, const OwnShip& ownShip, const Buoys& buoys, const OtherShips& otherShips, irr::f32 weather, irr::f32 rain, irr::f32 tideHeight, irr::f32 deltaTime, uint64_t absoluteTime)
{

    //IPROF_FUNC;
    const irr::u32 SECONDS_BETWEEN_SCANS = 10;

    irr::core::vector3df position = ownShip.getPosition();
    //Get absolute position relative to SW corner of world model
    irr::core::vector3d<int64_t> absolutePosition = offsetPosition;
    absolutePosition.X += position.X;
    absolutePosition.Y += position.Y;
    absolutePosition.Z += position.Z;

    //Some tuning constants
    irr::f32 radarFactorLand = 2.0;
    irr::f32 radarFactorVessel = 0.0001;

    //Convert range to cell size
    irr::f32 cellLength = M_IN_NM * radarRangeNm.at(radarRangeIndex) / rangeResolution; ; //Assume that radarRangeIndex is in bounds

    //Load radar data for other contacts
    std::vector<RadarData> radarData;
    //For other ships
    for (std::vector<RadarData>::size_type contactID = 1; contactID <= otherShips.getNumber(); contactID++) {
        radarData.push_back(otherShips.getRadarData(contactID, position));
    }
    //For buoys
    for (std::vector<RadarData>::size_type contactID = 1; contactID <= buoys.getNumber(); contactID++) {
        radarData.push_back(buoys.getRadarData(contactID, position));
    }

    const irr::f32 RADAR_RPM = 25; //Todo: Make a ship parameter
    const irr::f32 RPMtoDEGPERSECOND = 6;
    irr::u32 scansPerLoop = RADAR_RPM * RPMtoDEGPERSECOND * deltaTime / (irr::f32)scanAngleStep + (irr::f32)rand() / RAND_MAX; //Add random value (0-1, mean 0.5), so with rounding, we get the correct radar speed, even though we can only do an integer number of scans

    if (scansPerLoop > 30) { scansPerLoop = 30; } //Limit to reasonable bounds
    for (irr::u32 i = 0; i < scansPerLoop; i++) { //Start of repeatable scan section

        // the actual angle we want to work with has to be determined here
        currentScanAngle = ((irr::f32)currentScanLine / (irr::f32)angularResolution) * 360.0f;

        const irr::f32 SCAN_SLOPE_INITIAL = -0.5f; //Slope at start of scan (in metres/metre) - Make slightly negative so vessel contacts close in get detected
        irr::f32 scanSlope = SCAN_SLOPE_INITIAL; //running "highest so far" beam slope; only ever raised by obstructions

        for (irr::u32 currentStep = 1; currentStep < rangeResolution; currentStep++) { //Note that currentStep starts as 1, not 0. This is used in anti-rain clutter filter, which checks element at currentStep-1
            //scan into array, accessed as  scanArray[row (angle)][column (step)]

            //Clear old value
            scanArray[currentScanLine][currentStep] = 0.0;

            //Get location of area being scanned
            irr::f32 localRange = cellLength * currentStep;
            irr::f32 relX = localRange * sin(currentScanAngle * irr::core::DEGTORAD); //Distance from ship
            irr::f32 relZ = localRange * cos(currentScanAngle * irr::core::DEGTORAD);
            irr::f32 localX = position.X + relX;
            irr::f32 localZ = position.Z + relZ;

            //get extents
            irr::f32 minCellAngle = Angles::normaliseAngle(currentScanAngle - scanAngleStep / 2.0);
            irr::f32 maxCellAngle = Angles::normaliseAngle(currentScanAngle + scanAngleStep / 2.0);
            irr::f32 minCellRange = localRange - cellLength / 2.0;
            irr::f32 maxCellRange = localRange + cellLength / 2.0;

            // Get extreme points
            irr::f32 relXCorner1 = minCellRange * sin(minCellAngle * irr::core::DEGTORAD);
            irr::f32 relXCorner2 = minCellRange * sin(maxCellAngle * irr::core::DEGTORAD);
            irr::f32 relXCorner3 = maxCellRange * sin(minCellAngle * irr::core::DEGTORAD);
            irr::f32 relXCorner4 = maxCellRange * sin(maxCellAngle * irr::core::DEGTORAD);
            irr::f32 relZCorner1 = minCellRange * cos(minCellAngle * irr::core::DEGTORAD);
            irr::f32 relZCorner2 = minCellRange * cos(maxCellAngle * irr::core::DEGTORAD);
            irr::f32 relZCorner3 = maxCellRange * cos(minCellAngle * irr::core::DEGTORAD);
            irr::f32 relZCorner4 = maxCellRange * cos(maxCellAngle * irr::core::DEGTORAD);

            //get adjustment of height for earth's curvature
            irr::f32 dropWithCurvature = std::pow(localRange, 2) / (2 * EARTH_RAD_M * EARTH_RAD_CORRECTION);

            //Calculate noise
            irr::f32 localNoise = radarNoise(radarNoiseLevel, radarSeaClutter, radarRainClutter, weather, localRange, currentScanAngle, 0, scanSlope, rain); //FIXME: Needs wind direction

            //Scan other contacts here
            for (unsigned int thisContact = 0; thisContact < radarData.size(); thisContact++) {
                //kyara: cheap range reject before the expensive ellipse tests below
                if (radarData.at(thisContact).maxRange < minCellRange - cellLength ||
                    radarData.at(thisContact).minRange > maxCellRange + cellLength) {
                    continue;
                }
                irr::f32 contactHeightAboveLine = (radarData.at(thisContact).height - radarScannerHeight - dropWithCurvature) - scanSlope * localRange;

                //kyara: the -0.5 baseline beam slope leaves a near-field shadow cone around own ship
                //(~2*radarScannerHeight for a waterline target). Physical, but at short display ranges it
                //becomes a black disc that clips the inner edge of close contacts into crescents.
                //Fully remove it: reveal ANY contact that is only hidden because the beam is still at its
                //unshadowed baseline. This is safe because -
                //  * beyond the cone the normal test already passes (the +0.5*range term dominates any
                //    real range), so this changes nothing in the far field / radar horizon;
                //  * genuine shadowing behind land or a ship RAISES scanSlope above the baseline, which
                //    fails the guard below and leaves that shadow fully intact.
                //No range cap is needed - the scanSlope guard alone bounds the effect to the cone.
                bool contactVisible = (contactHeightAboveLine > 0);
                if (!contactVisible
                    && scanSlope <= SCAN_SLOPE_INITIAL + 0.0001f) {          //nothing between us and the target has raised the beam
                    contactVisible = true;
                }

                if (contactVisible) {
                    //Contact would be visible if in this cell. Check if it is

                    //Ellipse based check - check if any corner point of the cell is within the contact ellipse. If so, then it's definitely visible. If not, fall back to old checks
                    bool contactEllipseFound = false;

                    if (isPointInEllipse(relX, relZ, radarData.at(thisContact).relX, radarData.at(thisContact).relZ, radarData.at(thisContact).width, radarData.at(thisContact).length, radarData.at(thisContact).heading)) {
                        contactEllipseFound = true;
                    }
                    else if (isPointInEllipse(relXCorner1, relZCorner1, radarData.at(thisContact).relX, radarData.at(thisContact).relZ, radarData.at(thisContact).width, radarData.at(thisContact).length, radarData.at(thisContact).heading)) {
                        contactEllipseFound = true;
                    }
                    else if (isPointInEllipse(relXCorner2, relZCorner2, radarData.at(thisContact).relX, radarData.at(thisContact).relZ, radarData.at(thisContact).width, radarData.at(thisContact).length, radarData.at(thisContact).heading)) {
                        contactEllipseFound = true;
                    }
                    else if (isPointInEllipse(relXCorner3, relZCorner3, radarData.at(thisContact).relX, radarData.at(thisContact).relZ, radarData.at(thisContact).width, radarData.at(thisContact).length, radarData.at(thisContact).heading)) {
                        contactEllipseFound = true;
                    }
                    else if (isPointInEllipse(relXCorner4, relZCorner4, radarData.at(thisContact).relX, radarData.at(thisContact).relZ, radarData.at(thisContact).width, radarData.at(thisContact).length, radarData.at(thisContact).heading)) {
                        contactEllipseFound = true;
                    }


                    //Start of B3D code
                    //Check if centre of target within the cell. If not then check if Either min range or max range of contact is within the cell, or min and max span the cell
                    if (contactEllipseFound
                        || (radarData.at(thisContact).range >= minCellRange && radarData.at(thisContact).range <= maxCellRange)
                        || (radarData.at(thisContact).minRange >= minCellRange && radarData.at(thisContact).minRange <= maxCellRange)
                        || (radarData.at(thisContact).maxRange >= minCellRange && radarData.at(thisContact).maxRange <= maxCellRange)
                        || (radarData.at(thisContact).minRange < minCellRange && radarData.at(thisContact).maxRange > maxCellRange)) {

                        //Check if centre of target within the cell. If not then check if either min angle or max angle of contact is within the cell, or min and max span the cell
                        if (contactEllipseFound
                            || (Angles::isAngleBetween(radarData.at(thisContact).angle, minCellAngle, maxCellAngle))
                            || (Angles::isAngleBetween(radarData.at(thisContact).minAngle, minCellAngle, maxCellAngle))
                            || (Angles::isAngleBetween(radarData.at(thisContact).maxAngle, minCellAngle, maxCellAngle))
                            || (Angles::normaliseAngle(radarData.at(thisContact).minAngle - minCellAngle) > 270 && Angles::normaliseAngle(radarData.at(thisContact).maxAngle - maxCellAngle) < 90)) {

                            irr::f32 rangeAtCellMin = rangeAtAngle(minCellAngle, radarData.at(thisContact).relX, radarData.at(thisContact).relZ, radarData.at(thisContact).heading);
                            irr::f32 rangeAtCellMax = rangeAtAngle(maxCellAngle, radarData.at(thisContact).relX, radarData.at(thisContact).relZ, radarData.at(thisContact).heading);

                            //check if the contact intersects this exact cell, if its extremes overlap it
                            //Also check if the target centre is in the cell, or the extended target spans the cell (ie RangeAtCellMin less than minCellRange and rangeAtCellMax greater than maxCellRange and vice versa)
                            if (contactEllipseFound
                                || (((radarData.at(thisContact).range >= minCellRange && radarData.at(thisContact).range <= maxCellRange)
                                    && (Angles::isAngleBetween(radarData.at(thisContact).angle, minCellAngle, maxCellAngle)))
                                    || (rangeAtCellMin >= minCellRange && rangeAtCellMin <= maxCellRange)
                                    || (rangeAtCellMax >= minCellRange && rangeAtCellMax <= maxCellRange)
                                    || (rangeAtCellMin < minCellRange && rangeAtCellMax > maxCellRange)
                                    || (rangeAtCellMax < minCellRange && rangeAtCellMin > maxCellRange))) {

                                irr::f32 radarEchoStrength = radarFactorVessel * std::pow(M_IN_NM / localRange, 4) * radarData.at(thisContact).rcs;
                                scanArray[currentScanLine][currentStep] += radarEchoStrength;

                                //Start ARPA section
                                // ARPA mode - 0: Off/Manual, 1: MARPA, 2: ARPA
                                if ((arpaMode > 0 || (radarData.at(thisContact).isBuoy && showArpaOnBuoys)) && radarEchoStrength * 2 > localNoise) {
                                    //Contact is detectable in noise

                                    //Iterate through arpaContacts array, checking if this contact is in the list (by checking the if the 'contact' pointer is to the same underlying ship/buoy)
                                    int existingArpaContact = -1;
                                    for (unsigned int j = 0; j < arpaContacts.size(); j++) {
                                        if (arpaContacts.at(j).contact == radarData.at(thisContact).contact) {
                                            existingArpaContact = j;
                                        }
                                    }
                                    //If it doesn't exist, add it, and make existingArpaContact point to it
                                    if (existingArpaContact < 0) {

                                        ARPAContact newContact;
                                        newContact.contact = radarData.at(thisContact).contact;
                                        newContact.isBuoy = radarData.at(thisContact).isBuoy;
                                        newContact.contactType = CONTACT_NORMAL;
                                        newContact.mmsi = radarData.at(thisContact).mmsi; // NEW: Extract real MMSI
                                        //newContact.displayID = 0; //Initially not displayed
                                        newContact.totalXMovementEst = 0;
                                        newContact.totalZMovementEst = 0;

                                        //Zeros for estimated state
                                        newContact.estimate.displayID = 0;
                                        newContact.estimate.stationary = true;
                                        newContact.estimate.lost = false;
                                        newContact.estimate.absVectorX = 0;
                                        newContact.estimate.absVectorZ = 0;
                                        newContact.estimate.absHeading = 0;
                                        newContact.estimate.bearing = 0;
                                        newContact.estimate.range = 0;
                                        newContact.estimate.speed = 0;
                                        newContact.estimate.contactType = newContact.contactType; //Redundant here, but useful to pass to the GUI later
                                        newContact.estimate.isBuoy = newContact.isBuoy;

                                        arpaContacts.push_back(newContact);
                                        existingArpaContact = arpaContacts.size() - 1;
                                        //std::cout << "Adding contact " << existingArpaContact << std::endl;
                                    }
                                    //Add this scan (if not already scanned in the last X seconds
                                    size_t scansSize = arpaContacts.at(existingArpaContact).scans.size();
                                    if (scansSize == 0 || absoluteTime > SECONDS_BETWEEN_SCANS + arpaContacts.at(existingArpaContact).scans.at(scansSize - 1).timeStamp) {
                                        ARPAScan newScan;
                                        newScan.timeStamp = absoluteTime;

                                        //Add noise/uncertainty
                                        irr::f32 angleUncertainty = scanAngleStep / 2.0 * (2.0 * (irr::f32)rand() / RAND_MAX - 1);
                                        irr::f32 rangeUncertainty = rangeSensitivity * (2.0 * (irr::f32)rand() / RAND_MAX - 1) / M_IN_NM;

                                        newScan.bearingDeg = angleUncertainty + radarData.at(thisContact).angle;
                                        newScan.rangeNm = rangeUncertainty + radarData.at(thisContact).range / M_IN_NM;

                                        newScan.x = absolutePosition.X + newScan.rangeNm * M_IN_NM * sin(newScan.bearingDeg * RAD_IN_DEG);
                                        newScan.z = absolutePosition.Z + newScan.rangeNm * M_IN_NM * cos(newScan.bearingDeg * RAD_IN_DEG);;
                                        //newScan.estimatedRCS = 100;//Todo: Implement

                                        //Keep track of estimated total movement if in full ARPA
                                        // 0: Off/Manual, 1: MARPA, 2: ARPA
                                        if (scansSize > 0 && arpaMode == 2) {
                                            arpaContacts.at(existingArpaContact).totalXMovementEst += arpaContacts.at(existingArpaContact).scans.at(scansSize - 1).x - newScan.x;
                                            arpaContacts.at(existingArpaContact).totalZMovementEst += arpaContacts.at(existingArpaContact).scans.at(scansSize - 1).z - newScan.z;
                                        }
                                        else {
                                            arpaContacts.at(existingArpaContact).totalXMovementEst = 0;
                                            arpaContacts.at(existingArpaContact).totalZMovementEst = 0;
                                        }

                                        // Keep historical scans for the trail, cap at ~6 minutes of history (180 scans)
                                        while (arpaContacts.at(existingArpaContact).scans.size() >= 180) {
                                            arpaContacts.at(existingArpaContact).scans.erase(arpaContacts.at(existingArpaContact).scans.begin());
                                        }

                                        arpaContacts.at(existingArpaContact).scans.push_back(newScan);
                                        arpaContacts.at(existingArpaContact).estimate.trueHeading = radarData.at(thisContact).heading; //kyara: rafraîchit le cap réel à chaque scan
                                        //std::cout << "ARPA update on " << existingArpaContact << std::endl;
                                        //Todo: should we limit the size of this, so it doesn't continue accumulating?

                                    }


                                } //End ARPA Section
                                //Todo: Also check for contacts beyond the current scan range.

                                /*
                                ;check how visible against noise/clutter. If visible, record as detected for ARPA tracking
                                If radarEchoStrength#*2 > radarNoiseValueNoBlock(radarNoiseLevel#, radarSeaClutter#, radarRainClutter#, weather#, AllRadarTargets(i)\range, rainIntensity)

                                    ;DebugLog "Contact:"
                                    ;DebugLog Str(radarNoiseValueNoBlock(radarNoiseLevel#, radarSeaClutter#, radarRainClutter#, weather#, AllRadarTargets(i)\range, rainIntensity))
                                    ;DebugLog radarEchoStrength#*2

                                    contactLastDetected(i) = absolute_time
                                EndIf

                                RadarIntensity#(Int(radarBrg#),RadarCurrentStep) = RadarIntensity#(Int(radarBrg#),RadarCurrentStep) + radarEchoStrength# ;add target reflection to array

                                ;RACON code
                                ;make an echo line behind the contact
                                If AllRadarTargets(i)\racon <> ""

                                    If Float(time#+AllRadarTargets(i)\raconOffsetTime) Mod 60 <= RaconOnTime# ;Show for RaconOnTime# seconds per minute

                                        Local raconEchoStrength# = radarFactorRACON * (1852/radarRange#)^2;RACON/SART goes with inverse square law as we are receiving the direct signal, not echo

                                        ;set start point for racon echo (global variable)
                                        raconCurrentStep = RadarCurrentStep

                                        addRaconString(raconEchoStrength, radarBrg#, 750, radarStep#, AllRadarTargets(i)\racon$)

                                    EndIf

                                EndIf
                                */
                                //if a target entirely covers the angle of a cell, then use its blocking height and increase radarHeight, so it blocks reflections from behind
                                if (Angles::normaliseAngle(radarData.at(thisContact).minAngle - minCellAngle) > 270 && Angles::normaliseAngle(radarData.at(thisContact).maxAngle - maxCellAngle) < 90) {
                                    //reset scanSlope to new value if the solid height is higher
                                    scanSlope = std::max(scanSlope, (radarData.at(thisContact).solidHeight - radarScannerHeight - dropWithCurvature) / localRange);

                                }
                            }


                        }
                    }
                    //End of B3D code
                }
            }

            //Add land scan
           //Add land scan
            irr::f32 landEcho = 0; //Land part of this cell's echo, to pick the display curve below
            //kyara: supersample the terrain inside this cell and keep the highest point found.
            //A single centre sample misses breakwaters, jetties, moles and low coastline whenever
            //the cell is larger than the feature.
            irr::f32 terrainHeightAboveSea;
            {
                const irr::f32 SAMPLE_SPACING_M = 60.0f;   //kyara: matched to the 65 m terrain grid - finer sampling reads the same cell twice
                const irr::u32 MAX_SAMPLES_PER_AXIS = 4;   //CPU guard

                irr::f32 cellWidth = localRange * scanAngleStep * irr::core::DEGTORAD; //cross-range cell size

                irr::u32 nRangeSamples = (irr::u32)std::ceil(cellLength / SAMPLE_SPACING_M);
                irr::u32 nAngleSamples = (irr::u32)std::ceil(cellWidth / SAMPLE_SPACING_M);
                if (nRangeSamples < 1) { nRangeSamples = 1; }
                if (nAngleSamples < 1) { nAngleSamples = 1; }
                if (nRangeSamples > MAX_SAMPLES_PER_AXIS) { nRangeSamples = MAX_SAMPLES_PER_AXIS; }

                if (nAngleSamples > MAX_SAMPLES_PER_AXIS) { nAngleSamples = MAX_SAMPLES_PER_AXIS; }

                irr::f32 maxHeight = terrain.getHeight(localX, localZ); //centre sample
                for (irr::u32 sr = 0; sr < nRangeSamples; sr++) {
                    irr::f32 sampleRange = minCellRange + cellLength * ((sr + 0.5f) / (irr::f32)nRangeSamples);
                    for (irr::u32 sa = 0; sa < nAngleSamples; sa++) {
                        irr::f32 sampleAngle = currentScanAngle
                            + scanAngleStep * (((sa + 0.5f) / (irr::f32)nAngleSamples) - 0.5f);
                        irr::f32 sampleX = position.X + sampleRange * sin(sampleAngle * irr::core::DEGTORAD);
                        irr::f32 sampleZ = position.Z + sampleRange * cos(sampleAngle * irr::core::DEGTORAD);
                        irr::f32 h = terrain.getHeight(sampleX, sampleZ);
                        if (h > maxHeight) { maxHeight = h; }
                    }
                }
                terrainHeightAboveSea = maxHeight - tideHeight;
            }
            irr::f32 radarHeight = terrainHeightAboveSea - dropWithCurvature - radarScannerHeight;
            irr::f32 localSlope = radarHeight / localRange;
            irr::f32 heightAboveLine = radarHeight - scanSlope * localRange; //Find height above previous maximum scan slope

            //kyara: shadowing with a grazing-incidence tolerance.
             //A strict slope test makes flat land above the antenna self-shadow after its leading edge
             //(Dakhla is ~7 m, scanner 5.5 m), so only the coastline outline was drawn.
             //A max-height test over-shadows rolling terrain and punches holes in the landmass.
             //A small angular tolerance does both jobs: flat ground falls short of the scan slope line
             //by centimetres, while a real hill shadows by tens of metres.
            const irr::f32 SHADOW_GRAZING_TOLERANCE = 0.004f; //~0.23 deg. Raise for fewer holes, lower for stronger shadows.
            irr::f32 shadowTolerance = SHADOW_GRAZING_TOLERANCE * localRange + 2.0f;
            bool isLand = (terrainHeightAboveSea > 0);
            bool landIlluminated = isLand && (heightAboveLine > -shadowTolerance);

            if (landIlluminated) {
                const irr::f32 GRADIENT_REFERENCE_M = 50.0f;
                irr::f32 radarLocalGradient = std::max(0.0f, heightAboveLine) / std::min(cellLength, GRADIENT_REFERENCE_M);
                if (localSlope > scanSlope) { scanSlope = localSlope; } //Highest so far on scan

                //Diffuse (surface roughness) component, so flat land returns an echo at grazing incidence
                const irr::f32 LAND_DIFFUSE_FLOOR = 0.15f; //0.0 = original behaviour, 1.0 = wall everywhere
                irr::f32 landDiffuse = LAND_DIFFUSE_FLOOR;
                if (landTexture) {
                    //Patchy ground cover (fixed to the geography) and a small scan-to-scan
                    //fluctuation (sigma about 0.35 in natural log), so the inside of a landmass is mottled
                    //and alive instead of one flat block. Coastlines and slopes facing the radar are not
                    //affected: their gradient term below still takes them to full strength.
                    const double worldX = (double)offsetPosition.X + (double)localX;
                    const double worldZ = (double)offsetPosition.Z + (double)localZ;
                    const irr::f32 fluctuation = std::exp(0.85f * ((irr::f32)rand() / RAND_MAX + (irr::f32)rand() / RAND_MAX - 1.0f));
                    landDiffuse = std::min(1.0f, LAND_DIFFUSE_FLOOR * landReflectivity(worldX, worldZ) * fluctuation);
                }
                irr::f32 shapeFactor = std::atan(radarLocalGradient) * (2 / PI);
                shapeFactor = landDiffuse + (1.0f - landDiffuse) * shapeFactor;

                landEcho = radarFactorLand * shapeFactor / std::pow(localRange / M_IN_NM, 3);
                scanArray[currentScanLine][currentStep] += landEcho;
            }
            //Add radar noise
            scanArray[currentScanLine][currentStep] += localNoise;

            //Do amplification: scanArrayAmplified between 0 and 1 will set displayed intensity, values above 1 will be limited at max intensity

            //Calculate from parameters
            //localRange is range in metres
            irr::f32 rainFilter = pow(radarRainClutterReduction / 100.0, 0.1);
            irr::f32 maxSTCdistance = 8 * M_IN_NM * radarSeaClutterReduction / 100.0;
            irr::f32 radarSTCGain;
            if (maxSTCdistance > 0) {
                radarSTCGain = pow(localRange / maxSTCdistance, 3);
                if (radarSTCGain > 1) { radarSTCGain = 1; } //Gain should never be increased (above 1.0)
            }
            else {
                radarSTCGain = 1;
            }

            //calculate high pass filter
            irr::f32 intensityGradient = scanArray[currentScanLine][currentStep] - scanArray[currentScanLine][currentStep - 1];
            if (intensityGradient < 0) { intensityGradient = 0; }

            irr::f32 filteredSignal = intensityGradient * rainFilter + scanArray[currentScanLine][currentStep] * (1 - rainFilter);
            irr::f32 radarLocalGain = 500000 * (8 * pow(radarGain / 100.0, 4)) * radarSTCGain;

            //take log (natural) of signal
            irr::f32 logSignal = log(filteredSignal * radarLocalGain);
            scanArrayAmplified[currentScanLine][currentStep] = std::max(0.0f, logSignal);

            //Land shown with graded brightness. Displayed brightness is the log signal clipped at 1,
            //so anything a few dB over the threshold is at full brightness and land was one flat block.
            //For cells where land is most of the echo, use a soft knee instead: same threshold (land
            //appears and disappears with gain, sea clutter and rain exactly as before), but weak land is
            //dim and only strong returns - coastlines, slopes facing the radar - reach full brightness.
            //Ships, buoys, noise and clutter keep the original curve.
            if (landTexture && landEcho > 0.5f * scanArray[currentScanLine][currentStep]) {
                const irr::f32 LAND_DISPLAY_KNEE = 2.5f; //log units; larger = dimmer, more graded land
                scanArrayAmplified[currentScanLine][currentStep] = 1.0f - std::exp(-scanArrayAmplified[currentScanLine][currentStep] / LAND_DISPLAY_KNEE);
            }

            //Generate a filtered version, based on the angles around. Lag behind by (for example) 3 steps, so we can filter on what's ahead, as well as what's behind
            irr::s32 filterAngle = (irr::s32)currentScanLine - 3;
            while (filterAngle < 0) { filterAngle += angularResolution; }
            while (filterAngle >= angularResolution) { filterAngle -= angularResolution; }
            irr::s32 filterAngle_1 = filterAngle - 3;
            while (filterAngle_1 < 0) { filterAngle_1 += angularResolution; }
            while (filterAngle_1 >= angularResolution) { filterAngle_1 -= angularResolution; }
            irr::s32 filterAngle_2 = filterAngle - 2;
            while (filterAngle_2 < 0) { filterAngle_2 += angularResolution; }
            while (filterAngle_2 >= angularResolution) { filterAngle_2 -= angularResolution; }
            irr::s32 filterAngle_3 = filterAngle - 1;
            while (filterAngle_3 < 0) { filterAngle_3 += angularResolution; }
            while (filterAngle_3 >= angularResolution) { filterAngle_3 -= angularResolution; }
            irr::s32 filterAngle_4 = filterAngle;
            while (filterAngle_4 < 0) { filterAngle_4 += angularResolution; }
            while (filterAngle_4 >= angularResolution) { filterAngle_4 -= angularResolution; }
            irr::s32 filterAngle_5 = filterAngle + 1;
            while (filterAngle_5 < 0) { filterAngle_5 += angularResolution; }
            while (filterAngle_5 >= angularResolution) { filterAngle_5 -= angularResolution; }
            irr::s32 filterAngle_6 = filterAngle + 2;
            while (filterAngle_6 < 0) { filterAngle_6 += angularResolution; }
            while (filterAngle_6 >= angularResolution) { filterAngle_6 -= angularResolution; }
            irr::s32 filterAngle_7 = filterAngle + 3;
            while (filterAngle_7 < 0) { filterAngle_7 += angularResolution; }
            while (filterAngle_7 >= angularResolution) { filterAngle_7 -= angularResolution; }
            if (currentStep < rangeResolution * 0.1) {
                scanArrayToPlot[filterAngle][currentStep] = std::max({
                    scanArrayAmplified[filterAngle_1][currentStep],
                    scanArrayAmplified[filterAngle_2][currentStep],
                    scanArrayAmplified[filterAngle_3][currentStep],
                    scanArrayAmplified[filterAngle_4][currentStep],
                    scanArrayAmplified[filterAngle_5][currentStep],
                    scanArrayAmplified[filterAngle_6][currentStep],
                    scanArrayAmplified[filterAngle_7][currentStep]
                    });

            }
            else if (currentStep < rangeResolution * 0.2) {
                scanArrayToPlot[filterAngle][currentStep] = std::max({
                    scanArrayAmplified[filterAngle_2][currentStep],
                    scanArrayAmplified[filterAngle_3][currentStep],
                    scanArrayAmplified[filterAngle_4][currentStep],
                    scanArrayAmplified[filterAngle_5][currentStep],
                    scanArrayAmplified[filterAngle_6][currentStep]
                    });
            }
            else if (currentStep < rangeResolution * 0.3) {
                scanArrayToPlot[filterAngle][currentStep] = std::max({
                    scanArrayAmplified[filterAngle_3][currentStep],
                    scanArrayAmplified[filterAngle_4][currentStep],
                    scanArrayAmplified[filterAngle_5][currentStep],
                    });
            }
            else {
                scanArrayToPlot[filterAngle][currentStep] = scanArrayAmplified[filterAngle_4][currentStep];
            }
            // ECHO STRETCH ES_SEUIL = 0.5
             //Seuil: seules les cellules au-dessus de ES_SEUIL s'étendent; le clutter faible est ignoré.
            if (echoStretchLevel > 0) {
                const irr::f32 ES_SEUIL = 0.5f; //monter si le bruit s'étire encore; baisser si les échos lointains ne grossissent plus
                irr::f32 stretched = scanArrayToPlot[filterAngle][currentStep];
                for (irr::s32 d = 0; d <= echoStretchLevel; ++d) {
                    irr::s32 s = (irr::s32)currentStep - d;
                    if (s >= 1) {
                        irr::f32 v0 = scanArrayAmplified[filterAngle_4][s];
                        irr::f32 vL = scanArrayAmplified[filterAngle_3][s];
                        irr::f32 vR = scanArrayAmplified[filterAngle_5][s];
                        if (v0 > ES_SEUIL) stretched = std::max(stretched, v0);
                        if (vL > ES_SEUIL) stretched = std::max(stretched, vL);
                        if (vR > ES_SEUIL) stretched = std::max(stretched, vR);
                        if (echoStretchLevel >= 2) {
                            irr::f32 vL2 = scanArrayAmplified[filterAngle_2][s];
                            irr::f32 vR2 = scanArrayAmplified[filterAngle_6][s];
                            if (vL2 > ES_SEUIL) stretched = std::max(stretched, vL2);
                            if (vR2 > ES_SEUIL) stretched = std::max(stretched, vR2);
                        }
                    }
                }
                scanArrayToPlot[filterAngle][currentStep] = stretched;
            }
            toReplot[filterAngle] = true;

            //Clamp between 0 and 1
            if (scanArrayToPlot[filterAngle][currentStep] < 0) {
                scanArrayToPlot[filterAngle][currentStep] = 0;
            }
            if (scanArrayToPlot[filterAngle][currentStep] > 1) {
                scanArrayToPlot[filterAngle][currentStep] = 1;
            }

        } //End of for loop scanning out



        //Increment scan line for next time
        currentScanLine++;
        if (currentScanLine >= angularResolution) {
            currentScanLine = 0;
        }
    } //End of repeatable scan section



}

void RadarCalculation::addManualPoint(bool newContact, irr::core::vector3d<int64_t> offsetPosition, const OwnShip& ownShip, uint64_t absoluteTime)
{
    // Assumes that CursorRangeNm and CursorBrg reflect the current cursor point

    int existingArpaContact = -1;

    if (newContact) {
        ARPAContact newContact;
        newContact.contact = 0; // This is a pointer used for ARPA types, not relevant for manual
        newContact.contactType = CONTACT_MANUAL;
        //newContact.displayID = 0; //Initially not displayed
        newContact.totalXMovementEst = 0; // These are also not used for manual
        newContact.totalZMovementEst = 0; // These are also not used for manual

        //Zeros for estimated state
        newContact.estimate.displayID = 0;
        newContact.estimate.stationary = true;
        newContact.estimate.lost = false;
        newContact.estimate.absVectorX = 0;
        newContact.estimate.absVectorZ = 0;
        newContact.estimate.absHeading = 0;
        newContact.estimate.bearing = 0;
        newContact.estimate.range = 0;
        newContact.estimate.speed = 0;
        newContact.estimate.contactType = newContact.contactType; //Redundant here, but useful to pass to the GUI later

        arpaContacts.push_back(newContact);
        existingArpaContact = arpaContacts.size() - 1;

        //std::cout << "Created new contact, existingArpaContact now = " << existingArpaContact << std::endl;
    }
    else {
        // Update existing selected manual Contact:

        // Find if a manual contact is selected
        if (getARPAContactFromTrackIndex(arpaListSelection).contactType == CONTACT_MANUAL) {
            existingArpaContact = getARPAContactIDFromTrackIndex(arpaListSelection);
        }
        if (existingArpaContact < 0) {
            // No contact found, don't do anything
            return;
        }
    }

    // Set up
    irr::core::vector3df position = ownShip.getPosition();
    // Get absolute position relative to SW corner of world model
    irr::core::vector3d<int64_t> absolutePosition = offsetPosition;
    absolutePosition.X += position.X;
    absolutePosition.Y += position.Y;
    absolutePosition.Z += position.Z;

    //Add this 'scan' (Actually a MARPA Update) 
    ARPAScan newScan;
    newScan.timeStamp = absoluteTime;

    //Don't add noise/uncertainty
    newScan.bearingDeg = CursorBrg;
    newScan.rangeNm = CursorRangeNm;

    newScan.x = absolutePosition.X + newScan.rangeNm * M_IN_NM * sin(newScan.bearingDeg * RAD_IN_DEG);
    newScan.z = absolutePosition.Z + newScan.rangeNm * M_IN_NM * cos(newScan.bearingDeg * RAD_IN_DEG);;
    //newScan.estimatedRCS = 100;//Todo: Implement

    //Don't need to keep track of totalXMovementEst and totalZMovementEst for manual

    arpaContacts.at(existingArpaContact).scans.push_back(newScan);
    //Todo: should we limit the size of this, so it doesn't continue accumulating?
}

void RadarCalculation::clearManualPoints()
{
    int existingArpaContact = -1;
    if (getARPAContactFromTrackIndex(arpaListSelection).contactType == CONTACT_MANUAL) {
        existingArpaContact = getARPAContactIDFromTrackIndex(arpaListSelection);
    }

    if (existingArpaContact >= 0) {
        // Found the contact, remove all scans. Estimate will be regenerated later.
        arpaContacts.at(existingArpaContact).scans.clear();
    }

}

void RadarCalculation::clearTargetFromCursor()
{
    // Assumes that CursorRangeNm and CursorBrg reflect the current cursor point

    irr::f32 cursorRelX = CursorRangeNm * M_IN_NM * sin(CursorBrg * RAD_IN_DEG);
    irr::f32 cursorRelZ = CursorRangeNm * M_IN_NM * cos(CursorBrg * RAD_IN_DEG);

    // Iterate through ARPA contacts and find closest. 
    // If none within 1/10th of radar rang, don't do anything
    irr::f32 closestDistance = getRangeNm() * M_IN_NM / 10.0;
    int closeContact = -1;
    for (int i = 0; i < arpaContacts.size(); i++) {
        irr::f32 targetRelX = arpaContacts.at(i).estimate.range * M_IN_NM * sin(arpaContacts.at(i).estimate.bearing * RAD_IN_DEG);
        irr::f32 targetRelZ = arpaContacts.at(i).estimate.range * M_IN_NM * cos(arpaContacts.at(i).estimate.bearing * RAD_IN_DEG);

        irr::f32 targetRelXDiff = targetRelX - cursorRelX;
        irr::f32 targetRelZDiff = targetRelZ - cursorRelZ;

        // Only check if tracked (i.e. if not marked as 'stationary')
        if (arpaContacts.at(i).estimate.stationary == false) {
            irr::f32 targetRelDistance = std::sqrt(pow(targetRelXDiff, 2) + pow(targetRelZDiff, 2));
            if (targetRelDistance < closestDistance) {
                closeContact = i;
                closestDistance = targetRelDistance;
            }
        }
    }

    if (closeContact >= 0 && closeContact < arpaContacts.size()) {
        // Clear pointer to underlying contact, and mark as stationary
        arpaContacts.at(closeContact).estimate.stationary = true;
        arpaContacts.at(closeContact).contact = 0;
    }

}

void RadarCalculation::trackTargetFromCursor()
{
    // Assumes that CursorRangeNm and CursorBrg reflect the current cursor point

    irr::f32 cursorRelX = CursorRangeNm * M_IN_NM * sin(CursorBrg * RAD_IN_DEG);
    irr::f32 cursorRelZ = CursorRangeNm * M_IN_NM * cos(CursorBrg * RAD_IN_DEG);

    // Iterate through ARPA contacts and find closest. 
    // If none within 1/10th of radar rang, don't do anything
    irr::f32 closestDistance = getRangeNm() * M_IN_NM / 10.0;
    int closeContact = -1;
    for (int i = 0; i < arpaContacts.size(); i++) {
        irr::f32 targetRelX = arpaContacts.at(i).estimate.range * M_IN_NM * sin(arpaContacts.at(i).estimate.bearing * RAD_IN_DEG);
        irr::f32 targetRelZ = arpaContacts.at(i).estimate.range * M_IN_NM * cos(arpaContacts.at(i).estimate.bearing * RAD_IN_DEG);

        irr::f32 targetRelXDiff = targetRelX - cursorRelX;
        irr::f32 targetRelZDiff = targetRelZ - cursorRelZ;

        // Only check if not already tracked (i.e. if marked as 'stationary')
        if (arpaContacts.at(i).estimate.stationary == true) {
            irr::f32 targetRelDistance = std::sqrt(pow(targetRelXDiff, 2) + pow(targetRelZDiff, 2));
            if (targetRelDistance < closestDistance) {
                closeContact = i;
                closestDistance = targetRelDistance;
            }
        }
    }

    if (closeContact >= 0 && closeContact < arpaContacts.size()) {
        arpaContacts.at(closeContact).estimate.stationary = false;
    }

}

void RadarCalculation::updateARPA(irr::core::vector3d<int64_t> offsetPosition, const OwnShip& ownShip, uint64_t absoluteTime)
{

    //IPROF_FUNC;
    //Own ship absolute position
    irr::core::vector3df position = ownShip.getPosition();
    //Get absolute position relative to SW corner of world model
    irr::core::vector3d<int64_t> absolutePosition = offsetPosition;
    absolutePosition.X += position.X;
    absolutePosition.Y += position.Y;
    absolutePosition.Z += position.Z;

    //Based on scans data in arpaContacts, estimate current speed, heading and position
    for (unsigned int i = 0; i < arpaContacts.size(); i++) {
        updateArpaEstimate(arpaContacts.at(i), i, ownShip, absolutePosition, absoluteTime); //This will update the estimate etc.
    } //For loop through arpa contacts
}

void RadarCalculation::updateArpaEstimate(ARPAContact& thisArpaContact, int contactID, const OwnShip& ownShip, irr::core::vector3d<int64_t> absolutePosition, uint64_t absoluteTime)
{
    thisArpaContact.estimate.isBuoy = thisArpaContact.isBuoy;
    thisArpaContact.estimate.mmsi = thisArpaContact.mmsi;
    thisArpaContact.estimate.danger = false;
    thisArpaContact.estimate.trialDanger = false;

    // FIXED: Now properly clears the contact if it is a normal contact AND (ARPA is off OR it's a buoy and buoy ARPA is disabled)
    if (thisArpaContact.contactType == CONTACT_NORMAL && (arpaMode < 1 || (thisArpaContact.isBuoy && !showArpaOnBuoys))) {
        //Set all contacts to zero (untracked) if normal type if ARPA is off


        thisArpaContact.estimate.displayID = 0;
        thisArpaContact.estimate.stationary = true;
        thisArpaContact.estimate.lost = false;
        thisArpaContact.estimate.absVectorX = 0;
        thisArpaContact.estimate.absVectorZ = 0;
        thisArpaContact.estimate.absHeading = 0;
        thisArpaContact.estimate.bearing = 0;
        thisArpaContact.estimate.range = 0;
        thisArpaContact.estimate.speed = 0;
        thisArpaContact.estimate.contactType = CONTACT_NONE;
        thisArpaContact.estimate.mmsi = thisArpaContact.mmsi;
    }
    else {
        if (thisArpaContact.scans.size() == 0) {
            // Reset estimate if there are no scans at all
            //thisArpaContact.estimate.displayID = 0; // Don't reset display ID, so contact can be re-used
            thisArpaContact.estimate.stationary = true;
            thisArpaContact.estimate.lost = false;
            thisArpaContact.estimate.absVectorX = 0;
            thisArpaContact.estimate.absVectorZ = 0;
            thisArpaContact.estimate.absHeading = 0;
            thisArpaContact.estimate.bearing = 0;
            thisArpaContact.estimate.range = 0;
            thisArpaContact.estimate.speed = 0;
            thisArpaContact.estimate.cpa = 0;
            thisArpaContact.estimate.tcpa = 0;
            thisArpaContact.estimate.contactType = thisArpaContact.contactType;
        }
        else {
            // At least one scan

            //Record the contact type in the estimate
            thisArpaContact.estimate.contactType = thisArpaContact.contactType;
            //Buoys: fixed ARPA target. Shown (non-stationary) when enabled, hidden when toggled off.
            if (thisArpaContact.isBuoy) {
                thisArpaContact.estimate.stationary = !showArpaOnBuoys;
            }

            //Check stationary contacts to see if they've got detectable motion: TODO: Make this better: Should weight based on current range?
            //TODO: Test this weighting
            if (thisArpaContact.estimate.stationary) {
                irr::f32 latestRangeNm = thisArpaContact.scans.back().rangeNm;
                if (latestRangeNm < 1) {
                    latestRangeNm = 1;
                }
                irr::f32 weightedMotionX = fabs(thisArpaContact.totalXMovementEst / latestRangeNm);
                irr::f32 weightedMotionZ = fabs(thisArpaContact.totalZMovementEst / latestRangeNm);
                if (thisArpaContact.estimate.contactType == CONTACT_MANUAL ||
                    (weightedMotionX >= 100 ||
                        weightedMotionZ >= 100)) {
                    // Always show for manually acquired targets, or if movement has been detected
                    thisArpaContact.estimate.stationary = false;
                }
            }

            // Check if contact lost, if last scanned more than 60 seconds ago 
            // (exception for manual, don't detect as lost)
            if (absoluteTime - thisArpaContact.scans.back().timeStamp > 60 &&
                thisArpaContact.estimate.contactType == CONTACT_NORMAL) {
                thisArpaContact.estimate.lost = true;
                //std::cout << "Contact " << i << " lost" << std::endl;
            }
            else {
                if (!thisArpaContact.estimate.stationary) {
                    //If ID is 0 (unassigned), set id and increment
                    if (thisArpaContact.estimate.displayID == 0) {
                        arpaTracks.push_back(contactID);
                        thisArpaContact.estimate.displayID = getARPATracksSize(); // The display ID is the current size of the arpaTracks list.
                        if (contactID == pendingSelectContact) { //clicked on the scope: shown in the list
                            setArpaListSelection(thisArpaContact.estimate.displayID - 1);
                            pendingSelectContact = -1;
                        }
                        // If a manual contact, make it the selected one
                        if (thisArpaContact.contactType == CONTACT_MANUAL) {
                            setArpaListSelection(thisArpaContact.estimate.displayID - 1); // Zero indexed list
                        }
                    }
                }

                irr::s32 stepsBack = 60; //Default time for tracking (time = stepsBack * SECONDS_BETWEEN_SCANS)
                irr::s32 recentStepsBack = 10; //Shorter time for tracking (if motion has changed significantly)

                irr::s32 currentScanIndex = thisArpaContact.scans.size() - 1;
                irr::s32 referenceScanIndex = currentScanIndex - stepsBack;
                if (referenceScanIndex < 0) {
                    referenceScanIndex = 0;
                }

                ARPAScan currentScanData = thisArpaContact.scans.at(currentScanIndex);
                ARPAScan referenceScanData = thisArpaContact.scans.at(referenceScanIndex);

                //Check if heading/speed has changed dramatically, by taking last 10 scans. If so, reduce steps back to 10
                irr::s32 actualStepsBack = currentScanIndex - referenceScanIndex;
                if (actualStepsBack > recentStepsBack) {
                    ARPAScan recentScanData = thisArpaContact.scans.at(currentScanIndex - recentStepsBack);
                    irr::f32 deltaTimeRecent = currentScanData.timeStamp - recentScanData.timeStamp;
                    irr::f32 deltaTimeFull = currentScanData.timeStamp - referenceScanData.timeStamp;
                    if (deltaTimeRecent > 0 && deltaTimeFull > 0) {
                        irr::f32 deltaXRecent = currentScanData.x - recentScanData.x;
                        irr::f32 deltaZRecent = currentScanData.z - recentScanData.z;
                        irr::f32 deltaXFull = currentScanData.x - referenceScanData.x;
                        irr::f32 deltaZFull = currentScanData.z - referenceScanData.z;

                        //Absolute vector
                        irr::f32 absVectorXRecent = deltaXRecent / deltaTimeRecent; //m/s
                        irr::f32 absVectorZRecent = deltaZRecent / deltaTimeRecent; //m/s
                        irr::f32 absVectorXFull = deltaXFull / deltaTimeFull; //m/s
                        irr::f32 absVectorZFull = deltaZFull / deltaTimeFull; //m/s

                        //Difference in estimation
                        irr::f32 changeX = absVectorXRecent - absVectorXFull;
                        irr::f32 changeZ = absVectorZRecent - absVectorZFull;

                        // NEW: Hysteresis instead of a single hard threshold. Switching INTO the
                        // recent window needs a clear, sustained disagreement (>1.2 m/s). Once in,
                        // switching BACK to the full window needs the estimates to have clearly
                        // re-converged (<0.6 m/s).
                        irr::f32 changeMag = std::sqrt(changeX * changeX + changeZ * changeZ);
                        const irr::f32 ENTER_RECENT_THRESHOLD = 1.2;
                        const irr::f32 EXIT_RECENT_THRESHOLD = 0.6;

                        if (!thisArpaContact.usingRecentReference && changeMag > ENTER_RECENT_THRESHOLD) {
                            thisArpaContact.usingRecentReference = true;
                        }
                        else if (thisArpaContact.usingRecentReference && changeMag < EXIT_RECENT_THRESHOLD) {
                            thisArpaContact.usingRecentReference = false;
                        }

                        if (thisArpaContact.usingRecentReference) {
                            referenceScanData = recentScanData;
                        }
                    }
                }
                //end of fix kyara flickering radar 
                //Find difference in time, position x, position z
                irr::f32 deltaTime = currentScanData.timeStamp - referenceScanData.timeStamp;
                if (deltaTime <= 0) {
                    // Special case to just show estimated position if nothing else can be calculated
                    irr::f32 relXEst = currentScanData.x - absolutePosition.X;
                    irr::f32 relZEst = currentScanData.z - absolutePosition.Z;
                    thisArpaContact.estimate.bearing = std::atan2(relXEst, relZEst) / RAD_IN_DEG;
                    while (thisArpaContact.estimate.bearing < 0) {
                        thisArpaContact.estimate.bearing += 360;
                    }
                    thisArpaContact.estimate.range = std::sqrt(pow(relXEst, 2) + pow(relZEst, 2)) / M_IN_NM; //Nm
                }
                else {
                    irr::f32 deltaX = currentScanData.x - referenceScanData.x;
                    irr::f32 deltaZ = currentScanData.z - referenceScanData.z;

                    //Absolute vector
                    thisArpaContact.estimate.absVectorX = deltaX / deltaTime; //m/s
                    thisArpaContact.estimate.absVectorZ = deltaZ / deltaTime; //m/s
                    thisArpaContact.estimate.absHeading = std::atan2(deltaX, deltaZ) / RAD_IN_DEG;
                    while (thisArpaContact.estimate.absHeading < 0) {
                        thisArpaContact.estimate.absHeading += 360;
                    }
                    //Relative vector:
                    thisArpaContact.estimate.relVectorX = thisArpaContact.estimate.absVectorX - ownShip.getSOG() * sin((ownShip.getCOG()) * irr::core::DEGTORAD);
                    thisArpaContact.estimate.relVectorZ = thisArpaContact.estimate.absVectorZ - ownShip.getSOG() * cos((ownShip.getCOG()) * irr::core::DEGTORAD); //ownShipSpeed in m/s
                    thisArpaContact.estimate.relHeading = std::atan2(thisArpaContact.estimate.relVectorX, thisArpaContact.estimate.relVectorZ) / RAD_IN_DEG;
                    while (thisArpaContact.estimate.relHeading < 0) {
                        thisArpaContact.estimate.relHeading += 360;
                    }

                    //Estimated current position:
                    irr::f32 relXEst = currentScanData.x - absolutePosition.X + thisArpaContact.estimate.absVectorX * (absoluteTime - currentScanData.timeStamp);
                    irr::f32 relZEst = currentScanData.z - absolutePosition.Z + thisArpaContact.estimate.absVectorZ * (absoluteTime - currentScanData.timeStamp);
                    thisArpaContact.estimate.bearing = std::atan2(relXEst, relZEst) / RAD_IN_DEG;
                    while (thisArpaContact.estimate.bearing < 0) {
                        thisArpaContact.estimate.bearing += 360;
                    }
                    thisArpaContact.estimate.range = std::sqrt(pow(relXEst, 2) + pow(relZEst, 2)) / M_IN_NM; //Nm
                    thisArpaContact.estimate.speed = std::sqrt(pow(thisArpaContact.estimate.absVectorX, 2) + pow(thisArpaContact.estimate.absVectorZ, 2)) * MPS_TO_KTS;

                    //TODO: CPA AND TCPA here: Need checking/testing
                    irr::f32 contactRelAngle = thisArpaContact.estimate.relHeading - (180 + thisArpaContact.estimate.bearing);
                    irr::f32 contactRange = thisArpaContact.estimate.range; //(Nm)
                    irr::f32 relDistanceToCPA = contactRange * cos(contactRelAngle * RAD_IN_DEG); //Distance along the other ship's relative motion line
                    irr::f32 relativeSpeed = std::sqrt(pow(thisArpaContact.estimate.relVectorX, 2) + pow(thisArpaContact.estimate.relVectorZ, 2)) * MPS_TO_KTS;
                    if (fabs(relativeSpeed) < 0.001) { relativeSpeed = 0.001; } //Avoid division by zero

                    thisArpaContact.estimate.cpa = contactRange * sin(contactRelAngle * RAD_IN_DEG);
                    thisArpaContact.estimate.tcpa = 60 * relDistanceToCPA / relativeSpeed; // (nm / (nm/hr)), so time in hours, converted to minutes
                    //std::cout << "Contact " << thisArpaContact.estimate.displayID << " CPA: " <<  thisArpaContact.estimate.cpa << " nm in " << thisArpaContact.estimate.tcpa << " minutes" << std::endl;

                    //Dangerous: a tracked ship passing closer than the CPA limit within the TCPA limit
                    const bool trackedShip = !thisArpaContact.estimate.stationary && !thisArpaContact.isBuoy &&
                        thisArpaContact.estimate.displayID > 0;
                    if (cpaAlarmOn && trackedShip) {
                        thisArpaContact.estimate.danger = fabs(thisArpaContact.estimate.cpa) < cpaLimitNm &&
                            thisArpaContact.estimate.tcpa >= 0 && thisArpaContact.estimate.tcpa <= tcpaLimitMinutes;
                    }

                    //Trial manoeuvre: own ship keeps her course and speed for the delay, then takes the
                    //trial ones. Closest approach over both legs.
                    if (trialOn) {
                        const irr::f32 vo1x = ownSogMps * sin(ownCogDeg * RAD_IN_DEG);
                        const irr::f32 vo1z = ownSogMps * cos(ownCogDeg * RAD_IN_DEG);
                        const irr::f32 trialMps = trialSpeedKts / MPS_TO_KTS;
                        const irr::f32 vo2x = trialMps * sin(trialCourseDeg * RAD_IN_DEG);
                        const irr::f32 vo2z = trialMps * cos(trialCourseDeg * RAD_IN_DEG);
                        const irr::f32 vtx = thisArpaContact.estimate.absVectorX;
                        const irr::f32 vtz = thisArpaContact.estimate.absVectorZ;
                        const irr::f32 d = trialDelayMinutes * 60.0f;
                        //Closest approach of r + v t for t in [0, tMax] (tMax < 0: no end)
                        auto closest = [](irr::f32 rx, irr::f32 rz, irr::f32 vx, irr::f32 vz, irr::f32 tMax, irr::f32& tBest) -> irr::f32 {
                            const irr::f32 v2 = vx * vx + vz * vz;
                            irr::f32 t = (v2 > 1e-6f) ? -(rx * vx + rz * vz) / v2 : 0.0f;
                            if (t < 0) { t = 0; }
                            if (tMax >= 0 && t > tMax) { t = tMax; }
                            tBest = t;
                            return std::sqrt((rx + vx * t) * (rx + vx * t) + (rz + vz * t) * (rz + vz * t));
                        };
                        irr::f32 t1 = 0, t2 = 0;
                        const irr::f32 dist1 = closest(relXEst, relZEst, vtx - vo1x, vtz - vo1z, d, t1);
                        const irr::f32 r1x = relXEst + (vtx - vo1x) * d;
                        const irr::f32 r1z = relZEst + (vtz - vo1z) * d;
                        const irr::f32 dist2 = closest(r1x, r1z, vtx - vo2x, vtz - vo2z, -1.0f, t2);
                        irr::f32 bestDist = dist2, bestT = d + t2;
                        if (d > 0 && dist1 < dist2) { bestDist = dist1; bestT = t1; }
                        thisArpaContact.estimate.trialCpa = bestDist / M_IN_NM;
                        //Opening from the start: the closest point is behind (shown as "past")
                        thisArpaContact.estimate.trialTcpa = (bestT <= 0.0f && d <= 0) ? -1.0f : bestT / 60.0f;
                        if (cpaAlarmOn && trackedShip) {
                            thisArpaContact.estimate.trialDanger = thisArpaContact.estimate.trialCpa < cpaLimitNm &&
                                thisArpaContact.estimate.trialTcpa >= 0 && thisArpaContact.estimate.trialTcpa <= tcpaLimitMinutes;
                        }
                    }


                } //If time between scans > 0 
            } //Contact not lost
        } //If at least 2 scans
    } //If ARPA is on
}

void RadarCalculation::render(irr::video::IImage* radarImage, irr::video::IImage* radarImageOverlaid, irr::f32 ownShipHeading, irr::f32 ownShipSpeed, irr::core::vector3d<int64_t> absolutePosition)
{
    //A ship that is no longer dangerous: its alarm can sound again next time
    for (size_t i = 0; i < arpaContacts.size(); i++) {
        if (!arpaContacts[i].estimate.danger) { arpaContacts[i].dangerAcknowledged = false; }
    }

#ifdef WITH_PROFILING
    IPROF_FUNC;
#endif
    //*************************
    //generate image from array
    //*************************

    //Render background radar picture into radarImage, then copy to radarImageOverlaid and do any 2d drawing on top (so we don't have to redraw all pixels each time
    irr::u32 bitmapWidth = radarRadiusPx * 2; //Set width to use - to map GUI radar display diameter in screen pixels

    //If the image is smaller than ideal, render anyway
    if (radarImage->getDimension().Width < bitmapWidth) {
        bitmapWidth = radarImage->getDimension().Width;
    }

    //draw from array to image
    irr::f32 centrePixel = (bitmapWidth - 1.0) / 2.0; //The centre of the bitmap. Normally this will be a fractional number (##.5)
    //kyara: décentrage -> origine (navire) décalée DANS le bitmap; le cercle du scope reste fixe
    const irr::f32 halfBitmap = (irr::f32)bitmapWidth * 0.5f;
    const irr::f32 originX = centrePixel + offsetXFraction * halfBitmap;
    const irr::f32 originY = centrePixel - offsetYFraction * halfBitmap;
    //precalculate cell max/min range for speed outside nested loop
    std::vector<irr::f32> cellMinRange;
    std::vector<irr::f32> cellMaxRange;

    cellMinRange.push_back(0);
    cellMaxRange.push_back(0);
    //kyara: never let a cell be thinner than ~1.5 px, or drawSector() skips it entirely
     //when rangeResolution exceeds the display radius in pixels.
    const irr::f32 pxPerCell = bitmapWidth * 0.5f / (irr::f32)rangeResolution;
    const irr::f32 halfBand = std::max(pxPerCell * 0.5f, 0.75f);
    for (irr::u32 currentStep = 1; currentStep < rangeResolution; currentStep++) {
        irr::f32 centreR = currentStep * pxPerCell;
        cellMinRange.push_back(centreR - halfBand);
        cellMaxRange.push_back(centreR + halfBand);
    }

    irr::f32 scanAngle;

    for (int scanLine = 0; scanLine < angularResolution; scanLine++) {

        scanAngle = ((irr::f32)scanLine / (irr::f32)angularResolution) * 360.0f;

        irr::f32 cellMinAngle = scanAngle - scanAngleStep / 2.0;
        irr::f32 cellMaxAngle = scanAngle + scanAngleStep / 2.0;

        for (irr::u32 currentStep = 1; currentStep < rangeResolution; currentStep++) {

            //If the sector has changed, draw it. If we're stabilising the picture, need to re-draw all in case the ship's head has changed
            if (toReplot[scanLine] || stabilised)
            {

                if (headUp || scanArrayToPlotPrevious[scanLine][currentStep] != scanArrayToPlot[scanLine][currentStep]) { //If north up, we only need to replot if the previous plot to this sector was different
                    irr::f32 pixelColour = scanArrayToPlot[scanLine][currentStep];

                    if (pixelColour > 1.0) { pixelColour = 1.0; }
                    if (pixelColour < 0) { pixelColour = 0; }

                    //Interpolate colour between foreground and background
                    irr::video::SColor thisColour = getRadarForegroundColour().getInterpolated(getRadarBackgroundColour(), pixelColour);
                    drawSector(radarImage,
                        originX,
                        originY,
                        cellMinRange[currentStep],
                        cellMaxRange[currentStep],
                        cellMinAngle,
                        cellMaxAngle,
                        thisColour.getAlpha(),
                        thisColour.getRed(),
                        thisColour.getGreen(),
                        thisColour.getBlue(),
                        ownShipHeading);

                    scanArrayToPlotPrevious[scanLine][currentStep] = scanArrayToPlot[scanLine][currentStep]; //Record what we have plotted
                }
            }
        }
        //We don't need to replot this line
        toReplot[scanLine] = false;
    }

    //Copy image into overlaid
    radarImage->copyTo(radarImageOverlaid);

    //Adjust for head up/course up
    irr::f32 radarOffsetAngle = 0;
    if (headUp) {
        radarOffsetAngle = -1 * ownShipHeading;
    }
    // ===== Professional overlay: range rings + bearing scale =====
    {
        irr::video::SColor scaleColour = getRadarForegroundColour();
        const irr::u32 sR = scaleColour.getRed();
        const irr::u32 sG = scaleColour.getGreen();
        const irr::u32 sB = scaleColour.getBlue();
        // 2 px inside drawCircle's display-disc clip, so the outer ring isn't decimated
        const irr::f32 maxRadiusPx = (irr::f32)bitmapWidth / 2.0f - 2.0f;

        // Anneaux concentriques : clair / faible / off (kyara)
            // L'affichage ignore l'alpha (texture opaque) -> pour "faible" on assombrit la COULEUR.
        if (rangeRingBrightness < 2) {
            const irr::f32 ringScale = (rangeRingBrightness == 0) ? 1.0f : 0.4f; // clair vs faible
            const irr::u32 rR = (irr::u32)(sR * ringScale);
            const irr::u32 rG = (irr::u32)(sG * ringScale);
            const irr::u32 rB = (irr::u32)(sB * ringScale);
            const int numberOfRings = 6;
            for (int ring = 1; ring <= numberOfRings; ring++) {
                irr::f32 ringRadius = maxRadiusPx * (irr::f32)ring / (irr::f32)numberOfRings;
                drawCircle(radarImageOverlaid, originX, originY, ringRadius, 255, rR, rG, rB);
            }
        }

        // Bearing ticks every 10 deg, long every 30 deg.
        // Same convention as your contacts: 0 deg = screen up, clockwise, head-up aware.
        //kyara: the ticks are part of the range-ring scale, so they follow the same
        //clair / faible / off cycle - previously they stayed on when the rings were switched off,
        //leaving stray lines around the edge of an otherwise clean scope.
        if (rangeRingBrightness < 2) {
            const irr::f32 tickScale = (rangeRingBrightness == 0) ? 1.0f : 0.4f;
            const irr::u32 tR = (irr::u32)(sR * tickScale);
            const irr::u32 tG = (irr::u32)(sG * tickScale);
            const irr::u32 tB = (irr::u32)(sB * tickScale);
            for (int brg = 0; brg < 360; brg += 10) {
                irr::f32 innerR = (brg % 30 == 0) ? maxRadiusPx * 0.93f : maxRadiusPx * 0.97f;
                irr::f32 a = ((irr::f32)brg + radarOffsetAngle) * RAD_IN_DEG;
                irr::f32 sinA = sin(a);
                irr::f32 cosA = cos(a);
                drawLine(radarImageOverlaid,
                    centrePixel + innerR * sinA, centrePixel - innerR * cosA,
                    centrePixel + maxRadiusPx * sinA, centrePixel - maxRadiusPx * cosA,
                    160, tR, tG, tB);
            }
        }
    }


    //Draw parallel indexes on here
    if (piRanges.size() == piBearings.size()) {
        for (unsigned int i = 0; i < piRanges.size(); i++) {
            irr::f32 thisPIrange = piRanges.at(i);
            irr::f32 thisPIbrg = piBearings.at(i);
            if (fabs(thisPIrange) > 0.0001 && fabs(thisPIrange) < getRangeNm()) {
                //Not zero range or off screen

                irr::f32 piRangePX = (irr::f32)bitmapWidth / 2.0 * thisPIrange / getRangeNm(); //Find range in Px

                //find sin and cos of PI angle (so we only need once)
                irr::f32 sinPIbrg = sin(-1 * (thisPIbrg + radarOffsetAngle) * RAD_IN_DEG);
                irr::f32 cosPIbrg = cos(-1 * (thisPIbrg + radarOffsetAngle) * RAD_IN_DEG);

                //find central point on line
                irr::f32 x_a = -1 * piRangePX * sin((90 - (-1 * (thisPIbrg + radarOffsetAngle))) * RAD_IN_DEG) + originX;
                irr::f32 z_a = piRangePX * cos((90 - (-1 * (thisPIbrg + radarOffsetAngle))) * RAD_IN_DEG) + originY;

                //find half chord length (length of PI line)
                irr::f32 halfChord = pow(pow((irr::f32)bitmapWidth / 2.0, 2) - pow(piRangePX, 2), 0.5); //already checked that PIRange is smaller, so should be valid

                //calculate end points of line
                irr::f32 x_1 = x_a - halfChord * sinPIbrg;
                irr::f32 z_1 = z_a - halfChord * cosPIbrg;
                irr::f32 x_2 = x_a + halfChord * sinPIbrg;
                irr::f32 z_2 = z_a + halfChord * cosPIbrg;

                drawLine(radarImageOverlaid, x_1, z_1, x_2, z_2, 255, 255, 255, 255);

                //Show line number
                //Find point towards centre from the line
                irr::f32 xDirection = originX - x_a;
                irr::f32 yDirection = originY - z_a;
                if (x_a != 0 && z_a != 0) {

                    irr::f32 mag = pow(pow(xDirection, 2) + pow(yDirection, 2), 0.5);
                    xDirection /= mag;
                    yDirection /= mag;

                    irr::s32 xTextPos = x_a + 15 * xDirection;
                    irr::s32 yTextPos = z_a + 15 * yDirection;

                    irr::video::IImage* idNumberImage = NumberToImage::getImage(i + 1, device);
                    if (idNumberImage) {
                        irr::core::rect<irr::s32> sourceRect = irr::core::rect<irr::s32>(0, 0, idNumberImage->getDimension().Width, idNumberImage->getDimension().Height);
                        idNumberImage->copyToWithAlpha(radarImageOverlaid, irr::core::position2d<irr::s32>(xTextPos, yTextPos), sourceRect, irr::video::SColor(255, 255, 255, 255));
                        idNumberImage->drop();
                    }
                }
            }
        }
    }

    //Screen position (bitmap pixels) of a point given in true metres east/north of own ship
    const irr::f32 pxPerMetre = ((irr::f32)bitmapWidth / 2.0f) / (M_IN_NM * getRangeNm());
    auto toScreen = [&](irr::f32 relX, irr::f32 relZ, irr::f32& px, irr::f32& py) {
        if (headUp) {
            const irr::f32 cosO = cos(-1 * radarOffsetAngle * irr::core::DEGTORAD);
            const irr::f32 sinO = sin(-1 * radarOffsetAngle * irr::core::DEGTORAD);
            const irr::f32 nx = relX * cosO - relZ * sinO;
            const irr::f32 nz = relX * sinO + relZ * cosO;
            relX = nx;
            relZ = nz;
        }
        px = originX + relX * pxPerMetre;
        py = originY - relZ * pxPerMetre;
    };
    //Inside the scope circle (fixed, whatever the off-centring)
    const irr::f32 scopeR = (irr::f32)bitmapWidth / 2.0f - 2.0f;
    auto inScopeCircle = [&](irr::f32 px, irr::f32 py) {
        return (px - centrePixel) * (px - centrePixel) + (py - centrePixel) * (py - centrePixel) <= scopeR * scopeR;
    };

    //Coastline (sea level contour), as on the chart
    if (showCoastline) {
        for (size_t k = 0; k + 3 < coastSegments.size(); k += 4) {
            irr::f32 x1, y1, x2, y2;
            toScreen(coastSegments[k] - absolutePosition.X, coastSegments[k + 1] - absolutePosition.Z, x1, y1);
            toScreen(coastSegments[k + 2] - absolutePosition.X, coastSegments[k + 3] - absolutePosition.Z, x2, y2);
            if (inScopeCircle(x1, y1) && inScopeCircle(x2, y2)) {
                drawLine(radarImageOverlaid, x1, y1, x2, y2, 255, 90, 220, 160);
            }
        }
    }

    //Trial manoeuvre: own ship's course for the delay, then the trial course and speed, over the
    //vector time
    if (trialOn) {
        const irr::f32 d = irr::core::min_(trialDelayMinutes, vectorLengthMinutes) * 60.0f;
        const irr::f32 rest = vectorLengthMinutes * 60.0f - d;
        const irr::f32 kx = ownSogMps * sin(ownCogDeg * RAD_IN_DEG) * d;
        const irr::f32 kz = ownSogMps * cos(ownCogDeg * RAD_IN_DEG) * d;
        const irr::f32 trialMps = trialSpeedKts / MPS_TO_KTS;
        const irr::f32 ex = kx + trialMps * sin(trialCourseDeg * RAD_IN_DEG) * rest;
        const irr::f32 ez = kz + trialMps * cos(trialCourseDeg * RAD_IN_DEG) * rest;
        irr::f32 ax, ay, bx, by;
        toScreen(kx, kz, ax, ay);
        toScreen(ex, ez, bx, by);
        //Two pixels wide, so it reads over the sea clutter
        auto thickLine = [&](irr::f32 x0, irr::f32 y0, irr::f32 x1, irr::f32 y1) {
            drawLine(radarImageOverlaid, x0, y0, x1, y1, 255, 255, 160, 0);
            drawLine(radarImageOverlaid, x0 + 1, y0, x1 + 1, y1, 255, 255, 160, 0);
            drawLine(radarImageOverlaid, x0, y0 + 1, x1, y1 + 1, 255, 255, 160, 0);
        };
        if (d > 0) { thickLine(originX, originY, ax, ay); }
        //Dashed: 8 px on, 8 px off
        const irr::f32 len = std::sqrt((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
        const int dashes = (int)(len / 8.0f);
        for (int k = 0; k < dashes; k += 2) {
            const irr::f32 f0 = (irr::f32)k / dashes, f1 = (irr::f32)(k + 1) / dashes;
            thickLine(ax + (bx - ax) * f0, ay + (by - ay) * f0, ax + (bx - ax) * f1, ay + (by - ay) * f1);
        }
        if (dashes < 2) { thickLine(ax, ay, bx, by); }
    }

    //Dangerous targets flash (half a second on, half off); the selected one gets a square
    const bool flashOn = device && ((device->getTimer()->getRealTime() / 500) % 2 == 0);
    const int selectedContact = getARPAContactIDFromTrackIndex(arpaListSelection);

    //Draw ARPA stuff here from arpaContacts, into radarImage
    for (unsigned int i = 0; i < arpaContacts.size(); i++) {
        ARPAEstimatedState thisEstimate = arpaContacts.at(i).estimate;

        bool contactIsBuoy = arpaContacts.at(i).isBuoy;
        //Pick the marker colour for this contact type
        irr::video::SColor markerColour = contactIsBuoy ? buoyContactColour : shipContactColour;
        const bool contactDanger = trialOn ? thisEstimate.trialDanger : thisEstimate.danger;
        if (contactDanger) { markerColour = irr::video::SColor(255, 255, 50, 40); }

        //A buoy is only drawn once it's been classified as stationary-or-not, but a SHIP
        //should be shown whenever we have a valid range estimate - even if the tracker
        //has (momentarily) flagged it stationary. This stops real ship contacts -
        //e.g. one dead ahead moving slowly - from disappearing off the overlay.
        bool inRange = (thisEstimate.range <= getRangeNm()) && thisEstimate.range != 0; //kyara fix: estimate.range is in Nm

        //range in pixels + screen location (computed up-front so we can scope-test)
        irr::f32 contactRangePx = (irr::f32)bitmapWidth / 2.0 * thisEstimate.range / getRangeNm();
        irr::s32 deltaX = originX + contactRangePx * sin((thisEstimate.bearing + radarOffsetAngle) * RAD_IN_DEG);
        irr::s32 deltaY = originY - contactRangePx * cos((thisEstimate.bearing + radarOffsetAngle) * RAD_IN_DEG);

        //kyara: décentrage -> ne pas dessiner un contact dont la position tombe hors du cercle FIXE du scope
        irr::f32 sdx = (irr::f32)deltaX - (irr::f32)radarRadiusPx;
        irr::f32 sdy = (irr::f32)deltaY - (irr::f32)radarRadiusPx;
        bool inScope = (sdx * sdx + sdy * sdy) <= (irr::f32)radarRadiusPx * (irr::f32)radarRadiusPx;

        bool drawThisContact = inRange && inScope && (!contactIsBuoy || !thisEstimate.stationary);

        if (drawThisContact) {
            //Contact is in range and inside the scope (deltaX/deltaY computed above)

            //Show contact on screen: ships as a triangle, buoys as a circle
            if (contactIsBuoy) {
                drawCircle(radarImageOverlaid, deltaX, deltaY, radarRadiusPx / 40, markerColour.getAlpha(), markerColour.getRed(), markerColour.getGreen(), markerColour.getBlue());
            }
            else {
                // Draw the ARPA triangle marker for ship contacts, rotated to the target's
                // actual heading (absHeading is the target's true/world compass heading).
                // radarOffsetAngle applies the same head-up/course-up screen rotation used
                // for the contact's position and the heading vector, so the triangle, the
                // vector line, and the trail all agree on-screen.
                irr::f32 markerHeadingDeg = (useRealHeading ? thisEstimate.trueHeading : thisEstimate.absHeading) + radarOffsetAngle;                drawTriangle(radarImageOverlaid, deltaX, deltaY, radarRadiusPx / 30,
                    markerColour.getAlpha(), markerColour.getRed(), markerColour.getGreen(), markerColour.getBlue(),
                    markerHeadingDeg);
            }

            if (contactDanger && flashOn) {
                const irr::f32 rr = (irr::f32)radarRadiusPx / 16.0f;
                drawCircle(radarImageOverlaid, deltaX, deltaY, rr, 255, 255, 50, 40);
                drawCircle(radarImageOverlaid, deltaX, deltaY, rr + 1.0f, 255, 255, 50, 40);
            }
            if ((int)i == selectedContact && !thisEstimate.stationary) {
                const irr::f32 h = (irr::f32)radarRadiusPx / 20.0f;
                drawLine(radarImageOverlaid, deltaX - h, deltaY - h, deltaX + h, deltaY - h, 255, 255, 255, 255);
                drawLine(radarImageOverlaid, deltaX + h, deltaY - h, deltaX + h, deltaY + h, 255, 255, 255, 255);
                drawLine(radarImageOverlaid, deltaX + h, deltaY + h, deltaX - h, deltaY + h, 255, 255, 255, 255);
                drawLine(radarImageOverlaid, deltaX - h, deltaY + h, deltaX - h, deltaY - h, 255, 255, 255, 255);
            }

            // Draw Real MMSI or fallback to ARPA Track ID
            irr::u32 numberToDraw = 0;
            if (showMMSI && !contactIsBuoy && arpaContacts.at(i).mmsi > 0) {
                numberToDraw = arpaContacts.at(i).mmsi; // Use REAL 9-digit MMSI
            }
            else if (thisEstimate.displayID > 0) {
                numberToDraw = thisEstimate.displayID; // Fallback to tracking 1, 2, 3...
            }

            if (numberToDraw > 0) {
                irr::video::IImage* idNumberImage = NumberToImage::getImage(numberToDraw, device);
                if (idNumberImage) {
                    irr::core::rect<irr::s32> sourceRect = irr::core::rect<irr::s32>(0, 0, idNumberImage->getDimension().Width, idNumberImage->getDimension().Height);
                    idNumberImage->copyToWithAlpha(radarImageOverlaid, irr::core::position2d<irr::s32>(deltaX - 10, deltaY - 10), sourceRect, irr::video::SColor(255, 255, 255, 255));
                    idNumberImage->drop();
                }
            }
            //draw a vector
            // ONLY draw the heading vector for ships. Buoys do not get a forward-pointing speed vector.
            if (!contactIsBuoy) {
                irr::f32 adjustedVectorX;
                irr::f32 adjustedVectorZ;
                if (trueVectors) {
                    adjustedVectorX = thisEstimate.absVectorX;
                    adjustedVectorZ = thisEstimate.absVectorZ;
                }
                else if (trialOn) {
                    //Relative to own ship on her trial course and speed
                    const irr::f32 trialMps = trialSpeedKts / MPS_TO_KTS;
                    adjustedVectorX = thisEstimate.absVectorX - trialMps * sin(trialCourseDeg * RAD_IN_DEG);
                    adjustedVectorZ = thisEstimate.absVectorZ - trialMps * cos(trialCourseDeg * RAD_IN_DEG);
                }
                else {
                    adjustedVectorX = thisEstimate.relVectorX;
                    adjustedVectorZ = thisEstimate.relVectorZ;
                }

                //Rotate if in head/course up mode
                if (headUp) {
                    irr::f32 cosOffsetAngle = cos(-1 * radarOffsetAngle * irr::core::DEGTORAD);
                    irr::f32 sinOffsetAngle = sin(-1 * radarOffsetAngle * irr::core::DEGTORAD);

                    //Implement rotation here
                    irr::f32 newX = adjustedVectorX * cosOffsetAngle - adjustedVectorZ * sinOffsetAngle;
                    irr::f32 newZ = adjustedVectorX * sinOffsetAngle + adjustedVectorZ * cosOffsetAngle;

                    adjustedVectorX = newX;
                    adjustedVectorZ = newZ;
                }

                irr::s32 headingVectorX = Utilities::round(((irr::f32)bitmapWidth / 2.0) * adjustedVectorX * 60 * vectorLengthMinutes / (M_IN_NM * getRangeNm())); //Vector length in pixels
                irr::s32 headingVectorY = Utilities::round(((irr::f32)bitmapWidth / 2.0) * -1 * adjustedVectorZ * 60 * vectorLengthMinutes / (M_IN_NM * getRangeNm()));

                drawLine(radarImageOverlaid, deltaX, deltaY, deltaX + headingVectorX, deltaY + headingVectorY, markerColour.getAlpha(), markerColour.getRed(), markerColour.getGreen(), markerColour.getBlue());
            }
        }
        // --- NEW: DRAW BUOY TRAILS (Historical Path, as dots) ---
        const uint64_t trailStart = (lastAbsoluteTime > (uint64_t)(trailMinutes * 60.0f)) ? lastAbsoluteTime - (uint64_t)(trailMinutes * 60.0f) : 0;
        if (showBuoyTrails && contactIsBuoy && arpaContacts.at(i).scans.size() > 1) {
            // Skip the most recent scan (index size()-1): that position is already
            // marked by the live contact symbol, so only the older scans get dots.
            for (size_t s = 0; s < arpaContacts.at(i).scans.size() - 1; s++) {
                if (arpaContacts.at(i).scans[s].timeStamp < trailStart) { continue; }
                irr::f32 relX, relZ;
                if (trueVectors) {
                    // True Trails: Absolute world position of the past scan, relative to current own ship position
                    relX = arpaContacts.at(i).scans[s].x - absolutePosition.X;
                    relZ = arpaContacts.at(i).scans[s].z - absolutePosition.Z;
                }
                else {
                    // Relative Trails: Where the target was relative to the ship at the time of the scan
                    relX = arpaContacts.at(i).scans[s].rangeNm * M_IN_NM * sin(arpaContacts.at(i).scans[s].bearingDeg * RAD_IN_DEG);
                    relZ = arpaContacts.at(i).scans[s].rangeNm * M_IN_NM * cos(arpaContacts.at(i).scans[s].bearingDeg * RAD_IN_DEG);
                }

                if (headUp) {
                    irr::f32 cosO = cos(-1 * radarOffsetAngle * irr::core::DEGTORAD);
                    irr::f32 sinO = sin(-1 * radarOffsetAngle * irr::core::DEGTORAD);
                    irr::f32 nx = relX * cosO - relZ * sinO;
                    irr::f32 nz = relX * sinO + relZ * cosO;
                    relX = nx; relZ = nz;
                }

                irr::s32 px = originX + (relX / (M_IN_NM * getRangeNm())) * (bitmapWidth / 2.0);
                irr::s32 py = originY - (relZ / (M_IN_NM * getRangeNm())) * (bitmapWidth / 2.0);

                // Small filled dot (a 1px cross reads more reliably than drawCircle's outline trace at tiny radii)
                drawLine(radarImageOverlaid, px - 1, py, px + 1, py,
                    255, buoyContactColour.getRed(), buoyContactColour.getGreen(), buoyContactColour.getBlue());
                drawLine(radarImageOverlaid, px, py - 1, px, py + 1,
                    255, buoyContactColour.getRed(), buoyContactColour.getGreen(), buoyContactColour.getBlue());
            }
        }
        // -----------------------------

        // --- NEW: DRAW SHIP TRAILS (Historical Path, as dots) ---
        if (showShipTrails && !contactIsBuoy && arpaContacts.at(i).scans.size() > 1) {
            // Skip the most recent scan (index size()-1): that position is already
            // marked by the live contact symbol, so only the older scans get dots.
            for (size_t s = 0; s < arpaContacts.at(i).scans.size() - 1; s++) {
                if (arpaContacts.at(i).scans[s].timeStamp < trailStart) { continue; }
                irr::f32 relX, relZ;
                if (trueVectors) {
                    // True Trails: Absolute world position of the past scan, relative to current own ship position
                    relX = arpaContacts.at(i).scans[s].x - absolutePosition.X;
                    relZ = arpaContacts.at(i).scans[s].z - absolutePosition.Z;
                }
                else {
                    // Relative Trails: Where the target was relative to the ship at the time of the scan
                    relX = arpaContacts.at(i).scans[s].rangeNm * M_IN_NM * sin(arpaContacts.at(i).scans[s].bearingDeg * RAD_IN_DEG);
                    relZ = arpaContacts.at(i).scans[s].rangeNm * M_IN_NM * cos(arpaContacts.at(i).scans[s].bearingDeg * RAD_IN_DEG);
                }

                if (headUp) {
                    irr::f32 cosO = cos(-1 * radarOffsetAngle * irr::core::DEGTORAD);
                    irr::f32 sinO = sin(-1 * radarOffsetAngle * irr::core::DEGTORAD);
                    irr::f32 nx = relX * cosO - relZ * sinO;
                    irr::f32 nz = relX * sinO + relZ * cosO;
                    relX = nx; relZ = nz;
                }

                irr::s32 px = originX + (relX / (M_IN_NM * getRangeNm())) * (bitmapWidth / 2.0);
                irr::s32 py = originY - (relZ / (M_IN_NM * getRangeNm())) * (bitmapWidth / 2.0);

                // Small filled dot (a 1px cross reads more reliably than drawCircle's outline trace at tiny radii)
                drawLine(radarImageOverlaid, px - 1, py, px + 1, py,
                    255, shipContactColour.getRed(), shipContactColour.getGreen(), shipContactColour.getBlue());
                drawLine(radarImageOverlaid, px, py - 1, px, py + 1,
                    255, shipContactColour.getRed(), shipContactColour.getGreen(), shipContactColour.getBlue());
            }
        }
        // -----------------------------

    }

    // --- NEW: DRAW OWN SHIP TRAIL (Historical Track, as dots) ---
    // Own ship sits at the radar centre by definition in relative motion mode, so a trail
    // is only meaningful in True motion mode (trueVectors): it traces where own ship actually
    // travelled, relative to its current absolute position (which is plotted at the centre).
    if (showOwnShipTrails && trueVectors && ownShipScans.size() > 1) {
        // Skip the most recent entry: that position is the current centre marker itself.
        const uint64_t ownTrailStart = (lastAbsoluteTime > (uint64_t)(trailMinutes * 60.0f)) ? lastAbsoluteTime - (uint64_t)(trailMinutes * 60.0f) : 0;
        for (size_t s = 0; s < ownShipScans.size() - 1; s++) {
            if (ownShipScans[s].timeStamp < ownTrailStart) { continue; }
            irr::f32 relX = ownShipScans[s].x - absolutePosition.X;
            irr::f32 relZ = ownShipScans[s].z - absolutePosition.Z;

            if (headUp) {
                irr::f32 cosO = cos(-1 * radarOffsetAngle * irr::core::DEGTORAD);
                irr::f32 sinO = sin(-1 * radarOffsetAngle * irr::core::DEGTORAD);
                irr::f32 nx = relX * cosO - relZ * sinO;
                irr::f32 nz = relX * sinO + relZ * cosO;
                relX = nx; relZ = nz;
            }

            irr::s32 px = originX + (relX / (M_IN_NM * getRangeNm())) * (bitmapWidth / 2.0);
            irr::s32 py = originY - (relZ / (M_IN_NM * getRangeNm())) * (bitmapWidth / 2.0);

            // Small filled dot, same style as other ship/buoy trails
            drawLine(radarImageOverlaid, px - 1, py, px + 1, py,
                255, shipContactColour.getRed(), shipContactColour.getGreen(), shipContactColour.getBlue());
            drawLine(radarImageOverlaid, px, py - 1, px, py + 1,
                255, shipContactColour.getRed(), shipContactColour.getGreen(), shipContactColour.getBlue());
        }
    }
    // -----------------------------

    //Keep the outermost rows and columns of the image in the surround colour. RadarScreen shows
    //the scope with a small margin and clamps the texture at its edges, so whatever is in the edge
    //pixels is stretched across that margin. The scope circle touches the top and left edges (at 000
    //and 270), and an echo there was drawn as a block of echo outside the ring.
    {
        const irr::video::SColor surround = getRadarSurroundColour();
        const irr::u32 w = radarImageOverlaid->getDimension().Width;
        const irr::u32 h = radarImageOverlaid->getDimension().Height;
        if (w > 0 && h > 0) {
            for (irr::u32 x = 0; x < w; x++) {
                radarImageOverlaid->setPixel(x, 0, surround);
                radarImageOverlaid->setPixel(x, h - 1, surround);
            }
            for (irr::u32 y = 0; y < h; y++) {
                radarImageOverlaid->setPixel(0, y, surround);
                radarImageOverlaid->setPixel(w - 1, y, surround);
            }
        }
    }
}
void RadarCalculation::drawSector(irr::video::IImage* radarImage, irr::f32 centreX, irr::f32 centreY, irr::f32 innerRadius, irr::f32 outerRadius, irr::f32 startAngle, irr::f32 endAngle, irr::u32 alpha, irr::u32 red, irr::u32 green, irr::u32 blue, irr::f32 ownShipHeading)
//draw a bounded sector
{

    //IPROF_FUNC;

    if (headUp) {
        startAngle -= ownShipHeading;
        endAngle -= ownShipHeading;
    }

    //find the corner points (Fixme: Not quite right when the extreme point is on the outer curve)
    irr::f32 sinStartAngle = std::sin(irr::core::DEGTORAD * startAngle);
    irr::f32 cosStartAngle = std::cos(irr::core::DEGTORAD * startAngle);
    irr::f32 sinEndAngle = std::sin(irr::core::DEGTORAD * endAngle);
    irr::f32 cosEndAngle = std::cos(irr::core::DEGTORAD * endAngle);

    irr::f32 point1X = centreX + sinStartAngle * innerRadius;
    irr::f32 point1Y = centreY - cosStartAngle * innerRadius;
    irr::f32 point2X = centreX + sinStartAngle * outerRadius;
    irr::f32 point2Y = centreY - cosStartAngle * outerRadius;
    irr::f32 point3X = centreX + sinEndAngle * outerRadius;
    irr::f32 point3Y = centreY - cosEndAngle * outerRadius;
    irr::f32 point4X = centreX + sinEndAngle * innerRadius;
    irr::f32 point4Y = centreY - cosEndAngle * innerRadius;

    //find the 'bounding box'
    irr::s32 minX = std::min(std::min(point1X, point2X), std::min(point3X, point4X));
    irr::s32 maxX = std::max(std::max(point1X, point2X), std::max(point3X, point4X));
    irr::s32 minY = std::min(std::min(point1Y, point2Y), std::min(point3Y, point4Y));
    irr::s32 maxY = std::max(std::max(point1Y, point2Y), std::max(point3Y, point4Y));

    irr::f32 innerRadiusSqr = innerRadius * innerRadius;
    irr::f32 outerRadiusSqr = outerRadius * outerRadius;

    //kyara: geometry of the FIXED scope circle, matching what render() uses (bitmapWidth and
    //centrePixel), so echoes are clipped to exactly the same circle the range rings are drawn on.
    //Hoisted out of the pixel loops below - it doesn't change per pixel.
    irr::f32 scopeBitmapWidth = (irr::f32)(radarRadiusPx * 2);
    if (radarImage) {
        irr::f32 imageWidth = (irr::f32)radarImage->getDimension().Width;
        if (imageWidth > 0 && imageWidth < scopeBitmapWidth) { scopeBitmapWidth = imageWidth; }
    }
    const irr::f32 scopeCentre = (scopeBitmapWidth - 1.0f) * 0.5f;
    const irr::f32 scopeRadiusSqr = (scopeBitmapWidth * 0.5f) * (scopeBitmapWidth * 0.5f);

    //draw the points
    for (int i = minX; i <= maxX; i++) {
        irr::f32 localX = i - centreX; //position referred to centre
        irr::f32 localXSq = localX * localX;

        for (int j = minY; j <= maxY; j++) {

            irr::f32 localY = j - centreY; //position referred to centre

            irr::f32 localRadiusSqr = localXSq + localY * localY; //check radius of points
            //irr::f32 localAngle = irr::core::RADTODEG*std::atan2(localX,-1*localY); //check angle of point
            //irr::f32 localAngle = irr::core::RADTODEG*fast_atan2f(localX,-1*localY);

       //if the point is within the limits, plot it
            if (localRadiusSqr >= innerRadiusSqr && localRadiusSqr <= outerRadiusSqr) {
                //if (Angles::isAngleBetween(localAngle,startAngle,endAngle)) {
                if (Angles::isAngleBetween(irr::core::vector2df(localX, -1 * localY), irr::core::vector2df(sinStartAngle, cosStartAngle), irr::core::vector2df(sinEndAngle, cosEndAngle))) {
                    //Plot i,j
                    if (i >= 0 && j >= 0) {
                        //kyara: ALWAYS bound the echo to the fixed scope circle (this used to be
                        //done only when décentrage was active). Without it, echoes spill past the
                        //circle into the square corners of the bitmap, and the straight bitmap edge
                        //then chops the picture flat at the top and bottom of the display instead of
                        //it ending on a clean circular edge.
                        irr::f32 scopeDX = (irr::f32)i - scopeCentre;
                        irr::f32 scopeDY = (irr::f32)j - scopeCentre;
                        bool insideScope = (scopeDX * scopeDX + scopeDY * scopeDY) <= scopeRadiusSqr;
                        if (insideScope) { radarImage->setPixel(i, j, irr::video::SColor(alpha, red, green, blue)); }
                    }
                }
            }
        }
    }
} // <--- This closes the drawSector function (if it was an inline block) OR the loops calling it.



void RadarCalculation::drawLine(irr::video::IImage* radarImage, irr::f32 startX, irr::f32 startY, irr::f32 endX, irr::f32 endY, irr::u32 alpha, irr::u32 red, irr::u32 green, irr::u32 blue)//Try with irr::f32 as inputs so we can do interpolation based on the theoretical start and end
{

    irr::f32 deltaX = endX - startX;
    irr::f32 deltaY = endY - startY;

    irr::f32 lengthSum = std::abs(deltaX) + std::abs(deltaY);

    irr::u32 radiusSquared = pow(radarRadiusPx, 2);

    if (lengthSum > 0) {
        for (irr::f32 i = 0; i <= 1; i += 1 / lengthSum) {
            irr::s32 thisX = Utilities::round(startX + deltaX * i);
            irr::s32 thisY = Utilities::round(startY + deltaY * i);
            //Find distance from centre
            irr::s32 centreToX = thisX - radarRadiusPx;
            irr::s32 centreToY = thisY - radarRadiusPx;
            if (thisX >= 0 && thisY >= 0) {
                radarImage->setPixel(thisX, thisY, irr::video::SColor(alpha, red, green, blue));
            }

        }
    }
    else {
        irr::s32 thisX = Utilities::round(startX);
        irr::s32 thisY = Utilities::round(startY);
        //Find distance from centre
        irr::s32 centreToX = thisX - radarRadiusPx;
        irr::s32 centreToY = thisY - radarRadiusPx;
        if (pow(centreToX, 2) + pow(centreToY, 2) <= radiusSquared) {
            if (thisX >= 0 && thisY >= 0) { radarImage->setPixel(thisX, thisY, irr::video::SColor(alpha, red, green, blue)); }
        }
    }
}

void RadarCalculation::drawCircle(irr::video::IImage* radarImage, irr::f32 centreX, irr::f32 centreY, irr::f32 radius, irr::u32 alpha, irr::u32 red, irr::u32 green, irr::u32 blue)//Try with irr::f32 as inputs so we can do interpolation based on the theoretical start and end
{
    irr::f32 circumference = 2.0 * PI * radius;

    irr::u32 radiusSquared = pow(radarRadiusPx, 2);

    if (circumference > 0) {
        for (irr::f32 i = 0; i <= 1; i += 1 / circumference) {
            irr::s32 thisX = Utilities::round(centreX + radius * sin(i * 2 * PI));
            irr::s32 thisY = Utilities::round(centreY + radius * cos(i * 2 * PI));
            //Find distance from centre
            irr::s32 centreToX = thisX - radarRadiusPx;
            irr::s32 centreToY = thisY - radarRadiusPx;
            if (pow(centreToX, 2) + pow(centreToY, 2) <= radiusSquared) {
                if (thisX >= 0 && thisY >= 0) {
                    radarImage->setPixel(thisX, thisY, irr::video::SColor(alpha, red, green, blue));
                }
            }
        }
    }
    else {
        irr::s32 thisX = Utilities::round(centreX);
        irr::s32 thisY = Utilities::round(centreY);
        //Find distance from centre
        irr::s32 centreToX = thisX - radarRadiusPx;
        irr::s32 centreToY = thisY - radarRadiusPx;
        if (pow(centreToX, 2) + pow(centreToY, 2) <= radiusSquared) {
            if (thisX >= 0 && thisY >= 0) {
                radarImage->setPixel(thisX, thisY, irr::video::SColor(alpha, red, green, blue));
            }
        }
    }
}

irr::f32 RadarCalculation::rangeAtAngle(irr::f32 checkAngle, irr::f32 centreX, irr::f32 centreZ, irr::f32 heading)
{
    //Special case is if heading and checkAngle are identical. In this case, return the centre point if it lies on the angle, and 0 if not
    if (std::abs(Angles::normaliseAngle(checkAngle - heading)) < 0.001) {
        if (Angles::normaliseAngle(irr::core::RADTODEG * std::atan2(centreX, centreZ) - checkAngle) < 0.1) {
            return std::sqrt(std::pow(centreX, 2) + std::pow(centreZ, 2));
        }
        else {
            return 0;
        }
    }

    irr::f32 lambda; //This is the distance from the centre of the contact

    lambda = (centreX - centreZ * tan(irr::core::DEGTORAD * checkAngle)) / (cos(irr::core::DEGTORAD * heading) * tan(irr::core::DEGTORAD * checkAngle) - sin(irr::core::DEGTORAD * heading));

    irr::f32 distanceSqr = std::pow(lambda, 2) + lambda * (2 * centreX * sin(irr::core::DEGTORAD * heading) + 2 * centreZ * cos(irr::core::DEGTORAD * heading)) + (std::pow(centreX, 2) + std::pow(centreZ, 2));

    irr::f32 distance = 0;

    if (distanceSqr > 0) {
        distance = std::sqrt(distanceSqr);
    }

    return distance;

}

irr::f32 RadarCalculation::radarNoise(irr::f32 radarNoiseLevel, irr::f32 radarSeaClutter, irr::f32 radarRainClutter, irr::f32 weather, irr::f32 radarRange, irr::f32 radarBrgDeg, irr::f32 windDirectionDeg, irr::f32 radarInclinationAngle, irr::f32 rainIntensity)
//radarRange in metres
{
    irr::f32 radarNoiseVal = 0;

    if (radarRange != 0) {

        irr::f32 randomValue = (irr::f32)rand() / RAND_MAX; //store this so we can manipulate the random distribution;
        irr::f32 randomValueSea = (irr::f32)rand() / RAND_MAX; //different value for sea clutter;

        //reshape the uniform random distribution into one with an infinite tail up to high values
        irr::f32 randomValueWithTail = 0;
        if (randomValue > 0) {
            //3rd power is to shape distribution so sufficient high energy returns are generated
            randomValueWithTail = randomValue * pow((1 / randomValue) - 1, 3);
        }

        //same for sea clutter noise
        irr::f32 randomValueWithTailSea = 0;
        if (randomValueSea > 0) {
            if (radarInclinationAngle > 0) {
                randomValueWithTailSea = 0; //if radar is scanning upwards, must be above sea surface, so don't add clutter
            }
            else {
                //3rd power is to shape distribution so sufficient high energy returns are generated
                randomValueWithTailSea = randomValueSea * pow((1 / randomValueSea) - 1, 3);
            }
        }

        //less high power returns for rain clutter - roughly gaussian, so get an average of independent random numbers
        irr::f32 randomValueWithTailRain = ((irr::f32)rand() / RAND_MAX + (irr::f32)rand() / RAND_MAX + (irr::f32)rand() / RAND_MAX + (irr::f32)rand() / RAND_MAX) / 4.0;

        //Apply directional correction to the clutter, so most is upwind, some is downwind. Mean value = 1
        irr::f32 relativeWindAngle = (windDirectionDeg - radarBrgDeg) * RAD_IN_DEG;
        irr::f32 windCorrectionFactor = 2.5 * (0.5 * (cos(2 * relativeWindAngle) + 1)) * (0.5 + sin(relativeWindAngle / 2.0) * 0.5);
        randomValueWithTailSea = randomValueWithTailSea * windCorrectionFactor;

        //noise is constant
        radarNoiseVal = radarNoiseLevel * randomValueWithTail;
        //clutter falls off with distance^3, and is normalised for weather#=6
        radarNoiseVal += radarSeaClutter * randomValueWithTailSea * (weather / 6.0) * pow((M_IN_NM / radarRange), 3);
        //rain clutter falls off with distance^2, and is normalised for rainIntensity#=10
        radarNoiseVal += radarRainClutter * randomValueWithTailRain * (rainIntensity / 10.0) * (rainIntensity / 10.0) * pow((M_IN_NM / radarRange), 2);
    }

    return radarNoiseVal;
}

bool RadarCalculation::isPointInEllipse(irr::f32 pointX, irr::f32 pointZ, irr::f32 centreX, irr::f32 centreZ, irr::f32 width, irr::f32 length, irr::f32 angle)
{

    // Quick first check
    if (fmax(abs(pointX - centreX), abs(pointZ - centreZ)) > fmax(width, length)) {
        return false;
    }

    // Detailed check

    // See https://stackoverflow.com/a/16824748/12829372
    irr::f32 cosAngle = cos(-1.0 * angle * irr::core::DEGTORAD);
    irr::f32 sinAngle = sin(-1.0 * angle * irr::core::DEGTORAD);

    irr::f32 halfWidth2 = width / 2 * width / 2;
    irr::f32 halfLength2 = length / 2 * length / 2;

    if (halfLength2 == 0 || halfWidth2 == 0) {
        return false;
    }

    irr::f32 paramA = pow(cosAngle * (pointX - centreX) + sinAngle * (pointZ - centreZ), 2);
    irr::f32 paramB = pow(sinAngle * (pointX - centreX) - cosAngle * (pointZ - centreZ), 2);

    irr::f32 ellipse = (paramA / halfWidth2) + (paramB / halfLength2);

    if (ellipse <= 1) {
        return true;
    }
    else {
        return false;
    }

}

irr::video::SColor RadarCalculation::getRadarForegroundColour() const
{
    if (currentRadarColourChoice < radarForegroundColours.size()) {
        return radarForegroundColours.at(currentRadarColourChoice);
    }
    else {
        return irr::video::SColor(255, 255, 220, 0);
    }
}

irr::video::SColor RadarCalculation::getRadarBackgroundColour() const
{
    if (currentRadarColourChoice < radarBackgroundColours.size()) {
        return radarBackgroundColours.at(currentRadarColourChoice);
    }
    else {
        return irr::video::SColor(255, 0, 0, 200);
    }
}

irr::video::SColor RadarCalculation::getRadarSurroundColour() const
{
    if (currentRadarColourChoice < radarSurroundColours.size()) {
        return radarSurroundColours.at(currentRadarColourChoice);
    }
    else {
        return irr::video::SColor(255, 0, 50, 20); // deep green fallback
    }
}


//Fixed colour palette. The index order MUST match the colour combo boxes in GUIMain.
irr::video::SColor RadarCalculation::paletteIndexToColour(int paletteIndex)
{
    switch (paletteIndex) {
    case 0: return irr::video::SColor(255, 255, 255, 0); //Yellow
    case 1: return irr::video::SColor(255, 0, 255, 255); //Cyan
    case 2: return irr::video::SColor(255, 0, 255, 0); //Green
    case 3: return irr::video::SColor(255, 255, 0, 0); //Red
    case 4: return irr::video::SColor(255, 255, 0, 255); //Magenta
    case 5: return irr::video::SColor(255, 255, 255, 255); //White
    case 6: return irr::video::SColor(255, 255, 150, 0); //Orange
    default: return irr::video::SColor(255, 255, 255, 255); //White fallback
    }
}

void RadarCalculation::setBuoyContactColour(int paletteIndex)
{
    buoyContactColour = paletteIndexToColour(paletteIndex);
}

void RadarCalculation::setShipContactColour(int paletteIndex)
{
    shipContactColour = paletteIndexToColour(paletteIndex);
}

irr::video::SColor RadarCalculation::getBuoyContactColour() const
{
    return buoyContactColour;
}

irr::video::SColor RadarCalculation::getShipContactColour() const
{
    return shipContactColour;
}

//Draw an upward-pointing triangle outline (used to distinguish ship contacts from buoys).
void RadarCalculation::drawTriangle(irr::video::IImage* radarImage, irr::f32 centreX, irr::f32 centreY, irr::f32 radius, irr::u32 alpha, irr::u32 red, irr::u32 green, irr::u32 blue, irr::f32 headingDeg)
{
    //KYARA HEADING TRIANGLE: rotate the marker to point towards headingDeg (screen-space
    //bearing, same convention as everywhere else in this file: 0 = up/top of screen,
    //clockwise positive). Previously this always pointed straight up regardless of the
    //target's actual heading, which meant the triangle gave no real directional cue and
    //couldn't be compared against the heading vector line. headingDeg defaults to 0 so any
    //caller that doesn't care about orientation keeps the old straight-up behaviour.
    irr::f32 headingRad = headingDeg * irr::core::DEGTORAD;

    //Three vertices, evenly spaced, with the first pointing towards headingDeg
    //(screen Y increases downward, so "up"/0 is -Y, consistent with the rest of the file)
    irr::f32 v1x = centreX + radius * sin(headingRad);                     irr::f32 v1y = centreY - radius * cos(headingRad);
    irr::f32 v2x = centreX + radius * sin(headingRad + 2.0 * PI / 3.0);     irr::f32 v2y = centreY - radius * cos(headingRad + 2.0 * PI / 3.0);
    irr::f32 v3x = centreX + radius * sin(headingRad + 4.0 * PI / 3.0);     irr::f32 v3y = centreY - radius * cos(headingRad + 4.0 * PI / 3.0);

    drawLine(radarImage, v1x, v1y, v2x, v2y, alpha, red, green, blue);
    drawLine(radarImage, v2x, v2y, v3x, v3y, alpha, red, green, blue);
    drawLine(radarImage, v3x, v3y, v1x, v1y, alpha, red, green, blue);
}

void RadarCalculation::setBuoyTrails(bool on) { showBuoyTrails = on; }
void RadarCalculation::setShipTrails(bool on) { showShipTrails = on; }
void RadarCalculation::setOwnShipTrails(bool on) { showOwnShipTrails = on; }
void RadarCalculation::setMMSI(bool on) { showMMSI = on; }
void RadarCalculation::setUseRealHeading(bool state) { useRealHeading = state; }
void RadarCalculation::cycleEchoStretch() { echoStretchLevel = (echoStretchLevel + 1) % 4; } //kyara: OFF/1/2/3
void RadarCalculation::toggleOffCentre() {
    //kyara: si centré -> décale vers le curseur (borné à 0.66 R); sinon -> recentre
    if (fabs(offsetXFraction) < 0.001f && fabs(offsetYFraction) < 0.001f) {
        irr::f32 rng = getRangeNm();
        if (rng <= 0) return;
        irr::f32 fx = cursorRangeXNm / rng;
        irr::f32 fy = cursorRangeYNm / rng;
        //kyara radar modding 
        const irr::f32 lim = 0.50f;
        if (fx > lim) fx = lim; if (fx < -lim) fx = -lim;
        if (fy > lim) fy = lim; if (fy < -lim) fy = -lim;
        offsetXFraction = fx; offsetYFraction = fy;
        radarScreenStale = true; //kyara: l'origine a bougé -> tout le bitmap doit être redessiné
    }
    else {
        offsetXFraction = 0.4f; offsetYFraction = 0.4f;

    }
}
irr::f32 RadarCalculation::getOffsetXFraction() const { return offsetXFraction; }
irr::f32 RadarCalculation::getOffsetYFraction() const { return offsetYFraction; }
int  RadarCalculation::getEchoStretch() const { return echoStretchLevel; }
bool RadarCalculation::getUseRealHeading() const { return useRealHeading; }

//=================================================================================================
//Mouse on the scope, parallel index lines, CPA alarm, trial manoeuvre, lengths, coastline
//=================================================================================================
void RadarCalculation::scopeMouse(bool left, bool down, irr::core::vector2di mouseRelPosition)
{
    ScopeMouseEvent e;
    e.left = left;
    e.down = down;
    e.rel = mouseRelPosition;
    scopeEvents.push_back(e);
}

bool RadarCalculation::takeArpaModeChangedByClick()
{
    const bool changed = arpaModeChangedByClick;
    arpaModeChangedByClick = false;
    return changed;
}

void RadarCalculation::setCursorFromMouse(irr::core::vector2di mouseRelPosition, irr::f32 ownShipHeading)
{
    if (radarRadiusPx == 0) { return; }
    const irr::f32 xNm = (irr::f32)mouseRelPosition.X / (irr::f32)radarRadiusPx * getRangeNm();
    const irr::f32 yNm = -1.0f * (irr::f32)mouseRelPosition.Y / (irr::f32)radarRadiusPx * getRangeNm();
    if (std::sqrt(xNm * xNm + yNm * yNm) <= getRangeNm()) {
        cursorRangeXNm = xNm;
        cursorRangeYNm = yNm;
    }
    CursorBrg = irr::core::RADTODEG * std::atan2(cursorRangeXNm, cursorRangeYNm);
    if (headUp) { CursorBrg += ownShipHeading; }
    CursorBrg = Angles::normaliseAngle(CursorBrg);
    CursorRangeNm = std::sqrt(cursorRangeXNm * cursorRangeXNm + cursorRangeYNm * cursorRangeYNm);
}

int RadarCalculation::contactNearPoint(irr::f32 xNm, irr::f32 yNm, bool tracked) const
{
    //Within a sixth of the range ring spacing... at least 150 m
    irr::f32 best = irr::core::max_(getRangeNm() / 12.0f, 150.0f / M_IN_NM);
    int found = -1;
    for (size_t i = 0; i < arpaContacts.size(); i++) {
        const ARPAEstimatedState& e = arpaContacts[i].estimate;
        if (arpaContacts[i].isBuoy || e.range <= 0 || e.lost) { continue; }
        if (tracked && e.stationary) { continue; }
        const irr::f32 cx = e.range * sin(e.bearing * RAD_IN_DEG);
        const irr::f32 cy = e.range * cos(e.bearing * RAD_IN_DEG);
        const irr::f32 dist = std::sqrt((cx - xNm) * (cx - xNm) + (cy - yNm) * (cy - yNm));
        if (dist < best) {
            best = dist;
            found = (int)i;
        }
    }
    return found;
}

void RadarCalculation::handleScopeEvents(irr::f32 ownShipHeading)
{
    for (size_t i = 0; i < scopeEvents.size(); i++) {
        const ScopeMouseEvent& e = scopeEvents[i];
        setCursorFromMouse(e.rel, ownShipHeading);
        const irr::f32 xNm = CursorRangeNm * sin(CursorBrg * RAD_IN_DEG); //true east
        const irr::f32 yNm = CursorRangeNm * cos(CursorBrg * RAD_IN_DEG); //true north

        if (e.left && e.down) {
            pressXNm = xNm;
            pressYNm = yNm;
            pressPending = true;
            continue;
        }
        const bool editingPI = (piEditLine >= 0 && piEditLine < (int)piBearings.size() && piEditLine < (int)piRanges.size());
        if (e.left && !e.down) {
            if (!pressPending) { continue; }
            pressPending = false;
            const irr::f32 dx = xNm - pressXNm;
            const irr::f32 dy = yNm - pressYNm;
            const irr::f32 moved = std::sqrt(dx * dx + dy * dy);
            if (editingPI) {
                //The line through the press and the release: its direction, and its signed distance
                //from own ship (as drawn: along 'bearing', through the point at 'range' on bearing - 90)
                if (moved > getRangeNm() * 0.02f) {
                    const irr::f32 brg = Angles::normaliseAngle(std::atan2(dx, dy) / RAD_IN_DEG);
                    irr::f32 range = pressXNm * -cos(brg * RAD_IN_DEG) + pressYNm * sin(brg * RAD_IN_DEG);
                    if (fabs(range) < 0.001f) { range = 0.001f; } //(0 means "no line")
                    piBearings.at(piEditLine) = brg;
                    piRanges.at(piEditLine) = range;
                }
            }
            else if (moved < getRangeNm() * 0.03f) {
                //A click (not a drag of the cursor): track the ship echo there
                if (arpaMode < 1) {
                    setArpaMode(1); //MARPA: targets taken by hand
                    arpaModeChangedByClick = true;
                    pendingAcquireXNm = xNm;
                    pendingAcquireYNm = yNm;
                    pendingAcquireTries = 3; //estimates come with the next updates
                    continue;
                }
                const int c = contactNearPoint(xNm, yNm, false);
                if (c >= 0) {
                    ARPAContact& contact = arpaContacts.at(c);
                    contact.estimate.stationary = false;
                    if (contact.estimate.displayID > 0) {
                        setArpaListSelection(contact.estimate.displayID - 1);
                    }
                    else {
                        pendingSelectContact = c;
                    }
                }
            }
            continue;
        }
        if (!e.left && e.down) {
            if (editingPI) {
                piBearings.at(piEditLine) = 0;
                piRanges.at(piEditLine) = 0;
            }
            else {
                //Right click: stop tracking the ship there
                const int c = contactNearPoint(xNm, yNm, true);
                if (c >= 0) {
                    arpaContacts.at(c).estimate.stationary = true;
                    arpaContacts.at(c).estimate.danger = false;
                    arpaContacts.at(c).contact = 0;
                }
            }
        }
    }
    scopeEvents.clear();
}

void RadarCalculation::setPIEditLine(int line)
{
    piEditLine = (line >= 0 && line < PI_LINES && line < (int)piBearings.size()) ? line : -1;
    pressPending = false;
}
int RadarCalculation::getPIEditLine() const { return piEditLine; }
void RadarCalculation::clearPILines()
{
    for (size_t i = 0; i < piBearings.size() && i < piRanges.size(); i++) {
        piBearings[i] = 0;
        piRanges[i] = 0;
    }
}
int RadarCalculation::countPILines() const
{
    int n = 0;
    for (size_t i = 0; i < piRanges.size(); i++) { if (fabs(piRanges[i]) > 0.0001f) { n++; } }
    return n;
}

void RadarCalculation::setCPALimit(irr::f32 cpaNm) { cpaLimitNm = cpaNm; }
void RadarCalculation::setTCPALimit(irr::f32 tcpaMinutes) { tcpaLimitMinutes = tcpaMinutes; }
irr::f32 RadarCalculation::getCPALimit() const { return cpaLimitNm; }
irr::f32 RadarCalculation::getTCPALimit() const { return tcpaLimitMinutes; }
void RadarCalculation::setCPAAlarmOn(bool on) { cpaAlarmOn = on; }
bool RadarCalculation::getCPAAlarmOn() const { return cpaAlarmOn; }
bool RadarCalculation::isCPAAlarmSounding() const
{
    if (!cpaAlarmOn || !radarOn) { return false; }
    for (size_t i = 0; i < arpaContacts.size(); i++) {
        if (arpaContacts[i].estimate.danger && !arpaContacts[i].dangerAcknowledged) { return true; }
    }
    return false;
}
void RadarCalculation::acknowledgeCPAAlarm()
{
    for (size_t i = 0; i < arpaContacts.size(); i++) {
        if (arpaContacts[i].estimate.danger) { arpaContacts[i].dangerAcknowledged = true; }
    }
}
int RadarCalculation::countDangerousTargets() const
{
    int n = 0;
    for (size_t i = 0; i < arpaContacts.size(); i++) { if (arpaContacts[i].estimate.danger) { n++; } }
    return n;
}

void RadarCalculation::setTrial(bool on, irr::f32 courseDeg, irr::f32 speedKts, irr::f32 delayMinutes)
{
    trialOn = on;
    trialCourseDeg = Angles::normaliseAngle(courseDeg);
    trialSpeedKts = irr::core::max_(0.0f, speedKts);
    trialDelayMinutes = irr::core::max_(0.0f, delayMinutes);
}
bool RadarCalculation::getTrialOn() const { return trialOn; }
irr::f32 RadarCalculation::getTrialCourse() const { return trialCourseDeg; }
irr::f32 RadarCalculation::getTrialSpeed() const { return trialSpeedKts; }
irr::f32 RadarCalculation::getTrialDelay() const { return trialDelayMinutes; }

irr::f32 RadarCalculation::getVectorMinutes() const { return vectorLengthMinutes; }
void RadarCalculation::setTrailMinutes(irr::f32 minutes) { trailMinutes = minutes; }
irr::f32 RadarCalculation::getTrailMinutes() const { return trailMinutes; }
bool RadarCalculation::getShipTrails() const { return showShipTrails; }

void RadarCalculation::setCoastline(bool on)
{
    showCoastline = on;
    if (!on) { coastSegments.clear(); }
    coastRangeNm = 0; //worked out again at the next update
}
bool RadarCalculation::getCoastline() const { return showCoastline; }

void RadarCalculation::updateCoastline(irr::core::vector3d<int64_t> offsetPosition, const Terrain& terrain, const OwnShip& ownShip, irr::f32 tideHeight, uint64_t absoluteTime)
{
    const irr::core::vector3df pos = ownShip.getPosition();
    const irr::f32 absX = (irr::f32)offsetPosition.X + pos.X;
    const irr::f32 absZ = (irr::f32)offsetPosition.Z + pos.Z;
    const irr::f32 half = getRangeNm() * M_IN_NM * 1.05f;
    //Again when the range changes, own ship has moved a good part of it, the tide has changed, or now and then
    const irr::f32 moved = std::sqrt((absX - coastCentreX) * (absX - coastCentreX) + (absZ - coastCentreZ) * (absZ - coastCentreZ));
    if (coastRangeNm == getRangeNm() && moved < half * 0.15f && fabs(tideHeight - coastTide) < 0.25f && absoluteTime < coastTime + 60) {
        return;
    }
    coastRangeNm = getRangeNm();
    coastCentreX = absX;
    coastCentreZ = absZ;
    coastTide = tideHeight;
    coastTime = absoluteTime;
    coastSegments.clear();

    //Heights on a square grid around own ship (scene coordinates), then the sea level contour by
    //marching squares
    const int N = 128;
    const irr::f32 cell = 2.0f * half / N;
    std::vector<irr::f32> h((N + 1) * (N + 1));
    for (int j = 0; j <= N; j++) {
        for (int i = 0; i <= N; i++) {
            h[j * (N + 1) + i] = terrain.getHeight(pos.X - half + i * cell, pos.Z - half + j * cell) - tideHeight;
        }
    }
    const irr::f32 baseX = absX - half;
    const irr::f32 baseZ = absZ - half;
    auto edgePoint = [&](int i0, int j0, int i1, int j1, irr::f32& x, irr::f32& z) {
        const irr::f32 a = h[j0 * (N + 1) + i0];
        const irr::f32 b = h[j1 * (N + 1) + i1];
        irr::f32 t = (fabs(a - b) > 1e-6f) ? a / (a - b) : 0.5f;
        if (t < 0) { t = 0; }
        if (t > 1) { t = 1; }
        x = baseX + (i0 + (i1 - i0) * t) * cell;
        z = baseZ + (j0 + (j1 - j0) * t) * cell;
    };
    for (int j = 0; j < N; j++) {
        for (int i = 0; i < N; i++) {
            //Corners: 0 (i,j), 1 (i+1,j), 2 (i+1,j+1), 3 (i,j+1); edges: 0 = 0-1, 1 = 1-2, 2 = 2-3, 3 = 3-0
            const int code = (h[j * (N + 1) + i] > 0 ? 1 : 0) | (h[j * (N + 1) + i + 1] > 0 ? 2 : 0) |
                             (h[(j + 1) * (N + 1) + i + 1] > 0 ? 4 : 0) | (h[(j + 1) * (N + 1) + i] > 0 ? 8 : 0);
            if (code == 0 || code == 15) { continue; }
            irr::f32 ex[4], ez[4];
            edgePoint(i, j, i + 1, j, ex[0], ez[0]);
            edgePoint(i + 1, j, i + 1, j + 1, ex[1], ez[1]);
            edgePoint(i + 1, j + 1, i, j + 1, ex[2], ez[2]);
            edgePoint(i, j + 1, i, j, ex[3], ez[3]);
            //Pairs of edges crossed by the contour, for each corner pattern
            static const int segs[16][4] = {
                {-1,-1,-1,-1}, {3,0,-1,-1}, {0,1,-1,-1}, {3,1,-1,-1},
                {1,2,-1,-1},   {3,0,1,2},   {0,2,-1,-1}, {3,2,-1,-1},
                {2,3,-1,-1},   {0,2,-1,-1}, {0,1,2,3},   {1,2,-1,-1},
                {1,3,-1,-1},   {0,1,-1,-1}, {3,0,-1,-1}, {-1,-1,-1,-1} };
            for (int k = 0; k < 4 && segs[code][k] >= 0; k += 2) {
                const int a = segs[code][k], b = segs[code][k + 1];
                coastSegments.push_back(ex[a]);
                coastSegments.push_back(ez[a]);
                coastSegments.push_back(ex[b]);
                coastSegments.push_back(ez[b]);
            }
        }
    }
}
