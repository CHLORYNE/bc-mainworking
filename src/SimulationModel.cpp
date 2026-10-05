#include "SimulationModel.hpp"
#include <cstdlib> //KYARA SLAM: std::rand for the per-impact detune

#include "ScenarioDataStructure.hpp"
#include "GUIMain.hpp"
#include "LoadingScreen.hpp" //KYARA CHARGEMENT
#include "Terrain.hpp"
#include "Sky.hpp"
#include "Buoys.hpp"
#include "Sound.hpp"

#include "IniFile.hpp"
#include "Constants.hpp"
#include "Utilities.hpp"

#include <cmath>
#include <fstream>
#include <cctype>
#include <queue>
#include <algorithm>
#ifdef WITH_PROFILING
#include "iprof.hpp"
#else
#define IPROF(a) //intentionally empty placeholder
#endif
// SCENARIO INCENDIE: the fire timings (abandon, spread, sinking, permanent list, survivor drop
// interval) now come from the scenario's incident.ini, written by the fire scenario editor.
// Without that file the IncidentConfig defaults apply, which are the values used here before.
// SAR RESCUE RUN (Kyara) --------------------------------------------------------------
// The boats' routes and the helicopter pads come from incident.ini too. Without it, the built-in
// DAKHLA preset (rescue boat name, homeward path, helo pads) in IncidentConfig.cpp applies.
static const irr::f32 kRescuePickupSecs = 3.0f;   // alongside each raft
static const irr::f32 kRescueArriveR = 15.0f;  // m: capture gate
static const irr::f32 kHeloDescendRate = 4.0f;    // m/s settling onto the deck
// ==============================================================================
// Helicopter airframe
static const irr::f32 kHeloYawRate = 80.0f;         // deg/s
static const irr::f32 kHeloModelYawOffset = 0.0f;   // add if the model's nose isn't +Z

// SAR air winch (Kyara)
static const irr::f32 kHeloHoverAlt = 40.0f;        // m, cruise/search altitude above datum
static const irr::f32 kHeloWinchAlt = 18.0f;        // m, drops to this to winch
// HELICOPTER CORD 
static const irr::f32 kHeloDescendSecs = 9.0f;        // cable pays out
static const irr::f32 kHeloHookupSecs = 6.0f;    // dwell at full payout: strop goes on
//SPEED OF PULLING CORD
static const irr::f32 kHeloLiftSecs = 9.0f;        // survivor comes up
static const irr::f32 kHeloTransitSpd = 32.0f;       // m/s over the water to the next MOB

// SAR HELICOPTER FEATURE
static std::string sarLower(const std::string& s)
{
    std::string o;
    for (char c : s) {
        o += (char)tolower((unsigned char)c);
    }
    return o;
}
// =====================================================================
//  Accurate ship-vs-ship hull overlap (replaces the capsule edge-gap).
//
//  WHY: the old capsule model (centreline segment + constant beam radius)
//  bulges a half-beam half-circle AHEAD of the bow and astern of the stern.
//  A real (pointed) hull does not, so a vessel sitting off another ship's
//  bow/stern quarter in clear water was wrongly flagged as touching.
//
//  THIS: models each hull as a convex deck-plan polygon (pointed bow,
//  parallel midbody, lightly tapered stern) and tests genuine 2D overlap
//  via the Separating Axis Theorem. Pure horizontal geometry, so it stays
//  immune to wave heave exactly like the capsule version did.
//

namespace {

    // Build a ship's convex deck-plan footprint in WORLD space.
    // Local frame: +z = ahead, +x = starboard. World ahead = (sin hdg, cos hdg),
    // world starboard = (cos hdg, -sin hdg)  -- matches OwnShip's movement convention.
    // The 2D point is stored as (X = worldX, Y = worldZ).
    inline void buildHullFootprint(const irr::core::vector3df& pos, irr::f32 hdgDeg,
        irr::f32 length, irr::f32 breadth, irr::f32 margin,
        irr::core::vector2df out[7])
    {
        // Shrink collision hull slightly so the visible meshes touch
  // before SAT reports overlap.
        const float COLLISION_SCALE_LENGTH = 0.96f;
        const float COLLISION_SCALE_BREADTH = 0.94f;

        const float L = 0.5f * (length * COLLISION_SCALE_LENGTH) + margin;
        const float B = 0.5f * (breadth * COLLISION_SCALE_BREADTH) + margin;

        // Convex hull plan: pointed bow at (0,+L), full beam amidships,
        // slightly tapered transom. Symmetric port/starboard.
        static const irr::f32 lx[7] = { 0.0f,  1.0f,  1.0f,  0.7f, -0.7f, -1.0f, -1.0f };
        static const irr::f32 lz[7] = {
    0.93f,
    0.55f,
   -0.25f,
   -0.95f,
   -0.95f,
   -0.25f,
    0.55f
        };

        const irr::f32 s = sin(hdgDeg * irr::core::DEGTORAD);
        const irr::f32 c = cos(hdgDeg * irr::core::DEGTORAD);

        for (int k = 0; k < 7; ++k) {
            const irr::f32 x = lx[k] * B; // starboard offset
            const irr::f32 z = lz[k] * L; // ahead offset
            out[k].X = pos.X + x * c + z * s;
            out[k].Y = pos.Z - x * s + z * c; // world Z
        }
    }

    inline void projectPoly(const irr::core::vector2df p[7], const irr::core::vector2df& axis,
        irr::f32& mn, irr::f32& mx)
    {
        mn = mx = p[0].X * axis.X + p[0].Y * axis.Y;
        for (int k = 1; k < 7; ++k) {
            const irr::f32 d = p[k].X * axis.X + p[k].Y * axis.Y;
            if (d < mn) mn = d;
            if (d > mx) mx = d;
        }
    }

    // Separating Axis Theorem overlap test for two convex polygons.
    inline bool satOverlap(const irr::core::vector2df A[7], const irr::core::vector2df B[7])
    {
        for (int poly = 0; poly < 2; ++poly) {
            const irr::core::vector2df* P = (poly == 0) ? A : B;
            for (int k = 0; k < 7; ++k) {
                const irr::core::vector2df e(P[(k + 1) % 7].X - P[k].X,
                    P[(k + 1) % 7].Y - P[k].Y);
                irr::core::vector2df axis(-e.Y, e.X); // edge normal
                const irr::f32 len = sqrt(axis.X * axis.X + axis.Y * axis.Y);
                if (len < 1e-6f) continue;
                axis.X /= len; axis.Y /= len;

                irr::f32 aMin, aMax, bMin, bMax;
                projectPoly(A, axis, aMin, aMax);
                projectPoly(B, axis, bMin, bMax);
                if (aMax < bMin || bMax < aMin) return false; // a separating axis exists
            }
        }
        return true; // no separating axis -> hulls overlap
    }

} // anonymous namespace

// margin: metres added to every hull edge. 0 = exact hull touch.
// Use a small NEGATIVE value (e.g. -0.5) to require slight overlap before
// flagging (kills edge jitter); a small POSITIVE value to warn slightly early.
static bool shipFootprintsOverlap(const irr::core::vector3df& ownPos, irr::f32 ownHdg,
    irr::f32 ownLen, irr::f32 ownBreadth,
    const irr::core::vector3df& othPos, irr::f32 othHdg,
    irr::f32 othLen, irr::f32 othBreadth, irr::f32 margin)
{
    irr::core::vector2df A[7], B[7];
    buildHullFootprint(ownPos, ownHdg, ownLen, ownBreadth, margin, A);
    buildHullFootprint(othPos, othHdg, othLen, othBreadth, margin, B);
    return satOverlap(A, B);
}

//#include <ctime>

//using namespace irr;

// Kyara: shortest distance between two 2D line segments (in the X/Z plane).
// Used to model each hull as a capsule (keel segment + beam radius) so a long,
// thin ship is no longer approximated as a fat circle for contact detection.
static irr::f32 segSegDistance2D(
    irr::f32 ax, irr::f32 az, irr::f32 bx, irr::f32 bz,   // segment A endpoints
    irr::f32 cx, irr::f32 cz, irr::f32 ex, irr::f32 ez)   // segment B endpoints
{
    irr::core::vector2df A(ax, az), B(bx, bz), C(cx, cz), E(ex, ez);
    irr::core::vector2df u = B - A; // direction of A
    irr::core::vector2df v = E - C; // direction of B
    irr::core::vector2df w = A - C;
    irr::f32 a = u.dotProduct(u); // |u|^2
    irr::f32 b = u.dotProduct(v);
    irr::f32 c = v.dotProduct(v); // |v|^2
    irr::f32 d = u.dotProduct(w);
    irr::f32 e = v.dotProduct(w);
    irr::f32 Dn = a * c - b * b;       // always >= 0
    irr::f32 sc, tc;

    if (Dn < 1.0e-7f) {            // segments almost parallel: pin sc to 0
        sc = 0.0f;
        tc = (c > 1.0e-7f) ? (e / c) : 0.0f;
    }
    else {
        sc = (b * e - c * d) / Dn;
        tc = (a * e - b * d) / Dn;
    }
    // Clamp to the actual segments [0,1]
    if (sc < 0.0f) sc = 0.0f; else if (sc > 1.0f) sc = 1.0f;
    if (tc < 0.0f) tc = 0.0f; else if (tc > 1.0f) tc = 1.0f;

    irr::core::vector2df p = A + u * sc;
    irr::core::vector2df q = C + v * tc;
    return (p - q).getLength();
}

SimulationModel::SimulationModel(irr::IrrlichtDevice* dev,
    irr::scene::ISceneManager* scene,
    GUIMain* gui,
    Sound* sound,
    ScenarioData scenarioData,
    ModelParameters modelParameters,
    LoadingScreen* loadingScreen) :
    manOverboard(irr::core::vector3df(0, 0, 0), scene, dev, this) //Initialise MOB
{
    //get reference to scene manager
    device = dev;
    smgr = scene;
    driver = scene->getVideoDriver();
    guiMain = gui;
    this->sound = sound;
    isMouseDown = false;
    moveViewWithPrimary = true;

    //Store a serialised form of the scenario loaded, as we may want to send this over the network
    serialisedScenarioData = scenarioData.serialise(false);

    scenarioName = scenarioData.scenarioName;

    // Store model parameters
    this->modelParameters = modelParameters;

    //Set loop number to zero
    loopNumber = 0;

    worldName = scenarioData.worldName;
    irr::f32 startTime = scenarioData.startTime;
    irr::u32 startDay = scenarioData.startDay;
    irr::u32 startMonth = scenarioData.startMonth;
    irr::u32 startYear = scenarioData.startYear;

    //load the sun times
    irr::f32 sunRise = scenarioData.sunRise;
    irr::f32 sunSet = scenarioData.sunSet;
    if (sunRise == 0.0) { sunRise = 6; }
    if (sunSet == 0.0) { sunSet = 18; }

    //load the weather:
    //Fixme: add in wind direction etc
    weather = scenarioData.weather;
    //KYARA METEO: same cap as setWeather() - the scenario file must not exceed it either
    if (weather < 0.0f) { weather = 0.0f; }
    if (weather > SIM_MAX_WEATHER) { weather = SIM_MAX_WEATHER; }
    rainIntensity = scenarioData.rainIntensity;
    visibilityRange = scenarioData.visibilityRange;
    if (visibilityRange < 0) { visibilityRange = 5; } //Default value

    windDirection = scenarioData.windDirection;
    windSpeed = scenarioData.windSpeed;

    //std::cout << "Wind direction: " << windDirection << " Wind speed: " << windSpeed << std::endl;

    //Fixme: Think about time zone handling
    //Fixme: Note that if the time_t isn't long enough, 2038 problem exists
    scenarioOffsetTime = Utilities::dmyToTimestamp(startDay, startMonth, startYear);//Time in seconds to start of scenario day (unix timestamp for 0000h on day scenario starts)

    //set internal scenario time to start
    scenarioTime = startTime * SECONDS_IN_HOUR;

    //Set initial tide height to zero
    tideHeight = 0;

    if (worldName == "") {
        //Could not load world name from scenario, so end here
        std::cerr << "World model name not defined" << std::endl;
        exit(EXIT_FAILURE);
    }

    //construct path to world model
    std::string worldPath = "World/";
    worldPath.append(worldName);

    //Check if this world model exists in the user dir.
    std::string userFolder = Utilities::getUserDir();
    if (Utilities::pathExists(userFolder + worldPath)) {
        worldPath = userFolder + worldPath;
    }

    // Store world model readme.txt file contents here if available
    std::string worldReadmePath = worldPath + "/readme.txt";
    worldModelReadmeText = "";
    if (Utilities::pathExists(worldReadmePath)) {
        std::ifstream file(worldReadmePath.c_str());
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                worldModelReadmeText.append(line);
                worldModelReadmeText.append("\n");
            }
        }
    }


    if (loadingScreen) { loadingScreen->setStage(0.08f, "Terrain et carte"); } //KYARA CHARGEMENT
    //Add terrain: Needs to happen first, so the terrain parameters are available
    terrain.load(worldPath, smgr, device, modelParameters.limitTerrainResolution);

    if (loadingScreen) { loadingScreen->setStage(0.30f, "Ciel et navire"); } //KYARA CHARGEMENT
    //sky box/dome
    sky.load(smgr);

    //Load own ship model.
    // TODO: It would be better to pass in modelParameters directly
    ownShip.load(scenarioData.ownShipData,
        modelParameters.numberOfContactPoints,
        modelParameters.minContactPointSpacing,
        modelParameters.contactStiffnessFactor,
        modelParameters.contactDampingFactor,
        modelParameters.frictionCoefficient,
        modelParameters.tanhFrictionFactor,
        smgr,
        this,
        &terrain,
        device);
    if (modelParameters.mode == OperatingMode::Secondary) {
        ownShip.setSpeed(0); //Don't start moving if in secondary mode
    }

    if (loadingScreen) { loadingScreen->setStage(0.42f, "Surface de la mer"); } //KYARA CHARGEMENT
    //add water
     // KYARA: reflectionMode 0=full,1=half,2=off. "off" drops the entire reflection RTT pass
     // (a second full drawAll of the scene) AND selects the no-reflection shaders -> max FPS.
    bool waterReflection = (modelParameters.reflectionMode != 2);
    if (modelParameters.vrMode == true) {
        waterReflection = false;
    }
    irr::u32 reflectionEveryN = (modelParameters.reflectionMode == 1) ? 2 : 1; // half vs full
    water.load(smgr, ownShip.getSceneNode(), weather, modelParameters.disableShaders, waterReflection, modelParameters.waterSegments, reflectionEveryN);
    // KYARA HOULE: the swell is drawn by the water vertex shader. With shaders off it could not be
    // seen, so keep the physics flat too - the ship must never move on water that isn't there.
    swell.setEnabled(modelParameters.disableShaders == 0);
    splash.load(smgr, dev); // KYARA SLAM: spray particle system, world space
    //KYARA SLAM: water on the wheelhouse glass, drawn on a pane just inside the windscreen so the
    //structure around it masks the drops through the depth test.
    screenSpray.load(smgr, dev, ownShip.getSceneNode(), ownShip.getWindscreenPosition(),
        ownShip.getWindscreenWidth(), ownShip.getWindscreenHeight(), ownShip.getWindscreenTilt());
    /* To be replaced by getting information and passing into gui load method.
    //Tell gui to hide the second engine scroll bar if we have a single engine
    if (ownShip.isSingleEngine()) {
        gui->setSingleEngine();
    }

    //Tell gui to hide all ship controls if in secondary mode
    if (mode == OperatingMode::Secondary) {
        gui->hideEngineAndRudder();
//      TODO      gui->hideWheel();
//	DEE_NOV22 todo hide schottels engine indicators etc
        }

        //Tell the GUI what instruments to display - currently GPS and depth sounder
        gui->setInstruments(ownShip.hasDepthSounder(),ownShip.getMaxSounderDepth(),ownShip.hasGPS());
        */

    if (loadingScreen) { loadingScreen->setStage(0.52f, "Radar et caméras"); } //KYARA CHARGEMENT
    //Load the radar with config parameters
    radarCalculation.load(ownShip.getRadarConfigFile(), device);

    //set camera zoom to 1
    currentZoom = 1.0;
    zoomLevel = 7.0; //Default zoom of 7x

    //make a camera, setting parent and offset
    std::vector<irr::core::vector3df> views = ownShip.getCameraViews(); //Get the initial camera offset from the own ship model
    std::vector<bool> isHighView = ownShip.getCameraIsHighView(); //Are these special 'looking down' views
    irr::f32 angleCorrection = ownShip.getAngleCorrection();
    camera.load(smgr, device->getLogger(), ownShip.getSceneNode(), views, isHighView, irr::core::degToRad(modelParameters.viewAngle), modelParameters.lookAngle, angleCorrection);
    camera.setNearValue(modelParameters.cameraMinDistance);
    camera.setFarValue(modelParameters.cameraMaxDistance);

    //make ambient light
    light.load(smgr, sunRise, sunSet, camera.getSceneNode());


    if (loadingScreen) { loadingScreen->setStage(0.58f, "Navires de l'exercice"); } //KYARA CHARGEMENT
    //Load other ships
    otherShips.load(scenarioData.otherShipsData, scenarioTime, modelParameters.mode, smgr, this, device);
    // SCENARIO INCENDIE: fire timings, survivors, SAR boats (and where they are berthed), helicopters.
    loadIncident(scenarioData.otherShipsData);
    if (loadingScreen) { loadingScreen->setStage(0.68f, "Bouées et balisage"); } //KYARA CHARGEMENT
    //Load buoys
    buoys.load(worldPath, smgr, this, device);

    if (loadingScreen) { loadingScreen->setStage(0.72f, "Objets à terre"); } //KYARA CHARGEMENT
    //Load land objects
    landObjects.load(worldPath, smgr, this, &terrain, device);

    if (loadingScreen) { loadingScreen->setStage(0.78f, "Feux à terre"); } //KYARA CHARGEMENT
    //Load land lights
    landLights.load(worldPath, smgr, this, terrain);

    if (loadingScreen) { loadingScreen->setStage(0.80f, "Marée et pluie"); } //KYARA CHARGEMENT
    //Load tidal information
    tide.load(worldPath, scenarioData);

    //Load rain
    rain.load(smgr, camera.getSceneNode(), device);

    if (loadingScreen) { loadingScreen->setStage(0.82f, "Commandes de la passerelle"); } //KYARA CHARGEMENT
    //Set up 3d engine/wheel controls/visualisation
    if (isAzimuthDrive()) {
        portEngineVisual.load(smgr, ownShip.getSceneNode(), ownShip.getPortEngineControlPosition(), 1.0 / ownShip.getScaleFactor(), 1, 2); // 2=schottel base
        stbdEngineVisual.load(smgr, ownShip.getSceneNode(), ownShip.getStbdEngineControlPosition(), 1.0 / ownShip.getScaleFactor(), 1, 2);
        portAzimuthThrottleVisual.load(smgr, portEngineVisual.getSceneNode(), irr::core::vector3df(0, 0, 0), 1.0, 0, 3); // 3 = schottel lever
        stbdAzimuthThrottleVisual.load(smgr, stbdEngineVisual.getSceneNode(), irr::core::vector3df(0, 0, 0), 1.0, 0, 3);
    }
    else {
        portEngineVisual.load(smgr, ownShip.getSceneNode(), ownShip.getPortEngineControlPosition(), 1.0 / ownShip.getScaleFactor(), 0, 0); // 0 = regular throttle
        stbdEngineVisual.load(smgr, ownShip.getSceneNode(), ownShip.getStbdEngineControlPosition(), 1.0 / ownShip.getScaleFactor(), 0, 0);
        wheelVisual.load(smgr, ownShip.getSceneNode(), ownShip.getWheelControlPosition(), ownShip.getWheelControlScale() / ownShip.getScaleFactor(), 2, 1); // 1 = wheel
    }

    //make a radar screen, setting parent and offset from own ship
    radarScreen.load(smgr, ownShip.getSceneNode(), ownShip.getScreenDisplayPosition(), ownShip.getScreenDisplaySize(), ownShip.getScreenDisplayTilt());

    //make radar image - one for the background render, and one with any 2d drawing on top
    //Make as big as the maximum screen display size (next power of 2), and then only use as much as is needed to get 1:1 image to screen pixel mapping
    irr::u32 radarTextureSize = driver->getScreenSize().Height * 0.4; // Optimised for the small radar screen (Where 0.6*screen height is used for the 3d view). We should have a higher resolution for full radar view
    irr::u32 largeRadarTextureSize = driver->getScreenSize().Height; // Optimised for the large radar screen
    //Find next power of 2 size
    radarTextureSize = std::pow(2, std::ceil(std::log2(radarTextureSize)));
    largeRadarTextureSize = std::pow(2, std::ceil(std::log2(largeRadarTextureSize)));

    //In simulationModel, keep track of the used size, and pass this to gui etc.
    radarImage = driver->createImage(irr::video::ECF_A8R8G8B8, irr::core::dimension2d<irr::u32>(radarTextureSize, radarTextureSize)); //Create image for radar calculation to work on
    radarImageOverlaid = driver->createImage(irr::video::ECF_A8R8G8B8, irr::core::dimension2d<irr::u32>(radarTextureSize, radarTextureSize)); //Create image for radar calculation to work on
    radarImageLarge = driver->createImage(irr::video::ECF_A8R8G8B8, irr::core::dimension2d<irr::u32>(largeRadarTextureSize, largeRadarTextureSize)); //Create image for radar calculation to work on
    radarImageOverlaidLarge = driver->createImage(irr::video::ECF_A8R8G8B8, irr::core::dimension2d<irr::u32>(largeRadarTextureSize, largeRadarTextureSize)); //Create image for radar calculation to work on
    //Images will be filled with background colour in RadarCalculation

    //make radar camera
    std::vector<irr::core::vector3df> radarViews; //Get the initial camera offset from the radar screen
    std::vector<bool> radarViewsLookDown; //Not needed for the radar camera, but needed for compatability
    irr::f32 screenTilt = ownShip.getScreenDisplayTilt();
    radarViews.push_back(ownShip.getScreenDisplayPosition() + irr::core::vector3df(0, 0.5 * sin(irr::core::DEGTORAD * screenTilt) * ownShip.getScreenDisplaySize(), -0.5 * cos(irr::core::DEGTORAD * screenTilt) * ownShip.getScreenDisplaySize()));
    radarViewsLookDown.push_back(false);
    radarCamera.load(smgr, device->getLogger(), ownShip.getSceneNode(), radarViews, radarViewsLookDown, irr::core::PI / 2.0, 0, 0);
    radarCamera.setLookUp(-1.0 * screenTilt); //FIXME: Why doesn't simply -1.0*screenTilt work?
    radarCamera.updateViewport(1.0);
    radarCamera.setNearValue(0.8 * 0.5 * ownShip.getScreenDisplaySize());
    radarCamera.setFarValue(1.2 * 0.5 * ownShip.getScreenDisplaySize());

    //Hide the man overboard model
    manOverboard.setVisible(false);
    // FIRE FEATURE: monitor lives on own ship (the trainee's fire/SAR vessel).
// nozzleOffset is LOCAL to the own-ship model - tune X/Y/Z onto your monitor/bow.
    fireBoatIsOwnShip = ownShip.isFireFightingVessel();   // Kyara FIRE: only certified own ships carry a monitor
    burningShipIndex = -1;
    fireMountNode = 0;
    if (fireBoatIsOwnShip && ownShip.getSceneNode()) {
        // Place the monitor from the OWN SHIP's own bounding box (model/local space), so it
        // sits on deck near the bow whatever the hull size - fixes it floating "outside"
        // a small hull like the helo. The offset is a child of the ship node, so the node's
        // own scaleFactor is applied for us (no 1/scale needed here).
        ownShip.getSceneNode()->updateAbsolutePosition();
        irr::core::vector3df monitorOffset;
        if (ownShip.getFireMonitorOffsetSet()) {
            // Exact mount point from boat.ini (MonitorX/Y/Z), in raw model units. Best for a specific
            // hull like the Haicho tug, where the monitor sits on the fifi platform, not a generic spot.
            monitorOffset = ownShip.getFireMonitorOffset();
        }
        else {
            // Fallback: estimate from the hull's bounding box (near deck level, forward toward the bow).
            irr::core::aabbox3df ob = ownShip.getSceneNode()->getBoundingBox();
            irr::f32 deckY = ob.MinEdge.Y + 0.85f * (ob.MaxEdge.Y - ob.MinEdge.Y); // near top of hull
            irr::f32 fwdZ = ob.MinEdge.Z + 0.75f * (ob.MaxEdge.Z - ob.MinEdge.Z); // 75% toward the bow
            monitorOffset = irr::core::vector3df(0.0f, deckY, fwdZ);
        }
        fireMonitor.mount(smgr, driver, ownShip.getSceneNode(), monitorOffset);
    }
    monitorFiringDesired = false; // FIRE FEATURE
    fireWasBurning = false; // FIRE FEATURE
    mobDropped = false;
    fireElapsed = 0.0f;
    fireElapsed = 0.0f; casualtySinking = false; // Kyara FIRE (escalation)
    // SAR HELICOPTER FEATURE
    sarActive = false;
    // SAR RESCUE RUN (the boats themselves are set up by loadIncident)
    heloRescuedCount = 0;
    sarDeparting = false;
    sarDepartTimer = 0.0f;
    heloCalled = false;
    heloCallElapsed = 0.0f;
    sarOnScene = false;
    abandonMobTotal = 0;

    rescueCasualtyIndex = -1;
    sarDatum = irr::core::vector3df(0, 0, 0);
    sarDatumSet = false;
    sarSweepPhase = 0.0f;
    sarInit = false;
    sarBaseY = 0.0f;   // Kyara SAR: hover altitude, set for real when helos activate
    abandonSpawned = false;
    abandonSpawning = false;
    abandonComplete = false;
    abandonSpawnStep = 0;
    abandonSpawnTimer = 0.0f;
    abandonCentre = irr::core::vector3df(0, 0, 0);
    heloRunActive = false;
    //initialise offset
    offsetPosition = irr::core::vector3d<int64_t>(0, 0, 0);
    motionScale = 1.0f; // KYARA HOULE
    slamCooldown = 0.0f; // KYARA SLAM

    //store time
    previousTime = device->getTimer()->getTime();

    guiData = new GUIData;

    // Initialise as paused to start with
    guiData->paused = true;
    //kyara collision 
// Initialise collision sequence state
    collisionSequenceTimeRemaining = 0.0f;
    collisionWasColliding = false;
    //KYARA CONTACT
    landContactWasTouching = false;
    otherCollisionWasTouching = false;
    contactSoundCooldown = 0.0f;
    //kyara collision
    collisionSoundTimeRemaining = 0.0f;
    // KYARA PROXY AND COLLISION 
    inCollision = false;
    collisionClearTimer = 0.0f;
    prevNearestShipDistance = 1.0e30f;
    proxyHoldTimer = 0.0f;
    proxyAlarmMuted = false; // Initialize as unmuted
    lightningFlash = 0.0f;
    thunderTimer = 4.0f;   // first possible strike a few seconds after a storm begins
    thunderEnabled = true;   // KYARA
    lightningEnabled = true;
    dayNightOffset = 0.0f;
    nightMode = false;
    filteredRelBearing = 0.0f;
    aimedAtOtherLatched = false;
    collisionReleaseHold = 0.0f; // NEW - Initialize release hold timer
    collisionStartupGrace = 2.0f; // suppress proxy/collision for the first 2 s of running// suppress proxy/collision for the first 2 s of running, so spawn settling can't false-trigger
} //end of SimulationModel constructor

SimulationModel::~SimulationModel()
{
    radarImage->drop(); //We created this with 'create', so drop it when we're finished
    radarImageOverlaid->drop(); //We created this with 'create', so drop it when we're finished
    radarImageLarge->drop(); //We created this with 'create', so drop it when we're finished
    radarImageOverlaidLarge->drop(); //We created this with 'create', so drop it when we're finished

    delete guiData;
}

irr::f32 SimulationModel::longToX(irr::f32 longitude) const
{
    return terrain.longToX(longitude); //Cascade to terrain
}

irr::f32 SimulationModel::latToZ(irr::f32 latitude) const
{
    return terrain.latToZ(latitude); //Cascade to terrain
}
irr::f32 SimulationModel::longToSceneX(irr::f32 longitude) const
{
    return terrain.longToX(longitude) - (irr::f32)offsetPosition.X;
}

irr::f32 SimulationModel::latToSceneZ(irr::f32 latitude) const
{
    return terrain.latToZ(latitude) - (irr::f32)offsetPosition.Z;
}

void SimulationModel::setSpeed(irr::f32 spd)
{
    ownShip.setSpeed(spd);
}

void SimulationModel::toggleRadarOffCentre() { radarCalculation.toggleOffCentre(); }
irr::f32 SimulationModel::getLat()  const {
    return terrain.zToLat(ownShip.getPosition().Z + offsetPosition.Z);
}

irr::f32 SimulationModel::getLong() const {
    return terrain.xToLong(ownShip.getPosition().X + offsetPosition.X);
}

irr::f32 SimulationModel::getPosX() const {
    return ownShip.getPosition().X + offsetPosition.X;
}

irr::f32 SimulationModel::getPosZ() const {
    return ownShip.getPosition().Z + offsetPosition.Z;
}

irr::f32 SimulationModel::getCOG() const {
    return ownShip.getCOG();
}

irr::f32 SimulationModel::getSOG() const {
    return ownShip.getSOG();
}

irr::f32 SimulationModel::getDepth() const {
    return ownShip.getDepth();
}

irr::f32 SimulationModel::getWaveHeight(irr::f32 posX, irr::f32 posZ) const {
    // KYARA HOULE: short FFT chop + long analytic swell. Everything that floats (own ship, other
    // ships, buoys, SAR objects) goes through here, so it all rides the same sea the shader draws.
    return water.getWaveHeight(posX, posZ) + swell.getHeight(posX, posZ);
}

// KYARA HOULE ---------------------------------------------------------------------------------
const Swell& SimulationModel::getSwell() const {
    return swell;
}

void SimulationModel::setMotionScale(irr::f32 scale) {
    if (scale < 0.0f) { scale = 0.0f; }
    if (scale > 1.5f) { scale = 1.5f; }
    motionScale = scale;
}

irr::f32 SimulationModel::getMotionScale() const {
    return motionScale;
}

void SimulationModel::getSwellNetworkState(irr::f32 out[Swell::NET_FIELDS]) const {
    swell.getNetworkState((irr::f64)offsetPosition.X, (irr::f64)offsetPosition.Z, out);
}

void SimulationModel::applySwellNetworkState(const irr::f32 in[Swell::NET_FIELDS]) {
    swell.applyNetworkState(in, (irr::f64)offsetPosition.X, (irr::f64)offsetPosition.Z);
}
// KYARA HOULE ^^^^

irr::core::vector2df SimulationModel::getLocalNormals(irr::f32 relPosX, irr::f32 relPosZ) const {
    return water.getLocalNormals(relPosX, relPosZ);
}

irr::core::vector2df SimulationModel::getTidalStream(irr::f32 longitude, irr::f32 latitude, uint64_t requestTime) const {

    if (streamOverride) {
        irr::core::vector2df overrideStream;
        overrideStream.X = sin(streamOverrideDirection * irr::core::DEGTORAD) * streamOverrideSpeed * KTS_TO_MPS;
        overrideStream.Y = cos(streamOverrideDirection * irr::core::DEGTORAD) * streamOverrideSpeed * KTS_TO_MPS;
        return overrideStream;
    }
    else {
        return tide.getTidalStream(longitude, latitude, requestTime);
    }
}

// void SimulationModel::getTime(irr::u8& hour, irr::u8& min, irr::u8& sec) const{
//    //FIXME: Complete
// }

 //void SimulationModel::getDate(irr::u8& day, irr::u8& month, irr::u16& year) const{
 //    //FIXME: Complete
 //}

uint64_t SimulationModel::getTimestamp() const {
    return absoluteTime;
}

uint64_t SimulationModel::getTimeOffset() const { //The timestamp at the start of the first day of the scenario
    return scenarioOffsetTime;
}

void SimulationModel::setTimeDelta(irr::f32 scenarioTime) {
    this->scenarioTime = scenarioTime;
}
//KYARA LIGHTING TIME: force the LIGHTING clock to a given hour of day. scenarioTime is never
//touched, so other ships' legs, tide, and light flash sequences are all unaffected.
//Light::update() does fmod(t, SECONDS_IN_DAY), so we simply shift the time it sees.
void SimulationModel::setLightingTimeOfDay(irr::f32 hourOfDay)
{
    if (hourOfDay < 0.0f) { hourOfDay = 0.0f; }
    if (hourOfDay >= 24.0f) { hourOfDay = 23.9833f; }

    irr::f32 timeOfDay = std::fmod(scenarioTime, 86400.0f);
    if (timeOfDay < 0.0f) { timeOfDay += 86400.0f; }

    dayNightOffset = (hourOfDay * 3600.0f) - timeOfDay;
}

irr::f32 SimulationModel::getLightingTimeOfDay() const
{
    irr::f32 lightingTime = std::fmod(scenarioTime + dayNightOffset, 86400.0f);
    if (lightingTime < 0.0f) { lightingTime += 86400.0f; }
    return lightingTime / 3600.0f;
}

irr::f32 SimulationModel::getTimeDelta() const { //The change in time (s) since the start of the start day of the scenario
    return scenarioTime;
}

irr::u32 SimulationModel::getNumberOfOtherShips() const {
    return otherShips.getNumber();
}

irr::u32 SimulationModel::getNumberOfBuoys() const {
    return buoys.getNumber();
}

std::string SimulationModel::getOtherShipName(int number) const {
    return otherShips.getName(number);
}

irr::f32 SimulationModel::getOtherShipPosX(int number) const {
    return otherShips.getPosition(number).X + offsetPosition.X;
}

irr::f32 SimulationModel::getOtherShipPosZ(int number) const {
    return otherShips.getPosition(number).Z + offsetPosition.Z;
}

irr::f32 SimulationModel::getOtherShipLong(int number) const {
    return terrain.xToLong(getOtherShipPosX(number));
}

irr::f32 SimulationModel::getOtherShipLat(int number) const {
    return terrain.zToLat(getOtherShipPosZ(number));
}

irr::f32 SimulationModel::getOtherShipHeading(int number) const {
    return otherShips.getHeading(number);
}
// cpp:
irr::f32 SimulationModel::getOtherShipLength(int number) const { return otherShips.getLength(number); }
irr::f32 SimulationModel::getOtherShipBreadth(int number) const { return otherShips.getBreadth(number); }
irr::f32 SimulationModel::getOtherShipSpeed(int number) const {
    return otherShips.getSpeed(number);
}

irr::u32 SimulationModel::getOtherShipMMSI(int number) const {
    return otherShips.getMMSI(number);
}

void SimulationModel::setOtherShipMMSI(int number, irr::u32 mmsi) {
    otherShips.setMMSI(number, mmsi);
}

void SimulationModel::setOtherShipHeading(int number, irr::f32 hdg) {
    otherShips.setHeading(number, hdg);
}

void SimulationModel::setOtherShipSpeed(int number, irr::f32 speed) {
    otherShips.setSpeed(number, speed);
}

void SimulationModel::setOtherShipPos(int number, irr::f32 positionX, irr::f32 positionZ) {
    otherShips.setPos(number, positionX - offsetPosition.X, positionZ - offsetPosition.Z);
}

void SimulationModel::setOtherShipRateOfTurn(int number, irr::f32 rateOfTurn) {
    otherShips.setRateOfTurn(number, rateOfTurn);
}

std::vector<Leg> SimulationModel::getOtherShipLegs(int number) const {
    return otherShips.getLegs(number);
}

irr::f32 SimulationModel::getBuoyPosX(int number) const {
    return buoys.getPosition(number).X + offsetPosition.X;
}

irr::f32 SimulationModel::getBuoyPosZ(int number) const {
    return buoys.getPosition(number).Z + offsetPosition.Z;
}

void SimulationModel::changeOtherShipLeg(int shipNumber, int legNumber, irr::f32 bearing, irr::f32 speed, irr::f32 distance) {
    otherShips.changeLeg(shipNumber, legNumber, bearing, speed, distance, scenarioTime);
}

void SimulationModel::addOtherShipLeg(int shipNumber, int afterLegNumber, irr::f32 bearing, irr::f32 speed, irr::f32 distance) {
    otherShips.addLeg(shipNumber, afterLegNumber, bearing, speed, distance, scenarioTime);
}

void SimulationModel::deleteOtherShipLeg(int shipNumber, int legNumber) {
    otherShips.deleteLeg(shipNumber, legNumber, scenarioTime);
}

void SimulationModel::resetOtherShipLegs(int shipNumber, irr::f32 course, irr::f32 speedKts, irr::f32 distanceNm) {
    otherShips.resetLegs(shipNumber, course, speedKts, distanceNm, scenarioTime);
}

std::string SimulationModel::getOwnShipEngineSound() const {

    //Check existence of sound file in base path, and if not fall back to default.
    std::string soundPath = ownShip.getBasePath();

    { //Create local scope for file
        soundPath.append("/Engine.wav");
        std::ifstream file(soundPath.c_str());
        if (file.good()) {
            return soundPath;
        }
    }

    //Check for lower case version
    {
        soundPath = ownShip.getBasePath();
        soundPath.append("/engine.wav");
        std::ifstream file(soundPath.c_str());
        if (file.good()) {
            return soundPath;
        }
    }

    //Fall back to default, again checking both upper and lower case

    {
        soundPath = "Sounds/Engine.wav";
        std::ifstream file(soundPath.c_str());
        if (file.good()) {
            return soundPath;
        }
    }

    {
        soundPath = "Sounds/engine.wav";
        std::ifstream file(soundPath.c_str());
        if (file.good()) {
            return soundPath;
        }
    }

    //In case nothing found
    return "";

}

std::string SimulationModel::getOwnShipWaveSound() const {

    //Check existence of sound file in base path, and if not fall back to default.
    std::string soundPath = ownShip.getBasePath();

    { //Create local scope for file
        soundPath.append("/Bwave.wav");
        std::ifstream file(soundPath.c_str());
        if (file.good()) {
            return soundPath;
        }
    }

    //Check for lower case version
    {
        soundPath = ownShip.getBasePath();
        soundPath.append("/bwave.wav");
        std::ifstream file(soundPath.c_str());
        if (file.good()) {
            return soundPath;
        }
    }

    //Fall back to default, again checking both upper and lower case

    {
        soundPath = "Sounds/Bwave.wav";
        std::ifstream file(soundPath.c_str());
        if (file.good()) {
            return soundPath;
        }
    }

    {
        soundPath = "Sounds/bwave.wav";
        std::ifstream file(soundPath.c_str());
        if (file.good()) {
            return soundPath;
        }
    }

    //In case nothing found
    return "";

}

std::string SimulationModel::getOwnShipHornSound() const {

    //Check existence of sound file in base path, and if not fall back to default.
    std::string soundPath = ownShip.getBasePath();

    { //Create local scope for file
        soundPath.append("/Horn.wav");
        std::ifstream file(soundPath.c_str());
        if (file.good()) {
            return soundPath;
        }
    }

    //Check for lower case version
    {
        soundPath = ownShip.getBasePath();
        soundPath.append("/horn.wav");
        std::ifstream file(soundPath.c_str());
        if (file.good()) {
            return soundPath;
        }
    }

    //Fall back to default, again checking both upper and lower case

    {
        soundPath = "Sounds/Horn.wav";
        std::ifstream file(soundPath.c_str());
        if (file.good()) {
            return soundPath;
        }
    }

    {
        soundPath = "Sounds/horn.wav";
        std::ifstream file(soundPath.c_str());
        if (file.good()) {
            return soundPath;
        }
    }

    //In case nothing found
    return "";

}
//KYARA COLLISION AND PROXY SEQUENCE
std::string SimulationModel::getOwnShipProxyAlarmSound() const {
    std::string soundPath;
    soundPath = ownShip.getBasePath(); soundPath.append("/proxy_alarm.wav");
    { std::ifstream file(soundPath.c_str()); if (file.good()) return soundPath; }
    soundPath = ownShip.getBasePath(); soundPath.append("/Proxy_alarm.wav");
    { std::ifstream file(soundPath.c_str()); if (file.good()) return soundPath; }
    soundPath = "Sounds/proxy_alarm.wav";
    { std::ifstream file(soundPath.c_str()); if (file.good()) return soundPath; }
    soundPath = "Sounds/Proxy_alarm.wav";
    { std::ifstream file(soundPath.c_str()); if (file.good()) return soundPath; }
    return getOwnShipAlarmSound(); // last resort: generic alarm
}
//------------------END
//kyara: radar guard-zone alarm sound (independent of proxy alarm)
std::string SimulationModel::getOwnShipRadarAlarmSound() const {
    std::string soundPath;
    soundPath = ownShip.getBasePath(); soundPath.append("/radar_alarm.wav");
    { std::ifstream file(soundPath.c_str()); if (file.good()) return soundPath; }
    soundPath = "Sounds/radar_alarm.wav";
    { std::ifstream file(soundPath.c_str()); if (file.good()) return soundPath; }
    soundPath = "Sounds/Radar_alarm.wav";
    { std::ifstream file(soundPath.c_str()); if (file.good()) return soundPath; }
    return ""; //no fallback: if missing, the channel just won't load (silent), which is fine
}

std::string SimulationModel::getOwnShipCollisionSound() const {
    std::string soundPath;
    soundPath = ownShip.getBasePath(); soundPath.append("/collision.wav");
    { std::ifstream file(soundPath.c_str()); if (file.good()) return soundPath; }
    soundPath = ownShip.getBasePath(); soundPath.append("/Collision.wav");
    { std::ifstream file(soundPath.c_str()); if (file.good()) return soundPath; }
    soundPath = "Sounds/collision.wav";
    { std::ifstream file(soundPath.c_str()); if (file.good()) return soundPath; }
    soundPath = "Sounds/Collision.wav";
    { std::ifstream file(soundPath.c_str()); if (file.good()) return soundPath; }
    return getOwnShipAlarmSound(); // last resort: generic alarm
}

// KYARA INSIDE / OUTSIDE / COLLISION / PROXY — PER-BOAT SOUND CONVENTION
//
// Every sound is resolved in three steps, tried in order:
//   1. Boat-specific folder   e.g. Models/Ownship/G10/inside_boat.wav
//   2. Boat-specific folder   same path, capital first letter  (Inside_boat.wav)
//   3. Global fallback folder Sounds/inside_boat.wav  (or capital variant)
//
// To give a boat its own sounds, drop the relevant file into its model folder:
//
//   Models/Ownship/<BoatName>/inside_boat.wav   ← heard on interior camera views (0 and 1)
//   Models/Ownship/<BoatName>/outside_boat.wav  ← heard on all exterior camera views
//   Models/Ownship/<BoatName>/collision.wav     ← one-shot bang on impact
//   Models/Ownship/<BoatName>/proxy_alarm.wav   ← looping alarm when closing on another ship
//   Models/Ownship/<BoatName>/Engine.wav        ← engine loop (volume tracks throttle)
//   Models/Ownship/<BoatName>/Bwave.wav         ← wave/water ambience (top-down view)
//   Models/Ownship/<BoatName>/Horn.wav          ← fog horn
//
// If a boat-specific file is absent the global Sounds/ file is used instead.
// If the global file is also absent the channel is simply silent (no crash).

std::string SimulationModel::getOwnShipInsideSound() const {
    std::string soundPath;
    // 1 & 2: boat-specific folder, lower and upper case
    soundPath = ownShip.getBasePath(); soundPath.append("/inside_boat.wav");
    { std::ifstream file(soundPath.c_str()); if (file.good()) return soundPath; }
    soundPath = ownShip.getBasePath(); soundPath.append("/Inside_boat.wav");
    { std::ifstream file(soundPath.c_str()); if (file.good()) return soundPath; }
    // 3: global fallback, lower and upper case
    soundPath = "Sounds/inside_boat.wav";
    { std::ifstream file(soundPath.c_str()); if (file.good()) return soundPath; }
    soundPath = "Sounds/Inside_boat.wav";
    { std::ifstream file(soundPath.c_str()); if (file.good()) return soundPath; }
    return ""; // absent → channel stays silent
}

std::string SimulationModel::getOwnShipOutsideSound() const {
    std::string soundPath;
    // 1 & 2: boat-specific folder, lower and upper case
    soundPath = ownShip.getBasePath(); soundPath.append("/outside_boat.wav");
    { std::ifstream file(soundPath.c_str()); if (file.good()) return soundPath; }
    soundPath = ownShip.getBasePath(); soundPath.append("/Outside_boat.wav");
    { std::ifstream file(soundPath.c_str()); if (file.good()) return soundPath; }
    // 3: global fallback, lower and upper case
    soundPath = "Sounds/outside_boat.wav";
    { std::ifstream file(soundPath.c_str()); if (file.good()) return soundPath; }
    soundPath = "Sounds/Outside_boat.wav";
    { std::ifstream file(soundPath.c_str()); if (file.good()) return soundPath; }
    return ""; // absent → channel stays silent
}
//----------------END OF INSIDE OUTSIDE SOUND CHANGE

//KYARA SEAGULL: single global sound file (Sounds/seagull.wav), no per-boat override -
// triggered once per 'G' key press, see MyEventReceiver.cpp.
std::string SimulationModel::getOwnShipSeagullSound() const {
    std::string soundPath;
    soundPath = "Sounds/seagull.wav";
    { std::ifstream file(soundPath.c_str()); if (file.good()) return soundPath; }
    soundPath = "Sounds/Seagull.wav";
    { std::ifstream file(soundPath.c_str()); if (file.good()) return soundPath; }
    return ""; // absent → key press plays nothing, no crash
}
// FIRE FEATURE
std::string SimulationModel::getOwnShipFireBurningSound() const {
    std::string soundPath = "Sounds/fire_burning.wav";
    { std::ifstream file(soundPath.c_str()); if (file.good()) return soundPath; }
    return "";
}
std::string SimulationModel::getOwnShipWaterSound() const {
    std::string soundPath = "Sounds/water.wav";
    { std::ifstream file(soundPath.c_str()); if (file.good()) return soundPath; }
    return "";
}
std::string SimulationModel::getOwnShipFireAlarmSound() const {
    std::string soundPath = "Sounds/fire_alarm.wav";
    { std::ifstream file(soundPath.c_str()); if (file.good()) return soundPath; }
    return "";
}

std::string SimulationModel::getOwnShipAbandonAlarmSound() const { std::string p = "Sounds/abandon_alarm.wav"; { std::ifstream f(p.c_str()); if (f.good()) return p; } return ""; }
std::string SimulationModel::getOwnShipExplosionSound()   const { std::string p = "Sounds/explosion.wav"; { std::ifstream f(p.c_str()); if (f.good()) return p; } return ""; }
std::string SimulationModel::getOwnShipGroanSound()       const { std::string p = "Sounds/groan.wav"; { std::ifstream f(p.c_str()); if (f.good()) return p; } return ""; }
std::string SimulationModel::getOwnShipSteamSound()       const { std::string p = "Sounds/steam.wav"; { std::ifstream f(p.c_str()); if (f.good()) return p; } return ""; }
std::string SimulationModel::getOwnShipVhfSound()         const { std::string p = "Sounds/vhf.wav"; { std::ifstream f(p.c_str()); if (f.good()) return p; } return ""; }
//KYARA CONTACT: single global sound file, same optional pattern as the seagull.
std::string SimulationModel::getOwnShipContactSound() const {
    std::string soundPath;
    soundPath = "Sounds/contact.wav";
    { std::ifstream file(soundPath.c_str()); if (file.good()) return soundPath; }
    return ""; // absent → no contact sound, no crash
}
//KYARA FEUX ------------------------------------------------------------------------------------
void SimulationModel::setOwnShipLightSituation(int situation) {
    ownShip.getLights().setSituation((ShipLights::Situation)situation);
}

int SimulationModel::getOwnShipLightSituation() const {
    return (int)const_cast<OwnShip&>(ownShip).getLights().getSituation();
}

void SimulationModel::setOwnShipInstrumentLights(int level) {
    ownShip.setInstrumentLights(level);
}

int SimulationModel::getOwnShipInstrumentLights() const {
    return const_cast<OwnShip&>(ownShip).getInstrumentLights();
}

int SimulationModel::getOwnShipInstrumentMaterialCount() const {
    return ownShip.getInstrumentMaterialCount();
}

//KYARA FEUX TAB
ShipLights* SimulationModel::getShipLights(int vessel) {
    if (vessel < 0) { return &ownShip.getLights(); }
    return otherShips.getLights(vessel);
}

//KYARA FEUX EDIT ---------------------------------------------------------------------------------
bool SimulationModel::beginLightEdit(int vessel)
{
    if (lightEditVessel != -2) { endLightEdit(); }
    setFreeView(false); //the lamp editor has its own orbit
    ShipLights* lights = getShipLights(vessel);
    if (!lights) { return false; }
    lights->beginEdit();
    lightEditVessel = vessel;
    //Start far enough out to see the whole vessel round the first lamp
    const irr::f32 len = (vessel < 0) ? ownShip.getLength() : otherShips.getLength(vessel);
    irr::f32 radius = 0.9f * len;
    if (radius < 12.0f) { radius = 12.0f; }
    camera.setOrbit(true, radius);
    device->getLogger()->log(("Light editor: " + lights->getIniFilename()).c_str());
    return true;
}

void SimulationModel::endLightEdit()
{
    if (lightEditVessel == -2) { return; }
    ShipLights* lights = getShipLights(lightEditVessel);
    if (lights) { lights->endEdit(); }
    lightEditVessel = -2;
    camera.setOrbit(false);
}

bool SimulationModel::isLightEditing() const
{
    return lightEditVessel != -2;
}

int SimulationModel::getLightEditVessel() const
{
    return lightEditVessel;
}

void SimulationModel::lightEditOrbit(irr::f32 dYawDeg, irr::f32 dPitchDeg, irr::f32 zoomFactor)
{
    camera.orbitBy(dYawDeg, dPitchDeg, zoomFactor);
}

void SimulationModel::setOwnShipDeckLights(bool on) {
    ownShip.getLights().setDeckLights(on);
}

bool SimulationModel::getOwnShipDeckLights() const {
    return const_cast<OwnShip&>(ownShip).getLights().getDeckLights();
}

void SimulationModel::setOtherShipLightSituation(int shipNumber, int situation) {
    otherShips.setLightSituation(shipNumber, situation);
}

int SimulationModel::getOtherShipLightSituation(int shipNumber) const {
    return const_cast<OtherShips&>(otherShips).getLightSituation(shipNumber);
}

//KYARA SLAM: optional. Missing file just means no slam sound; nothing else is affected.
//KYARA SLAM: which views count as being inside the wheelhouse (the first two in boat.ini).
//Change this if a vessel's view list is ordered differently.
bool SimulationModel::viewIsInside() const {
    const irr::u32 v = camera.getView();
    return (v == 0 || v == 1);
}

void SimulationModel::setScreenSprayMode(int mode) {
    screenSpray.setMode(mode);
}

std::string SimulationModel::getOwnShipSlamSound() const {
    std::string p = "Sounds/slam.wav";
    { std::ifstream f(p.c_str()); if (f.good()) return p; }
    return "";
}

void SimulationModel::triggerSeagull() {
    sound->triggerSeagull();
}
//MUTE PROXY ALARM
void SimulationModel::toggleProxyAlarmMute() {
    proxyAlarmMuted = !proxyAlarmMuted;
}
// KYARA "MAUVAIS TEMPS": one button -> heavy sea, heavy rain, very low visibility. These three
// values already sync to the secondary and the LAN radar PC in send-record 7, so the whole
// storm state propagates for free. Storm ambience / thunder / overcast sky are derived from
// these values in update(), so they switch on automatically here and off when the sliders drop.
void SimulationModel::setBadWeatherPreset() {
    setWeather(4.0f);      // Beaufort ~8
    setRain(4.7f);         // 0..10
    setVisibility(10.0f);   // nm
}
void SimulationModel::setThunderEnabled(bool e) { thunderEnabled = e; }
bool SimulationModel::getThunderEnabled() const { return thunderEnabled; }
irr::f32 SimulationModel::getLightningFlash() const { return lightningFlash; }
void SimulationModel::setLightningEnabled(bool e) { lightningEnabled = e; }
bool SimulationModel::getLightningEnabled() const { return lightningEnabled; }
void SimulationModel::setLightningFlash(irr::f32 v) { lightningFlash = (v < 0 ? 0 : (v > 1 ? 1 : v)); }

bool SimulationModel::getProxyAlarmMuted() const {
    return proxyAlarmMuted;
}

std::string SimulationModel::getOwnShipRainSound() const {
    std::string p = "Sounds/rain.wav";
    if (Utilities::pathExists(p)) return p;
    p = "Sounds/Rain.wav";
    return p;
}
std::string SimulationModel::getOwnShipStormSound() const {
    std::string p = "Sounds/storm.wav";
    if (Utilities::pathExists(p)) return p;
    p = "Sounds/Storm.wav";
    return p;
}
std::string SimulationModel::getOwnShipThunderSound() const {
    std::string p = "Sounds/thunderseq.wav";
    if (Utilities::pathExists(p)) return p;
    p = "Sounds/Thunderseq.wav";
    return p;
}
std::string SimulationModel::getOwnShipThunderboltSound() const {
    std::string p = "Sounds/thunderbolt.wav";
    if (Utilities::pathExists(p)) return p;
    p = "Sounds/Thunderbolt.wav";
    if (Utilities::pathExists(p)) return p;
    return ""; // absent → hero bolt is silent, no crash
}

std::string SimulationModel::getOwnShipAlarmSound() const {

    //Check existence of sound file in base path, and if not fall back to default.
    std::string soundPath = ownShip.getBasePath();

    { //Create local scope for file
        soundPath.append("/Alarm.wav");
        std::ifstream file(soundPath.c_str());
        if (file.good()) {
            return soundPath;
        }
    }

    //Check for lower case version
    {
        soundPath = ownShip.getBasePath();
        soundPath.append("/alarm.wav");
        std::ifstream file(soundPath.c_str());
        if (file.good()) {
            return soundPath;
        }
    }

    //Fall back to default, again checking both upper and lower case

    {
        soundPath = "Sounds/Alarm.wav";
        std::ifstream file(soundPath.c_str());
        if (file.good()) {
            return soundPath;
        }
    }

    {
        soundPath = "Sounds/alarm.wav";
        std::ifstream file(soundPath.c_str());
        if (file.good()) {
            return soundPath;
        }
    }

    //In case nothing found
    return "";

}

void SimulationModel::setHeading(irr::f32 hdg)
{
    ownShip.setHeading(hdg);
}

irr::f32 SimulationModel::getRateOfTurn() const
{
    return ownShip.getRateOfTurn();
}

void SimulationModel::setRateOfTurn(irr::f32 rateOfTurn)
{
    ownShip.setRateOfTurn(rateOfTurn);
}

void SimulationModel::setPos(irr::f32 positionX, irr::f32 positionZ)
{
    ownShip.setPosition(positionX - offsetPosition.X, positionZ - offsetPosition.Z);
}


irr::f32 SimulationModel::getHeading() const
{
    return(ownShip.getHeading());
}
irr::f32 SimulationModel::getPitch() const
{
    return(ownShip.getPitch());
}

irr::f32 SimulationModel::getRoll() const
{
    return(ownShip.getRoll());
}
void SimulationModel::setRudder(irr::f32 rudder)
{
    //Set the rudder (-ve is port, +ve is stbd)
    ownShip.setRudder(rudder);
}


irr::f32 SimulationModel::getRudder() const
{
    return ownShip.getRudder();
}


// DEE vvvvvvvvvvv
void SimulationModel::setWheel(irr::f32 wheel, bool force)
{
    //Set the wheel (-ve is port, +ve is stbd)
    ownShip.setWheel(wheel, force);
}

irr::f32 SimulationModel::getWheel() const
{
    return ownShip.getWheel();
}
// DEE ^^^^^^^^^^^

void SimulationModel::setAzimuth1Master(bool isMaster)
{ // Set if azimuth 1 should also control azimuth 2
    ownShip.setAzimuth1Master(isMaster);
}

void SimulationModel::setAzimuth2Master(bool isMaster)
{ // Set if azimuth 2 should also control azimuth 1
    ownShip.setAzimuth2Master(isMaster);
}

bool SimulationModel::getAzimuth1Master() const
{
    return ownShip.getAzimuth1Master();
}

bool SimulationModel::getAzimuth2Master() const
{
    return ownShip.getAzimuth2Master();
}

// DEE_NOV22 vvvv Azimuth Drive follow up code

    // Schottels

void SimulationModel::setPortSchottel(irr::f32 portAngle)
{ // Set the Port Schottel control angle in degrees (-ve is anticlockwise, +ve is clockwise)
    ownShip.setPortSchottel(portAngle);
}

void SimulationModel::setStbdSchottel(irr::f32 stbdAngle)
{ // Set the Stbd Schottel control angle in degrees (-ve is anticlockwise, +ve is clockwise)
    ownShip.setStbdSchottel(stbdAngle);
}

irr::f32 SimulationModel::getPortSchottel()
{ // Gets the Port Schottel angle, (-ve is anticlockwise, +ve is clockwise)
    return ownShip.getPortSchottel();
}

irr::f32 SimulationModel::getStbdSchottel()
{ // Gets the Stbd Schottel angle, (-ve is anticlockwise, +ve is clockwise)
    return ownShip.getStbdSchottel();
}


// DEE_NOV22 btn control of shcottels ... this is for when you dont use a mouse of control console,
//           however it is also close enough to emergency steering mode of azimuth drives for all
//		 practical playability purposes.
void SimulationModel::btnIncrementPortSchottel()
{
    ownShip.btnIncrementPortSchottel(); // DEE_NOV22 stbd schottel clockwise
}

void SimulationModel::btnDecrementPortSchottel()
{
    ownShip.btnDecrementPortSchottel(); // DEE_NOV22 port schottel anticlockwise
}

void SimulationModel::btnIncrementStbdSchottel()
{
    ownShip.btnIncrementStbdSchottel(); // DEE_NOV22 stbd shcottel clockwise in response to KEY_KEY_L
}

void SimulationModel::btnDecrementStbdSchottel()
{
    ownShip.btnDecrementStbdSchottel(); // DEE_NOV22 port schottel anticlockwise in response to KEY_KEY_J
}




// Thrust levers

void SimulationModel::setPortAzimuthThrustLever(irr::f32 portThrustLever)
{
    ownShip.setPortAzimuthThrustLever(portThrustLever);
    //        ownShip.setPortThrustLever(irr::f32 portThrustLever);
}

void SimulationModel::setStbdAzimuthThrustLever(irr::f32 stbdThrustLever)
{
    //        ownShip.setStbdThrustLever(irr::f32 stbdThrustLever);
    ownShip.setStbdAzimuthThrustLever(stbdThrustLever);
}

irr::f32 SimulationModel::getPortAzimuthThrustLever()
{
    return ownShip.getPortAzimuthThrustLever();
}

irr::f32 SimulationModel::getStbdAzimuthThrustLever()
{
    return ownShip.getStbdAzimuthThrustLever();
}


// DEE_NOV22 below in response to keyboard presses
//		 todo implement an emergency steering mode
//           respond to physical control's buttons emergency mode
//		 other code for follow up response to physical controls

void SimulationModel::btnIncrementPortThrustLever()
{
    ownShip.btnIncrementPortThrustLever();
}

void SimulationModel::btnDecrementPortThrustLever()
{
    ownShip.btnDecrementPortThrustLever();
}

void SimulationModel::btnIncrementStbdThrustLever()
{
    ownShip.btnIncrementStbdThrustLever();
}

void SimulationModel::btnDecrementStbdThrustLever()
{
    ownShip.btnDecrementStbdThrustLever();
}


// DEE_NOV22 Clutches , in normal operation these would be automatic, however in emergency (non follow up) mode they are manual
// DEE_NOV22 in future perhaps model engine stall for when clutch engaged at too low a revs and prop shaft snap if clutch
// DEE_NOV22 is engaged at too high a revs

void SimulationModel::setPortClutch(bool portClutch)
{
    ownShip.setPortClutch(portClutch);
}

void SimulationModel::setStbdClutch(bool stbdClutch)
{
    ownShip.setStbdClutch(stbdClutch);
}

bool SimulationModel::getPortClutch()
{
    return ownShip.getPortClutch();
}

bool SimulationModel::getStbdClutch()
{
    return ownShip.getStbdClutch();
}


// DEE_NOV22 todo need to assign keys to this is emergency steering mode where there is no automatic clutch
//           I think we could use the follow up / non follow up flag to determine if it is in normal or
//		 emergency steering mode.
//		 todo is it better to use SimulationModel::setXXXXClutch(xxxx) for this

void SimulationModel::engagePortClutch()
{
    ownShip.setPortClutch(true);
}

void SimulationModel::disengagePortClutch()
{
    ownShip.setPortClutch(false);
}

void SimulationModel::engageStbdClutch()
{
    ownShip.setStbdClutch(true);
}

void SimulationModel::disengageStbdClutch()
{
    ownShip.setStbdClutch(false);
}





// DEE_NOV22 ^^^^ Azimuth Drive follow up code

void SimulationModel::setPortAzimuthAngle(irr::f32 angle)
{// Set the azimuth angle, in degrees (-ve is port, +ve is stbd)
    ownShip.setPortAzimuthAngle(angle);
}

void SimulationModel::setStbdAzimuthAngle(irr::f32 angle)
{// Set the azimuth angle, in degrees (-ve is port, +ve is stbd)
    ownShip.setStbdAzimuthAngle(angle);
}

void SimulationModel::setPortEngine(irr::f32 port)
{
    //Set the engine, (-ve astern, +ve ahead)
    ownShip.setPortEngine(port); //This method limits the range applied

    //Set engine sound level
    // DEE_NOV22 unless this is a controllable pitch propellor,
    // where the engine turns at a constant rpm
    // where with increased power then the sound of the engine
    // results in the same frequency engine noise, only louder.
    // Vessels where engine rpm controls power then the frequency
    // of the engine noise should change with engine rpm

    //KYARA ENGINE IDLE: a real diesel never goes silent at idle - it keeps running at a
    //steady, quieter rumble. ENGINE_IDLE_VOLUME is the volume at throttle==0; full throttle
    //still reaches 0.5 as before. Without this floor, fabs(throttle)*0.5 hits exactly zero
    //at the idle notch and the engine sound cuts out completely, which is what you were
    //hearing.
    const irr::f32 ENGINE_IDLE_VOLUME = 0.40f; // volume at idle (throttle == 0)
    const irr::f32 ENGINE_FULL_VOLUME = 0.5f;  // volume at full throttle (unchanged from before)

    if (ownShip.isSingleEngine()) {
        irr::f32 throttleFraction = fabs(getPortEngine());
        sound->setVolumeEngine(ENGINE_IDLE_VOLUME + throttleFraction * (ENGINE_FULL_VOLUME - ENGINE_IDLE_VOLUME));
    }
    else {
        irr::f32 throttleFraction = 0.5f * (fabs(getPortEngine()) + fabs(getStbdEngine()));
        sound->setVolumeEngine(ENGINE_IDLE_VOLUME + throttleFraction * (ENGINE_FULL_VOLUME - ENGINE_IDLE_VOLUME));
    }

}

void SimulationModel::setStbdEngine(irr::f32 stbd)
{
    //Set the engine, (-ve astern, +ve ahead)
    ownShip.setStbdEngine(stbd); //This method limits the range applied

    //Set engine sound level
    // DEE_NOV22 same comment as for port engine
    //KYARA ENGINE IDLE: same idle-floor mapping as setPortEngine, see comment there.
    const irr::f32 ENGINE_IDLE_VOLUME = 0.78f;
    const irr::f32 ENGINE_FULL_VOLUME = 0.5f;

    if (ownShip.isSingleEngine()) {
        irr::f32 throttleFraction = fabs(getPortEngine());
        sound->setVolumeEngine(ENGINE_IDLE_VOLUME + throttleFraction * (ENGINE_FULL_VOLUME - ENGINE_IDLE_VOLUME));
    }
    else {
        irr::f32 throttleFraction = 0.5f * (fabs(getPortEngine()) + fabs(getStbdEngine()));
        sound->setVolumeEngine(ENGINE_IDLE_VOLUME + throttleFraction * (ENGINE_FULL_VOLUME - ENGINE_IDLE_VOLUME));
    }
}

irr::f32 SimulationModel::getPortEngine() const
{
    return ownShip.getPortEngine();

}

irr::f32 SimulationModel::getStbdEngine() const
{
    return ownShip.getStbdEngine();
}

irr::f32 SimulationModel::getPortEngineRPM() const
{
    return ownShip.getPortEngineRPM();
}

irr::f32 SimulationModel::getStbdEngineRPM() const
{
    return ownShip.getStbdEngineRPM();
}

void SimulationModel::setBowThruster(irr::f32 proportion)
{
    ownShip.setBowThruster(proportion);
}

void SimulationModel::setSternThruster(irr::f32 proportion)
{
    ownShip.setSternThruster(proportion);
}

void SimulationModel::setBowThrusterRate(irr::f32 bowThrusterRate) {
    //Sets the rate of increase of bow thruster, used for joystick button control
    ownShip.setBowThrusterRate(bowThrusterRate);
}

void SimulationModel::setSternThrusterRate(irr::f32 sternThrusterRate) {
    //Sets the rate of increase of bow thruster, used for joystick button control
    ownShip.setSternThrusterRate(sternThrusterRate);
}

irr::f32 SimulationModel::getBowThruster() const
{
    return ownShip.getBowThruster();
}

irr::f32 SimulationModel::getSternThruster() const
{
    return ownShip.getSternThruster();
}

void SimulationModel::setRudderPumpState(int whichPump, bool rudderPumpState) {
    ownShip.setRudderPumpState(whichPump, rudderPumpState);
}

bool SimulationModel::getRudderPumpState(int whichPump) const
{
    return ownShip.getRudderPumpState(whichPump);
}

void SimulationModel::setFollowUpRudderWorking(bool followUpRudderWorking) {
    ownShip.setFollowUpRudderWorking(followUpRudderWorking);
}

void SimulationModel::setAccelerator(irr::f32 accelerator)
{
    device->getTimer()->setSpeed(accelerator);
}

irr::f32 SimulationModel::getAccelerator() const
{
    return device->getTimer()->getSpeed();
}

void SimulationModel::setWeather(irr::f32 weather)
{
    //KYARA METEO: capped here so the slider, the scenario file and the network all obey it
    if (weather < 0.0f) { weather = 0.0f; }
    if (weather > SIM_MAX_WEATHER) { weather = SIM_MAX_WEATHER; }
    this->weather = weather;
}

irr::f32 SimulationModel::getWeather() const
{
    return weather;
}

void SimulationModel::setRain(irr::f32 rainIntensity)
{
    this->rainIntensity = rainIntensity;
}

irr::f32 SimulationModel::getRain() const
{
    return rainIntensity;
}

void SimulationModel::setVisibility(irr::f32 visibilityNm)
{
    this->visibilityRange = visibilityNm;
}

irr::f32 SimulationModel::getVisibility() const
{
    return visibilityRange;
}

void SimulationModel::setWindDirection(irr::f32 windDirection) //Range 0-360.
{
    this->windDirection = windDirection;
}

irr::f32 SimulationModel::getWindDirection() const
{
    return windDirection;
}

void SimulationModel::setWindSpeed(irr::f32 windSpeed) //Nm/h
{
    this->windSpeed = windSpeed;
}

irr::f32 SimulationModel::getWindSpeed() const
{
    return windSpeed;
}

void SimulationModel::setStreamOverrideDirection(irr::f32 streamDirection) //Range 0-360.
{
    this->streamOverrideDirection = streamDirection;
}

irr::f32 SimulationModel::getStreamOverrideDirection() const
{
    return streamOverrideDirection;
}

void SimulationModel::setStreamOverrideSpeed(irr::f32 streamSpeed) //Nm/h
{
    this->streamOverrideSpeed = streamSpeed;
}

irr::f32 SimulationModel::getStreamOverrideSpeed() const
{
    return streamOverrideSpeed;
}

void SimulationModel::setStreamOverride(bool streamOverride)
{
    this->streamOverride = streamOverride;
}

bool SimulationModel::getStreamOverride() const
{
    return streamOverride;
}

void SimulationModel::setWaterVisible(bool visible)
{
    water.setVisible(visible);
}

void SimulationModel::lookUp()
{
    camera.lookUp();
}

void SimulationModel::lookDown()
{
    camera.lookDown();
}

void SimulationModel::lookLeft()
{
    camera.lookLeft();
}

void SimulationModel::lookRight()
{
    camera.lookRight();
}

void SimulationModel::setPanSpeed(irr::f32 horizontalPanSpeed)
{
    camera.setPanSpeed(horizontalPanSpeed);
}

void SimulationModel::setVerticalPanSpeed(irr::f32 verticalPanSpeed)
{
    camera.setVerticalPanSpeed(verticalPanSpeed);
}

void SimulationModel::changeLookPx(irr::s32 deltaX, irr::s32 deltaY)
{
    irr::f32 proportionalX = deltaX / (irr::f32)driver->getScreenSize().Width;
    irr::f32 proportionalY = deltaY / (irr::f32)driver->getScreenSize().Width;
    camera.lookChange(proportionalX, proportionalY);
}

void SimulationModel::lookStepLeft()
{
    camera.lookStepLeft();
}

void SimulationModel::lookStepRight()
{
    camera.lookStepRight();
}

void SimulationModel::moveCameraForwards()
{
    camera.moveForwards();
}

void SimulationModel::moveCameraBackwards()
{
    camera.moveBackwards();
}

void SimulationModel::lookAhead()
{
    camera.lookAhead();
}

void SimulationModel::lookAstern()
{
    camera.lookAstern();
}

void SimulationModel::lookPort()
{
    camera.lookPort();
}

void SimulationModel::lookStbd()
{
    camera.lookStbd();
}

void SimulationModel::changeView()
{
    //The last boat.ini view leads to the free view, which leads back to the first view.
    if (freeView) {
        setFreeView(false);
        camera.setView(0);
    }
    else if (lightEditVessel == -2 && camera.getView() + 1 >= camera.getViewCount()) {
        setFreeView(true);
        return;
    }
    else {
        camera.changeView();
    }
    ownShip.setViewVisibility(camera.getView());
}

void SimulationModel::setView(irr::u32 view)
{
    if (freeView) { setFreeView(false); }
    camera.setView(view);
    ownShip.setViewVisibility(camera.getView());
}

bool SimulationModel::isFreeView() const
{
    return freeView;
}

void SimulationModel::setFreeView(bool on)
{
    if (on == freeView || (on && lightEditVessel != -2)) { return; }
    freeView = on;
    if (on) {
        //Far enough out to see the whole ship, whatever her size
        irr::f32 radius = 1.5f * ownShip.getLength();
        if (radius < 25.0f) { radius = 25.0f; }
        camera.setOrbit(true, radius);
        camera.setOrbitMinPitch(2.0f); //never down to the sea surface
    }
    else {
        camera.setOrbit(false);
    }
}

irr::u32 SimulationModel::getCameraView() const
{
    return camera.getView();
}

irr::core::vector3df SimulationModel::getCameraBasePosition() const
{
    return camera.getBasePosition();
}

irr::core::matrix4 SimulationModel::getCameraBaseRotation() const
{
    return camera.getBaseRotation();
}

void SimulationModel::setFrozenCamera(bool frozen)
{
    camera.setFrozen(frozen);
}

void SimulationModel::toggleFrozenCamera()
{
    camera.toggleFrozen();
}

void SimulationModel::setAlarm(bool alarmState)
{
    if (alarmState) {
        sound->setVolumeAlarm(1.0);
    }
    else {
        sound->setVolumeAlarm(0.0);
    }
}

void SimulationModel::toggleRadarOn()
{
    radarCalculation.toggleRadarOn();
}

void SimulationModel::setArpaOnBuoys(bool state)
{
    radarCalculation.setArpaOnBuoys(state);
}
void SimulationModel::setRadarHeadingMode(bool useReal)
{
    radarCalculation.setUseRealHeading(useReal);
}
void SimulationModel::cycleRadarEchoStretch()
{
    radarCalculation.cycleEchoStretch();
}
int SimulationModel::getRadarEchoStretch() const
{
    return radarCalculation.getEchoStretch();
}
bool SimulationModel::getRadarHeadingMode() const
{
    return radarCalculation.getUseRealHeading();
}

void SimulationModel::setRadarBuoyTrails(bool state)
{
    radarCalculation.setBuoyTrails(state);
}

void SimulationModel::setRadarShipTrails(bool state)
{
    radarCalculation.setShipTrails(state);
}

void SimulationModel::setRadarOwnShipTrails(bool state)
{
    radarCalculation.setOwnShipTrails(state);
}

void SimulationModel::setRadarMMSI(bool state)
{
    radarCalculation.setMMSI(state);
}

void SimulationModel::setRadarBuoyMarkerColour(int paletteIndex)
{
    radarCalculation.setBuoyContactColour(paletteIndex);
}

void SimulationModel::setRadarShipMarkerColour(int paletteIndex)
{
    radarCalculation.setShipContactColour(paletteIndex);
}

bool SimulationModel::isRadarOn() const
{
    return radarCalculation.isRadarOn();
}

irr::video::SColor SimulationModel::getRadarSurroundColour() const
{
    return radarCalculation.getRadarSurroundColour();
}

void SimulationModel::increaseRadarRange()
{
    radarCalculation.increaseRange();
}

void SimulationModel::decreaseRadarRange()
{
    radarCalculation.decreaseRange();
}

void SimulationModel::setRadarGain(irr::f32 value)
{
    radarCalculation.setGain(value);
}

void SimulationModel::setRadarClutter(irr::f32 value)
{
    radarCalculation.setClutter(value);
}

void SimulationModel::setRadarRain(irr::f32 value)
{
    radarCalculation.setRainClutter(value);
}

void SimulationModel::increaseRadarGain(irr::f32 value)
{
    radarCalculation.increaseGain(value);
}

void SimulationModel::decreaseRadarGain(irr::f32 value)
{
    radarCalculation.decreaseGain(value);
}

void SimulationModel::increaseRadarClutter(irr::f32 value)
{
    radarCalculation.increaseClutter(value);
}

void SimulationModel::decreaseRadarClutter(irr::f32 value)
{
    radarCalculation.decreaseClutter(value);
}

void SimulationModel::increaseRadarRain(irr::f32 value)
{
    radarCalculation.increaseRainClutter(value);
}

void SimulationModel::decreaseRadarRain(irr::f32 value)
{
    radarCalculation.decreaseRainClutter(value);
}

void SimulationModel::setPIData(irr::s32 PIid, irr::f32 PIbearing, irr::f32 PIrange)
{
    radarCalculation.setPIData(PIid, PIbearing, PIrange);
}

irr::f32 SimulationModel::getPIbearing(irr::s32 PIid) const
{
    return radarCalculation.getPIbearing(PIid);
}

irr::f32 SimulationModel::getPIrange(irr::s32 PIid) const
{
    return radarCalculation.getPIrange(PIid);
}

void SimulationModel::increaseRadarEBLRange() { radarCalculation.increaseEBLRange(); }
void SimulationModel::decreaseRadarEBLRange() { radarCalculation.decreaseEBLRange(); }
void SimulationModel::increaseRadarEBLBrg() { radarCalculation.increaseEBLBrg(); }
void SimulationModel::decreaseRadarEBLBrg() { radarCalculation.decreaseEBLBrg(); }

void SimulationModel::selectRadarEBL() { radarCalculation.selectNextEBL(); }
void SimulationModel::selectRadarVRM() { radarCalculation.selectNextVRM(); }
void SimulationModel::cycleRadarGuardAlarm() { radarCalculation.cycleGuardAlarmMode(); }
void SimulationModel::cycleRadarRangeRings() { radarCalculation.cycleRangeRingBrightness(); }   //kyara
int SimulationModel::getRadarGuardAlarmMode() const { return radarCalculation.getGuardAlarmMode(); }
void SimulationModel::setRadarEBLBrg(irr::u32 index, irr::f32 brg) { radarCalculation.setEBLBrg(index, brg); }
void SimulationModel::setRadarVRMRange(irr::u32 index, irr::f32 rangeNm) { radarCalculation.setVRMRange(index, rangeNm); }
void SimulationModel::setRadarActiveEBL(irr::u32 index) { radarCalculation.setActiveEBL(index); }
void SimulationModel::setRadarActiveVRM(irr::u32 index) { radarCalculation.setActiveVRM(index); }
void SimulationModel::setRadarGuardAlarmMode(int mode) { radarCalculation.setGuardAlarmMode(mode); }
irr::f32 SimulationModel::getRadarEBLBrg(irr::u32 index) const { return radarCalculation.getEBLBrg(index); }
irr::f32 SimulationModel::getRadarVRMRange(irr::u32 index) const { return radarCalculation.getVRMRangeNm(index); }
irr::u32 SimulationModel::getRadarActiveEBL() const { return radarCalculation.getActiveEBL(); }
irr::u32 SimulationModel::getRadarActiveVRM() const { return radarCalculation.getActiveVRM(); }

void SimulationModel::increaseRadarXCursor() { radarCalculation.increaseCursorRangeXNm(); }
void SimulationModel::decreaseRadarXCursor() { radarCalculation.decreaseCursorRangeXNm(); }
void SimulationModel::increaseRadarYCursor() { radarCalculation.increaseCursorRangeYNm(); }
void SimulationModel::decreaseRadarYCursor() { radarCalculation.decreaseCursorRangeYNm(); }

void SimulationModel::setRadarNorthUp()
{
    radarCalculation.setNorthUp();
}

void SimulationModel::setRadarCourseUp()
{
    radarCalculation.setCourseUp();
}

void SimulationModel::setRadarHeadUp()
{
    radarCalculation.setHeadUp();
}

void SimulationModel::changeRadarColourChoice()
{
    radarCalculation.changeRadarColourChoice();
}

int SimulationModel::getArpaMode() const
{
    return radarCalculation.getArpaMode();
}

void SimulationModel::setArpaMode(int mode)
{
    radarCalculation.setArpaMode(mode);
}
//buoy toggle on and off



void SimulationModel::setArpaListSelection(irr::s32 selection)
{
    radarCalculation.setArpaListSelection(selection);
}

void SimulationModel::setRadarARPARel()
{
    radarCalculation.setRadarARPARel();
}

void SimulationModel::setRadarARPATrue()
{
    radarCalculation.setRadarARPATrue();
}

void SimulationModel::setRadarARPAVectors(irr::f32 vectorMinutes)
{
    radarCalculation.setRadarARPAVectors(vectorMinutes);
}

void SimulationModel::setRadarDisplayRadius(irr::u32 radiusPx)
{
    radarCalculation.setRadarDisplayRadius(radiusPx);
    radarScreen.setRadarDisplayRadius(radiusPx);
}
void SimulationModel::setSecondaryRadarMaster(bool isMaster) { secondaryRadarMaster = isMaster; }
bool SimulationModel::getIsSecondaryRadarMaster() const { return secondaryRadarMaster; }

void SimulationModel::addManualPoint(bool newContact)
{
    radarCalculation.addManualPoint(newContact, offsetPosition, ownShip, absoluteTime);
}

void SimulationModel::clearManualPoints()
{
    radarCalculation.clearManualPoints();
}

void SimulationModel::trackTargetFromCursor()
{
    radarCalculation.trackTargetFromCursor();
}

void SimulationModel::clearTargetFromCursor()
{
    radarCalculation.clearTargetFromCursor();
}

irr::u32 SimulationModel::getARPATracksSize() const
{
    return radarCalculation.getARPATracksSize();
}

ARPAContact SimulationModel::getARPAContactFromTrackIndex(irr::u32 index) const
{
    return radarCalculation.getARPAContactFromTrackIndex(index);
}

void SimulationModel::setMainCameraActive()
{
    camera.setActive();
}

void SimulationModel::setRadarCameraActive()
{
    radarCamera.setActive();
}

void SimulationModel::setZoom(bool zoomOn) {
    if (zoomOn) {
        currentZoom = zoomLevel;
    }
    else {
        currentZoom = 1;
    }
    camera.setHFOV(irr::core::degToRad(modelParameters.viewAngle) / currentZoom);
}

void SimulationModel::setZoom(bool zoomOn, irr::f32 zoomLevel)
{
    this->zoomLevel = zoomLevel;
    setZoom(zoomOn);
}

void SimulationModel::setViewAngle(irr::f32 viewAngle)
{
    modelParameters.viewAngle = viewAngle;
    camera.setHFOV(irr::core::degToRad(modelParameters.viewAngle) / currentZoom);
}

void SimulationModel::setMouseDown(bool isMouseDown)
{
    this->isMouseDown = isMouseDown;
}

void SimulationModel::updateViewport(irr::f32 aspect)
{
    camera.updateViewport(aspect);
}

//NAUTITECH triple-screen: render one angled column of the main view.
//columnHFOVdeg is the horizontal FOV of a single TV (degrees); yawOffsetDeg
//points this column to port (-) or starboard (+). Zoom still applies.
void SimulationModel::renderMainColumn(irr::f32 columnAspect, irr::f32 columnHFOVdeg, irr::f32 yawOffsetDeg)
{
    camera.renderColumn(columnAspect, irr::core::degToRad(columnHFOVdeg) / currentZoom, yawOffsetDeg);
}
void SimulationModel::setTripleScreen(bool on, irr::f32 perScreenFOVdeg, irr::f32 bezelYawDeg)
{
    triScreenMooring = on;
    perScreenFOVMooring = perScreenFOVdeg;
    bezelYawMooring = bezelYawDeg;
}

irr::core::line3df SimulationModel::getMooringRay(irr::s32 mouseX, irr::s32 mouseY, bool showInterface)
{
    setMainCameraActive();
    irr::core::dimension2du ss = driver->getScreenSize();
    irr::s32 screenW = (irr::s32)ss.Width;
    irr::s32 screenH = (irr::s32)ss.Height;

    // Same Y scaling as the legacy path when the interface strip is shown
    irr::s32 scaledY = showInterface ? (irr::s32)(mouseY / VIEW_PROPORTION_3D) : mouseY;

    if (!triScreenMooring) {
        return smgr->getSceneCollisionManager()->getRayFromScreenCoordinates(
            irr::core::position2d<irr::s32>(mouseX, scaledY));
    }

    // Triple-screen: find the column clicked, then build the ray directly from the camera
     // vectors. getRayFromScreenCoordinates can't be used here - it reads the camera's cached
     // frustum, which only refreshes on render, so it always picks with the last-rendered
     // (right) column no matter which column we set. That was the "click right, lands left" bug.
    irr::s32 colW = screenW / 3;
    if (colW < 1) { colW = 1; }
    irr::s32 c = mouseX / colW;
    if (c < 0) { c = 0; }
    if (c > 2) { c = 2; }
    irr::s32 baseH = showInterface ? (irr::s32)(screenH * VIEW_PROPORTION_3D) : screenH;
    irr::f32 colAspect = (irr::f32)colW / (irr::f32)baseH;
    irr::f32 yaw[3] = { -bezelYawMooring, 0.0f, bezelYawMooring };

    // Column-local normalised device coordinates (-1..1), centre 0
    irr::f32 ndcX = (2.0f * (irr::f32)(mouseX - c * colW) / (irr::f32)colW) - 1.0f;
    irr::f32 ndcY = 1.0f - (2.0f * (irr::f32)mouseY / (irr::f32)baseH);

    return camera.getPickRay(ndcX, ndcY,
        irr::core::degToRad(perScreenFOVMooring) / currentZoom,
        colAspect, yaw[c]);
}
irr::u32 SimulationModel::getLoopNumber() const
{
    return loopNumber;
}

std::string SimulationModel::getSerialisedScenario() const
{
    return serialisedScenarioData;
}

std::string SimulationModel::getScenarioName() const
{
    return scenarioName;
}

std::string SimulationModel::getWorldName() const
{
    return worldName;
}

std::string SimulationModel::getWorldReadme() const
{
    return worldModelReadmeText;
}

void SimulationModel::releaseManOverboard()
{
    //Only release/update if not already released
    if (!manOverboard.getVisible()) {
        manOverboard.setVisible(true);
        irr::core::vector3df ownShipPos = ownShip.getPosition();
        irr::core::vector3df relativePosition;
        relativePosition.Y = 0;
        //Put randomly on port or starboard side of the ship
        if (rand() > RAND_MAX / 2) {
            relativePosition.X = ownShip.getBreadth() * 0.6 * cos(ownShip.getHeading() * irr::core::DEGTORAD);
            relativePosition.Z = ownShip.getBreadth() * -0.6 * sin(ownShip.getHeading() * irr::core::DEGTORAD);
            //PositionEntity(mob,EntityX( ship_parent )+(OwnShipWidth#*0.6)*Cos(angle#),THeight#,EntityZ( ship_parent )-(OwnShipWidth#*0.6)*Sin(angle#), True)
        }
        else {
            relativePosition.X = ownShip.getBreadth() * -0.6 * cos(ownShip.getHeading() * irr::core::DEGTORAD);
            relativePosition.Z = ownShip.getBreadth() * 0.6 * sin(ownShip.getHeading() * irr::core::DEGTORAD);
            //PositionEntity(mob,EntityX( ship_parent )-(OwnShipWidth#*0.6)*Cos(angle#),THeight#,EntityZ( ship_parent )+(OwnShipWidth#*0.6)*Sin(angle#), True)
        }
        manOverboard.setPosition(ownShipPos + relativePosition);
    }

}



void SimulationModel::retrieveManOverboard()
{
    manOverboard.setVisible(false);
}

bool SimulationModel::getManOverboardVisible() const
{
    return manOverboard.getVisible();
}

irr::f32 SimulationModel::getManOverboardPosX() const
{
    return manOverboard.getPosition().X + offsetPosition.X;
}

irr::f32 SimulationModel::getManOverboardPosZ() const
{
    return manOverboard.getPosition().Z + offsetPosition.Z;
}


void SimulationModel::setManOverboardVisible(bool visible)
{
    //To be used directly, eg when in secondary display mode only
    manOverboard.setVisible(visible);
}

void SimulationModel::setManOverboardPos(irr::f32 positionX, irr::f32 positionZ)
{
    //To be used directly, eg when in secondary display mode only
    manOverboard.setPosition(irr::core::vector3df(positionX - offsetPosition.X, 0, positionZ - offsetPosition.Z));
}

bool SimulationModel::hasGPS() const
{
    return ownShip.hasGPS();
}

bool SimulationModel::isSingleEngine() const
{
    return ownShip.isSingleEngine();
}

bool SimulationModel::isAzimuthDrive() const
{
    return ownShip.isAzimuthDrive();
}

bool SimulationModel::isAzimuthAsternAllowed() const
{
    return ownShip.isAzimuthAsternAllowed();
}

irr::f32 SimulationModel::inputToAzimuthEngineMapping(irr::f32 inputAngle) const
{
    irr::f32 tempEngLevel; // temporary variable 0..1 to represent attempted engine setting

    if (isAzimuthAsternAllowed()) {
        if ((inputAngle >= 0) && (inputAngle < 135)) {
            tempEngLevel = (inputAngle / 135.0); // Gives range 0->1 for inputs between 0->135deg
        }
        else if ((inputAngle >= 135) && (inputAngle < 180)) {
            tempEngLevel = 1; // Gives 1 for inputs between 135 and 180
        }
        else if ((inputAngle >= 180) && (inputAngle < 225)) {
            tempEngLevel = -1; // Gives -1 for inputs between 180 and 225
        }
        else if ((inputAngle >= 225) && (inputAngle < 360)) {
            tempEngLevel = -1 + ((inputAngle - 225.0) / 135.0); // Gives range -1->0 for inputs between 225 and 360
        }
    }
    else {
        if ((inputAngle >= 0) && (inputAngle < 135))
        {
            tempEngLevel = (0.5 + inputAngle / 270); // Gives range 0.5->1 for inputs between 0->135deg
        }
        if ((inputAngle >= 135) && (inputAngle < 180)) // Gives 1 for inputs between 135 and 180
        {
            tempEngLevel = 1;
        }
        if ((inputAngle >= 225) && (inputAngle < 360)) // Gives range 0->0.5 for inputs between 225->360
        {
            tempEngLevel = ((inputAngle - 225) / 270);
        }
        // DEE_Boxing_Day_2022 I am sure there is a far more elegant solution than the above

        // limit the output to 0..1 only leaving this in for future elegant solution
        if (tempEngLevel < 0)
        {
            tempEngLevel = 0;
        }
        if (tempEngLevel > 1)
        {
            tempEngLevel = 1;
        }

    }
    return tempEngLevel;
}

irr::f32 SimulationModel::azimuthToInputEngineMapping(irr::f32 inputEngine) const
{
    if (isAzimuthAsternAllowed()) {
        return (inputEngine * 135);
    }
    else {
        return (inputEngine * 270) - 135;
    }
}

bool SimulationModel::hasDepthSounder() const
{
    return ownShip.hasDepthSounder();
}

bool SimulationModel::hasBowThruster() const
{
    return ownShip.hasBowThruster();
}

bool SimulationModel::hasSternThruster() const
{
    return ownShip.hasSternThruster();
}

bool SimulationModel::hasTurnIndicator() const
{
    return ownShip.hasTurnIndicator();
}

bool SimulationModel::debugModeOn() const
{
    return modelParameters.debugMode;
}

irr::f32 SimulationModel::getOwnShipMass() const
{
    return ownShip.getShipMass();
}

irr::f32 SimulationModel::getOwnShipMassEstimate() const
{
    return ownShip.getEstimatedDisplacement();
}

irr::f32 SimulationModel::getOtherShipMassEstimate(int number) const
{
    return otherShips.getEstimatedDisplacement(number);
}

irr::f32 SimulationModel::getMaxSounderDepth() const
{
    return ownShip.getMaxSounderDepth();
}

void SimulationModel::startHorn() {
    sound->setVolumeHorn(1.0);
}

void SimulationModel::endHorn() {
    sound->setVolumeHorn(0.0);
}

bool SimulationModel::getMoveViewWithPrimary() const {
    return moveViewWithPrimary;
}

void SimulationModel::setMoveViewWithPrimary(bool moveView) {
    moveViewWithPrimary = moveView;
}

SimulationModel::ModelParameters SimulationModel::getModelParameters() const {
    return modelParameters;
}

bool SimulationModel::getIsSecondaryControlWheel() const {
    return modelParameters.secondaryControlWheel;
}

bool SimulationModel::getIsSecondaryControlPortEngine() const {
    return modelParameters.secondaryControlPortEngine;
}

bool SimulationModel::getIsSecondaryControlStbdEngine() const {
    return modelParameters.secondaryControlStbdEngine;
}

bool SimulationModel::getIsSecondaryControlPortSchottel() const {
    return modelParameters.secondaryControlPortSchottel;
}

bool SimulationModel::getIsSecondaryControlStbdSchottel() const {
    return modelParameters.secondaryControlStbdSchottel;
}

bool SimulationModel::getIsSecondaryControlPortThrustLever() const {
    return modelParameters.secondaryControlPortThrustLever;
}

bool SimulationModel::getIsSecondaryControlStbdThrustLever() const {
    return modelParameters.secondaryControlStbdThrustLever;
}

bool SimulationModel::getIsSecondaryControlBowThruster() const {
    return modelParameters.secondaryControlBowThruster;
}

bool SimulationModel::getIsSecondaryControlSternThruster() const {
    return modelParameters.secondaryControlSternThruster;
}

irr::f32 SimulationModel::getLineStiffnessFactor() const {
    return modelParameters.lineStiffnessFactor;
}

irr::f32 SimulationModel::getLineDampingFactor() const {
    return modelParameters.lineDampingFactor;
}

irr::scene::ISceneNode* SimulationModel::getContactFromRay(irr::core::line3d<irr::f32> ray, irr::s32 linesMode) {

    // Temporarily enable all required triangle selectors
    if (linesMode == 1) {
        // Start - on own ship
        ownShip.enableTriangleSelector(true);
    }
    else if (linesMode == 2) {
        // End - not on own ship
        otherShips.enableAllTriangleSelectors(); //This will be reset next time otherShips.update is called
        buoys.enableAllTriangleSelectors(); //This will be reset next time otherShips.update is called
        // TODO: Temporarily enable triangle selector for:
        //   Terrain
        //   Land objects
    }
    else {
        // Not start or end, return null;
        return 0;
    }

    irr::core::vector3df intersection;
    irr::core::triangle3df hitTriangle;

    irr::scene::ISceneNode* selectedSceneNode =
        smgr->getSceneCollisionManager()->getSceneNodeAndCollisionPointFromRay(
            ray,
            intersection, // This will be the position of the collision
            hitTriangle, // This will be the triangle hit in the collision
            IDFlag_IsPickable, // (bitmask), 0 for all
            0); // Check all nodes

    irr::scene::ISceneNode* contactPointNode = 0;

    if (selectedSceneNode &&
        (
            ((linesMode == 1) && (selectedSceneNode == ownShip.getSceneNode())) || // Valid start node
            ((linesMode == 2) && (selectedSceneNode != ownShip.getSceneNode()))    // Valid end node
            )
        ) {

        // Add a 'sphere' scene node, with selectedSceneNode as parent.
        // Find local coordinates from the global one
        irr::core::vector3df localPosition(intersection);
        irr::core::matrix4 worldToLocal = selectedSceneNode->getAbsoluteTransformation();
        worldToLocal.makeInverse();
        worldToLocal.transformVect(localPosition);

        irr::core::vector3df sphereScale = irr::core::vector3df(1.0, 1.0, 1.0);
        if (selectedSceneNode && selectedSceneNode->getScale().X > 0) {
            sphereScale = irr::core::vector3df(1.0f / selectedSceneNode->getScale().X,
                1.0f / selectedSceneNode->getScale().X,
                1.0f / selectedSceneNode->getScale().X);
        }

        contactPointNode = smgr->addSphereSceneNode(0.25f, 16, selectedSceneNode, -1,
            localPosition,
            irr::core::vector3df(0, 0, 0),
            sphereScale);

        // Set name to match parent for convenience
        contactPointNode->setName(selectedSceneNode->getName());
    }

    // Reset triangle selectors
    ownShip.enableTriangleSelector(false); // Own ship should not need triangle selectors at runtime (todo: for future robustness, check previous state and restore to this)
    // buoys and otherShips will be reset when their update() method is called

    return contactPointNode;
}
// Inc 3 (comms): widen an ASCII string (vessel names) for the wide-char GUI overlay.
static std::wstring widen(const std::string& s) { return std::wstring(s.begin(), s.end()); }

// Inc 3 (comms): format a position as deg-min with hemisphere for the MAYDAY prompt.
static std::wstring formatLatLong(irr::f32 lat, irr::f32 lon)
{
    wchar_t buf[64];
    wchar_t ns = (lat >= 0) ? L'N' : L'S';
    wchar_t ew = (lon >= 0) ? L'E' : L'W';
    irr::f32 aLat = fabs(lat), aLon = fabs(lon);
    int latD = (int)aLat; irr::f32 latM = (aLat - latD) * 60.0f;
    int lonD = (int)aLon; irr::f32 lonM = (aLon - lonD) * 60.0f;
    swprintf(buf, 64, L"%02d\u00B0%04.1f'%lc  %03d\u00B0%04.1f'%lc", latD, latM, ns, lonD, lonM, ew);
    return std::wstring(buf);
}
// FIRE FEATURE ------------------------------------------------------------
void SimulationModel::igniteNearestOtherShipFire()
{
    irr::u32 n = otherShips.getNumber();
    if (n == 0) { return; }

    // Clear any previous fire and upright the previous casualty (instructor may re-trigger).
    fire.remove();
    if (fireMountNode) { fireMountNode->remove(); fireMountNode = 0; }
    if (burningShipIndex >= 0) { otherShips.setCasualty(burningShipIndex, false); } // Inc 3
    burningShipIndex = -1;

    // Nearest OtherShip to own ship, but skip our own rescue craft so Ctrl+F sets the
 // CASUALTY alight, not the helo/pilot boat that happens to be closest.
    irr::core::vector3df ownPos = ownShip.getPosition();
    int best = -1; irr::f32 bestDist = 1.0e30f;
    // SCENARIO INCENDIE: the ship chosen in the fire scenario editor, if there is one.
    bool casualtyChosen = (incident.casualtyShip >= 1 && incident.casualtyShip <= (int)n);
    if (casualtyChosen) { best = incident.casualtyShip - 1; }
    for (irr::u32 i = 0; i < n && !casualtyChosen; i++) {
        if (otherShips.isFireFightingVessel(i)) { continue; }
        if (isRescueBoat((int)i)) { continue; }   // SAR RESCUE RUN: never a rescuer
        irr::f32 d = otherShips.getPosition(i).getDistanceFrom(ownPos);
        if (d < bestDist) { bestDist = d; best = (int)i; }
    }
    if (best < 0) { return; }

    irr::scene::ISceneNode* shipNode = otherShips.getSceneNode(best);
    if (!shipNode) { return; }

    // Size and place the fire from the ship's ACTUAL world bounding box, so it's always
 // visible and correctly scaled whether the casualty is a skiff or a 90 m NOAA vessel.
    shipNode->updateAbsolutePosition();
    irr::core::aabbox3df bb = shipNode->getTransformedBoundingBox();
    irr::core::vector3df bbSize = bb.getExtent();                 // world metres
    irr::f32 shipHeight = bbSize.Y;   if (shipHeight < 2.0f) { shipHeight = 2.0f; }
    irr::f32 shipXZ = (bbSize.X > bbSize.Z) ? bbSize.X : bbSize.Z; if (shipXZ < 4.0f) { shipXZ = 4.0f; }

    irr::f32 hitRadiusWorld = 0.20f * shipXZ;                     // footprint ~ hull size
    irr::f32 originY = shipNode->getAbsolutePosition().Y;
    //fire position Nudge between 0.10 and 0.22 to sit it exactly on the working deck.
    irr::f32 deckWorldY = bb.MinEdge.Y + 0.20f * shipHeight;  // was 0.45f — sit at the base/deck
    irr::f32 aboveOriginW = deckWorldY - originY;

    irr::f32 s = shipNode->getScale().X;
    if (s <= 0.0f) { s = 1.0f; }
    fireMountNode = smgr->addEmptySceneNode(shipNode);
    fireMountNode->setScale(irr::core::vector3df(1.0f / s, 1.0f / s, 1.0f / s));
    fireMountNode->setPosition(irr::core::vector3df(0.0f, aboveOriginW / s, 0.0f));
    //FIRE IGNITE 

    fire.ignite(smgr, driver, fireMountNode, irr::core::vector3df(0, 0, 0), hitRadiusWorld,
        // Kyara FIRE: half-extents of the burnable deck, in world metres. Slightly inside the
        // full hull (bow/stern taper), and Fire::ignite insets these further by the flame
        // particle radius, so the blaze stays aboard instead of spilling onto the water.
        0.45f * otherShips.getLength(best),      // fore-aft
        0.40f * otherShips.getBreadth(best));    // port-stbd
    burningShipIndex = best;
    fireElapsed = 0.0f; casualtySinking = false;       // Kyara FIRE (escalation)

    otherShips.setCasualty(best, true); // Inc 3: lose way, drift, list
    beginDistressComms(best);            // Inc 3 (comms): trainee receives the casualty's MAYDAY
    sound->setVolumeVhf(1.0f);
    // FIRE FEATURE sound: one-shot alarm now, start the looping burn bed
    sound->setVolumeFireAlarm(1.0f);     // general alarm: loops until she sinks
    sound->triggerExplosion();           // initial blast as the fire starts
    sound->setVolumeFireBurning(0.7f);   // crackle
    sound->setVolumeGroan(0.25f);        // metal groan (grows with escalation)
    fireElapsed = 0.0f; casualtySinking = false; mobDropped = false;
    deactivateSarHelicopters(); clearAbandonScene(); abandonSpawned = false; sarInit = false;
    abortRescueRun();
}

void SimulationModel::extinguishAllFires()
{
    fire.remove();
    if (fireMountNode) { fireMountNode->remove(); fireMountNode = 0; }
    if (burningShipIndex >= 0) { otherShips.setCasualty(burningShipIndex, false); }
    burningShipIndex = -1;
    // SAR HELICOPTER FEATURE
    deactivateSarHelicopters();
    clearAbandonScene();
    abandonSpawned = false;
    sarInit = false;
    endDistressComms();                  // Inc 3 (comms): close out the distress log on instructor reset
    sound->setVolumeFireBurning(0.0f); // FIRE FEATURE
    sound->setVolumeWater(0.0f);
    sound->setVolumeVhf(0.0f);
}
// Inc 3 (comms) ------------------------------------------------------------


// SAR HELICOPTER FEATURE ---------------------------------------------------

irr::scene::ISceneNode* SimulationModel::spawnModelNode(const std::string& folderName, irr::core::vector3df worldPos, irr::f32 extraScale)
{
    std::string userFolder = Utilities::getUserDir();
    std::string tries[3] = { "Models/Othership/" + folderName + "/",
                             "Models/" + folderName + "/",
                             "Models/Ownship/" + folderName + "/" };
    std::string basePath;
    for (int b = 0; b < 3; b++) {
        if (Utilities::pathExists(userFolder + tries[b])) { basePath = userFolder + tries[b]; break; }
        if (Utilities::pathExists(tries[b])) { basePath = tries[b]; break; }
    }
    if (basePath.empty()) { return 0; }

    // Some models (ManOverboard, rafts) use "<FolderName>.ini" instead of "boat.ini".
    std::string iniFilename = basePath + "boat.ini";
    if (!Utilities::pathExists(iniFilename)) { iniFilename = basePath + folderName + ".ini"; }
    std::string fileName = IniFile::iniFileToString(iniFilename, "FileName", folderName + ".x");
    irr::f32 scaleFactor = IniFile::iniFileTof32(iniFilename, "Scalefactor", 1.0f);
    std::string fullPath = basePath + fileName;

    irr::scene::IAnimatedMesh* mesh = smgr->getMesh(fullPath.c_str());
    if (!mesh) {
        if (device) { device->getLogger()->log("SAR spawn: failed to load model:"); device->getLogger()->log(fullPath.c_str()); }
        return 0;
    }
    irr::scene::ISceneNode* node = smgr->addAnimatedMeshSceneNode(mesh, 0, -1);
    if (!node) { return 0; }
    node->setPosition(worldPos);
    irr::f32 s = scaleFactor * extraScale;
    node->setScale(irr::core::vector3df(s, s, s));
    node->setMaterialFlag(irr::video::EMF_FOG_ENABLE, true);
    node->setMaterialFlag(irr::video::EMF_NORMALIZE_NORMALS, true);
    return node;
}

// SCENARIO INCENDIE: search station of helo k over the datum. Two helos sit either side of it
// as before; any more are fanned out between those two bearings.
static irr::f32 heloStationAngle(size_t k, size_t count)
{
    if (count < 2) { return 0.7f; }
    return 0.7f - 1.4f * (irr::f32)k / (irr::f32)(count - 1);
}

irr::core::vector3df SimulationModel::heloStation(size_t k) const
{
    irr::f32 ang = heloStationAngle(k, incident.helos.size());
    const irr::f32 off = 55.0f;
    return irr::core::vector3df(sarDatum.X + off * std::sin(ang),
        sarBaseY + kHeloHoverAlt,
        sarDatum.Z + off * std::cos(ang));
}

// Longest base -> station leg at the configured speed: the helos lift off this long before
// they are due, so the furthest one still arrives on time.
irr::f32 SimulationModel::heloInboundFlightSecs() const
{
    irr::f32 speed = incident.heloSpeedKts * KTS_TO_MPS;
    if (speed <= 0.0f) { return 0.0f; }
    irr::f32 longest = 0.0f;
    for (size_t k = 0; k < incident.helos.size(); k++) {
        const IncidentHelo& h = incident.helos[k];
        if (!h.hasBase) { continue; }
        irr::core::vector3df st = heloStation(k);
        irr::f32 dx = st.X - longToSceneX((irr::f32)h.base.lon);
        irr::f32 dz = st.Z - latToSceneZ((irr::f32)h.base.lat);
        irr::f32 t = std::sqrt(dx * dx + dz * dz) / speed;
        if (t > longest) { longest = t; }
    }
    return longest;
}

// Where helo k lands once the operation is over: her pad, else her base. False = neither set,
// she just flies off scene.
bool SimulationModel::heloPadPosition(size_t k, irr::core::vector3df& pad) const
{
    if (k >= incident.helos.size()) { return false; }
    const IncidentHelo& h = incident.helos[k];
    if (h.hasPad) {
        pad = irr::core::vector3df(longToSceneX((irr::f32)h.pad.lon), sarBaseY + h.padHeight, latToSceneZ((irr::f32)h.pad.lat));
        return true;
    }
    if (h.hasBase) {
        pad = irr::core::vector3df(longToSceneX((irr::f32)h.base.lon), sarBaseY + h.baseHeight, latToSceneZ((irr::f32)h.base.lat));
        return true;
    }
    return false;
}

// The trainee has made the OSC call (3rd Ctrl+A). With no delay set the helicopters are on
// scene at once, as before; otherwise they arrive incident.heloDelay seconds later.
void SimulationModel::callSarHelicopters()
{
    if (sarActive || heloCalled || burningShipIndex < 0) { return; }
    heloCalled = true;
    heloCallElapsed = 0.0f;
    sarOnScene = false;
    // Kyara SAR: freeze the hover altitude to the casualty's deck level NOW (she's still
    // floating here). All vertical placement uses sarBaseY, never the live casualty Y - so when
    // the hull founders the helos keep station overhead instead of riding it under.
    sarDatum = otherShips.getPosition(burningShipIndex);
    sarDatumSet = true;
    sarBaseY = sarDatum.Y;

    if (incident.heloDelay <= 0.0f) {
        activateSarHelicopters(false);
        return;
    }
    int secs = (int)(incident.heloDelay + 0.5f);
    wchar_t line[128];
    swprintf(line, 128, L"H\u00E9licopt\u00E8res SAR en route - sur zone dans %02d:%02d", secs / 60, secs % 60);
    pushComms(line);
}

void SimulationModel::updateHeloCall(irr::f32 deltaTime)
{
    if (!heloCalled || sarOnScene || sarDeparting) { return; }
    if (burningShipIndex >= 0) { sarDatum = otherShips.getPosition(burningShipIndex); }   // follow her drift while she floats
    heloCallElapsed += deltaTime;
    irr::f32 remaining = incident.heloDelay - heloCallElapsed;

    if (!sarActive) {
        if (remaining > heloInboundFlightSecs()) { return; }   // not time to lift off yet
        activateSarHelicopters(true);
        if (!sarActive) { heloCalled = false; return; }        // nothing could be spawned
    }

    bool allThere = true;
    irr::f32 nearest = 1.0e9f;
    irr::core::vector3df ownPos = ownShip.getPosition();
    for (size_t k = 0; k < sarNodes.size(); k++) {
        if (k >= heloInbound.size() || !heloInbound[k]) { continue; }
        irr::scene::ISceneNode* node = sarNodes[k];
        if (!node) { heloInbound[k] = false; continue; }
        irr::core::vector3df st = heloStation(k);

        if (!node->isVisible()) {
            // No base set: she comes into sight on station when due.
            node->setPosition(st);
            if (remaining <= 0.0f) {
                node->setVisible(true);
                if (k < sarLights.size() && sarLights[k]) { sarLights[k]->setVisible(true); }
                heloInbound[k] = false;
            }
            else { allThere = false; }
            continue;
        }

        // Close the remaining distance in the remaining time, so she is on station exactly
        // when the delay runs out.
        irr::core::vector3df p = node->getPosition();
        irr::core::vector3df d = st - p;
        if (remaining <= deltaTime || d.getLength() < 1.0f) {
            p = st;
            heloInbound[k] = false;
        }
        else {
            p += d * (deltaTime / remaining);
            node->setRotation(irr::core::vector3df(0.0f, std::atan2(d.X, d.Z) * irr::core::RADTODEG + kHeloModelYawOffset, 0.0f));
            allThere = false;
        }
        node->setPosition(p);
        if (k < sarLights.size() && sarLights[k]) { sarLights[k]->setPosition(p + irr::core::vector3df(0, -6.0f, 0)); }
        irr::f32 ox = p.X - ownPos.X, oz = p.Z - ownPos.Z;
        irr::f32 dOwn = std::sqrt(ox * ox + oz * oz);
        if (dOwn < nearest) { nearest = dOwn; }
    }

    if (sound) {
        // Rotor noise builds as they close: full inside 400 m, silent beyond 3 km.
        irr::f32 vol = 1.0f - (nearest - 400.0f) / 2600.0f;
        if (vol > 1.0f) { vol = 1.0f; }
        if (vol < 0.0f) { vol = 0.0f; }
        sound->setVolumeHelo(allThere ? 1.0f : vol);
    }

    if (allThere) {
        sarOnScene = true;
        pushComms(L"H\u00E9licopt\u00E8res SAR sur zone");
        if (abandonComplete && !heloRunActive) { beginHeloRun(); }   // survivors already waiting
    }
}

void SimulationModel::activateSarHelicopters(bool inbound)
{
    if (sarActive || !sarDatumSet) { return; }

    irr::core::vector3df cpos = sarDatum;
    size_t count = incident.helos.size();
    heloInbound.assign(count, false);

    for (size_t hIdx = 0; hIdx < count; hIdx++) {
        const IncidentHelo& cfg = incident.helos[hIdx];
        irr::core::vector3df hpos = heloStation(hIdx);
        bool hidden = false;
        if (inbound) {
            heloInbound[hIdx] = true;
            if (cfg.hasBase) {
                hpos = irr::core::vector3df(longToSceneX((irr::f32)cfg.base.lon), sarBaseY + cfg.baseHeight, latToSceneZ((irr::f32)cfg.base.lat));
            }
            else {
                hidden = true;   // no base: appears on station when due
            }
        }

        irr::scene::ISceneNode* node = spawnModelNode(cfg.model, hpos, 1.6f);
        if (node) {
            node->setVisible(!hidden);
            if (inbound && !hidden) {
                irr::core::vector3df st = heloStation(hIdx);
                node->setRotation(irr::core::vector3df(0.0f, std::atan2(st.X - hpos.X, st.Z - hpos.Z) * irr::core::RADTODEG + kHeloModelYawOffset, 0.0f));
            }
        }
        sarNodes.push_back(node);   // may be 0 if the model is missing: every user null-checks, and indices stay aligned

        // Downward SPOT light on the casualty deck (point lights barely register here).
        irr::core::vector3df lightPos = inbound ? hpos + irr::core::vector3df(0, -6.0f, 0)
            : irr::core::vector3df(cpos.X, sarBaseY + 26.0f, cpos.Z);
        irr::scene::ILightSceneNode* L =
            smgr->addLightSceneNode(0, lightPos,
                irr::video::SColorf(1.0f, 1.0f, 0.95f), 340.0f);   // was 0.85,0.85,0.75 / 200.0f
        if (L) {
            L->setLightType(irr::video::ELT_SPOT);
            irr::video::SLight& ld = L->getLightData();
            ld.Direction = irr::core::vector3df(0.0f, -1.0f, 0.0f);
            ld.InnerCone = 10.0f; ld.OuterCone = 26.0f; ld.Falloff = 3.0f;
            ld.DiffuseColor = irr::video::SColorf(1.0f, 1.0f, 0.90f);
            L->setVisible(!hidden);
        }
        sarLights.push_back(L);

        // Visible searchlight beam: additive cone from the helo down onto the deck.
        const irr::scene::IGeometryCreator* gc = smgr->getGeometryCreator();
        // Additive blending ADDS rgb, so a near-white cone saturates into a solid slab.
        // Keep rgb low: a faint haze shaft that reads as light, not geometry.
        irr::scene::IMesh* coneMesh = gc->createConeMesh(1.0f, 1.0f, 20,
            irr::video::SColor(255, 60, 58, 45), irr::video::SColor(255, 14, 13, 9), 0.0f); // was 34,32,25 / 7,7,5

        irr::scene::ISceneNode* beamPivot = smgr->addEmptySceneNode(0);
        irr::scene::ISceneNode* cone = smgr->addMeshSceneNode(coneMesh, beamPivot);
        coneMesh->drop();
        irr::core::vector3df beamBase(cpos.X, sarBaseY + 4.0f, cpos.Z);   // deck end (wide)
        irr::f32 beamLen = beamBase.getDistanceFrom(hpos);
        cone->setRotation(irr::core::vector3df(90.0f, 0.0f, 0.0f));      // cone +Y -> pivot +Z
        cone->setScale(irr::core::vector3df(4.5f, beamLen, 4.5f));       // narrow shaft, not a slab
        cone->setMaterialFlag(irr::video::EMF_LIGHTING, false);
        cone->setMaterialFlag(irr::video::EMF_ZWRITE_ENABLE, false);
        cone->setMaterialType(irr::video::EMT_TRANSPARENT_ADD_COLOR);
        beamPivot->setPosition(beamBase);
        beamPivot->setRotation((hpos - beamBase).getHorizontalAngle());
        beamPivot->setVisible(!inbound && light.getLightLevel() < 160);   // a shaft only reads as light at dusk/night
        sarBeams.push_back(beamPivot);
    }

    sarActive = !sarNodes.empty();
    if (sarActive && sound) { sound->setVolumeHelo(inbound ? 0.0f : 1.0f); }   // inbound: faded in by updateHeloCall
    if (sarActive && !inbound) {
        sarOnScene = true;
        if (abandonComplete && !heloRunActive) { beginHeloRun(); }   // survivors already waiting
    }
}


void SimulationModel::updateSarHelicopters()
{
    if (!sarActive) { return; }
    if (sarDeparting) {   // now: recover to the pad and land
        irr::f32 speed = incident.heloSpeedKts * KTS_TO_MPS;
        for (size_t k = 0; k < sarNodes.size(); k++) {
            if (!sarNodes[k] || !sarNodes[k]->isVisible()) { continue; }   // never came into sight / already gone
            irr::core::vector3df p = sarNodes[k]->getPosition();

            // Pad per helo from the scenario (built-in preset: incidentBuiltInPreset()).
            irr::core::vector3df pad;
            bool landing = heloPadPosition(k, pad);
            if (!landing) {
                // Nowhere to land: fly 4 km out from the datum and drop out of sight.
                irr::core::vector3df away(p.X - sarDatum.X, 0.0f, p.Z - sarDatum.Z);
                if (away.getLength() < 1.0f) { away = irr::core::vector3df(0, 0, 1.0f); }
                away.normalize();
                pad = irr::core::vector3df(sarDatum.X + away.X * 4000.0f, p.Y, sarDatum.Z + away.Z * 4000.0f);
            }

            irr::f32 dx = pad.X - p.X, dz = pad.Z - p.Z;
            irr::f32 horiz = std::sqrt(dx * dx + dz * dz);

            if (horiz > 6.0f) {
                // still inbound: hold height, fly toward the pad
                irr::f32 step = speed * deltaTime;
                if (step > horiz) { step = horiz; }
                p.X += (dx / horiz) * step;
                p.Z += (dz / horiz) * step;
                irr::f32 crs = std::atan2(dx, dz) * irr::core::RADTODEG;
                sarNodes[k]->setRotation(irr::core::vector3df(0.0f, crs + kHeloModelYawOffset, 0.0f));
            }
            else if (!landing) {
                sarNodes[k]->setVisible(false);
                if (k < sarLights.size() && sarLights[k]) { sarLights[k]->setVisible(false); }
            }
            else {
                // over the pad: settle straight down onto the deck, then park
                p.X = pad.X; p.Z = pad.Z;
                if (p.Y > pad.Y) {
                    p.Y -= kHeloDescendRate * deltaTime;
                    if (p.Y < pad.Y) { p.Y = pad.Y; }
                }
            }
            sarNodes[k]->setPosition(p);
            if (k < sarBeams.size() && sarBeams[k]) { sarBeams[k]->setVisible(false); }
            if (k < sarLights.size() && sarLights[k]) { sarLights[k]->setPosition(p); }
        }
        return;   // helos stay parked on the quay - do not deactivate or clear
    }
    if (!sarOnScene) { return; }   // still flying in: updateHeloCall moves them
    if (burningShipIndex >= 0) { sarDatum = otherShips.getPosition(burningShipIndex); sarDatumSet = true; }
    if (!sarDatumSet) { return; }
    irr::core::vector3df cpos = sarDatum;

    // While the winch run owns the helos, only push their scene nodes to the winch positions
    // and keep the beams pointing down from each; skip the search sweep.
    if (heloRunActive) {
        for (size_t k = 0; k < sarNodes.size() && k < heloUnits.size(); k++) {
            if (sarNodes[k]) {
                sarNodes[k]->setPosition(heloUnits[k].pos);
                sarNodes[k]->setRotation(irr::core::vector3df(0.0f, heloUnits[k].yaw + kHeloModelYawOffset, 0.0f));
            }
            if (k < sarLights.size() && sarLights[k]) {
                sarLights[k]->setPosition(heloUnits[k].pos + irr::core::vector3df(0, -6.0f, 0));
            }
            if (k < sarBeams.size() && sarBeams[k]) {
                irr::core::vector3df bb(heloUnits[k].pos.X, sarBaseY + 2.0f, heloUnits[k].pos.Z);
                sarBeams[k]->setPosition(bb);
                sarBeams[k]->setRotation((heloUnits[k].pos - bb).getHorizontalAngle());
                const irr::core::list<irr::scene::ISceneNode*>& kids = sarBeams[k]->getChildren();
                if (!kids.empty()) {
                    (*kids.begin())->setScale(irr::core::vector3df(4.5f, bb.getDistanceFrom(heloUnits[k].pos), 4.5f));
                }
                sarBeams[k]->setVisible(light.getLightLevel() < 120);
            }
        }
        return;
    }

    // --- pre-winch search sweep ---------------------------------------------------------
    sarSweepPhase += deltaTime * 0.55f;
    const irr::f32 sweepAmp = 60.0f;
    for (size_t k = 0; k < sarNodes.size(); k++) {
        irr::f32 ang = heloStationAngle(k, sarNodes.size());
        irr::core::vector3df hp = heloStation(k);
        if (sarNodes[k]) { sarNodes[k]->setPosition(hp); }
        irr::f32 sweep = std::sin(sarSweepPhase + (irr::f32)k * 2.3f);
        irr::f32 gx = cpos.X + sweepAmp * sweep * std::cos(ang);
        irr::f32 gz = cpos.Z - sweepAmp * sweep * std::sin(ang);
        if (k < sarLights.size() && sarLights[k]) {
            sarLights[k]->setPosition(irr::core::vector3df(gx, sarBaseY + 22.0f, gz));
        }
        if (k < sarBeams.size() && sarBeams[k]) {
            irr::core::vector3df bb(gx, sarBaseY + 4.0f, gz);
            sarBeams[k]->setPosition(bb);
            sarBeams[k]->setRotation((hp - bb).getHorizontalAngle());
            const irr::core::list<irr::scene::ISceneNode*>& kids = sarBeams[k]->getChildren();
            if (!kids.empty()) {
                (*kids.begin())->setScale(irr::core::vector3df(4.5f, bb.getDistanceFrom(hp), 4.5f));
            }
            sarBeams[k]->setVisible(light.getLightLevel() < 120);
        }
    }
}


void SimulationModel::deactivateSarHelicopters()
{
    for (size_t k = 0; k < sarLights.size(); k++) { if (sarLights[k]) { sarLights[k]->remove(); } }
    for (size_t k = 0; k < sarNodes.size(); k++) { if (sarNodes[k]) { sarNodes[k]->remove(); } }
    for (size_t k = 0; k < sarBeams.size(); k++) { if (sarBeams[k]) { sarBeams[k]->remove(); } }
    sarLights.clear();
    sarNodes.clear();
    sarBeams.clear();
    sarActive = false;
    // SCENARIO INCENDIE: a hard reset also cancels a pending call and the winch run, so the
    // next exercise starts clean (previously sarDeparting stayed set after a first run).
    heloCalled = false;
    heloCallElapsed = 0.0f;
    sarOnScene = false;
    sarDeparting = false;
    heloInbound.clear();
    heloRunActive = false;
    for (size_t k = 0; k < heloUnits.size(); k++) { if (heloUnits[k].cable) { heloUnits[k].cable->remove(); } }
    heloUnits.clear();
    heloRescuedCount = 0;
    if (sound) { sound->setVolumeHelo(0.0f); }
}

// Send the helicopters off scene under their own power, then remove them. deactivateSar-
// Helicopters() stays the hard cut, used by instructor resets.
void SimulationModel::departSarHelicopters()
{
    if (heloCalled && !sarActive) {   // stood down before they lifted off
        heloCalled = false;
        pushComms(L"H\u00E9licopt\u00E8res SAR annul\u00E9s");
        return;
    }
    if (!sarActive || sarDeparting) { return; }
    sarDeparting = true;
    sarDepartTimer = 0.0f;
    heloRunActive = false;
    for (size_t k = 0; k < heloUnits.size(); k++) {
        if (heloUnits[k].cable) { heloUnits[k].cable->remove(); heloUnits[k].cable = 0; }
        heloUnits[k].state = Helo_Depart;
    }
    pushComms(L"H\u00E9licopt\u00E8res SAR quittent la zone");
}

void SimulationModel::spawnAbandonScene()
{
    if (burningShipIndex < 0) { return; }
    abandonCentre = otherShips.getPosition(burningShipIndex);
    // The editor places the survivors around the casualty where she starts; if she has drifted
    // since, they go into the water the same distance downstream, still around her.
    abandonDriftX = abandonDriftZ = 0.0f;
    if (casualtyStartKnown && burningShipIndex == incident.casualtyShip - 1) {
        abandonDriftX = abandonCentre.X - longToSceneX((irr::f32)casualtyStart.lon);
        abandonDriftZ = abandonCentre.Z - latToSceneZ((irr::f32)casualtyStart.lat);
    }
    abandonSpawning = true;
    abandonComplete = false;
    abandonSpawnStep = 0;
    abandonSpawnTimer = 0.0f;   // first node drops on the next frame
    abandonMobTotal = 0;
    pushComms(L"Abandon du navire - mise \u00E0 l'eau en cours");
}

// Drop node number abandonSpawnStep. With an incident.ini: the editor's survivors, in order, at
// their own positions. Built-in preset: steps 0..4 = MOB in a ring, 5 = Radeau, 6 = Liferaft.
void SimulationModel::spawnAbandonStep()
{
    if (incidentFromFile) {
        int total = (int)incident.survivors.size();
        if (abandonSpawnStep < total) {
            const IncidentSurvivor& sv = incident.survivors[abandonSpawnStep];
            irr::f32 px = longToSceneX((irr::f32)sv.pos.lon) + abandonDriftX;
            irr::f32 pz = latToSceneZ((irr::f32)sv.pos.lat) + abandonDriftZ;
            irr::f32 py = tideHeight + getWaveHeight(px, pz);
            irr::scene::ISceneNode* n = spawnModelNode(IncidentConfig::survivorModel(sv.kind), irr::core::vector3df(px, py, pz), 1.0f);
            if (n) {
                bool isMob = (sv.kind == Survivor_MOB);
                // A raft assigned to a boat that is not in the scenario goes to whichever boat is nearest.
                int boat = 0;
                for (size_t b = 0; b < rescueBoats.size() && !isMob; b++) {
                    if (rescueBoats[b].number == sv.boat) { boat = sv.boat; }
                }
                abandonNodes.push_back(n);
                abandonKind.push_back(isMob ? Ab_MOB : Ab_Raft);
                abandonBoat.push_back(boat);
                if (isMob) { abandonMobTotal++; }
            }
        }
        abandonSpawnStep++;
        if (abandonSpawnStep >= total) {
            abandonSpawning = false;
            abandonComplete = true;
            pushComms(L"Tous les naufrag\u00E9s et radeaux \u00E0 l'eau - moyens SAR engag\u00E9s");
            beginHeloRun();      // air recovers the swimmers (once the helos are on scene)
            beginRescueRun();    // boats recover the rafts
        }
        return;
    }

    const int numMOB = 5;
    irr::core::vector3df cc = abandonCentre;
    irr::f32 ringR = 0.75f * ((burningShipIndex >= 0) ? otherShips.getLength(burningShipIndex) : 40.0f) + 14.0f;

    if (abandonSpawnStep < numMOB) {
        int k = abandonSpawnStep;
        irr::f32 a = (irr::f32)k / (irr::f32)numMOB * 6.2832f;
        irr::f32 r = ringR + (irr::f32)(k % 2) * 4.0f;
        irr::f32 px = cc.X + r * std::sin(a), pz = cc.Z + r * std::cos(a);
        irr::f32 py = tideHeight + getWaveHeight(px, pz);
        irr::scene::ISceneNode* n = spawnModelNode("ManOverboard", irr::core::vector3df(px, py, pz), 1.0f);
        if (n) { abandonNodes.push_back(n); abandonKind.push_back(Ab_MOB); abandonBoat.push_back(0); abandonMobTotal++; }
    }
    else if (abandonSpawnStep == numMOB) {
        irr::f32 rx = cc.X - ringR * 1.25f, rz = cc.Z - 8.0f;
        irr::scene::ISceneNode* r2 = spawnModelNode("Radeau_Sauvetage",
            irr::core::vector3df(rx, tideHeight + getWaveHeight(rx, rz), rz), 1.0f);
        if (r2) { abandonNodes.push_back(r2); abandonKind.push_back(Ab_Raft); abandonBoat.push_back(0); }
    }
    else if (abandonSpawnStep == numMOB + 1) {
        irr::f32 rx = cc.X + ringR * 1.25f, rz = cc.Z + 8.0f;
        irr::scene::ISceneNode* r1 = spawnModelNode("Liferaft",
            irr::core::vector3df(rx, tideHeight + getWaveHeight(rx, rz), rz), 1.0f);
        if (r1) { abandonNodes.push_back(r1); abandonKind.push_back(Ab_Raft); abandonBoat.push_back(0); }
    }
    abandonSpawnStep++;

    // Sequence finished once the liferaft (last step) is down.
    if (abandonSpawnStep > numMOB + 1) {
        abandonSpawning = false;
        abandonComplete = true;
        pushComms(L"Tous les naufrag\u00E9s et radeaux \u00E0 l'eau - moyens SAR engag\u00E9s");
        beginHeloRun();      // air recovers the swimmers
        beginRescueRun();    // boat recovers the rafts
    }
}

void SimulationModel::updateAbandonSpawn(irr::f32 deltaTime)
{
    if (!abandonSpawning || deltaTime <= 0.0f) { return; }
    abandonSpawnTimer -= deltaTime;
    if (abandonSpawnTimer <= 0.0f) {
        spawnAbandonStep();
        abandonSpawnTimer = incident.survivorInterval;
    }
}

void SimulationModel::clearAbandonScene()
{
    for (size_t k = 0; k < abandonNodes.size(); k++) { if (abandonNodes[k]) { abandonNodes[k]->remove(); } }
    abandonNodes.clear();
    abandonKind.clear();
    abandonBoat.clear();
    abandonSpawned = false;
    abandonSpawning = false;
    abandonComplete = false;
    abandonSpawnStep = 0;
    abandonMobTotal = 0;
    for (size_t b = 0; b < rescueBoats.size(); b++) {
        for (size_t i = 0; i < rescueBoats[b].carried.size(); i++) { if (rescueBoats[b].carried[i]) { rescueBoats[b].carried[i]->remove(); } }
        rescueBoats[b].carried.clear();
    }
}
void SimulationModel::beginHeloRun()
{
    if (!sarActive || !sarOnScene) { return; }  // helos must be on scene (Ctrl+A step 2, plus any delay)
    if (heloUnits.empty()) {
        // Build one HeloUnit per spawned helicopter node.
        for (size_t k = 0; k < sarNodes.size(); k++) {
            HeloUnit u;
            u.state = Helo_Search; u.targetNode = -1; u.cableLen = 0.0f; u.phase = 0.0f;
            u.pos = sarNodes[k] ? sarNodes[k]->getPosition()
                : irr::core::vector3df(sarDatum.X, sarBaseY + kHeloHoverAlt, sarDatum.Z);
            u.cable = 0;
            u.yaw = 0.0f;
            heloUnits.push_back(u);
        }
    }
    if (heloUnits.empty()) { return; }

    // All MOB nodes still afloat.
    std::vector<int> mobs;
    for (size_t k = 0; k < abandonNodes.size(); k++) {
        if (abandonNodes[k] && k < abandonKind.size() && abandonKind[k] == Ab_MOB) {
            mobs.push_back((int)k);
        }
    }

    // Split nearest-first between the two helos, alternating so the load is even (≈3/2).
    for (size_t h = 0; h < heloUnits.size(); h++) { heloUnits[h].queue.clear(); }
    irr::core::vector3df from0 = heloUnits[0].pos;
    // Order MOBs by distance from helo 0, then deal them out round-robin.
    std::sort(mobs.begin(), mobs.end(), [&](int a, int b) {
        irr::core::vector3df pa = abandonNodes[a]->getAbsolutePosition();
        irr::core::vector3df pb = abandonNodes[b]->getAbsolutePosition();
        irr::f32 da = (pa.X - from0.X) * (pa.X - from0.X) + (pa.Z - from0.Z) * (pa.Z - from0.Z);
        irr::f32 db = (pb.X - from0.X) * (pb.X - from0.X) + (pb.Z - from0.Z) * (pb.Z - from0.Z);
        return da < db;
        });
    for (size_t i = 0; i < mobs.size(); i++) {
        heloUnits[i % heloUnits.size()].queue.push_back(mobs[i]);
        // Each helo re-orders its share as a nearest-neighbour chain from its own position, so
// the flight reads as a sweep forward rather than darting back and forth.
        for (size_t h = 0; h < heloUnits.size(); h++) {
            HeloUnit& u = heloUnits[h];
            std::vector<int> chain;
            irr::core::vector3df from = u.pos;
            while (!u.queue.empty()) {
                size_t bi = 0; irr::f32 bd = 1.0e12f;
                for (size_t i = 0; i < u.queue.size(); i++) {
                    irr::core::vector3df p = abandonNodes[u.queue[i]]->getAbsolutePosition();
                    irr::f32 d = (p.X - from.X) * (p.X - from.X) + (p.Z - from.Z) * (p.Z - from.Z);
                    if (d < bd) { bd = d; bi = i; }
                }
                chain.push_back(u.queue[bi]);
                from = abandonNodes[u.queue[bi]]->getAbsolutePosition();
                u.queue.erase(u.queue.begin() + bi);
            }
            u.queue = chain;
            u.yaw = sarNodes.size() > h && sarNodes[h] ? sarNodes[h]->getRotation().Y : 0.0f;
        }
    }
    heloRunActive = true;
}

void SimulationModel::setHeloCable(size_t k, irr::core::vector3df top, irr::f32 length)
{
    if (k >= heloUnits.size()) { return; }
    HeloUnit& u = heloUnits[k];
    if (length <= 0.1f) {
        if (u.cable) { u.cable->setVisible(false); }
        return;
    }
    if (!u.cable) {
        u.cable = smgr->addCubeSceneNode(1.0f, 0, -1);
        if (u.cable) {
            u.cable->setMaterialFlag(irr::video::EMF_LIGHTING, false);
            u.cable->setMaterialFlag(irr::video::EMF_FOG_ENABLE, true);
        }
    }
    if (!u.cable) { return; }
    u.cable->setVisible(true);
    // Cube is 1 m; scale thin in X/Z, to 'length' in Y, and drop its centre half-way down.
    u.cable->setScale(irr::core::vector3df(0.25f, length, 0.25f));
    u.cable->setPosition(irr::core::vector3df(top.X, top.Y - length * 0.5f, top.Z));
}


void SimulationModel::updateHeloRun(irr::f32 deltaTime)
{
    if (!heloRunActive || deltaTime <= 0.0f) { return; }

    for (size_t k = 0; k < heloUnits.size(); k++) {
        HeloUnit& u = heloUnits[k];

        // Pull the next MOB off the queue when idle.
        if (u.state == Helo_Search && u.targetNode < 0) {
            while (!u.queue.empty()) {
                int cand = u.queue.front(); u.queue.erase(u.queue.begin());
                if (cand >= 0 && cand < (int)abandonNodes.size() && abandonNodes[cand]) {
                    u.targetNode = cand; break;
                }
            }
        }

        // No target and empty queue -> this helo is finished with pickups.
        if (u.targetNode < 0 && u.queue.empty() && u.state == Helo_Search) {
            u.state = Helo_Done;
        }

        switch (u.state) {
        case Helo_Search: {
            irr::core::vector3df tp = abandonNodes[u.targetNode]->getAbsolutePosition();
            irr::core::vector3df want(tp.X, sarBaseY + kHeloHoverAlt, tp.Z);
            irr::core::vector3df d = want - u.pos;
            irr::f32 horiz = std::sqrt(d.X * d.X + d.Z * d.Z);

            // Nose toward the target, rate-limited.
            if (horiz > 2.0f) {
                irr::f32 wantYaw = std::atan2(d.X, d.Z) * irr::core::RADTODEG;
                irr::f32 dy = wantYaw - u.yaw;
                while (dy > 180.0f) { dy -= 360.0f; }
                while (dy < -180.0f) { dy += 360.0f; }
                irr::f32 maxY = kHeloYawRate * deltaTime;
                if (dy > maxY) { dy = maxY; }
                if (dy < -maxY) { dy = -maxY; }
                u.yaw += dy;
            }

            // Only make way once roughly pointed at the mark - no crabbing backwards.
            irr::f32 travelYaw = std::atan2(d.X, d.Z) * irr::core::RADTODEG;
            irr::f32 off = travelYaw - u.yaw;
            while (off > 180.0f) { off -= 360.0f; }
            while (off < -180.0f) { off += 360.0f; }
            irr::f32 step = kHeloTransitSpd * deltaTime;
            if (std::fabs(off) > 35.0f) { step *= 0.25f; }   // turning: ease off the speed

            if (horiz > step) {
                d.Y = 0; d.normalize();
                u.pos += d * step;
            }
            else {
                u.pos = want;
                u.state = Helo_Descend; u.phase = 0.0f;
            }
            setHeloCable(k, u.pos, 0.0f);
        } break;

        case Helo_Descend: {
            u.phase += deltaTime;
            irr::f32 t = u.phase / kHeloDescendSecs; if (t > 1.0f) t = 1.0f;
            u.cableLen = (kHeloHoverAlt - kHeloWinchAlt + kHeloWinchAlt) * t; // pay out to the sea
            // Cable reaches from the helo down to the water surface at full payout.
            irr::f32 payout = (sarBaseY + kHeloHoverAlt) * 0.0f + (kHeloHoverAlt)*t; // = kHeloHoverAlt*t
            setHeloCable(k, u.pos, payout);
            if (t >= 1.0f) { u.state = Helo_Lift; u.phase = 0.0f; }
        } break;

        case Helo_Lift: {
            u.phase += deltaTime;
            irr::f32 t = u.phase / kHeloLiftSecs; if (t > 1.0f) t = 1.0f;
            // Raise the MOB up the cable: interpolate its Y from the sea to the helo.
            if (u.targetNode >= 0 && u.targetNode < (int)abandonNodes.size() && abandonNodes[u.targetNode]) {
                irr::scene::ISceneNode* mob = abandonNodes[u.targetNode];
                irr::core::vector3df mp = mob->getPosition();
                irr::f32 seaY = tideHeight + getWaveHeight(mp.X, mp.Z);
                mp.Y = seaY + (u.pos.Y - seaY) * t;
                mob->setPosition(mp);
            }
            irr::f32 payout = kHeloHoverAlt * (1.0f - t);   // reel in
            setHeloCable(k, u.pos, payout);
            if (t >= 1.0f) {
                // Survivor recovered.
                if (u.targetNode >= 0 && u.targetNode < (int)abandonNodes.size() && abandonNodes[u.targetNode]) {
                    abandonNodes[u.targetNode]->remove();
                    abandonNodes[u.targetNode] = 0;
                }
                heloRescuedCount++;
                wchar_t line[96];
                swprintf(line, 96, L"Naufrag\u00E9 h\u00E9litreuill\u00E9 (%d/%d)", heloRescuedCount, abandonMobTotal);
                pushComms(line);
                u.targetNode = -1;
                setHeloCable(k, u.pos, 0.0f);
                u.state = Helo_Search;   // next in queue, or Done if empty
            }
        } break;

        case Helo_Depart:
        case Helo_Done:
            setHeloCable(k, u.pos, 0.0f);
            break;
        }
    }

    // All helos finished AND the boat has cleared the rafts -> stand everyone down together.
    bool allHelosDone = true;
    for (size_t k = 0; k < heloUnits.size(); k++) {
        if (heloUnits[k].state != Helo_Done) { allHelosDone = false; break; }
    }
    bool waterClear = true;
    for (size_t k = 0; k < abandonNodes.size(); k++) {
        if (abandonNodes[k]) { waterClear = false; break; }
    }
    if (allHelosDone && waterClear) {
        pushComms(L"Op\u00E9ration SAR termin\u00E9e - moyens d\u00E9gag\u00E9s");
        for (size_t k = 0; k < heloUnits.size(); k++) {
            if (heloUnits[k].cable) { heloUnits[k].cable->remove(); heloUnits[k].cable = 0; }
        }
        departSarHelicopters();

    }
}




// SCENARIO INCENDIE: read the scenario's incident.ini (written by the fire scenario editor) and
// set up the rescue craft. Without the file, the built-in DAKHLA preset: one boat found by name,
// fixed homeward route, fixed helicopter pads (incidentBuiltInPreset()).
void SimulationModel::loadIncident(const std::vector<OtherShipData>& shipsData)
{
    std::string userFolder = Utilities::getUserDir();
    std::string scenarioPath = "Scenarios/";   // same lookup as main.cpp
    if (Utilities::pathExists(userFolder + scenarioPath)) { scenarioPath = userFolder + scenarioPath; }
    std::string incidentFile = scenarioPath + scenarioName + "/incident.ini";

    incident = IncidentConfig();
    incidentFromFile = incident.load(incidentFile);
    rescueBoats.clear();
    abandonDriftX = abandonDriftZ = 0.0f;
    casualtyStartKnown = false;
    if (incidentFromFile && incident.casualtyShip >= 1 && incident.casualtyShip <= (int)shipsData.size()) {
        casualtyStartKnown = true;
        casualtyStart = IncidentPoint(shipsData[incident.casualtyShip - 1].initialLat, shipsData[incident.casualtyShip - 1].initialLong);
    }

    if (incidentFromFile) {
        if (device) { device->getLogger()->log("Fire scenario settings loaded from incident.ini"); }
        for (size_t i = 0; i < incident.sarBoats.size(); i++) {
            const IncidentSarBoat& cfg = incident.sarBoats[i];
            int idx = cfg.ship - 1;
            if (idx < 0 || idx >= (int)otherShips.getNumber() || cfg.ship == incident.casualtyShip || isRescueBoat(idx)) {
                if (device) { device->getLogger()->log("SAR boat in incident.ini does not match a usable ship - skipped."); }
                continue;
            }
            addRescueBoat(idx, (int)i + 1, cfg.speedKts, cfg.launchDelay, cfg.moorHeading, cfg.outbound, cfg.inbound);
        }
        return;
    }

    const IncidentBuiltInPreset& preset = incidentBuiltInPreset();
    int idx = otherShips.findByName(preset.rescueBoatName);
    if (idx >= 0) {
        addRescueBoat(idx, 1, preset.boatSpeedKts, 0.0f, preset.boatMoorHeading, std::vector<IncidentPoint>(), preset.boatReturn);
        if (device) { device->getLogger()->log("SAR rescue craft found in scenario."); }
    }
    else if (device) {
        device->getLogger()->log("SAR rescue craft not in scenario - rescue run disabled.");
    }
    for (size_t k = 0; k < incident.helos.size() && k < 2; k++) {
        incident.helos[k].hasPad = true;
        incident.helos[k].pad = preset.heloPad[k];
        incident.helos[k].padHeight = preset.heloPadHeight[k];
    }
}

void SimulationModel::addRescueBoat(int shipIndex, int number, irr::f32 speedKts, irr::f32 launchDelay, irr::f32 moorHeading,
    const std::vector<IncidentPoint>& outbound, const std::vector<IncidentPoint>& homeward)
{
    RescueBoat b;
    b.shipIndex = shipIndex;
    b.number = number;
    b.state = RescueOff;
    irr::core::vector3df bp = otherShips.getPosition(shipIndex);   // where the instructor berthed her
    b.homeX = bp.X; b.homeZ = bp.Z; b.homeHdg = otherShips.getHeading(shipIndex);
    b.x = b.homeX; b.z = b.homeZ; b.hdg = b.homeHdg; b.spd = 0.0f;
    b.speedMps = speedKts * KTS_TO_MPS;
    b.moorHdg = moorHeading;
    b.launchDelay = launchDelay;
    b.launchTimer = 0.0f;
    b.pickupHold = 0.0f;
    b.rescued = 0;
    b.outLatLong = outbound;
    b.homeLatLong = homeward;
    b.outWp = 0;
    b.wp = 0;
    b.prevDist = 1.0e9f;
    b.departRun = 0.0f;
    b.returnStarted = false;
    b.target = -1;
    rescueBoats.push_back(b);
}

bool SimulationModel::isRescueBoat(int shipIndex) const
{
    for (size_t i = 0; i < rescueBoats.size(); i++) {
        if (rescueBoats[i].shipIndex == shipIndex) { return true; }
    }
    return false;
}

// "Vedette de sauvetage" when she is the only one (the original wording), her name otherwise.
std::wstring SimulationModel::rescueLabel(const RescueBoat& b) const
{
    if (rescueBoats.size() <= 1) { return L"Vedette de sauvetage"; }
    return L"Vedette " + widen(otherShips.getName(b.shipIndex));
}

void SimulationModel::beginRescueRun()
{
    if (!abandonSpawned) { return; }

    for (size_t i = 0; i < rescueBoats.size(); i++) {
        RescueBoat& b = rescueBoats[i];
        if (b.state != RescueOff && b.state != RescueMoored) { continue; }

        irr::core::vector3df bp = otherShips.getPosition(b.shipIndex);
        b.x = bp.X; b.z = bp.Z;
        b.hdg = otherShips.getHeading(b.shipIndex);
        b.spd = 0.0f;

        b.target = -1;
        b.rescued = 0;
        b.pickupHold = 0.0f;
        b.prevDist = 1.0e9f;
        b.returnStarted = false;
        b.route.clear();
        b.wp = 0;
        b.outRoute.clear();
        b.outWp = 0;
        for (size_t w = 0; w < b.outLatLong.size(); w++) {
            RescueWaypoint p;
            p.x = longToSceneX((irr::f32)b.outLatLong[w].lon);
            p.z = latToSceneZ((irr::f32)b.outLatLong[w].lat);
            p.nodeIndex = -1;
            b.outRoute.push_back(p);
        }

        b.launchTimer = b.launchDelay;
        if (b.launchTimer > 0.0f) {
            b.state = RescueWaiting;
            int secs = (int)(b.launchTimer + 0.5f);
            wchar_t line[64];
            swprintf(line, 64, L" appareille dans %02d:%02d", secs / 60, secs % 60);
            pushComms(rescueLabel(b) + line);
        }
        else {
            b.state = RescueRunning;
            pushComms(rescueLabel(b) + L" appareille - r\u00E9cup\u00E9ration des radeaux");
        }
    }
}
// Clear the scene on the same course as the helicopters, as a single SAR team.
void SimulationModel::beginRescueDeparture()
{
    for (size_t i = 0; i < rescueBoats.size(); i++) {
        RescueBoat& b = rescueBoats[i];
        if (b.state == RescueOff || b.state == RescueDeparting) { continue; }
        b.state = RescueDeparting;
        b.departRun = 0.0f;
        pushComms(rescueLabel(b) + L" quitte la zone");
    }
}
void SimulationModel::abortRescueRun()
{
    for (size_t i = 0; i < rescueBoats.size(); i++) {
        RescueBoat& b = rescueBoats[i];
        b.state = RescueOff;
        b.outRoute.clear();
        b.outWp = 0;
        b.route.clear();
        b.wp = 0;
        b.rescued = 0;
        b.pickupHold = 0.0f;
        b.launchTimer = 0.0f;
        b.target = -1;
        for (size_t c = 0; c < b.carried.size(); c++) { if (b.carried[c]) { b.carried[c]->remove(); } }
        b.carried.clear();
    }
}

bool SimulationModel::isRescueRunActive() const
{
    for (size_t i = 0; i < rescueBoats.size(); i++) {
        if (rescueBoats[i].state == RescueRunning || rescueBoats[i].state == RescueHolding) { return true; }
    }
    return false;
}

int SimulationModel::getRescuedCount() const
{
    int total = 0;
    for (size_t i = 0; i < rescueBoats.size(); i++) { total += rescueBoats[i].rescued; }
    return total;
}

void SimulationModel::rescueMoveNode(irr::f32 deltaX, irr::f32 deltaZ)
{
    for (size_t i = 0; i < rescueBoats.size(); i++) {
        RescueBoat& b = rescueBoats[i];
        b.homeX += deltaX; b.homeZ += deltaZ;
        b.x += deltaX; b.z += deltaZ;
        for (size_t w = 0; w < b.outRoute.size(); w++) { b.outRoute[w].x += deltaX; b.outRoute[w].z += deltaZ; }
        for (size_t w = 0; w < b.route.size(); w++) { b.route[w].x += deltaX; b.route[w].z += deltaZ; }
    }
}


// Nearest raft still in the water that this boat may take: one assigned to her, or a free one
// no other boat is already working. -1 if there is none.
int SimulationModel::nearestRemainingRaft(const RescueBoat& b, irr::f32 fromX, irr::f32 fromZ)
{
    int best = -1; irr::f32 bestD = 1.0e18f;
    for (size_t k = 0; k < abandonNodes.size(); k++) {
        if (!abandonNodes[k]) { continue; }
        if (k >= abandonKind.size() || abandonKind[k] != Ab_Raft) { continue; }
        int assigned = (k < abandonBoat.size()) ? abandonBoat[k] : 0;
        if (assigned != 0 && assigned != b.number) { continue; }
        if (assigned == 0) {
            bool claimed = false;
            for (size_t o = 0; o < rescueBoats.size(); o++) {
                if (&rescueBoats[o] != &b && rescueBoats[o].target == (int)k) { claimed = true; break; }
            }
            if (claimed) { continue; }
        }
        irr::core::vector3df p = abandonNodes[k]->getAbsolutePosition();
        irr::f32 d = (p.X - fromX) * (p.X - fromX) + (p.Z - fromZ) * (p.Z - fromZ);
        if (d < bestD) { bestD = d; best = (int)k; }
    }
    return best;
}

void SimulationModel::updateRescueRun(irr::f32 deltaTime)
{
    for (size_t i = 0; i < rescueBoats.size(); i++) {
        updateRescueBoat(rescueBoats[i], deltaTime);
    }
}

void SimulationModel::updateRescueBoat(RescueBoat& b, irr::f32 deltaTime)
{
    if (b.state == RescueOff) { return; }

    // --- alongside her berth until her launch delay runs out: her own legs keep her there --
    if (b.state == RescueWaiting) {
        if (deltaTime > 0.0f) { b.launchTimer -= deltaTime; }
        if (b.launchTimer > 0.0f) { return; }
        b.state = RescueRunning;
        pushComms(rescueLabel(b) + L" appareille - r\u00E9cup\u00E9ration des radeaux");
    }

    if (b.state == RescueMoored) {
        otherShips.setScriptedPose(b.shipIndex, b.x, b.z, b.hdg, 0.0f);
        placeCarriedRafts(b);
        return;
    }
    if (deltaTime <= 0.0f) {
        otherShips.setScriptedPose(b.shipIndex, b.x, b.z, b.hdg, b.spd);
        placeCarriedRafts(b);
        return;
    }

    // --- alongside a raft: hold, then take it aboard, then rescan --------------------------
    if (b.state == RescueHolding) {
        b.spd = 0.0f;
        b.pickupHold -= deltaTime;
        if (b.pickupHold <= 0.0f) {
            if (b.target >= 0 && b.target < (int)abandonNodes.size() && abandonNodes[b.target]) {
                b.carried.push_back(abandonNodes[b.target]);
                abandonNodes[b.target] = 0;
                b.rescued++;
                wchar_t line[96];
                swprintf(line, 96, L"Radeau r\u00E9cup\u00E9r\u00E9 \u00E0 bord (%d)", b.rescued);
                pushComms(rescueBoats.size() > 1 ? rescueLabel(b) + L" : " + line : std::wstring(line));
            }
            b.target = -1;
            b.prevDist = 1.0e9f;
            b.state = RescueRunning;
        }
        otherShips.setScriptedPose(b.shipIndex, b.x, b.z, b.hdg, 0.0f);
        placeCarriedRafts(b);
        return;
    }

    // --- running: choose a goal - outbound route, next raft, or the homeward route ---------
    irr::f32 goalX = b.x, goalZ = b.z;
    bool goalIsPickup = false, goalIsBerth = false;
    bool goalIsOutbound = false;

    if (b.outWp < b.outRoute.size()) {
        goalX = b.outRoute[b.outWp].x; goalZ = b.outRoute[b.outWp].z;
        goalIsOutbound = true;
    }
    else if (!b.returnStarted) {
        // Drop a stale target if its node vanished for any reason.
        if (b.target >= 0 && (b.target >= (int)abandonNodes.size() || !abandonNodes[b.target])) {
            b.target = -1;
        }
        if (b.target < 0) {
            b.target = nearestRemainingRaft(b, b.x, b.z);   // rescan every time
        }
        if (b.target >= 0) {
            irr::core::vector3df p = abandonNodes[b.target]->getAbsolutePosition();
            goalX = p.X; goalZ = p.Z; goalIsPickup = true;
        }
        else {
            buildReturnRoute(b);          // her rafts are aboard -> head home, once
            b.returnStarted = true;
        }
    }

    if (b.returnStarted) {
        if (b.wp >= b.route.size()) {      // route exhausted: loiter
            b.spd = 0.0f;
            otherShips.setScriptedPose(b.shipIndex, b.x, b.z, b.hdg, 0.0f);
            placeCarriedRafts(b);
            return;
        }
        const RescueWaypoint& wp = b.route[b.wp];
        goalX = wp.x; goalZ = wp.z;
        goalIsBerth = (wp.nodeIndex == -2);
    }

    // --- arrival --------------------------------------------------------------------------
    irr::f32 dx = goalX - b.x, dz = goalZ - b.z;
    irr::f32 dist = std::sqrt(dx * dx + dz * dz);
    if (dist < kRescueArriveR) {
        b.prevDist = 1.0e9f;
        if (goalIsOutbound) {
            b.outWp++;                   // outbound waypoint reached
        }
        else if (goalIsPickup) {
            b.state = RescueHolding;
            b.pickupHold = kRescuePickupSecs;
            b.spd = 0.0f;
        }
        else if (goalIsBerth) {
            b.x = goalX; b.z = goalZ; b.hdg = (b.moorHdg >= 0.0f) ? b.moorHdg : b.homeHdg; b.spd = 0.0f;
            b.state = RescueMoored;
            for (size_t i = 0; i < b.carried.size(); i++) { if (b.carried[i]) { b.carried[i]->remove(); } }
            b.carried.clear();
            pushComms(rescueLabel(b) + L" \u00E0 quai - radeaux d\u00E9barqu\u00E9s");
        }
        else {
            b.wp++;                      // homeward waypoint reached
        }
        otherShips.setScriptedPose(b.shipIndex, b.x, b.z, b.hdg, b.spd);
        placeCarriedRafts(b);
        return;
    }
    b.prevDist = dist;

    // --- move straight along the leg: no turning arc, no braking curve -------------------
    // Heading snaps to the bearing of the leg and the boat advances exactly along the
    // straight line to the next waypoint, so she follows her drawn route precisely.
    b.hdg = std::atan2(dx, dz) * irr::core::RADTODEG;
    while (b.hdg >= 360.0f) { b.hdg -= 360.0f; }
    while (b.hdg < 0.0f) { b.hdg += 360.0f; }

    b.spd = b.speedMps;
    irr::f32 step = b.spd * deltaTime;
    if (step > dist) { step = dist; }        // never overshoot the mark

    b.x += (dx / dist) * step;
    b.z += (dz / dist) * step;

    otherShips.setScriptedPose(b.shipIndex, b.x, b.z, b.hdg, b.spd);
    placeCarriedRafts(b);
}

// Homeward route. With a drawn return route: walk it, moor on its last point. Without one:
// back along the outbound route, and alongside where she started.
void SimulationModel::buildReturnRoute(RescueBoat& b)
{
    b.route.clear();
    b.wp = 0;

    if (!b.homeLatLong.empty()) {
        const size_t n = b.homeLatLong.size();
        for (size_t i = 0; i < n; i++) {
            RescueWaypoint w;
            w.x = longToSceneX((irr::f32)b.homeLatLong[i].lon);
            w.z = latToSceneZ((irr::f32)b.homeLatLong[i].lat);
            w.nodeIndex = (i == n - 1) ? -2 : -1;  // last = moor, others = pass through
            b.route.push_back(w);
        }
    }
    else {
        for (size_t i = b.outRoute.size(); i > 0; i--) {
            RescueWaypoint w = b.outRoute[i - 1];
            w.nodeIndex = -1;
            b.route.push_back(w);
        }
        RescueWaypoint berth;
        berth.x = b.homeX; berth.z = b.homeZ; berth.nodeIndex = -2;
        b.route.push_back(berth);
    }

    pushComms(rescueLabel(b) + L" - retour au quai");
}
// Rafts riding on the aft deck: re-seated every frame from the boat's live pose, so they
// follow her heading and wave motion without needing to be scene-graph children (which would
// inherit her model scale and complicate re-centring).
void SimulationModel::placeCarriedRafts(RescueBoat& b)
{
    if (b.carried.empty() || b.shipIndex < 0) { return; }
    irr::core::vector3df bp = otherShips.getPosition(b.shipIndex);
    irr::f32 h = otherShips.getHeading(b.shipIndex) * irr::core::DEGTORAD;
    irr::f32 sh = std::sin(h), ch = std::cos(h);
    irr::f32 boatLen = otherShips.getLength(b.shipIndex);
    for (size_t i = 0; i < b.carried.size(); i++) {
        if (!b.carried[i]) { continue; }
        // Deck slots: local (athwart, up, fore-aft). First raft amidships-aft, the next further aft.
        irr::f32 lx = 0.0f;
        irr::f32 ly = 1.4f;
        irr::f32 lz = -0.18f * boatLen - (irr::f32)i * 0.14f * boatLen;
        irr::f32 wx = bp.X + lx * ch + lz * sh;
        irr::f32 wz = bp.Z - lx * sh + lz * ch;
        b.carried[i]->setPosition(irr::core::vector3df(wx, bp.Y + ly, wz));
        b.carried[i]->setRotation(irr::core::vector3df(0.0f, otherShips.getHeading(b.shipIndex), 0.0f));
        b.carried[i]->setScale(irr::core::vector3df(0.5f, 0.5f, 0.5f));
    }
}
void SimulationModel::pushComms(const std::wstring& line)
{
    std::wstring ts = widen(Utilities::timestampToString(absoluteTime)); // trim to HH:MM:SS if too long
    std::wstring full = ts + L"  " + line;

    // Kyara: the comms overlay box has a fixed width, and a long line (e.g. the MAYDAY
    // acknowledgement, which carries both ship names) used to run straight out past its right
    // edge. Wrap on word boundaries here so no single stored line can overflow; continuation
    // lines are indented to stay visually attached to their timestamp.
    const size_t kMaxCols = 64;          // tune to match the overlay's usable width
    const std::wstring indent = L"    ";
    if (full.length() <= kMaxCols) {
        commsLog.push_back(full);
        return;
    }
    std::wstring current;
    bool first = true;
    size_t pos = 0;
    while (pos < full.length()) {
        size_t sp = full.find(L' ', pos);
        std::wstring word = (sp == std::wstring::npos) ? full.substr(pos) : full.substr(pos, sp - pos);
        pos = (sp == std::wstring::npos) ? full.length() : sp + 1;
        if (word.empty()) { continue; }

        std::wstring candidate = current.empty() ? word : current + L" " + word;
        size_t limit = first ? kMaxCols : (kMaxCols - indent.length());
        if (candidate.length() > limit && !current.empty()) {
            commsLog.push_back(first ? current : indent + current);
            first = false;
            current = word;
        }
        else {
            current = candidate;
        }
    }
    if (!current.empty()) { commsLog.push_back(first ? current : indent + current); }
}

void SimulationModel::beginDistressComms(int shipIndex)
{
    casualtyName = otherShips.getName(shipIndex);
    irr::core::vector3df p = otherShips.getPosition(shipIndex);
    std::wstring wname = widen(casualtyName);
    std::wstring wpos = formatLatLong(terrain.zToLat(p.Z), terrain.xToLong(p.X));

    maydayLines.clear();
    maydayLines.push_back(L"MAYDAY MAYDAY MAYDAY");
    maydayLines.push_back(L"ICI " + wname + L", " + wname + L", " + wname);
    maydayLines.push_back(L"MAYDAY " + wname);
    maydayLines.push_back(L"POSITION " + wpos);
    maydayLines.push_back(L"NATURE : INCENDIE \u00C0 BORD");
    maydayLines.push_back(L"ASSISTANCE IMM\u00C9DIATE REQUISE - POB INCONNU");
    maydayLines.push_back(L"\u00C0 L'\u00C9COUTE VHF CANAL 16");

    commsLog.clear();
    commsStep = 0;
    distressActive = true;
    pushComms(L"MAYDAY re\u00E7u de " + wname + L" (VHF 16)");
}

void SimulationModel::advanceComms()
{
    //COMMS 
    if (!distressActive) { return; }
    std::wstring wname = widen(casualtyName);
    // Kyara: identify OUR vessel by name on the VHF, instead of the literal "[VOTRE NAVIRE]"
    // placeholder. Falls back to a generic wording if the scenario gave the own ship no name.
    std::string ownName = ownShip.getName();
    std::wstring wOwn = ownName.empty() ? std::wstring(L"NAVIRE DE SAUVETAGE") : widen(ownName);
    switch (commsStep) {
    case 0: pushComms(L"Accus\u00E9 de r\u00E9ception : MAYDAY " + wname +
        L" - ICI " + wOwn + L" - RE\u00C7U MAYDAY"); break;
    case 1: pushComms(L"MAYDAY RELAY transmis au CROSS / MRCC (VHF 16)"); break;
    case 2:
        pushComms(L"Coordination sur zone assur\u00E9e (OSC) - h\u00E9licopt\u00E8re SAR engag\u00E9");
        callSarHelicopters();   // on scene now, or after the scenario's helicopter delay
        break;
    case 3: pushComms(L"Compte-rendu au MRCC : nature, POB, moyens engag\u00E9s"); break;
    default:
        // SCENARIO INCENDIE: the local centre comes from the scenario (MRSC DAKHLA without incident.ini).
        pushComms(incident.coordinationCentre.empty() ? std::wstring(L"Point de situation transmis au MRCC BOUZNIKA")
            : L"Point de situation transmis au MRCC BOUZNIKA via le " + widen(incident.coordinationCentre));
        break;
    }
    if (commsStep < 4) { commsStep++; }
}

void SimulationModel::endDistressComms()
{
    if (distressActive) {
        pushComms(L"Sinistre ma\u00EEtris\u00E9 - fin de la situation de d\u00E9tresse");
    }
    distressActive = false;
    commsStep = 0;
    maydayLines.clear();
    // commsLog is intentionally kept for the eventual Inc 6 debrief; overlay hides via distressActive.
}


void SimulationModel::setMonitorFiring(bool firing) { fireMonitor.setFiring(firing); }
void SimulationModel::toggleMonitorFiring()
{
    monitorFiringDesired = !monitorFiringDesired;
    fireMonitor.setFiring(monitorFiringDesired);
}
bool SimulationModel::isMonitorFiring() const { return fireMonitor.isFiring(); }
void SimulationModel::setMonitorAimFromRay(irr::core::line3d<irr::f32> ray)
{
    // Aim = where the cursor ray crosses the horizontal plane at the fire's height
    // (sea level if nothing burns). Plane-intersection always yields a valid point, so
    // aiming never depends on triangle-selector timing - more robust than surface picking.
    irr::f32 planeY = fire.isActive() ? fire.getWorldPosition().Y : 0.0f;
    irr::core::vector3df s = ray.start;
    irr::core::vector3df d = ray.end - ray.start;
    irr::core::vector3df aim;
    if (fabs(d.Y) > 1.0e-4f) {
        irr::f32 t = (planeY - s.Y) / d.Y;
        if (t < 0.0f) { t = 0.0f; }
        if (t > 1.0f) { t = 1.0f; }
        aim = s + d * t;
    }
    else {
        aim = fire.isActive() ? fire.getWorldPosition() : ray.end;
    }
    fireMonitor.setAimPoint(aim);
}

bool SimulationModel::hasFireBoat() const { return fireMonitor.isMounted(); }
bool SimulationModel::anyFireActive() const { return fire.isActive(); }
// end FIRE FEATURE --------------------------------------------------------
irr::scene::ISceneNode* SimulationModel::getOwnShipSceneNode()
{
    return (irr::scene::ISceneNode*)ownShip.getSceneNode();
}

irr::scene::ISceneNode* SimulationModel::getOtherShipSceneNode(int number)
{
    return otherShips.getSceneNode(number);
}

irr::scene::ISceneNode* SimulationModel::getBuoySceneNode(int number)
{
    return buoys.getSceneNode(number);
}

irr::scene::ISceneNode* SimulationModel::getLandObjectSceneNode(int number)
{
    return landObjects.getSceneNode(number);
}

irr::scene::ISceneNode* SimulationModel::getTerrainSceneNode(int number)
{
    return terrain.getSceneNode(number);
}

Terrain* SimulationModel::getTerrain()
{
    return &terrain;
}

irr::f32 SimulationModel::getTerrainHeight(irr::f32 posX, irr::f32 posZ) const
{
    return terrain.getHeight(posX, posZ);
}

void SimulationModel::addLine() // Add a line, which will be undefined
{
    lines.addLine(this);
}

Lines* SimulationModel::getLines() // Get pointer to lines object
{
    return &lines;
}

void SimulationModel::updateCameraVRPos(irr::core::quaternion quat, irr::core::vector3df pos, irr::core::vector2df lensShift)
{
    camera.update(0, quat, pos, lensShift, true);
}

void SimulationModel::update()
{

#ifdef WITH_PROFILING
    IPROF_FUNC;
#endif
    // DEE vvvv debug I think that this is effectively the cycle

            //Declare here, so scope added as part of profiling isn't a problem
    irr::u32 lightLevel;
    irr::f32 elevAngle;
    irr::core::vector2di cursorPositionRadar;
    std::vector<irr::f32> CPAs;
    std::vector<irr::f32> TCPAs;
    std::vector<irr::f32> headings;
    std::vector<irr::f32> speeds;
    bool paused;
    bool collided{};

    {
        IPROF("Increment time");

        // move time along .. this goes before everything else in the cycle

        //get delta time
        currentTime = device->getTimer()->getTime();
        deltaTime = (currentTime - previousTime) / 1000.f;
        //deltaTime = (currentTime - previousTime)/1000.f;



        // Clamp physics step so a frame hitch can't blow up the mooring-line springs
        if (deltaTime > 0.1f) {
            deltaTime = 0.1f;
        }
        previousTime = currentTime;

        //add this to the scenario time
        scenarioTime += deltaTime;
        absoluteTime = Utilities::round(scenarioTime) + scenarioOffsetTime;

        //increment loop number
        loopNumber++;

        // end move time along
    } {
        IPROF("Update swell");
        // KYARA HOULE: sea state follows the weather tab live (weather -> height & period, wind ->
        // direction & sea age). Must run before any ship samples getWaveHeight() this frame.
        swell.setFadeCentre(water.getPosition().X, water.getPosition().Z);
        swell.update(deltaTime, weather, windDirection, windSpeed, ownShip.getPosition().X, ownShip.getPosition().Z);
    } {
        IPROF("Set radar display radius");


        //Ensure we have the right radar screen resolution
        setRadarDisplayRadius(guiMain->getRadarPixelRadius());

    } {
        IPROF("Update tide");

        //Update tide height and tidal stream here.
        tide.update(absoluteTime);
        tideHeight = tide.getTideHeight();

    } {
        IPROF("Update lighting");

        //update ambient lighting
      //update ambient lighting
        light.update(scenarioTime + dayNightOffset);

        //Note that linear fog is hardcoded into the water shader, so should be changed there if we use other fog types
        irr::f32 appliedVisibilityRange = visibilityRange;
        // Lower bound of visibility of 0.01 Nm
        if (appliedVisibilityRange < 0.01) {
            appliedVisibilityRange = 0.01;
        }
        //KYARA: FOG COLOUR -----------------------------------------------------------------
                //Was light.getLightSColor() (the raw ambient), which scales with day/night brightness -
                //so the fog got brighter and "heavier" through the morning and washed everything to grey.
                //Instead: a fixed cool blue-white by day, reached early and HELD (so no hour-to-hour drift),
                //darkening to a dim blue at night, then tinted to the dawn/dusk horizon hue so the fog
                //matches the sky instead of clashing with it.
        irr::f32 dayF = ((irr::f32)light.getLightLevel() - 40.0f) / 70.0f; //0 deep night, 1 by early morning
        if (dayF < 0.0f) { dayF = 0.0f; }
        if (dayF > 1.0f) { dayF = 1.0f; }                    //clamped to 1 all day -> steady through the day

        const irr::f32 dayR = 188.0f, dayG = 201.0f, dayB = 212.0f;    //cool blue-white daylight haze
        const irr::f32 nightR = 16.0f, nightG = 22.0f, nightB = 32.0f; //dim blue night

        irr::f32 fR = nightR + (dayR - nightR) * dayF;
        irr::f32 fG = nightG + (dayG - nightG) * dayF;
        irr::f32 fB = nightB + (dayB - nightB) * dayF;

        //dawn/dusk horizon tint, matched to the Sky glow domes
        irr::f32 w = light.getWarmth();
        if (w < 0.0f) { w = 0.0f; }
        if (w > 1.0f) { w = 1.0f; }
        irr::f32 tR, tG, tB;
        if (light.isDawn()) { tR = 150.0f; tG = 140.0f; tB = 200.0f; } //cool rose/violet (matches dawn glow)
        else { tR = 235.0f; tG = 140.0f; tB = 80.0f; } //warm orange (matches dusk glow)
        irr::f32 tw = (weather >= 3.5f) ? 0.0f : (w * 0.55f);   // no warm fog tint under storm
        fR = fR * (1.0f - tw) + tR * tw;
        fG = fG * (1.0f - tw) + tG * tw;
        fB = fB * (1.0f - tw) + tB * tw;

        irr::video::SColor fogColour(255, (irr::u32)fR, (irr::u32)fG, (irr::u32)fB);
        //KYARA: exp2 so boats and the sky dome dissolve the same way the water shader now does.
                //Density keyed to the visibility range; 'end' kept meaningful because the water shader
                //still reads gl_Fog.end to derive its own density (FOG_K=2.0 there).
        //FOG DENSITY 
        irr::f32 fogDensity = 0.9f / (appliedVisibilityRange * M_IN_NM);
        driver->setFog(fogColour, irr::video::EFT_FOG_EXP2, 0.0, appliedVisibilityRange * M_IN_NM, fogDensity, true, true);

        //KYARA: sky updated AFTER appliedVisibilityRange exists - the haze dome is driven from it,
        //now tinted to the SAME fogColour so the sky matches the sea instead of clashing.
        bool skyStorm = (weather >= 3.5f); // sea state alone, no fog needed - matches the audio
        sky.update(light.getLightLevel(), light.getWarmth(), light.isDawn(), appliedVisibilityRange, fogColour, skyStorm);

        lightLevel = light.getLightLevel();

    } {
        IPROF("Update rain");
        //update rain
        rain.setIntensity(rainIntensity);
        rain.setWind(getWindSpeed());              //KYARA: was never called - slant was always 0
        rain.setWindDirection(getWindDirection());
        rain.update(scenarioTime);

    } {
        IPROF("Update other ships");
        //update other ship positions etc
        otherShips.update(deltaTime, scenarioTime, tideHeight, lightLevel, ownShip.getPosition(), ownShip.getLength()); //Update other ship motion (based on leg information), and light visibility.

    } {
        IPROF("Update buoys");
        //update buoys (for lights, floating, and if collision detection is turned on)
        buoys.update(deltaTime, scenarioTime, tideHeight, lightLevel, ownShip.getPosition(), ownShip.getLength());

    } {
        IPROF("Update land lights");
        //Update land lights
        landLights.update(deltaTime, scenarioTime, lightLevel);

    } {
        IPROF("Update lines");
        //update all lines, ready to be used for own ship force
        lines.update(deltaTime);
        // Kyara REMORQUAGE: apply tow constraints from taut ship-to-ship lines,
        // after line geometry is fresh and before own ship integrates the line force.
        updateTows(deltaTime);
    } {
        IPROF("Update own ship");
        //update own ship
        ownShip.update(deltaTime, scenarioTime, tideHeight, weather, lines.getOverallForceLocal(), lines.getOverallTorqueLocal());

        //KYARA ENGINE PITCH: drive engine pitch from actual speed through water, not throttle
        //lever position. Using speed (which only changes gradually, under the ship's own
        //acceleration/drag) rather than the lever (which can be slammed instantly) means
        //easing off the throttle produces a smooth falling pitch as the ship actually slows,
        //and gives a believable "spooling down" feel instead of an instant snap. fabs() is
        //correct here (not the earlier bug) because this maps speed magnitude to pitch
        //magnitude every frame as it actually changes - it isn't being used to collapse two
        //different lever directions into one ramp-up-only value.
        {
            // Reference speed at which the engine reaches max pitch. Tune to the vessel's
            // realistic cruising speed; ENGINE_PITCH_REFERENCE_SPEED_MPS is in m/s.
            // TODO: source this per-boat (e.g. from boat.ini maxSpeedAhead) once a public
            // getter for the ship's max speed is exposed - currently it's only used
            // internally within OwnShip's own dynamics setup.
            const irr::f32 ENGINE_PITCH_REFERENCE_SPEED_MPS = 7.5f; // ~14.5 knots
            const irr::f32 ENGINE_PITCH_MAX = 1.5f;                 // playback-rate multiplier at/above reference speed
            const irr::f32 ENGINE_PITCH_MIN = 1.0f;                 // playback-rate multiplier at zero speed

            irr::f32 speedMagnitude = fabs(ownShip.getSpeedThroughWater()); // m/s, sign discarded: ahead and astern both rev the engine up from idle
            irr::f32 speedFraction = speedMagnitude / ENGINE_PITCH_REFERENCE_SPEED_MPS;
            if (speedFraction > 1.0f) { speedFraction = 1.0f; }

            sound->setPitchEngine(ENGINE_PITCH_MIN + speedFraction * (ENGINE_PITCH_MAX - ENGINE_PITCH_MIN));
        }

    } {
        IPROF("Update MOB");
        //update man overboard
        manOverboard.update(deltaTime, tideHeight);

    } {
        IPROF("Update fire + monitor");
        fireMonitor.update(deltaTime);

        if (fireMonitor.isFiring()) {
            if (fire.isActive()) {
                fireMonitor.setAimPoint(fire.getWorldPosition());
                fire.applyWater(deltaTime);
            }
            else {
                irr::f32 h = ownShip.getHeading() * irr::core::DEGTORAD;
                fireMonitor.setAimPoint(fireMonitor.getNozzleWorldPos()
                    + irr::core::vector3df(40.0f * std::sin(h), -2.0f, 40.0f * std::cos(h)));
            }
        }
        // SAR HELICOPTER FEATURE: helos called on OSC ack; fly them in, then keep them tracking the casualty.
        updateHeloCall(deltaTime);
        updateSarHelicopters();

        // SAR SEQUENCERS: drive abandon-ship and helicopter rescue animations every frame.
        updateAbandonSpawn(deltaTime);
        updateHeloRun(deltaTime);

        bool active = fire.isActive();
        if (active) {
            fireElapsed += deltaTime;
            fire.setEscalation(fireElapsed / incident.fireSpreadTime);
            sound->setVolumeFireBurning(0.5f + 0.5f * fire.getEscalation());  // crackle grows
            sound->setVolumeGroan(0.25f + 0.5f * fire.getEscalation());       // metal groan grows
            bool abandon = (fireElapsed >= incident.abandonTime);
            if (abandon && !abandonSpawned) {
                abandonSpawned = true;
                spawnAbandonScene();        // begins the staggered mise-à-l'eau
                rescueCasualtyIndex = burningShipIndex;   // captured before she founders
            }
            sound->setVolumeAbandonAlarm(abandon ? 1.0f : 0.0f);
            sound->setVolumeFireAlarm(abandon ? 0.0f : 1.0f);   // hand off at 3 min: fire alarm -> abandon

            if (!casualtySinking && burningShipIndex >= 0 && fire.isActive()
                && fireElapsed >= incident.sinkStartTime()) {
                casualtySinking = true;
                otherShips.startSinking(burningShipIndex, incident.sinkLeadTime);   // fully under at fireDuration

                // Kyara FIRE: she's going down - the tow parts. Drop every line made fast to her,
                // so the tow solver stops asserting a pose and the trainee's lines are freed.
                for (int l = lines.getNumberOfLines() - 1; l >= 0; l--) {
                    if (lines.getLineEndType(l) == 2 && lines.getLineEndID(l) == burningShipIndex) {
                        lines.removeLine(l);
                    }
                }
                lines.setSelectedLine(-1);   // indices shifted; drop any stale selection
                otherShips.setTowState(burningShipIndex, false, 0.0f, 0.0f, 0.0f);

                pushComms(L"\u00C9CHEC : incendie non ma\u00EEtris\u00E9 - le navire coule");
            }
        }

        // jet + steam only while a live fire is being hosed
        sound->setVolumeWater(fireMonitor.isFiring() ? 1.0f : 0.0f);
        sound->setVolumeSteam((fireMonitor.isFiring() && active) ? 1.0f : 0.0f);

        // fully under -> silence EVERYTHING (incl. the alarms)
        if (casualtySinking && burningShipIndex >= 0 && otherShips.isSunk(burningShipIndex)) {
            fire.remove();
            if (fireMountNode) { fireMountNode->remove(); fireMountNode = 0; }

            sound->setVolumeFireAlarm(0.0f);
            sound->setVolumeAbandonAlarm(0.0f);
            sound->setVolumeFireBurning(0.0f);
            sound->setVolumeGroan(0.0f);
            sound->setVolumeSteam(0.0f);
            sound->setVolumeWater(0.0f);
            sound->setVolumeVhf(0.0f);

            pushComms(L"Navire coul\u00E9 - fin de l'intervention");
            // SAR RESCUE RUN: recover the survivors after the foundering
           // SAR assets + survivors intentionally REMAIN on scene after the sinking.
            casualtySinking = false;
            burningShipIndex = -1;
            fireWasBurning = false;   // FIX: stops the false "Feu eteint" line after a sinking
            distressActive = false;   // FIX: hide comms tab when the intervention ends (failure)
            distressTimer.clear();
            // SAR HELICOPTER FEATURE

           // sarInit = false;

        }

        fire.update(deltaTime, getWindDirection(), getWindSpeed());

        // extinguished (win): stop fire vfx and ALL alarms - the emergency is over
        bool activeNow = fire.isActive();
        if (fireWasBurning && !activeNow && !casualtySinking) {
            sound->setVolumeFireBurning(0.0f);
            sound->setVolumeGroan(0.0f);
            sound->setVolumeSteam(0.0f);
            sound->setVolumeAbandonAlarm(0.0f);
            sound->setVolumeWater(0.0f);
            //kyara: the general fire alarm used to be left ringing deliberately after a successful
            //knockdown, which meant it never stopped for the rest of the exercise. The fire is out,
            //so stand the alarm down with everything else.
            sound->setVolumeFireAlarm(0.0f);
            sound->setVolumeVhf(0.0f);

            monitorFiringDesired = false; fireMonitor.setFiring(false);
            pushComms(L"Feu \u00E9teint - sinistre ma\u00EEtris\u00E9");
            departSarHelicopters();   // knockdown - air assets released

            // Under 3 minutes she is saved and rights herself; past that the flooding has gone
            // too far and the list stays as permanent damage.
            if (fireElapsed < incident.permanentListTime) {
                if (burningShipIndex >= 0) { otherShips.setCasualty(burningShipIndex, false); }
                pushComms(L"Navire stabilis\u00E9 - assiette r\u00E9tablie");
            }
            else {
                pushComms(L"Navire sauv\u00E9 mais g\u00EEt\u00E9 - avarie permanente");
            }
            // SAR RESCUE RUN: recover the survivors after the knockdown
            distressActive = false;   // FIX: hide comms tab when the fire is out (success)
            distressTimer.clear();
            // SAR assets + survivors intentionally REMAIN on scene after the fire is out.
        }
        fireWasBurning = activeNow;

        // countdown for the comms overlay
        if (distressActive) {
            irr::s32 rem = (irr::s32)(incident.sinkStartTime() - fireElapsed); if (rem < 0) rem = 0;
            wchar_t tb[64];
            swprintf(tb, 64, L"INCENDIE  T+%02d:%02d   (\u00E9ch\u00E9ance %02d:%02d)",
                (int)fireElapsed / 60, (int)fireElapsed % 60, rem / 60, rem % 60);
            distressTimer = tb;
        }
    } {

        // --- PROXY ALARM + COLLISION (heave-immune, horizontal geometry) ---
        // Everything here uses X/Z distance only, so the cosmetic wave heave on Y can never
        // affect proximity warning or collision detection.

        irr::core::vector3df ownPos = ownShip.getPosition();
        irr::f32 ownHdg = ownShip.getHeading();
        irr::f32 ownLen = ownShip.getLength();
        irr::f32 ownBreadth = ownShip.getBreadth();
        irr::u32 numShips = otherShips.getNumber();

        // Find the nearest other ship (horizontal gap), its bearing relative to our bow,
        // and the centre-to-centre gap minus both ships' effective radii (the real edge gap).
        irr::f32 shipGap = 1.0e30f;        // centre-to-centre, nearest ship
        irr::f32 nearestRelBearing = 0.0f; // deg, relative to our heading
        irr::f32 nearestEdgeGap = 1.0e30f; // hull-edge to hull-edge, nearest ship
        bool haveNearest = false;

        for (irr::u32 i = 0; i < numShips; i++) {
            irr::core::vector3df otherPos = otherShips.getPosition(i);
            irr::f32 dx = otherPos.X - ownPos.X;
            irr::f32 dz = otherPos.Z - ownPos.Z;
            irr::f32 dist = sqrt(dx * dx + dz * dz);

            if (dist < shipGap) {
                shipGap = dist;
                haveNearest = true;

                irr::f32 absBearing = irr::core::radToDeg(atan2(dx, dz));
                nearestRelBearing = absBearing - ownHdg;
                while (nearestRelBearing <= -180.0f) nearestRelBearing += 360.0f;
                while (nearestRelBearing > 180.0f) nearestRelBearing -= 360.0f;

                // Capsule hull model: each ship is a keel SEGMENT (length L - B, centred on
                // the hull, aligned with heading) plus a beam RADIUS (B/2). The real edge gap
                // is the segment-to-segment distance minus both beam radii. This respects the
                // ship's elongated shape, so a boat lying alongside a long thin hull with clear
                // water between is no longer counted as in contact. Ahead unit vector is
                // (sin(hdg), cos(hdg)), matching OwnShip's movement convention.
                irr::f32 otherLen = otherShips.getLength(i);
                irr::f32 otherBreadth = otherShips.getBreadth(i);
                irr::f32 otherHdg = otherShips.getHeading(i);

                irr::f32 ownHalf = 0.5f * irr::core::max_(0.0f, ownLen - ownBreadth);
                irr::f32 otherHalf = 0.5f * irr::core::max_(0.0f, otherLen - otherBreadth);
                irr::f32 ownBeamR = 0.5f * ownBreadth;
                irr::f32 otherBeamR = 0.5f * otherBreadth;

                irr::f32 ownFx = sin(ownHdg * irr::core::DEGTORAD), ownFz = cos(ownHdg * irr::core::DEGTORAD);
                irr::f32 othFx = sin(otherHdg * irr::core::DEGTORAD), othFz = cos(otherHdg * irr::core::DEGTORAD);

                irr::f32 oA_x = ownPos.X - ownFx * ownHalf, oA_z = ownPos.Z - ownFz * ownHalf;
                irr::f32 oB_x = ownPos.X + ownFx * ownHalf, oB_z = ownPos.Z + ownFz * ownHalf;
                irr::f32 cA_x = otherPos.X - othFx * otherHalf, cA_z = otherPos.Z - othFz * otherHalf;
                irr::f32 cB_x = otherPos.X + othFx * otherHalf, cB_z = otherPos.Z + othFz * otherHalf;

                irr::f32 keelGap = segSegDistance2D(oA_x, oA_z, oB_x, oB_z, cA_x, cA_z, cB_x, cB_z);
                nearestEdgeGap = keelGap - ownBeamR - otherBeamR;
            }
        }

        // ---- Startup grace ----
        // For the first couple of seconds of *running* simulation, suppress both the proxy
        // alarm and collision detection. On spawn the hull settles vertically (yPos starts at
        // 0, tide/wave not yet applied) which can momentarily report a false grounding/contact,
        // and we don't want a boat that simply happens to be ahead at spawn to sound anything.
        // deltaTime is ~0 while paused, so this grace is consumed by real running time only.
        if (collisionStartupGrace > 0.0f && deltaTime > 0.0f) {
            collisionStartupGrace -= deltaTime;
        }
        bool startupGraceActive = (collisionStartupGrace > 0.0f);

        // ---- Proxy alarm: only while CLOSING in ----
        // Requirement: warn when we are getting CLOSER to another ship, not merely because one
        // is parked ahead of us. So the alarm needs (a) the nearest ship within warning range,
        // (b) it broadly ahead of the bow, AND (c) the gap actually shrinking since last frame.
        // A very small gap also force-holds the alarm even if we've stopped closing, so a
        // near-miss keeps warning. A short hold timer smooths jitter between frames.
        //KYARA CHANGE PROXIIMITY RANGE 
        const irr::f32 PROXIMITY_WARN_EDGE = 80.0f; // edge-gap (m) within which closing-in warns
        const irr::f32 PROXIMITY_WARN_CENTRE = 130.0f; // centre-gap (m) fallback within which closing-in warns
        const irr::f32 CONE_OF_DETECTION = 40.0f;  // +/- degrees off the bow (wide, so it isn't lost in turns)
        const irr::f32 PROXY_HOLD_TIME = 1.5f;   // seconds the alarm holds after the last 'closing' frame
        const irr::f32 CLOSING_EPSILON = 0.05f;  // metres/frame the gap must shrink to count as 'closing'
        const irr::f32 PROXIMITY_FORCE_EDGE = 30.0f;  // edge-gap (m) at which we warn even if not closing

        // Low-pass filter the relative bearing before testing the cone, so a few
  // degrees of wave-induced yaw don't register as 'lost aim'. Time constant
  // is slower than a typical wave yaw period but still tracks a deliberate
  // turn within roughly a second.
        const irr::f32 BEARING_FILTER_TIME_CONSTANT = 0.8f; // seconds
        if (deltaTime <= 0.0f || !haveNearest) {
            filteredRelBearing = nearestRelBearing; // paused, or nothing to track: snap, don't filter toward stale value
        }
        else {
            irr::f32 filterFactor = deltaTime / (BEARING_FILTER_TIME_CONSTANT + deltaTime);
            // Shortest-path filtering across the +/-180 wrap (so 179 -> -179 filters as
            // a 2-degree step, not a 358-degree one).
            irr::f32 bearingDelta = nearestRelBearing - filteredRelBearing;
            while (bearingDelta > 180.0f) bearingDelta -= 360.0f;
            while (bearingDelta < -180.0f) bearingDelta += 360.0f;
            filteredRelBearing += bearingDelta * filterFactor;
            while (filteredRelBearing > 180.0f) filteredRelBearing -= 360.0f;
            while (filteredRelBearing <= -180.0f) filteredRelBearing += 360.0f;
        }

        // Hysteresis: once aimed, require the bearing to drift clearly WIDER than the
        // entry cone before counting as 'lost' - stops chatter right at the boundary.
        const irr::f32 CONE_EXIT_DETECTION = CONE_OF_DETECTION + 10.0f; // +/- degrees
        if (!aimedAtOtherLatched && fabs(filteredRelBearing) < CONE_OF_DETECTION) {
            aimedAtOtherLatched = true;
        }
        else if (aimedAtOtherLatched && fabs(filteredRelBearing) > CONE_EXIT_DETECTION) {
            aimedAtOtherLatched = false;
        }
        bool aimedAtOther = aimedAtOtherLatched;
        bool withinWarnRange = haveNearest &&
            (nearestEdgeGap < PROXIMITY_WARN_EDGE || shipGap < PROXIMITY_WARN_CENTRE);
        // 'Closing' = nearest centre gap shrank meaningfully versus the previous frame.
        bool closingIn = haveNearest && (shipGap < prevNearestShipDistance - CLOSING_EPSILON);
        bool veryClose = haveNearest && (nearestEdgeGap < PROXIMITY_FORCE_EDGE);
        bool approachTrigger = aimedAtOther && withinWarnRange && closingIn;
        bool proxyTrigger = !startupGraceActive && (approachTrigger || veryClose);

        if (proxyTrigger) {
            proxyHoldTimer = PROXY_HOLD_TIME;
        }
        else if (proxyHoldTimer > 0.0f) {
            proxyHoldTimer -= deltaTime;
        }
        bool proxyAlarmActive = (proxyHoldTimer > 0.0f);

        // Remember this frame's nearest gap for next frame's closing test.
        if (haveNearest) {
            prevNearestShipDistance = shipGap;
        }

        // Run the mesh-based 3D collision check. This applies physical push-back forces
        // and accurately sets ownShip.isOtherShipCollision() / isBuoyCollision() /
        // isTerrainCollision() by ray-casting against the actual hull geometry.
        //
        // FIX: Use this mesh result for actualCollision instead of the geometric capsule
        // model (nearestEdgeGap <= 0). The capsule model's semicircular bow/stern caps are
        // as wide as the full beam, so they overlap before pointed hulls visually touch —
        // that was causing the ship to stop and the COLLISION sign to appear well before
        // any visual contact. The 3D ray check fires at the real mesh boundary.
        bool meshCollision = checkOwnShipCollision();
        bool actualCollision = !startupGraceActive && meshCollision;

        // Play the collision sound ONCE, on the rising edge of contact while actually moving.
        // (The SOG gate is only on the SOUND, so we don't replay it for a stationary nudge.)
       // KYARA CONTACT SOUND: separate "hull touched the quay" from every other collision.
        // A land object (quay wall, fender, pontoon) gets the metal contact sound; buoys,
        // other ships and groundings keep the general collision sound.
        // isTerrainCollision() is deliberately NOT used as the test here - it is also set
        // for seabed grounding, which must not produce a metal clang.
        bool landContact = !startupGraceActive && ownShip.isLandObjectCollision();

        bool otherCollision = actualCollision && !landContact;

        irr::f32 impactSpeed = fabs(ownShip.getSOG());

        if (contactSoundCooldown > 0.0f) {
            contactSoundCooldown -= deltaTime;
        }

        // Low speed gate (0.05 m/s, vs 0.5 for a collision): coming alongside is a slow
        // manoeuvre, and a gentle nudge onto the fenders should still be heard.
        if (landContact && !landContactWasTouching && contactSoundCooldown <= 0.0f && impactSpeed > 0.05f) {
            // Scale volume with closing speed: soft at a crawl, full weight from ~2 m/s up.
            // Stays inside setVolumeContact()'s 0..6 range.
            // Berthing contact happens at 0.1-0.75 m/s, not 2 m/s - ramp over that range instead,
            // so even a gentle touch lands near the middle of the volume range rather than the floor.
            // 1.2 .. 2.2 here maps to an output peak of roughly 0.38 .. 0.69 after the 0.33 mix
            // coefficient in Sound.hpp, which is clearly audible over the engine and wave loops
            // without clipping when summed with them.
            // Berthing contact happens at 0.1-0.75 m/s, not 2 m/s - ramp over that range instead,
            // so even a gentle touch lands near the middle of the volume range rather than the floor.
            irr::f32 contactVol = 2.2f + 2.0f * fmin(1.0f, impactSpeed / 0.75f);
            sound->setVolumeContact(contactVol);
            sound->triggerContact();
            contactSoundCooldown = 0.35f; // seconds between contact sounds
        }
        // COLLISION VOLUME 
        else if (otherCollision && !otherCollisionWasTouching && impactSpeed > 0.5f) {
            sound->setVolumeCollision(6.0f);
            sound->triggerCollision();

        }

        landContactWasTouching = landContact;
        otherCollisionWasTouching = otherCollision;

        // inCollision tracks ongoing contact. Release requires a short SUSTAINED period of no
         // contact (not just one clean raycast frame), and is reset any time contact reappears -
         // so the sign/stop only lets go once the player has genuinely opened a gap (e.g. by going
         // astern), not on a single-frame miss during a momentary near-miss.
        const irr::f32 COLLISION_RELEASE_DELAY = 0.5f; // seconds of clean separation required to release
        // ----------------------------------------------------------------------------------
        if (actualCollision) {
            inCollision = true;
            collisionReleaseHold = COLLISION_RELEASE_DELAY; // reset the release timer every contact frame
        }
        else if (inCollision) {
            collisionReleaseHold -= deltaTime;
            if (collisionReleaseHold <= 0.0f) {
                inCollision = false;
            }
        }
        collisionWasColliding = actualCollision;



        // STOP THE SHIP whenever it is in contact. We arm OwnShip's per-frame hard stop, which
        // zeroes forward speed INSIDE ownShip.update() (before the hull moves) every frame while
        // in contact - this is immune to the engine telegraph still being commanded ahead, and
        // it does not switch control mode. Astern is left available so the player can reverse out.
        ownShip.setCollisionStop(false);

        if (inCollision) {
            // Sign stays on; proxy alarm keeps sounding through the collision, UNLESS muted.
            sound->setVolumeProxyAlarm(proxyAlarmMuted ? 0.0f : 2.0f);
            collided = true;
        }
        else {
            // Proxy alarm follows the approach/hold AND the mute state; collision sign off.
            sound->setVolumeProxyAlarm((proxyAlarmActive && !proxyAlarmMuted) ? 2.0f : 0.0f);
            collided = false;
        }
        //-----------------------------------------------------------------------------

        //kyara: radar guard-zone alarm on its own channel, independent of proxy & its mute
        sound->setVolumeRadarAlarm(radarCalculation.isGuardAlarmSounding() ? 1.0f : 0.0f);
        // ---- KYARA WEATHER AUDIO + LIGHTNING --------------------------------------------
        // Storm is DERIVED from the (already-synced) weather + visibility, so primary and
        // secondary agree without any extra network flag.
        const irr::f32 STORM_WEATHER_MIN = 3.5f;   //tweak: sea state that switches the storm on
        // Visibility is NO LONGER required - storms happen without fog.
        bool stormActive = (weather >= STORM_WEATHER_MIN);
        // Interior bridge views (0/1) hear rain/storm muffled through the glass. Apply the muffle
                // HERE so each volume is written exactly ONCE per frame. Writing it a second time later
                // (in the inside/outside block) made the audio thread see two values per frame -> the hiss
                // amplitude jumped buffer-to-buffer = the "stutter" when going inside.
        irr::u32 weatherView = camera.getView();
        bool insideView = (weatherView == 0 || weatherView == 1);
        irr::f32 rainMuffle = insideView ? 0.5f : 1.0f;
        irr::f32 stormMuffle = insideView ? 0.35f : 1.0f;

        // Rain sound: plays whenever there's rain at all, louder as it builds (0..10 -> 0..0.9).
        irr::f32 rainVol = (rainIntensity / 10.0f) * 0.9f;//tweak: rain sound loudness
        if (rainVol < 0.0f) rainVol = 0.0f; if (rainVol > 1.0f) rainVol = 1.0f;
        sound->setVolumeRain(rainVol* rainMuffle);

        // Storm ambience: fades in/out with the storm state (simple 1s ramp so it isn't abrupt).
        static irr::f32 stormRamp = 0.0f;
        irr::f32 stormTarget = stormActive ? 0.8f : 0.0f;//tweak: wind/storm ambience volume
        stormRamp += (stormTarget - stormRamp) * fmin(1.0f, deltaTime * 1.0f);
        sound->setVolumeStorm(stormRamp* stormMuffle);

        // ---- KYARA: looping thunder sequence + random lightning ----
            // Thunder is a continuous loop through the storm (like storm ambience), gated by the
            // thunder checkbox. Secondaries have no sound loaded, so this is a no-op there.
        {
            static irr::f32 thunderRamp = 0.0f;
            irr::f32 thunderTarget = (stormActive && thunderEnabled) ? 0.9f : 0.0f; //tweak: thunder loop volume
            thunderRamp += (thunderTarget - thunderRamp) * fmin(1.0f, deltaTime * 1.0f);
            sound->setVolumeThunder(thunderRamp);
        }

        // Lightning locked to the thunder-loop claps: fire when playback crosses a clap onset.
        if (modelParameters.mode != OperatingMode::Secondary) {
            static const irr::f32 clapTimes[] = {   //tweak: each value = a second in the loop where lightning fires
                 0.02f,4.04f,6.48f,9.22f,11.5f,12.52f,13.58f,14.6f,15.62f,17.02f,18.34f,19.46f,22.06f,24.84f,26.58f,
                 34.86f,36.02f,37.1f,41.9f,43.4f,46.18f,47.34f,50.18f,51.7f,53.04f,56.54f,57.8f,59.16f,62.48f,63.98f,
                 65.0f,66.04f,68.9f,69.92f,70.94f,73.76f,74.82f,75.94f,82.32f,83.62f,85.52f,86.54f,87.6f,88.64f,89.66f,
                 90.88f,92.1f,93.32f,95.12f,97.14f,98.52f,99.8f,100.9f,102.2f,103.22f,104.32f,105.62f,106.66f,107.7f,
                 108.98f,110.12f,111.44f,112.72f,113.76f,114.78f,116.22f,117.28f,118.44f,119.94f,122.06f,123.46f,124.96f,
                 126.34f,127.4f,132.58f,134.88f,137.62f,138.64f,140.8f,141.96f,143.04f,146.06f,148.78f,149.98f,152.0f,
                 156.08f,157.14f,159.12f,164.72f,166.2f,167.7f,168.74f,169.76f,172.64f,173.66f,175.3f,177.04f,180.22f,
                 181.72f,182.76f,183.78f,184.88f,186.3f,187.36f,189.64f,190.68f,191.7f,193.26f,194.84f,195.94f,198.78f,
                 199.8f,200.82f,207.1f,208.24f,209.94f,211.04f,212.12f,214.02f,215.12f,216.42f,218.0f,220.04f,221.36f,
                 222.38f,223.48f,224.66f,225.72f,226.74f,227.98f,229.14f };
            static const int numClaps = sizeof(clapTimes) / sizeof(clapTimes[0]);
            static irr::f32 lastThunderPos = 0.0f;
            irr::f32 pos = (irr::f32)sound->getThunderPositionSeconds();
            if (stormActive && lightningEnabled) {
                bool wrapped = (pos < lastThunderPos);   // loop restarted
                for (int i = 0; i < numClaps; i++) {
                    bool crossed = wrapped ? (clapTimes[i] > lastThunderPos || clapTimes[i] <= pos)
                        : (clapTimes[i] > lastThunderPos && clapTimes[i] <= pos);
                    if (crossed) { lightningFlash = 0.5f + (irr::f32)(rand() % 51) / 100.0f; break; }  //tweak: flash brightness range0.5..1.0 strength
                }
            }
            lastThunderPos = pos;
            lightningFlash -= deltaTime * 9.0f;//tweak: flash fade speed (higher = quicker)
            if (lightningFlash < 0.0f) lightningFlash = 0.0f;
        }
        // NAUTITECH HERO BOLT: one big descending bolt when the storm begins, then every 10 s,
        // storm only. Base clapTimes flash sequence above is untouched. Primary only; the sky
        // billboard lives in 3D so it's occluded by terrain and never hits the radar.
        if (modelParameters.mode != OperatingMode::Secondary) {
            if (stormActive) {
                bool firstStrike = !heroBoltStormWasActive; // storm just switched on
                heroBoltTimer -= deltaTime;
                if (lightningEnabled && (firstStrike || heroBoltTimer <= 0.0f)) {
                    heroBoltTimer = 10.0f;
                    irr::core::vector3df camPos = camera.getPosition();
                    irr::f32 bearing = irr::core::DEGTORAD * (irr::f32)(rand() % 360);
                    irr::f32 dist = 2.2f * M_IN_NM;            // out near the sky dome (3.5 NM)
                    irr::core::vector3df top(camPos.X + dist * sin(bearing),
                        camPos.Y + 1.2f * M_IN_NM,   // high in the sky
                        camPos.Z + dist * cos(bearing));
                    //tweak size of bolt 
                    sky.triggerHeroBolt(top, 3.0f * M_IN_NM /*height*/, 1.3f * M_IN_NM /*width, thick*/);
                    if (thunderEnabled) { sound->triggerThunderbolt(); }
                }
            }
            else {
                heroBoltTimer = 0.0f; // reset so the next storm fires immediately
            }
            heroBoltStormWasActive = stormActive;
            sky.updateHeroBolt(deltaTime);
            // NAUTITECH TENDER LIGHTNING: every 2 s while the storm is active, flash ALL the
                       // tenders at once, each at its own fixed bearing -> a sky-wide burst all around.
            if (stormActive && lightningEnabled) {
                tenderTimer -= deltaTime;
                if (tenderTimer <= 0.0f) {
                    tenderTimer = 2.0f;
                    for (int t = 0; t < sky.getNumTenders(); t++) {
                        sky.triggerTender(t, camera.getPosition());
                    }
                }
            }
            else {
                tenderTimer = 0.0f;   // reset so the next storm starts the burst immediately
            }
            sky.updateTenders(deltaTime);
        }
        // (secondary: lightningFlash is set by the network each frame; do not touch it here.)
        // ----------------------------------------------------------------------------------
    } {
        IPROF("Update water pos");
        //update water position
        //Hull mask: the sea is not drawn inside the own ship's waterline outline (measured from
        //her model at load), so high crests never show through the plating or inside the wheelhouse.
        {
            irr::f32 zMin, zMax, centreX, halfWidths[OwnShip::HULL_STATIONS], keels[OwnShip::HULL_STATIONS];
            if (ownShip.getSceneNode() && ownShip.getHullWaterline(zMin, zMax, centreX, halfWidths, keels)) {
                water.setHullMask(ownShip.getSceneNode()->getPosition(), ownShip.getSceneNode()->getRotation(),
                    zMin, zMax, centreX, halfWidths, keels, true);
            }
        }
        water.update(tideHeight, camera.getPosition(), light.getLightLevel(), weather, windDirection, rainIntensity); // KYARA HOULE: + wind direction for the FFT chop, KYARA METEO: + rain
        {
            // KYARA HOULE: hand the same swell to the water shader
            irr::f32 comp[Swell::NCOMP * 4];
            irr::f32 fade[4];
            swell.setFadeCentre(water.getPosition().X, water.getPosition().Z);
            swell.getShaderData(comp, fade);
            water.setSwellShaderData(comp, fade);
        }

    } {
        IPROF("Normalise ");
        //Normalise positions if required (More than 1000 metres from origin)
        //FIXME: TEMPORARY MODS WITH REALISTICWATERSCENENODE
        if (ownShip.getPosition().getLength() > 1000) {
            irr::core::vector3df ownShipPos = ownShip.getPosition();
            irr::s32 deltaX = -1 * (irr::s32)ownShipPos.X;
            irr::s32 deltaZ = -1 * (irr::s32)ownShipPos.Z;
            //Round to nearest 1000 metres - (multiple of water tile width, to avoid jumps here)
            deltaX = 500.0 * Utilities::round(deltaX / 500.0);
            deltaZ = 500.0 * Utilities::round(deltaZ / 500.0);

            //Move all objects
            ownShip.moveNode(deltaX, 0, deltaZ);
            terrain.moveNode(deltaX, 0, deltaZ); //SLOW!
            otherShips.moveNode(deltaX, 0, deltaZ);
            buoys.moveNode(deltaX, 0, deltaZ);
            landObjects.moveNode(deltaX, 0, deltaZ);
            landLights.moveNode(deltaX, 0, deltaZ);
            manOverboard.moveNode(deltaX, 0, deltaZ);
            splash.moveNode(deltaX, 0, deltaZ); // KYARA SLAM
            // Kyara: the SAR scene is built from root-level nodes, so it has to be shifted too.
            for (size_t k = 0; k < abandonNodes.size(); k++) {
                if (abandonNodes[k]) { abandonNodes[k]->setPosition(abandonNodes[k]->getPosition() + irr::core::vector3df(deltaX, 0, deltaZ)); }
            }
            for (size_t k = 0; k < sarNodes.size(); k++) {
                if (sarNodes[k]) { sarNodes[k]->setPosition(sarNodes[k]->getPosition() + irr::core::vector3df(deltaX, 0, deltaZ)); }
            }
            for (size_t k = 0; k < sarLights.size(); k++) {
                if (sarLights[k]) { sarLights[k]->setPosition(sarLights[k]->getPosition() + irr::core::vector3df(deltaX, 0, deltaZ)); }
            }
            for (size_t k = 0; k < sarBeams.size(); k++) {
                if (sarBeams[k]) {
                    sarBeams[k]->setPosition(
                        sarBeams[k]->getPosition() +
                        irr::core::vector3df(deltaX, 0, deltaZ)
                    );
                }
            }

            // SAR HELICOPTERS: shift stored helicopter positions and winch cables
            // together with the rest of the SAR scene during re-centring.
            for (size_t k = 0; k < heloUnits.size(); k++) {
                heloUnits[k].pos.X += deltaX;
                heloUnits[k].pos.Z += deltaZ;

                if (heloUnits[k].cable) {
                    heloUnits[k].cable->setPosition(
                        heloUnits[k].cable->getPosition() +
                        irr::core::vector3df(deltaX, 0, deltaZ)
                    );
                }
            }

            sarBaseY += 0.0f; // Y is untouched by re-centring
            rescueMoveNode(deltaX, deltaZ);   // SAR RESCUE RUN
            if (sarDatumSet) { sarDatum.X += deltaX; sarDatum.Z += deltaZ; }
            // Also move camera if in 'frozen' mode
            camera.applyOffset(deltaX, 0, deltaZ);

            //Change stored offset
            offsetPosition.X -= deltaX;
            offsetPosition.Z -= deltaZ;

            // KYARA HOULE: keep the same physical swell after the world shift
            swell.shiftOrigin((irr::f32)deltaX, (irr::f32)deltaZ);

            std::string normalisedLogMessage = "Normalised, offset X: ";
            normalisedLogMessage.append(Utilities::lexical_cast<std::string>(offsetPosition.X));
            normalisedLogMessage.append(" Z: ");
            normalisedLogMessage.append(Utilities::lexical_cast<std::string>(offsetPosition.Z));
            device->getLogger()->log(normalisedLogMessage.c_str());

            //Debugging
            //std::cout << normalisedLogMessage << std::endl;

        }
    } {
        IPROF("Update camera pos");

        //KYARA FEUX EDIT: keep the orbit centred on the lamp being moved, and turning with the vessel
        if (lightEditVessel != -2) {
            ShipLights* lights = getShipLights(lightEditVessel);
            irr::scene::ISceneNode* node = (lightEditVessel < 0) ? ownShip.getSceneNode()
                                                                  : otherShips.getSceneNode(lightEditVessel);
            irr::core::vector3df centre;
            if (lights && node && lights->getSelectedWorldPosition(centre)) {
                camera.setOrbitCentre(centre, node->getRotation().Y - lights->getAngleCorrection());
            }
        }

        //Free view: circle the middle of the own ship's model, turning with her
        if (freeView && lightEditVessel == -2 && ownShip.getSceneNode()) {
            camera.setOrbitCentre(ownShip.getSceneNode()->getTransformedBoundingBox().getCenter(), ownShip.getHeading());
            //Never under the water: a storm crest can rise above a low camera, which then looks at
            //the seabed from below the surface
            const irr::core::vector3df eye = camera.getOrbitPosition();
            camera.setOrbitMinHeight(tideHeight + getWaveHeight(eye.X, eye.Z) + 1.5f);
        }

        //update the camera position
        camera.update(deltaTime);
    } {
        IPROF("Update controls visualisation");
        if (isAzimuthDrive()) {
            portEngineVisual.update(ownShip.getPortSchottel());
            stbdEngineVisual.update(ownShip.getStbdSchottel());
            portAzimuthThrottleVisual.update(45 * getPortAzimuthThrustLever());
            stbdAzimuthThrottleVisual.update(45 * getStbdAzimuthThrustLever());
        }
        else {
            portEngineVisual.update(45.0 * ownShip.getPortEngine());
            stbdEngineVisual.update(45.0 * ownShip.getStbdEngine());
            wheelVisual.update(-6.0 * ownShip.getWheel());
        }
    }
    if (radarCalculation.isRadarOn()) {
        {
            IPROF("Update radar cursor position");
            //set radar screen position, and update it with a radar image from the radar calculation
            cursorPositionRadar = guiMain->getCursorPositionRadar();
        } {
            IPROF("Update radar calculation");
            //Choose which radar images to use, depending on the size of the display being used
            if (2 * guiMain->getRadarPixelRadius() > radarImage->getDimension().Width) {
                radarImageChosen = radarImageLarge;
                radarImageOverlaidChosen = radarImageOverlaidLarge;
            }
            else {
                radarImageChosen = radarImage;
                radarImageOverlaidChosen = radarImageOverlaid;
            }
            radarCalculation.update(radarImageChosen, radarImageOverlaidChosen, offsetPosition, terrain, ownShip, buoys, otherShips, weather, rainIntensity, tideHeight, deltaTime, absoluteTime, cursorPositionRadar, isMouseDown);
        } {
            IPROF("Update radar screen");
            radarScreen.setDisplayOffset(radarCalculation.getOffsetXFraction(), radarCalculation.getOffsetYFraction()); //kyara
            radarScreen.update(radarImageOverlaidChosen);
        } {
            IPROF("Update radar camera");
            radarCamera.update();
        }
    }
    else {
        radarScreen.getSceneNode()->setVisible(false);
    }
    {
        IPROF("Check if paused ");
        //check if paused
        paused = device->getTimer()->getSpeed() == 0.0;

    } {
        IPROF("Get radar ARPA data for GUI");

        //get radar ARPA data to show
        irr::u32 numberOfARPATracks = radarCalculation.getARPATracksSize();
        guiData->arpaContactStates.clear();
        for (unsigned int i = 0; i < numberOfARPATracks; i++) {
            guiData->arpaContactStates.push_back(radarCalculation.getARPAContactFromTrackIndex(i).estimate);
            guiData->arpaContactStates.back().mmsi = radarCalculation.getARPAContactFromTrackIndex(i).mmsi; //kyara: MMSI réel pour l'onglet AIS
        }
        guiData->arpaListSelection = radarCalculation.getArpaListSelection();

    } {
        IPROF("Collate GUI data ");

        //Collate data to show in gui
        guiData->lat = getLat();
        guiData->radarOffsetX = radarCalculation.getOffsetXFraction(); //kyara
        guiData->radarOffsetY = radarCalculation.getOffsetYFraction();
        //kyara: position lat/long du curseur (portée + relèvement VRAI depuis le navire)
        {
            irr::f32 cRangeM = radarCalculation.getCursorRangeNm() * 1852.0f;
            irr::f32 cBrgRad = radarCalculation.getCursorBrg() * irr::core::DEGTORAD;
            irr::f32 cX = ownShip.getPosition().X + offsetPosition.X + cRangeM * sin(cBrgRad);
            irr::f32 cZ = ownShip.getPosition().Z + offsetPosition.Z + cRangeM * cos(cBrgRad);
            guiData->cursorLat = terrain.zToLat(cZ);
            guiData->cursorLong = terrain.xToLong(cX);
        }
        guiData->longitude = getLong();
        guiData->hdg = ownShip.getHeading();

        irr::core::vector3df cameraForwardVector = camera.getForwardVector();
        guiData->viewAngle = atan2(cameraForwardVector.X, cameraForwardVector.Z) * irr::core::RADTODEG;
        guiData->viewElevationAngle = asin(cameraForwardVector.Y) * irr::core::RADTODEG;
        // KYARA: dim the flash a little in the interior bridge views (0/1) - the cabin is partly
         // shadowed, so a strike reads softer inside than out on deck. LOCAL/display dim only: the
         // raw lightningFlash that getLightningFlash() sends to the secondary stays full, so every
         // screen dims by ITS OWN view, not the primary's.
        {
            irr::u32 flashView = camera.getView();
            bool flashInside = (flashView == 0 || flashView == 1);
            guiData->lightningFlash = lightningFlash * (flashInside ? 0.6f : 1.0f);
        }
        guiData->spd = ownShip.getSpeedThroughWater();
        guiData->cog = ownShip.getCOG(); //kyara: instrument console
        guiData->portRPM = ownShip.getPortEngineRPM(); //kyara: shaft tachometer
        guiData->stbdRPM = ownShip.getStbdEngineRPM();
        guiData->maxRPM = ownShip.getMaxEngineRPM();
        guiData->sog = ownShip.getSOG(); //kyara: instrument console (m/s)
        guiData->portEng = ownShip.getPortEngine();
        guiData->stbdEng = ownShip.getStbdEngine();
        guiData->rudder = ownShip.getRudder();  // inner workings of this will be modified in model DEE
        guiData->wheel = ownShip.getWheel();    // inner workings of this will be modified in model DEE
        guiData->portAzimuthAngle = ownShip.getPortAzimuthAngle();
        guiData->stbdAzimuthAngle = ownShip.getStbdAzimuthAngle();
        guiData->azimuth1Master = ownShip.getAzimuth1Master();
        guiData->azimuth2Master = ownShip.getAzimuth2Master();
        guiData->bowThruster = ownShip.getBowThruster();
        guiData->sternThruster = ownShip.getSternThruster();
        guiData->depth = ownShip.getDepth();
        guiData->weather = weather;
        // KYARA HOULE: attitude for the TANGAGE / GITE dials, and the sea for the weather tab
        guiData->pitch = ownShip.getPitch();
        guiData->roll = ownShip.getRoll();
        guiData->motionScale = motionScale;
        guiData->swellHs = swell.getSignificantHeight();
        guiData->seaHs = swell.getSeaStateHeight();
        guiData->swellTp = swell.getPeakPeriod();
        guiData->swellDirFrom = swell.getDirectionFrom();
        //storm 

        guiData->rain = rainIntensity;
        guiData->visibility = visibilityRange;
        guiData->windDirection = windDirection;
        guiData->windSpeed = windSpeed;
        guiData->streamDirection = streamOverrideDirection;
        guiData->streamSpeed = streamOverrideSpeed;
        guiData->streamOverride = streamOverride;
        guiData->radarRangeNm = radarCalculation.getRangeNm();
        guiData->radarRangeRingBrightness = radarCalculation.getRangeRingBrightness(); //kyara: so GUI labels dim/hide with the rings
        guiData->radarGain = radarCalculation.getGain();
        guiData->radarClutter = radarCalculation.getClutter();
        guiData->radarRain = radarCalculation.getRainClutter();

        guiData->guiRadarEBLBrg[0] = radarCalculation.getEBLBrg(0);
        guiData->guiRadarEBLBrg[1] = radarCalculation.getEBLBrg(1);
        guiData->guiRadarVRMNm[0] = radarCalculation.getVRMRangeNm(0);
        guiData->guiRadarVRMNm[1] = radarCalculation.getVRMRangeNm(1);
        guiData->guiRadarActiveEBL = radarCalculation.getActiveEBL();
        guiData->guiRadarActiveVRM = radarCalculation.getActiveVRM();
        guiData->guiRadarGuardAlarmMode = radarCalculation.getGuardAlarmMode();





        guiData->guiRadarCursorBrg = radarCalculation.getCursorBrg();
        guiData->guiRadarCursorRangeNm = radarCalculation.getCursorRangeNm();
        guiData->currentTime = Utilities::timestampToString(absoluteTime);
        guiData->distressActive = distressActive;   // Inc 3 (comms)
        guiData->maydayLines = maydayLines;
        guiData->commsLog = commsLog;
        guiData->distressTimer = distressTimer;   // Inc 3: countdown atop the comms box
        guiData->paused = paused;
        guiData->collided = collided;
        guiData->proxyAlarmMuted = proxyAlarmMuted;   //KYARA: so the GUI can show the silenced-alarm badge
        guiData->headUp = radarCalculation.getHeadUp();
        guiData->radarStabilised = radarCalculation.getStabilised();   //kyara: distingue ROUTE (course-up) de CAP (head-up)
        guiData->guiRadarRingLevel = radarCalculation.getRangeRingBrightness();
        guiData->guiRadarEchoStretch = radarCalculation.getEchoStretch();
        guiData->radarOn = radarCalculation.isRadarOn();
        guiData->pump1On = ownShip.getRudderPumpState(1);
        guiData->pump2On = ownShip.getRudderPumpState(2);


        // DEE_NOV22 vvvv
        guiData->schottelPort = ownShip.getPortSchottel();
        guiData->schottelStbd = ownShip.getStbdSchottel();

        guiData->azimuthEnginePort = azimuthToInputEngineMapping(ownShip.getPortEngine());
        guiData->azimuthEngineStbd = azimuthToInputEngineMapping(ownShip.getStbdEngine());

        guiData->azimuthClutchPort = ownShip.getPortClutch();
        guiData->azimuthClutchStbd = ownShip.getStbdClutch();

        guiData->emergencySteering = !(ownShip.getFollowUpRudderWorking());

        // DEE_NOV22 ^^^^

            // DEE FEB 23 vvv
        guiData->tideHeight = tideHeight;
        // DEE FEB 23 ^^^

    // DEE vvvv units are rad per second
        guiData->RateOfTurn = ownShip.getRateOfTurn();
        // DEE ^^^^

// PER-BOAT CAMERA-VIEW SOUND SWITCHING
// The active boat's sounds are already loaded from its model folder (see getOwnShip*Sound()).
// This block decides which of those sounds to bring up/down based on the current camera view:
//
//   Views 0 & 1   → interior bridge positions   → play inside_boat.wav,  mute outside & wave
//   High/top view → overhead camera             → play Bwave.wav,        mute inside & outside
//   All others    → exterior (stern/port/stbd)  → play outside_boat.wav, mute inside & wave
//
// The engine sound (Engine.wav) plays on ALL views — its volume tracks the throttle.
// The collision and proxy alarm sounds are also always ready regardless of view.
        {
            irr::u32 currentView = camera.getView();
            std::vector<bool> highViews = ownShip.getCameraIsHighView();
            bool isTopView = (currentView < highViews.size()) && highViews[currentView];

            if (currentView == 0 || currentView == 1) {
                // Interior bridge views: inside ambient on, everything else off
                sound->setVolumeInside(1.0f);
                sound->setVolumeOutside(0.0f);
                sound->setVolumeWave(0.0f);
            }
            else if (isTopView) {
                // Overhead view: default wave/water sound on, inside/outside off
                sound->setVolumeInside(0.0f);
                sound->setVolumeOutside(0.0f);
                sound->setVolumeWave(1.0f);
            }
            else {
                // Exterior views (stern, port quarter, starboard quarter, etc.): outside ambient on
                sound->setVolumeInside(0.0f);
                sound->setVolumeOutside(1.0f);
                sound->setVolumeWave(0.0f);
            }

        }

        //--------------- END

    } {
        IPROF("Own ship lights");
        //KYARA FEUX: the own ship's lamps, lit according to her situation. Other ships do this
        //inside their own update.
        ownShip.getLights().update(scenarioTime, light.getLightLevel(), fabs(ownShip.getSpeed()) > 0.2f);

    } {
        IPROF("Slam sound");
        //KYARA SLAM ---------------------------------------------------------------------------
        //One event, one sound: OwnShip reports the moment the tangage drops below normal trim
        //while still falling - she has just come down on the water. Severity (how fast the bow
        //was going down) sets how loud and how deep it sounds, and how big the spray is.
        if (slamCooldown > 0.0f) { slamCooldown -= deltaTime; }

        irr::f32 slamSeverity = 0.0f;
        irr::f32 slamBowSpeed = 0.0f;
        if (ownShip.consumeSlam(slamSeverity, slamBowSpeed)) {

            //A much harder landing may interrupt one already sounding; similar ones wait their turn.
            if (slamCooldown <= 0.0f || slamSeverity > 1.8f) {

                irr::f32 loudness = slamSeverity * slamSeverity / 2.25f; // impact pressure ~ v^2
                if (loudness > 1.0f) { loudness = 1.0f; }
                irr::f32 gain = 0.25f + 0.75f * loudness;

                //A heavy landing is a deep boom, a gentle one a slap.
                irr::f32 pitch = 1.14f - 0.16f * slamSeverity;
                if (pitch < 0.82f) { pitch = 0.82f; }
                if (pitch > 1.20f) { pitch = 1.20f; }
                pitch *= 0.97f + 0.06f * ((irr::f32)std::rand() / (irr::f32)RAND_MAX);

                //Heard from inside the wheelhouse it is duller and quieter than on the bridge wing.
                if (viewIsInside()) {
                    gain *= 0.55f;
                    pitch *= 0.94f;
                }

                sound->triggerSlam(gain, pitch);
                slamCooldown = 0.40f;

                //Spray: thrown from where the bow meets the water, on the surface as it is at
                //that instant (tide + waves), so the sheet starts at the waterline and not in
                //mid-air or under the sea.
                const irr::f32 hdg = ownShip.getHeading() * irr::core::DEGTORAD;
                const irr::f32 bowX = ownShip.getPosition().X + 0.45f * ownShip.getLength() * sin(hdg);
                const irr::f32 bowZ = ownShip.getPosition().Z + 0.45f * ownShip.getLength() * cos(hdg);
                const irr::f32 bowY = tideHeight + getWaveHeight(bowX, bowZ);
                splash.trigger(irr::core::vector3df(bowX, bowY, bowZ), slamSeverity, slamBowSpeed,
                    ownShip.getHeading(), ownShip.getLength(), ownShip.getBreadth());

                //And on a hard landing, the sheet comes back over the wheelhouse and lands on the
                //glass. It is real geometry on a pane at the window now, so it is collected
                //whatever view you are in - step outside and back and the water is still there.
                screenSpray.trigger(slamSeverity);
            }
        }
        splash.update(deltaTime);
        screenSpray.update(deltaTime);
        //KYARA SLAM ^^^^

    } {
        IPROF("Update gui data");
        //send data to gui
        guiMain->updateGuiData(guiData); //Set GUI heading in degrees and speed (in m/s)
    }
}
void SimulationModel::updateTows(irr::f32 deltaTime)
{
    updateRescueRun(deltaTime);   // SAR RESCUE RUN
    if (deltaTime <= 0.0f) { return; }

    const int numOther = (int)otherShips.getNumber();
    if (numOther <= 0) { return; }

    // --- Tunables ---
    const irr::f32 TOW_MAX_TURN_RATE = 6.0f;   // deg/s the towed bow may swing toward the line
    const irr::f32 TOW_GAIN = 0.6f;   // fraction of overshoot removed per frame
    const irr::f32 TOW_SLACK_MARGIN = 0.5f;   // m of extension before a line counts as taut
    const irr::f32 TOW_MAX_SPEED = 8.0f;   // m/s clamp on pull speed (anti-yank)
    const irr::f32 TOW_MIN_LENGTH = 10.0f;  // m: never pull attach points closer than this

    irr::scene::ISceneNode* ownNode = getOwnShipSceneNode();
    if (!ownNode) { return; }
    ownNode->updateAbsolutePosition();

    Lines* linesPtr = getLines();
    const int numLines = (int)linesPtr->getNumberOfLines();

    for (int s = 0; s < numOther; s++) {

        irr::scene::ISceneNode* towNode = getOtherShipSceneNode(s);
        if (!towNode) { continue; }
        towNode->updateAbsolutePosition();
        irr::core::vector3df towCentre = towNode->getAbsolutePosition();

        bool connected = false;   // a line-to-vessel joins own ship to this vessel (taut OR slack)
        bool anyTaut = false;   // ...and at least one such line is stretched past nominal
        irr::f32 worstOvershoot = 0.0f;
        irr::core::vector3df ownAttach, towAttach;

        for (int l = 0; l < numLines; l++) {
            if (linesPtr->getLineEndType(l) != 2) { continue; } // 2 == other ship
            if (linesPtr->getLineEndID(l) != s) { continue; } // this vessel

            connected = true; // a line is attached; hold the tow even if it goes slack

            if (linesPtr->getKeepSlack(l)) { continue; } // slack-kept line exerts no pull

            irr::f32 nominal = linesPtr->getLineNominalLength(l);
            if (nominal < TOW_MIN_LENGTH) { nominal = TOW_MIN_LENGTH; }

            irr::core::vector3df a = linesPtr->getLineStartAbsolutePosition(l); // own ship attach
            irr::core::vector3df b = linesPtr->getLineEndAbsolutePosition(l);   // towed attach
            irr::core::vector3df v = a - b; v.Y = 0.0f;
            irr::f32 dist = v.getLength();
            if (dist <= 0.0f) { continue; }

            irr::f32 overshoot = dist - nominal;
            if (overshoot > TOW_SLACK_MARGIN && overshoot > worstOvershoot) {
                anyTaut = true;
                worstOvershoot = overshoot;
                ownAttach = a;
                towAttach = b;
            }
        }

        if (!connected) { continue; } // no tow line: resume normal leg behaviour

        // Defaults while connected but slack: HOLD current heading & position (no snap to legs).
        irr::f32 curHdg = getOtherShipHeading(s);
        irr::f32 newHdg = curHdg;
        irr::core::vector3df newCentre = towCentre;
        irr::f32 towSpeed = 0.0f;

        if (anyTaut) {
            irr::core::vector3df pull = ownAttach - towAttach; pull.Y = 0.0f;
            irr::f32 dist = pull.getLength();
            if (dist > 0.0f) {
                irr::core::vector3df dir = pull / dist;

                irr::f32 move = worstOvershoot * TOW_GAIN;
                irr::f32 maxStep = TOW_MAX_SPEED * deltaTime;
                if (move > maxStep) { move = maxStep; }
                irr::core::vector3df correction = dir * move;

                // Weathervane the bow toward the tug, rate-limited. hdg: 0=+Z, 90=+X.
                irr::f32 targetHdg = irr::core::RADTODEG * std::atan2(dir.X, dir.Z);
                irr::f32 dHdg = targetHdg - curHdg;
                while (dHdg < -180.0f) { dHdg += 360.0f; }
                while (dHdg > 180.0f) { dHdg -= 360.0f; }
                irr::f32 maxTurn = TOW_MAX_TURN_RATE * deltaTime;
                if (dHdg > maxTurn) { dHdg = maxTurn; }
                if (dHdg < -maxTurn) { dHdg = -maxTurn; }
                newHdg = curHdg + dHdg;
                while (newHdg < 0) { newHdg += 360.0f; }
                while (newHdg >= 360.0f) { newHdg -= 360.0f; }

                // Pivot about the attach point, not the centre.
                irr::core::vector3df offW = towAttach - towCentre; offW.Y = 0.0f;
                irr::f32 ch = curHdg * irr::core::DEGTORAD;
                irr::f32 sch = sin(ch), cch = cos(ch);
                irr::f32 lx = offW.X * cch - offW.Z * sch;
                irr::f32 lz = offW.X * sch + offW.Z * cch;
                irr::f32 nh = newHdg * irr::core::DEGTORAD;
                irr::f32 snh = sin(nh), cnh = cos(nh);
                irr::core::vector3df newOffW;
                newOffW.X = lz * snh + lx * cnh;
                newOffW.Z = lz * cnh - lx * snh;
                newOffW.Y = 0.0f;

                irr::core::vector3df targetAttach = towAttach + correction;
                newCentre = targetAttach - newOffW;

                towSpeed = (newCentre - towCentre).getLength() / deltaTime;
                if (towSpeed > TOW_MAX_SPEED) { towSpeed = TOW_MAX_SPEED; }
            }
        }

        otherShips.setTowState(s, true, newCentre.X, newCentre.Z, newHdg);
        otherShips.setSpeed(s, towSpeed);
    }
}

//KYARA COLLISION 
irr::f32 SimulationModel::nearestOtherShipDistance() const
{
    irr::f32 nearest = 1.0e30f; // large default when there are no other ships
    irr::core::vector3df ownPos = ownShip.getPosition();
    irr::u32 number = otherShips.getNumber();
    for (irr::u32 i = 0; i < number; i++) {
        irr::core::vector3df otherPos = otherShips.getPosition(i);
        irr::f32 dx = otherPos.X - ownPos.X;
        irr::f32 dz = otherPos.Z - ownPos.Z;
        irr::f32 dist = sqrt(dx * dx + dz * dz);
        if (dist < nearest) {
            nearest = dist;
        }
    }
    return nearest;
}

bool SimulationModel::checkOwnShipCollision()
{

    //return (ownShip.isBuoyCollision() || ownShip.isOtherShipCollision());
    //KYARA UPDATE COLLISION 
    return (ownShip.isBuoyCollision() || ownShip.isOtherShipCollision() || ownShip.isTerrainCollision());

    /*

    irr::u32 numberOfOtherShips = otherShips.getNumber();
    irr::u32 numberOfBuoys = buoys.getNumber();

    irr::core::vector3df thisShipPosition = ownShip.getPosition();
    irr::f32 thisShipLength = ownShip.getLength();
    irr::f32 thisShipWidth = ownShip.getWidth();
    irr::f32 thisShipHeading = ownShip.getHeading();

    for (irr::u32 i = 0; i<numberOfOtherShips; i++) {
        irr::core::vector3df otherPosition = otherShips.getPosition(i);
        irr::f32 otherShipLength = otherShips.getLength(i);
        irr::f32 otherShipWidth = otherShips.getWidth(i);
        irr::f32 otherShipHeading = otherShips.getHeading(i);

        irr::core::vector3df relPosition = otherPosition - thisShipPosition;
        irr::f32 distanceToShip = relPosition.getLength();
        irr::f32 bearingToOtherShipDeg = irr::core::radToDeg(atan2(relPosition.X, relPosition.Z));

        //Bearings relative to ship's head (from this ship and from other)
        irr::f32 relativeBearingOwnShip = bearingToOtherShipDeg - thisShipHeading;
        irr::f32 relativeBearingOtherShip = 180 + bearingToOtherShipDeg - otherShipHeading;

        //Find the minimum distance before a collision occurs
        irr::f32 minDistanceOwn = 0.5*fabs(thisShipWidth*sin(irr::core::degToRad(relativeBearingOwnShip))) + 0.5*fabs(thisShipLength*cos(irr::core::degToRad(relativeBearingOwnShip)));
        irr::f32 minDistanceOther = 0.5*fabs(otherShipWidth*sin(irr::core::degToRad(relativeBearingOtherShip))) + 0.5*fabs(otherShipLength*cos(irr::core::degToRad(relativeBearingOtherShip)));
        irr::f32 minDistance = minDistanceOther + minDistanceOwn;

        if (distanceToShip < minDistance) {
            return true;
        }
    }

    for (irr::u32 i = 0; i<numberOfBuoys; i++) { //Collision with buoy
        irr::core::vector3df otherPosition = buoys.getPosition(i);

        irr::core::vector3df relPosition = otherPosition - thisShipPosition;
        irr::f32 distanceToBuoy = relPosition.getLength();
        irr::f32 bearingToBuoyDeg = irr::core::radToDeg(atan2(relPosition.X, relPosition.Z));

        //Bearings relative to ship's head (from this ship and from other)
        irr::f32 relativeBearingOwnShip = bearingToBuoyDeg - thisShipHeading;

        //Find the minimum distance before a collision occurs
        irr::f32 minDistanceOwn = 0.5*fabs(thisShipWidth*sin(irr::core::degToRad(relativeBearingOwnShip))) + 0.5*fabs(thisShipLength*cos(irr::core::degToRad(relativeBearingOwnShip)));

        if (distanceToBuoy < minDistanceOwn) {
            return true;
        }
    }

    return false; //If no collision has been found
    */


}