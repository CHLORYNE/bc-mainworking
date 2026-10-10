/*   Bridge Command 5.0 Ship Simulator
     Copyright (C) 2014 James Packer

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation

     This program is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY Or FITNESS For A PARTICULAR PURPOSE.  See the
     GNU General Public License For more details.

     You should have received a copy of the GNU General Public License along
     with this program; if not, write to the Free Software Foundation, Inc.,
     51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. */

#ifndef __SIMULATIONMODEL_HPP_INCLUDED__
#define __SIMULATIONMODEL_HPP_INCLUDED__

#include <iostream> //For debugging
#include <string>
#include <vector>
#include <stdint.h> //for uint64_t

#include "irrlicht.h"

     //Forward declarations
class ScenarioData;
class GUIMain;
class GUIData;
class Sound;
class LoadingScreen; //KYARA CHARGEMENT

//KYARA METEO: highest sea state ("etat de mer", the weather value) the simulator accepts.
//Applied in setWeather() and when the scenario is loaded, so it caps the weather slider, the
//scenario files AND values arriving over the network. The weather slider's range follows it.
//Scale is the original Bridge Command one (0-12); storm effects start at 3.5, the
//"Mauvais temps" preset uses 4.0 - keep this at 4.0 or above so the preset is not clipped.
const irr::f32 SIM_MAX_WEATHER = 6.0f;

#include "Terrain.hpp"
#include "Light.hpp"
#include "Sky.hpp"
#include "Water.hpp"
#include "Swell.hpp"          // KYARA HOULE
#include "Splash.hpp"         // KYARA SLAM
#include "ScreenSpray.hpp"    // KYARA SLAM
#include "ShipLights.hpp"     // KYARA FEUX
#include "Rain.hpp"
#include "Snow.hpp"
#include "Tide.hpp"
#include "Buoys.hpp"
#include "OtherShips.hpp"
#include "LandObjects.hpp"
#include "LandLights.hpp"
#include "OwnShip.hpp"
#include "ManOverboard.hpp"
#include "Fire.hpp"          // FIRE FEATURE
#include "FireMonitor.hpp"   // FIRE FEATURE
#include "IncidentConfig.hpp" // SCENARIO INCENDIE: per-scenario fire / SAR settings (incident.ini)
#include "Camera.hpp"
#include "RadarCalculation.hpp"
#include "RadarScreen.hpp"
#include "ControlVisualiser.hpp"
#include "Lines.hpp"
#include "OperatingModeEnum.hpp"
#include "ExerciseLog.hpp"

class SimulationModel //Start of the 'Model' part of MVC
{

public:

    struct ModelParameters {
        OperatingMode::Mode mode;
        bool vrMode;
        irr::f32 viewAngle;
        irr::f32 lookAngle;
        irr::f32 cameraMinDistance;
        irr::f32 cameraMaxDistance;
        irr::u32 disableShaders;
        irr::u32 waterSegments;
        irr::u32 reflectionMode;   // KYARA: 0 full, 1 half (every 2nd frame), 2 off
        irr::core::vector3di numberOfContactPoints;
        irr::f32 minContactPointSpacing;
        irr::f32 contactStiffnessFactor;
        irr::f32 contactDampingFactor;
        irr::f32 lineStiffnessFactor;
        irr::f32 lineDampingFactor;
        irr::f32 frictionCoefficient;
        irr::f32 tanhFrictionFactor;
        irr::u32 limitTerrainResolution;
        bool secondaryControlWheel;
        bool secondaryControlPortEngine;
        bool secondaryControlStbdEngine;
        bool secondaryControlPortSchottel;
        bool secondaryControlStbdSchottel;
        bool secondaryControlPortThrustLever;
        bool secondaryControlStbdThrustLever;
        bool secondaryControlBowThruster;
        bool secondaryControlSternThruster;
        bool debugMode;

    };

    SimulationModel(irr::IrrlichtDevice* dev,
        irr::scene::ISceneManager* scene,
        GUIMain* gui,
        Sound* sound,
        ScenarioData scenarioData,
        ModelParameters modelParameters,
        LoadingScreen* loadingScreen = 0); //KYARA CHARGEMENT: optional, progress stages
    ~SimulationModel();
    irr::f32 longToX(irr::f32 longitude) const;
    irr::f32 latToZ(irr::f32 latitude) const;

    // Lat/long -> SCENE coordinates (re-centring offset removed). Use these for anything
// that touches scene nodes or setScriptedPose; the raw longToX/latToZ are absolute.
    irr::f32 longToSceneX(irr::f32 longitude) const;
    irr::f32 latToSceneZ(irr::f32 latitude) const;
    void setSpeed(irr::f32 spd); //Sets the own ship's speed
    void setHeading(irr::f32 hdg); //Sets the own ship's heading


    // KYARA DAY/NIGHT TOGGLE
    void     setLightingTimeOfDay(irr::f32 hourOfDay); // 0..24, lighting only
    irr::f32 getLightingTimeOfDay() const;

    irr::f32 getRateOfTurn() const;
    void setRateofTurn(irr::f32 rudder); //Set the rate of turn (-ve is port, +ve is stbd)



    void setRateOfTurn(irr::f32 rateOfTurn);
    void setPos(irr::f32 positionX, irr::f32 positionZ);
    void setRudder(irr::f32 rudder); //Set the rudder (-ve is port, +ve is stbd)
    void setWheel(irr::f32 wheel, bool force = false); //Set the wheel (-ve is port, +ve is stbd) DEE. If force is true, the wheel change is applied even if the follow up rudder is failed
    irr::f32 getRudder() const;
    irr::f32 getWheel() const; // DEE
    void setAzimuth1Master(bool isMaster); // Set if azimuth 1 should also control azimuth 2
    void setAzimuth2Master(bool isMaster); // Set if azimuth 2 should also control azimuth 1
    bool getAzimuth1Master() const;
    bool getAzimuth2Master() const;
    void setPortAzimuthAngle(irr::f32 angle); // Set the azimuth angle, in degrees (-ve is port, +ve is stbd)
    void setStbdAzimuthAngle(irr::f32 angle); // Set the azimuth angle, in degrees (-ve is port, +ve is stbd)

    // DEE_NOV22 vvvv for follow up shcottel and automatic clutch
    void setPortSchottel(irr::f32 angle); // Set Port Schottel angle
    irr::f32 getPortSchottel();
    void setStbdSchottel(irr::f32 angle); // Set Stbd Schottel angle
    irr::f32 getStbdSchottel();
    bool getPortClutch();
    void setPortClutch(bool);
    bool getStbdClutch();
    void setStbdClutch(bool);
    void engagePortClutch();
    void disengagePortClutch();
    void engageStbdClutch();
    void disengageStbdClutch();
    void setPortAzimuthThrustLever(irr::f32);   // sets port thrust lever range is 0..+1 or -1..+1
    irr::f32 getPortAzimuthThrustLever(); 	 // gets port thrust lever range is 0..+1 or -1..+1
    void setStbdAzimuthThrustLever(irr::f32);   // sets starboard thrust lever range is 0..+1 or -1..+1
    irr::f32 getStbdAzimuthThrustLever(); // gets starboard thrust lever range is 0..+1 or -1..+1

    void btnIncrementPortThrustLever(); // increments the port thrust lever
    void btnDecrementPortThrustLever(); // decrements the port thrust lever
    void btnIncrementStbdThrustLever(); // increments the stbd thrust lever
    void btnDecrementStbdThrustLever(); // decrements the stbd thrust lever

    void btnIncrementPortSchottel(); // clockwise turn of the port schottel in response to a key press
    void btnDecrementPortSchottel(); // anticlockwise turn of the port schottel in response to a key press
    void btnIncrementStbdSchottel(); // clockwise turn of the starboard schottel in response to a key press
    void btnDecrementStbdSchottel(); // anticlockwise turn of the starboard schottel in response to a key press

    // DEE_NOV22 ^^^^

    void setPortEngine(irr::f32 port); //Set the engine, (-ve astern, +ve ahead), range is +-1
    void setStbdEngine(irr::f32 stbd); //Set the engine, (-ve astern, +ve ahead), range is +-1
    irr::f32 getPortEngine() const; //Range +-1
    irr::f32 getStbdEngine() const; //Range +-1
    irr::f32 getPortEngineRPM() const;
    irr::f32 getStbdEngineRPM() const;
    void setBowThruster(irr::f32 proportion);
    void setSternThruster(irr::f32 proportion);
    void setBowThrusterRate(irr::f32 bowThrusterRate); //Sets rate of change, for joystick button control
    void setSternThrusterRate(irr::f32 sternThrusterRate); //Sets rate of change, for joystick button control
    irr::f32 getBowThruster() const;
    irr::f32 getSternThruster() const;
    void setRudderPumpState(int whichPump, bool rudderPumpState); //Sets how the rudder is responding. Assumed that whichPump can be 1 or 2
    bool getRudderPumpState(int whichPump) const;
    void setFollowUpRudderWorking(bool followUpRudderWorking); //Sets if the normal (follow up) rudder is working
    bool getFollowUpRudderWorking();
    void setAccelerator(irr::f32 accelerator); //Set simulation time compression
    irr::f32 getAccelerator() const;
    irr::f32 getHeading() const; //Gets the own ship's heading
    irr::f32 getPitch() const; //NAUTITECH: own ship pitch (tangage), degrees
    irr::f32 getRoll() const;  //NAUTITECH: own ship roll (roulis), degrees

    irr::f32 getLat() const;
    irr::f32 getLong() const;
    irr::f32 getPosX() const;
    irr::f32 getPosZ() const;
    irr::f32 getCOG() const;
    irr::f32 getSOG() const; //In metres/second
    irr::f32 getDepth() const;

    irr::f32 getWaveHeight(irr::f32 posX, irr::f32 posZ) const; //Return wave height (not tide) at the world position specified (KYARA HOULE: FFT chop + swell)
    // KYARA HOULE: swell and instructor motion scale
    const Swell& getSwell() const;
    void setMotionScale(irr::f32 scale); // 0..1.5, 1 = realistic (pitch/roll shown to the trainee)
    irr::f32 getMotionScale() const;
    void getSwellNetworkState(irr::f32 out[Swell::NET_FIELDS]) const;   // primary -> network
    void applySwellNetworkState(const irr::f32 in[Swell::NET_FIELDS]);  // network -> secondary
    irr::core::vector2df getLocalNormals(irr::f32 relPosX, irr::f32 relPosZ) const;

    irr::core::vector2df getTidalStream(irr::f32 longitude, irr::f32 latitude, uint64_t requestTime) const; //Tidal stream in m/s for the specified absolute position

    //void getTime(irr::u8& hour, irr::u8& min, irr::u8& sec) const;
    //void getDate(irr::u8& day, irr::u8& month, irr::u16& year) const;
    uint64_t getTimestamp() const; //The unix timestamp in s
    uint64_t getTimeOffset() const; //The timestamp at the start of the first day of the scenario
    irr::f32 getTimeDelta() const; //The change in time (s) since the start of the start day of the scenario
    void     setTimeDelta(irr::f32 scenarioTime);


    irr::u32 getNumberOfOtherShips() const;
    irr::u32 getNumberOfBuoys() const;
    std::string getOtherShipName(int number) const;
    irr::f32 getOtherShipPosX(int number) const;
    irr::f32 getOtherShipPosZ(int number) const;
    irr::f32 getOtherShipLat(int number) const;
    irr::f32 getOtherShipLong(int number) const;
    irr::f32 getOtherShipHeading(int number) const;
    irr::f32 getOtherShipSpeed(int number) const; //Speed in m/s
    // header:
    irr::f32 getOtherShipLength(int number) const;
    irr::f32 getOtherShipBreadth(int number) const;
    irr::u32 getOtherShipMMSI(int number) const;
    void setOtherShipHeading(int number, irr::f32 hdg);
    void setOtherShipPos(int number, irr::f32 positionX, irr::f32 positionZ);
    void setOtherShipRateOfTurn(int number, irr::f32 rateOfTurn);
    void setOtherShipAbsent(int number, bool absent);   //multiplayer: no student on this ship
    bool isOtherShipAbsent(int number) const;
    void setOtherShipSpeed(int number, irr::f32 speed); //Speed in m/s
    void setOtherShipMMSI(int number, irr::u32 mmsi);
    std::vector<Leg> getOtherShipLegs(int number) const;
    irr::f32 getBuoyPosX(int number) const;
    irr::f32 getBuoyPosZ(int number) const;
    void changeOtherShipLeg(int shipNumber, int legNumber, irr::f32 bearing, irr::f32 speed, irr::f32 distance);
    void addOtherShipLeg(int shipNumber, int afterLegNumber, irr::f32 bearing, irr::f32 speed, irr::f32 distance);
    void deleteOtherShipLeg(int shipNumber, int legNumber);
    void resetOtherShipLegs(int shipNumber, irr::f32 course, irr::f32 speedKts, irr::f32 distanceNm);
    std::string getOwnShipEngineSound() const;
    std::string getOwnShipWaveSound() const;
    std::string getOwnShipHornSound() const;
    std::string getOwnShipAlarmSound() const;
    std::string getOwnShipRainSound() const;
    std::string getOwnShipStormSound() const;
    std::string getOwnShipThunderSound() const;
    std::string getOwnShipThunderboltSound() const;
    void setBadWeatherPreset();                 // KYARA "mauvais temps"
    irr::f32 getLightningFlash() const;         // 0..1, synced to the secondary
    void setLightningFlash(irr::f32 v);         // used by the secondary from the network
    void setCogSogFromNetwork(irr::f32 cogDeg, irr::f32 sogKts);      // secondary: the primary's COG / SOG
    //Secondary: what the primary shows, so its screens look the same (palette -1 / glow -1: not received)
    void setNetworkDisplay(int paletteMode, int glowLevel) { networkPalette = paletteMode; networkGlow = glowLevel; }
    void setNetworkSquall(irr::f32 level) { networkSquall = level; }
    irr::f32 getSquallLevel() const { return squallLevel; }
    void setThunderEnabled(bool e);             // KYARA: enable/disable thunder
    bool getThunderEnabled() const;
    void setLightningEnabled(bool e);
    bool getLightningEnabled() const;
    //KYARA PROXY AND COLLISION 
    std::string getOwnShipProxyAlarmSound() const;
    //kyara: radar guard-zone alarm sound (independent of proxy alarm)

    std::string getOwnShipCollisionSound() const;

    //KYARA INSIDE OUTSIDE 
    std::string getOwnShipInsideSound() const;
    std::string getOwnShipOutsideSound() const;

    //KYARA SEAGULL
    std::string getOwnShipSeagullSound() const;
    // FIRE FEATURE
    std::string getOwnShipFireBurningSound() const;
    std::string getOwnShipWaterSound() const;
    std::string getOwnShipFireAlarmSound() const;
    std::string getOwnShipAbandonAlarmSound() const;
    std::string getOwnShipExplosionSound() const;
    std::string getOwnShipGroanSound() const;
    std::string getOwnShipSteamSound() const;
    std::string getOwnShipVhfSound() const;
    void toggleMonitorFiring();   // FIRE FEATURE: Ctrl+E on/off, auto-aims at the blaze
    bool isMonitorFiring() const;
    //KYARA CONTACT SOUND
    std::string getOwnShipContactSound() const;
    //KYARA FEUX: COLREG light situations. Situations are ShipLights::Situation values.
    void setOwnShipLightSituation(int situation);
    int getOwnShipLightSituation() const;
    void setOwnShipDeckLights(bool on);
    void setOwnShipInstrumentLights(int level); // 0 off, 1 dim, 2 bright
    int getOwnShipInstrumentLights() const;
    int getOwnShipInstrumentMaterialCount() const; //KYARA FEUX TAB
    //KYARA FEUX TAB: the lights of one vessel. -1 = own ship, 0.. = other ship. 0 if none.
    ShipLights* getShipLights(int vessel);
    //KYARA FEUX EDIT: in-simulator placement of a vessel's lamps (-1 = own ship)
    bool beginLightEdit(int vessel);
    void endLightEdit();
    bool isLightEditing() const;
    int getLightEditVessel() const;
    void lightEditOrbit(irr::f32 dYawDeg, irr::f32 dPitchDeg, irr::f32 zoomFactor);
    //Size (ScaleFactor) and waterline (YCorrection) of a vessel, changed live and
    //written back to her boat.ini. -1 = own ship. Other ships using the same boat.ini are the
    //same model, so they all change together.
    struct VesselSize {
        std::string iniFile;
        irr::f32 scale = 1.0f;
        irr::f32 yCorrection = 0.0f;
        irr::f32 length = 0.0f;     //overall, metres
        irr::f32 breadth = 0.0f;
        irr::f32 draught = 0.0f;    //waterline down to the lowest point of the model
        irr::f32 airDraught = 0.0f; //waterline up to the highest point
        int sharing = 1;            //vessels in this scenario drawn from that boat.ini
    };
    bool beginSizeEdit(int vessel);
    void endSizeEdit();
    bool isSizeEditing() const;
    int getSizeEditVessel() const;
    bool getVesselSize(int vessel, VesselSize& out);
    void sizeEditSetLength(irr::f32 metres);
    void sizeEditStepLength(irr::f32 deltaMetres);
    void sizeEditSetDraught(irr::f32 metres);
    void sizeEditStepDraught(irr::f32 deltaMetres); //positive = deeper in the water
    void sizeEditRevert();                          //back to the values at opening
    bool sizeEditSave(std::wstring& message);
    int getSizeEditRevision() const;                //changes whenever the size does
    //Instrument lighting editor (own ship): click a screen or gauge in the view to light it at
    //night; saved as InstrumentMaterials= in her boat.ini.
    bool beginInstrumentEdit();
    void endInstrumentEdit();
    bool isInstrumentEditing() const;
    int instrumentEditPick(const irr::core::line3df& ray); //lights or darkens what was clicked; its material, or -1
    void instrumentEditToggle(int material);
    void instrumentEditSelect(int material);              //flashes it for a moment
    int getInstrumentEditSelected() const;
    void instrumentEditRevert();                          //back to the list at opening
    bool instrumentEditSave(std::wstring& message);
    int getInstrumentEditRevision() const;
    irr::u32 getOwnShipMaterialCount() const;
    std::string getOwnShipMaterialTexture(irr::u32 material) const;
    bool isOwnShipInstrumentMaterial(irr::u32 material) const;
    //Free view: a camera circling the own ship (mouse drag turns, wheel zooms), for any ship
    //without views in boat.ini. It comes after the last boat.ini view in the "change view" cycle.
    bool isFreeView() const;
    void setFreeView(bool on);
    bool getOwnShipDeckLights() const;
    void setOtherShipLightSituation(int shipNumber, int situation);
    int getOtherShipLightSituation(int shipNumber) const;

    //KYARA SLAM: one sample, played when the bow comes down on the water
    std::string getOwnShipSlamSound() const;
    //bc5.ini screen_spray: 0 = off, 1 = normal, 2 = test (fires on every landing)
    void setScreenSprayMode(int mode);
    std::string getOwnShipRadarAlarmSound() const;
    std::string getOwnShipCpaAlarmSound() const;   //radar CPA/TCPA alarm
    std::string getOwnShipDepthAlarmSound() const; //echo sounder shallow-water alarm
    void triggerSeagull(); //Plays the seagull one-shot sound (bound to the 'G' key)
    //MUTE ALARM 
    void toggleProxyAlarmMute();
    bool getProxyAlarmMuted() const;


    void setWeather(irr::f32 weather); //Range 0-12.
    irr::f32 getWeather() const;
    void setRain(irr::f32 rainIntensity); //Range 0-10
    irr::f32 getRain() const;
    void setVisibility(irr::f32 visibilityNm);
    irr::f32 getVisibility() const;
    void setWindDirection(irr::f32 windDirection); //Range 0-360.
    irr::f32 getWindDirection() const;
    void setWindSpeed(irr::f32 windSpeed); //Nm/h
    irr::f32 getWindSpeed() const;              //with the gusts, as the ship feels it
    irr::f32 getWindSpeedBase() const;          //the mean wind set in the weather (no gusts)
    irr::f32 getWindDirectionBase() const;      //the mean direction (no variation)

    //WEATHER WINDOW (METEO). Everything the weather is made of, in one block: a preset or the
    //sliders set it, at once or over a while (setWeatherState with seconds > 0). Setting any
    //single value by hand stops a change or a front that is under way.
    struct WeatherState
    {
        irr::f32 cloud = 0.0f;          //cloud cover 0..1 (over the fair-weather sky)
        irr::f32 windKn = 0.0f;         //mean wind, knots
        irr::f32 windDir = 0.0f;        //direction it blows FROM, degrees
        irr::f32 windVariation = 0.0f;  //how far its direction swings either side, degrees
        irr::f32 gustKn = 0.0f;         //gusts, knots above the mean
        irr::f32 visibilityNm = 10.0f;
        irr::f32 rain = 0.0f;           //0..10
        irr::f32 snow = 0.0f;           //0..1
        irr::f32 dust = 0.0f;           //sand / dust in the air 0..1
        irr::f32 sea = 0.0f;            //sea state (weather) 0..12
    };
    WeatherState getWeatherState() const;
    void setWeatherState(const WeatherState& state, irr::f32 seconds);
    bool isWeatherChanging() const;             //a gradual change (or a front) is under way
    irr::f32 getWeatherChangeProgress() const;  //0..1
    void setCloudCover(irr::f32 cover);   irr::f32 getCloudCover() const;
    void setSnow(irr::f32 snow);          irr::f32 getSnow() const;
    void setDust(irr::f32 dust);          irr::f32 getDust() const;
    void setWindGust(irr::f32 knots);     irr::f32 getWindGust() const;
    void setWindVariation(irr::f32 deg);  irr::f32 getWindVariation() const;
    //Significant weather: 0 auto (a storm comes with a heavy sea, as before), 1 none, 2 thunderstorm, 3 squalls
    void setSignificantWeather(int mode); int getSignificantWeather() const;
    //Approaching weather front: 0 none, 1 slow (about an hour), 2 fast (about 20 minutes). The weather
    //worsens to the front, holds, then clears back to what it was.
    void setWeatherFront(int mode);       int getWeatherFront() const;
    irr::f32 getWeatherFrontProgress() const; //0..1 over the whole front
    void setStreamOverrideDirection(irr::f32 streamDirection); //Range 0-360.
    irr::f32 getStreamOverrideDirection() const;
    void setStreamOverrideSpeed(irr::f32 streamSpeed); //Nm/h
    irr::f32 getStreamOverrideSpeed() const;
    void setStreamOverride(bool streamOverride);
    bool getStreamOverride() const;
    void setWaterVisible(bool visible);
    void lookUp();
    void lookDown();
    void lookLeft();
    void lookRight();
    void setPanSpeed(irr::f32 horizontalPanSpeed);
    void setVerticalPanSpeed(irr::f32 verticalPanSpeed);
    void changeLookPx(irr::s32 deltaX, irr::s32 deltaY);
    void lookStepLeft();
    void lookStepRight();
    void moveCameraForwards();
    void moveCameraBackwards();
    void lookAhead();
    void lookAstern();
    void lookPort();
    void lookStbd();
    void changeView();
    void setView(irr::u32 view);
    irr::u32 getCameraView() const;
    irr::core::vector3df getCameraBasePosition() const;
    irr::core::matrix4 getCameraBaseRotation() const;
    void setFrozenCamera(bool frozen);
    void toggleFrozenCamera();
    void setAlarm(bool alarmState);
    //The radar's settings and targets (clicks on the scope, CPA alarm, trial manoeuvre, lines...)
    RadarCalculation& getRadar() { return radarCalculation; }
    //Longitude / latitude of a point in scene coordinates (the scene is re-centred on own ship, so
    //the offset has to be added before terrain.xToLong / zToLat)
    irr::f32 sceneXToLong(irr::f32 x) const;
    irr::f32 sceneZToLat(irr::f32 z) const;

    //Exercise record for the debrief (track, closest approaches, events). The report is written in
    //the user folder, Bilans/, at the end of the exercise or when the instructor asks for it.
    ExerciseLog& getExerciseLog() { return exerciseLog; }

    //Failures the instructor can give, in two grades:
    // - degraded (FAILURE_DEGRADED): an engine gives half power; the gyro drifts slowly and the GPS
    //   position wanders off, both without any alarm (the student has to find it by cross-checking);
    // - failed (FAILURE_FAILED): an engine stops; the gyro and GPS displays freeze ("lost" shown);
    //   the radar goes off and cannot be switched on.
    //What the sensors give (displays, NMEA) comes from getGyroHeading() and getGpsFix().
    enum Failure { FAIL_PORT_ENGINE, FAIL_STBD_ENGINE, FAIL_GYRO, FAIL_GPS, FAIL_RADAR, FAIL_COUNT };
    enum FailureLevel { FAILURE_NONE = 0, FAILURE_DEGRADED = 1, FAILURE_FAILED = 2 };
    void setFailure(Failure which, bool failed) { setFailureLevel(which, failed ? FAILURE_FAILED : FAILURE_NONE); }
    bool getFailure(Failure which) const { return getFailureLevel(which) == FAILURE_FAILED; }
    void setFailureLevel(Failure which, int level);
    int getFailureLevel(Failure which) const;
    irr::f32 getGyroError() const { return gyroError; }          //degrees, + = reads high
    irr::f32 getGpsErrorMetres() const;                           //distance of the GPS position from the true one
    //Sensor readings, failures included
    irr::f32 getGyroHeading() const;
    bool isGyroValid() const { return failureLevel[FAIL_GYRO] < FAILURE_FAILED; }
    void getGpsFix(irr::f32& lat, irr::f32& lon, irr::f32& cogDeg, irr::f32& sogMps) const;
    bool isGpsValid() const { return failureLevel[FAIL_GPS] < FAILURE_FAILED; }

    //Failures given later (the student must not see it coming): an action (a Failure, or one of the
    //steering ones below) to a level, after delaySeconds of exercise time.
    enum { ACTION_PUMP_1 = 100, ACTION_PUMP_2 = 101, ACTION_FOLLOW_UP = 102 };
    struct ScheduledFailure { int action; int level; irr::f32 at; };
    void scheduleFailure(int action, int level, irr::f32 delaySeconds);
    void cancelScheduledFailure(int action);
    const std::vector<ScheduledFailure>& getScheduledFailures() const { return scheduledFailures; }
    irr::f32 getFailureClock() const { return failureClock; }
    static std::wstring failureActionName(int action, int level);
    //Everything back in service (failures, steering, what was scheduled)
    void repairAll();
    //The instructor notes that the student has reported the last failure: its detection time goes in the debrief
    void markFailureReported();

    //Multiplayer instructor station (the hub): failures it gives (now, later, or back in service),
    //the weather it imposes (the student's weather window is then locked), messages it sends,
    //and the state reported back to it.
    void instructorFailure(int action, int level, irr::f32 delaySeconds);
    void setWeatherByInstructor(bool on) { weatherByInstructor = on; }
    bool getWeatherByInstructor() const { return weatherByInstructor; }
    void showInstructorMessage(const std::wstring& text);
    std::string instructorStatus();

    //Bridge alert list (like a central alert panel): one line per active alarm condition, red until
    //acknowledged. Acknowledge (the console's button) acknowledges them all and silences the alarms.
    struct BridgeAlert { int id; std::wstring text; irr::f32 since; bool acked; };
    const std::vector<BridgeAlert>& getBridgeAlerts() const { return bridgeAlerts; }
    void acknowledgeAlerts();

    //Sound signals on the whistle (COLREG rules 34 and 35): count short blasts (1, 2, 3, 5), or
    //count < 0 for prolonged blasts. Fog signals: automatic, every 2 minutes, one prolonged blast
    //making way, two when stopped.
    void soundSignal(int count);
    void setFogSignals(bool on);
    bool getFogSignals() const { return fogSignalsOn; }

    //Echo sounder alarm (metres under the keel, 0 = off)
    void setDepthAlarm(irr::f32 limitMetres);
    irr::f32 getDepthAlarm() const { return depthAlarmLimit; }

    //Man overboard mark: the position, kept with its bearing and distance shown on the GPS and radar
    void markManOverboard();
    void clearManOverboardMark();
    bool hasManOverboardMark() const { return mobMarked; }
    void logEvent(ExerciseLog::Category category, const std::wstring& text);
    bool writeExerciseReport(std::string& path);
    void toggleRadarOn();
    bool isRadarOn() const;
    //The radar picture's texture and the part of it shown (see RadarScreen::getTexture)
    irr::video::ITexture* getRadarTexture(irr::f32& scale, irr::f32& offset) const;
    irr::video::SColor getRadarSurroundColour() const;
    void increaseRadarRange();
    void decreaseRadarRange();
    void setRadarGain(irr::f32 value);
    void setRadarClutter(irr::f32 value);
    void setRadarRain(irr::f32 value);
    void increaseRadarGain(irr::f32 value);
    void decreaseRadarGain(irr::f32 value);
    void increaseRadarClutter(irr::f32 value);
    void decreaseRadarClutter(irr::f32 value);
    void increaseRadarRain(irr::f32 value);
    void decreaseRadarRain(irr::f32 value);
    void setPIData(irr::s32 PIid, irr::f32 PIbearing, irr::f32 PIrange);
    irr::f32 getPIbearing(irr::s32 PIid) const;
    irr::f32 getPIrange(irr::s32 PIid) const;
    void increaseRadarEBLRange();
    void decreaseRadarEBLRange();
    void increaseRadarEBLBrg();
    void decreaseRadarEBLBrg();
    void selectRadarEBL(); //kyara: toggle EBL1/EBL2
    void selectRadarVRM(); //kyara: toggle VRM1/VRM2
    void cycleRadarGuardAlarm();
    void cycleRadarRangeRings();   //kyara: anneaux de portée clair/faible/off
    int getRadarGuardAlarmMode() const;
    //kyara: radar state sync over network
    void setRadarEBLBrg(irr::u32 index, irr::f32 brg);
    void setRadarVRMRange(irr::u32 index, irr::f32 rangeNm);
    void setRadarActiveEBL(irr::u32 index);
    void setRadarActiveVRM(irr::u32 index);
    void setRadarGuardAlarmMode(int mode);
    irr::f32 getRadarEBLBrg(irr::u32 index) const;
    irr::f32 getRadarVRMRange(irr::u32 index) const;
    irr::u32 getRadarActiveEBL() const;
    irr::u32 getRadarActiveVRM() const;

    void increaseRadarXCursor();
    void decreaseRadarXCursor();
    void increaseRadarYCursor();
    void decreaseRadarYCursor();
    void setRadarNorthUp();
    void setRadarCourseUp();
    void setRadarHeadUp();
    void changeRadarColourChoice();
    int getArpaMode() const;
    void setArpaMode(int mode);
    void setRadarHeadingMode(bool useReal);
    void cycleRadarEchoStretch();
    int  getRadarEchoStretch() const;
    bool getRadarHeadingMode() const;
    void toggleRadarOffCentre();

    // NEW RADAR TOGGLES
    void setArpaOnBuoys(bool state);
    void setRadarBuoyTrails(bool state);
    void setRadarShipTrails(bool state);
    void setRadarOwnShipTrails(bool state);
    void setRadarMMSI(bool state);

    void setRadarBuoyMarkerColour(int paletteIndex);
    void setRadarShipMarkerColour(int paletteIndex);

    void setArpaListSelection(irr::s32 selection);
    void setRadarARPARel();
    void setRadarARPATrue();
    void setRadarARPAVectors(irr::f32 vectorMinutes);
    void setRadarDisplayRadius(irr::u32 radiusPx);
    //kyara: true only on the secondary instance that acts as the radar station (full_radar=1).
    //Only that instance sends radar state sync to the primary.
    void setSecondaryRadarMaster(bool isMaster);
    bool getIsSecondaryRadarMaster() const;
    void addManualPoint(bool newContact);
    void clearManualPoints();
    void trackTargetFromCursor();
    void clearTargetFromCursor();
    irr::u32 getARPATracksSize() const;
    ARPAContact getARPAContactFromTrackIndex(irr::u32 index) const;
    void setMainCameraActive();
    void setTripleScreen(bool on, irr::f32 perScreenFOVdeg, irr::f32 bezelYawDeg); //NAUTITECH
    irr::core::line3df getMooringRay(irr::s32 mouseX, irr::s32 mouseY, bool showInterface); //NAUTITECH: column-aware pick ray
    void setRadarCameraActive();
    void updateViewport(irr::f32 aspect);
    void renderMainColumn(irr::f32 columnAspect, irr::f32 columnHFOVdeg, irr::f32 yawOffsetDeg, irr::f32 shiftY = 0.0f); //NAUTITECH triple-screen
    void setMouseDown(bool isMouseDown);
    void setZoom(bool zoomOn);
    void setZoom(bool zoomOn, irr::f32 zoomLevel);
    void setViewAngle(irr::f32 viewAngle);
    irr::u32 getLoopNumber() const;
    std::string getSerialisedScenario() const;
    std::string getScenarioName() const;
    std::string getWorldName() const;
    std::string getWorldReadme() const;
    void releaseManOverboard();
    void retrieveManOverboard();
    bool getManOverboardVisible() const;
    irr::f32 getManOverboardPosX() const;
    irr::f32 getManOverboardPosZ() const;
    void setManOverboardVisible(bool visible); //To be used directly, eg when in secondary display mode only
    void setManOverboardPos(irr::f32 positionX, irr::f32 positionZ);   //To be used directly, eg when in secondary display mode only
    bool hasGPS() const;
    bool isSingleEngine() const;
    bool isAzimuthDrive() const;
    bool isAzimuthAsternAllowed() const;
    irr::f32 inputToAzimuthEngineMapping(irr::f32 inputAngle) const;
    irr::f32 azimuthToInputEngineMapping(irr::f32 inputEngine) const;
    bool hasDepthSounder() const;
    irr::f32 getMaxSounderDepth() const;
    bool hasBowThruster() const;
    bool hasSternThruster() const;
    bool hasTurnIndicator() const;
    bool debugModeOn() const;
    irr::f32 getOwnShipMass() const;
    irr::f32 getOwnShipMassEstimate() const;
    irr::f32 getOtherShipMassEstimate(int number) const;

    bool getMoveViewWithPrimary() const;
    void setMoveViewWithPrimary(bool moveView);

    ModelParameters getModelParameters() const;

    // TODO: Most of these can be replaced with getModelParameters()
    bool getIsSecondaryControlWheel() const;
    bool getIsSecondaryControlPortEngine() const;
    bool getIsSecondaryControlStbdEngine() const;
    bool getIsSecondaryControlPortSchottel() const;
    bool getIsSecondaryControlStbdSchottel() const;
    bool getIsSecondaryControlPortThrustLever() const;
    bool getIsSecondaryControlStbdThrustLever() const;
    bool getIsSecondaryControlBowThruster() const;
    bool getIsSecondaryControlSternThruster() const;

    irr::f32 getLineStiffnessFactor() const;
    irr::f32 getLineDampingFactor() const;

    void startHorn();
    void endHorn();
    // FIRE FEATURE
    void igniteNearestOtherShipFire();                    // ignite the OtherShip nearest own ship
    void extinguishAllFires();                            // instructor reset
    void advanceComms();   // Inc 3 (comms): trainee performs the next standard distress-comms action (Ctrl+A)
    void setMonitorFiring(bool firing);
    void setMonitorAimFromRay(irr::core::line3d<irr::f32> ray);
    bool hasFireBoat() const;
    bool anyFireActive() const;
    // SAR RESCUE RUN (Kyara): the rescue craft recover the abandon-ship rafts once everyone is
// in the water, then return to their berths.
    void beginRescueRun();
    void beginRescueDeparture();
    void abortRescueRun();
    bool isRescueRunActive() const;
    int  getRescuedCount() const;
    irr::scene::ISceneNode* getContactFromRay(irr::core::line3d<irr::f32> ray, irr::s32 linesMode);

    irr::scene::ISceneNode* getOwnShipSceneNode();
    irr::scene::ISceneNode* getOtherShipSceneNode(int number);
    irr::scene::ISceneNode* getBuoySceneNode(int number);
    irr::scene::ISceneNode* getLandObjectSceneNode(int number);
    irr::scene::ISceneNode* getTerrainSceneNode(int number);

    Terrain* getTerrain();

    irr::f32 getTerrainHeight(irr::f32 posX, irr::f32 posZ) const;

    void addLine(); // Add a line, which will be undefined

    Lines* getLines(); // Get pointer to lines object

    void updateCameraVRPos(irr::core::quaternion quat, irr::core::vector3df pos, irr::core::vector2df lensShift);

    void update();

private:
    irr::IrrlichtDevice* device;
    irr::video::IVideoDriver* driver;
    irr::scene::ISceneManager* smgr;
    bool triScreenMooring = false;      //NAUTITECH triple-screen picking state
    irr::f32 perScreenFOVMooring = 45;
    irr::f32 bezelYawMooring = 45;
    bool thunderEnabled;
    bool lightningEnabled;
    ModelParameters modelParameters;

    irr::video::IImage* radarImage; //Basic radar image
    irr::video::IImage* radarImageOverlaid; //WIth any 2d overlay
    irr::video::IImage* radarImageLarge; //Basic radar image, for full screen display
    irr::video::IImage* radarImageOverlaidLarge; //WIth any 2d overlay, for full screen display
    irr::video::IImage* radarImageChosen; //Should point to one of radarImage or radarImageLarge
    irr::video::IImage* radarImageOverlaidChosen; //Should point to one of radarImageOverlaid or radarImageOverlaidLarge
    //irr::f32 accelerator;
    irr::f32 tideHeight;
    irr::f32 weather; //0-12.0
    irr::f32 rainIntensity; //0-10
    irr::f32 lightningFlash;   // KYARA: 0..1 screen-flash level (decays after each strike)
    irr::f32 heroBoltTimer = 0.0f;        // NAUTITECH: seconds to next hero bolt
    bool heroBoltStormWasActive = false;  // detect storm start for the first strike
    irr::f32 tenderTimer = 0.0f;   // NAUTITECH: seconds to next tender flash
    int tenderIndex = 0;           // which tender fires next`  
    irr::f32 thunderTimer;     // KYARA: seconds until the next thunder strike (primary only)
    std::vector<irr::f32> flashDelays;   // KYARA: queued flash timings
    std::vector<irr::f32> flashLevels;
    irr::f32 visibilityRange; //Nm
    irr::f32 windDirection; //0-360
    irr::f32 windSpeed; //Nm
    //Weather window (see WeatherState)
    irr::f32 cloudCover = 0.0f, snowIntensity = 0.0f, dustLevel = 0.0f, windGust = 0.0f, windVariation = 0.0f;
    irr::f32 windSpeedNow = 0.0f, windDirectionNow = 0.0f;  //with gusts and swings, worked out each frame
    irr::f32 squallLevel = 0.0f;                            //0..1 while a squall passes
    irr::f32 networkSquall = -1.0f;                         //secondary: the primary's squall (-1: not received)
    int networkPalette = -1, networkGlow = -1;              //secondary: the primary's colours and glow
    irr::f32 weatherClock = 0.0f;                           //seconds, drives gusts and squalls
    int significantWeather = 0;
    struct WeatherChange { bool active = false; WeatherState from, to; irr::f32 time = 0, duration = 0; } weatherChange;
    int weatherFront = 0;              //0 none, 1 slow, 2 fast
    int weatherFrontPhase = 0;         //1 coming, 2 overhead, 3 clearing
    irr::f32 weatherFrontTime = 0.0f;  //seconds into the current phase
    WeatherState weatherFrontBase;     //the weather before the front, to come back to
    void applyWeatherState(const WeatherState& state);
    void stopWeatherChanges();
    void updateWeatherDynamics(irr::f32 deltaTime);
    Snow snow;
    irr::f32 streamOverrideDirection; //0-360
    irr::f32 streamOverrideSpeed; //Nm
    bool streamOverride;
    irr::u32 loopNumber; //u32 should be up to 4,294,967,295, so over 2 years at 60 fps
    irr::f32 currentZoom; // Zoom currently in use
    irr::f32 zoomLevel; // Zoom level that should be used if binos are on
    Terrain terrain;
    Light light;
    Sky sky;
    OwnShip ownShip;
    OtherShips otherShips;
    Buoys buoys;
    LandObjects landObjects;
    LandLights landLights;
    Camera camera;
    int lightEditVessel = -2; //KYARA FEUX EDIT: -2 = not editing
    int sizeEditVessel = -2;  //size and waterline editor: -2 = not editing
    irr::f32 sizeEditOrigScale = 1.0f;
    irr::f32 sizeEditOrigYCorrection = 0.0f;
    int sizeEditRevision = 0;
    void applyVesselSize(int vessel, irr::f32 scale, irr::f32 yCorrection);
    bool instrumentEditOpen = false;
    std::vector<irr::u32> instrumentEditSnapshot;
    int instrumentEditSavedLevel = 0;
    int instrumentEditSelected = -1;
    irr::u32 instrumentFlashEnd = 0;  //real time (ms) the flash of the selected material stops
    int instrumentEditRevision = 0;
    bool freeView = false;
    Camera radarCamera;
    Water water;
    Swell swell;           // KYARA HOULE
    Splash splash;         // KYARA SLAM: spray when the bow lands
    ScreenSpray screenSpray; // KYARA SLAM: water on the wheelhouse glass
    bool viewIsInside() const; // KYARA SLAM: wheelhouse views (see bc5 view list)
    irr::f32 motionScale;  // KYARA HOULE: instructor motion scale, 1 = realistic
    Tide tide;
    Rain rain;
    Lines lines;
    RadarCalculation radarCalculation;
    RadarScreen radarScreen;
    ControlVisualiser portEngineVisual;
    ControlVisualiser stbdEngineVisual;
    ControlVisualiser portAzimuthThrottleVisual;
    ControlVisualiser stbdAzimuthThrottleVisual;
    ControlVisualiser wheelVisual;
    GUIMain* guiMain;
    Sound* sound;
    irr::f32 slamCooldown; //KYARA SLAM: seconds until the next slam may sound
    bool isMouseDown; //Updated by the event receiver, used by radar
    bool secondaryRadarMaster = false;
    bool moveViewWithPrimary;
    ManOverboard manOverboard;
    // FIRE FEATURE
    Fire fire;
    FireMonitor fireMonitor;
    bool fireBoatIsOwnShip;
    int  burningShipIndex;                 // -1 = none
    irr::f32 fireElapsed;
    bool     casualtySinking;
    bool     mobDropped;
    std::wstring distressTimer;   // shown atop the comms overlay
    // SCENARIO INCENDIE -------------------------------------------------------
    // Timings, survivor positions, SAR boats and helicopters for this scenario. Read from the
    // scenario's incident.ini (fire scenario editor); without one, the built-in DAKHLA preset.
    IncidentConfig incident;
    bool incidentFromFile;
    bool casualtyStartKnown;        // the configured casualty's scenario start position, to measure her drift
    IncidentPoint casualtyStart;
    irr::f32 abandonDriftX, abandonDriftZ;   // how far she has drifted when abandon ship starts (m)
    void loadIncident(const std::vector<OtherShipData>& shipsData);
    // SAR RESCUE RUN ---------------------------------------------------------
    // One entry per rescue craft. Each walks her outbound route, recovers her rafts (or the
    // nearest free ones), then follows her homeward route and moors.
    enum RescueState { RescueOff, RescueWaiting, RescueRunning, RescueHolding, RescueDeparting, RescueMoored };
    struct RescueWaypoint {
        irr::f32 x, z;      // local metres
        int nodeIndex;      // >=0 -> pick up abandonNodes[nodeIndex]; -1 -> transit; -2 -> berth
    };
    struct RescueBoat {
        int shipIndex;                         // OtherShip index of the rescue craft
        int number;                            // 1-based boat number, as in incident.ini (raft assignment)
        RescueState state;
        irr::f32 homeX, homeZ, homeHdg;        // where she started, local metres
        irr::f32 x, z, hdg, spd;               // live scripted pose (m/s)
        irr::f32 speedMps;                     // transit speed
        irr::f32 moorHdg;                      // heading once alongside, < 0 = homeHdg
        irr::f32 launchDelay;                  // s after the run is ordered before she slips
        irr::f32 launchTimer;                  // s remaining before she slips
        irr::f32 pickupHold;
        int rescued;
        std::vector<IncidentPoint> outLatLong; // configured routes, lat/long
        std::vector<IncidentPoint> homeLatLong;
        std::vector<RescueWaypoint> outRoute;  // outbound, local metres
        size_t outWp;
        std::vector<RescueWaypoint> route;     // homeward, local metres
        size_t wp;
        irr::f32 prevDist;                     // last frame's range to the active waypoint
        irr::f32 departRun;                    // metres run since departure began
        bool returnStarted;                    // committed to the homeward route
        int target;                            // abandonNodes index of the raft being worked, -1 = none
        std::vector<irr::scene::ISceneNode*> carried;   // rafts riding on the aft deck
    };
    std::vector<RescueBoat> rescueBoats;
    int rescueCasualtyIndex;   // the wreck the survivors cluster around, -1 if none
    void addRescueBoat(int shipIndex, int number, irr::f32 speedKts, irr::f32 launchDelay, irr::f32 moorHeading,
        const std::vector<IncidentPoint>& outbound, const std::vector<IncidentPoint>& homeward);
    bool isRescueBoat(int shipIndex) const;
    std::wstring rescueLabel(const RescueBoat& b) const;
    void updateRescueRun(irr::f32 deltaTime);
    void updateRescueBoat(RescueBoat& b, irr::f32 deltaTime);
    void buildReturnRoute(RescueBoat& b);
    int  nearestRemainingRaft(const RescueBoat& b, irr::f32 fromX, irr::f32 fromZ);   // rafts still afloat for this boat
    void rescueMoveNode(irr::f32 deltaX, irr::f32 deltaZ);   // world re-centring
    irr::f32 sarSweepPhase;   // SAR helo searchlight sweep

    irr::core::vector3df sarDatum;   // SAR search datum - survives the casualty being removed
    bool sarDeparting;          // flying off scene before removal
    irr::f32 sarDepartTimer;    // seconds spent departing
    int heloRescuedCount;       // swimmers recovered by air (separate from rafts)

    bool sarDatumSet;
    // SAR HELICOPTER FEATURE ------------------------------------------------
    bool sarActive;
    bool sarInit;
    irr::f32 sarBaseY;   // Kyara SAR: fixed hover altitude captured at activation, so helos DON'T follow the casualty hull down as it sinks
    std::vector<irr::scene::ISceneNode*> sarNodes;
    std::vector<irr::scene::ISceneNode*> sarBeams;
    std::vector<irr::scene::ILightSceneNode*> sarLights;

    // Called by the trainee (3rd Ctrl+A): on scene incident.heloDelay later, flying in from their base.
    bool heloCalled;            // call made, helos pending or inbound
    irr::f32 heloCallElapsed;   // s since the call
    bool sarOnScene;            // every helo has reached the scene
    std::vector<bool> heloInbound;   // per helo: still flying in (or not yet in sight)
    void callSarHelicopters();
    void updateHeloCall(irr::f32 deltaTime);
    irr::core::vector3df heloStation(size_t k) const;   // search station over the datum
    irr::f32 heloInboundFlightSecs() const;            // longest base -> station leg
    bool heloPadPosition(size_t k, irr::core::vector3df& pad) const;   // false = no pad, fly off scene

    void activateSarHelicopters(bool inbound);
    void updateSarHelicopters();
    void deactivateSarHelicopters();
    void departSarHelicopters();
    void placeCarriedRafts(RescueBoat& b);
    std::vector<irr::scene::ISceneNode*> abandonNodes;
    enum AbandonKind { Ab_MOB = 0, Ab_Raft = 1 };
    std::vector<int> abandonKind;         // parallel to abandonNodes
    std::vector<int> abandonBoat;         // parallel to abandonNodes: rafts' assigned boat number, 0 = any
    int abandonMobTotal;                  // MOB put in the water this run
    // Staggered spawn sequencer.
    bool abandonSpawning;                 // mid-sequence
    int  abandonSpawnStep;                // how many nodes dropped so far
    irr::f32 abandonSpawnTimer;           // s until the next drop
    irr::core::vector3df abandonCentre;   // casualty position captured at sequence start
    bool abandonComplete;                 // all nodes in the water -> services may start
    void spawnAbandonStep();              // drop node number abandonSpawnStep
    void updateAbandonSpawn(irr::f32 deltaTime);
    bool abandonSpawned;
    void spawnAbandonScene();
    void clearAbandonScene();
    irr::scene::ISceneNode* spawnModelNode(const std::string& folderName, irr::core::vector3df worldPos, irr::f32 extraScale);
    // SAR WINCH (Kyara): each helo lowers a cable, lifts one MOB, flies it up, repeats.
    enum HeloState { Helo_Search = 0, Helo_Descend = 1, Helo_Lift = 2, Helo_Depart = 3, Helo_Done = 4 };
    struct HeloUnit {
        HeloState state;
        int targetNode;
        irr::f32 cableLen;
        irr::f32 phase;
        irr::core::vector3df pos;
        irr::f32 yaw;            // heading the airframe is pointing, deg
        std::vector<int> queue;
        irr::scene::ISceneNode* cable;
    };
    std::vector<HeloUnit> heloUnits;
    bool heloRunActive;
    void beginHeloRun();
    void updateHeloRun(irr::f32 deltaTime);
    void setHeloCable(size_t k, irr::core::vector3df top, irr::f32 length);
    // Inc 3 (comms): received-MAYDAY prompt + timestamped communications log for the active distress.
    bool distressActive = false;
    int  commsStep = 0;
    std::string casualtyName;
    std::vector<std::wstring> maydayLines;   // the received distress message, one entry per line
    std::vector<std::wstring> commsLog;      // timestamped actions performed by the trainee
    void beginDistressComms(int shipIndex);
    void endDistressComms();
    void pushComms(const std::wstring& line); // timestamps + appends one log entry
    bool monitorFiringDesired; // FIRE FEATURE
    bool fireWasBurning; // FIRE FEATURE: detect the moment the blaze dies
    irr::scene::ISceneNode* fireMountNode; // scale-normalised child of the burning ship
    //Simulation time handling
    irr::u32 currentTime; //Computer clock time
    irr::u32 previousTime; //Computer clock time
    irr::f32 deltaTime;
    irr::f32 scenarioTime; //Simulation internal time, starting at zero at 0000h on start day of simulation
    // KYARA DAY/NIGHT: an offset applied ONLY to the lighting clock. 
    // scenarioTime itself is never touched, so other ships' legs, tide and light flash sequences are unaffected.
    irr::f32 dayNightOffset;
    bool nightMode;
    uint64_t scenarioOffsetTime; //Simulation day's start time from unix epoch (1 Jan 1970)
    uint64_t absoluteTime; //Unix timestamp for current time, including start day. Calculated from scenarioTime and scenarioOffsetTime

    //utility function to check for collision
    bool checkOwnShipCollision();

    void updateTows(irr::f32 deltaTime); // Kyara REMORQUAGE: drag taut line-to-vessel targets
    //KYARA COLLISION  Adding member variable
    //Collision sequence state (sound + warning sign + ship stopped, lasting 10 s)
    irr::f32 collisionSequenceTimeRemaining; // Seconds left in the current collision sequence (0 = not colliding)
    bool collisionWasColliding;              // Raw collision state on the previous loop (for edge detection)
    irr::f32 collisionSoundTimeRemaining;      // Collision sound keeps playing this many more seconds after contact ends
    irr::f32 nearestOtherShipDistance() const; // Centre-to-centre distance (m) to the closest other ship, large if none
    //KYARA COLLISION
    bool inCollision;                   // True while a collision is in progress (contact ongoing)
    irr::f32 collisionClearTimer;       // Small debounce so brief contact gaps don't stutter the alarm
    irr::f32 prevNearestShipDistance;   // Previous-frame nearest other-ship distance (to detect closing)
    irr::f32 proxyHoldTimer;            // Keeps the proxy alarm on briefly after the last 'approaching' frame
    // (smooths jitter, and releases once you stop closing in)

    bool proxyAlarmMuted;               // Tracks if the proxy alarm is manually muted

    irr::f32 filteredRelBearing;   // low-pass filtered version of nearestRelBearing
    bool aimedAtOtherLatched;      // hysteresis state for the bow-aim cone
    ExerciseLog exerciseLog;
    int failureLevel[FAIL_COUNT] = { 0, 0, 0, 0, 0 };
    irr::f32 requestedPortEngine = 0, requestedStbdEngine = 0;   //lever positions, kept through an engine failure
    irr::f32 frozenHeading = 0, frozenLat = 0, frozenLong = 0, frozenCog = 0, frozenSog = 0;
    irr::f32 gyroError = 0, gyroDriftRate = 0;                   //degrees, degrees per second
    irr::f32 gpsErrorX = 0, gpsErrorZ = 0, gpsDriftX = 0, gpsDriftZ = 0; //metres, metres per second
    std::vector<ScheduledFailure> scheduledFailures;
    irr::f32 failureClock = 0;                                    //exercise seconds (stops when paused)
    irr::f32 lastFailureTime = -1;
    std::wstring lastFailureName;
    bool lastFailureReported = true;
    std::vector<BridgeAlert> bridgeAlerts;
    bool weatherByInstructor = false;
    std::wstring instructorMessage;
    irr::f32 instructorMessageAt = -1000;
    void applyFailureAction(int action, int level);
    void updateFailures(irr::f32 deltaTime);
    void updateBridgeAlerts();
    std::vector<irr::f32> hornSchedule;     //seconds on, off, on, off...
    size_t hornStep = 0;
    irr::f32 hornStepLeft = 0;
    bool fogSignalsOn = false;
    irr::f32 fogSignalTimer = 0;
    irr::f32 depthAlarmLimit = 0;
    bool depthAlarmActive = false;
    bool depthAlarmAcked = false;
    bool mobMarked = false;
    irr::f32 mobAbsX = 0, mobAbsZ = 0;      //world metres (scene + offset)
    void updateSoundSignals(irr::f32 deltaTime);
    irr::f32 exerciseStartScenarioTime = 0;
    bool logWasShipContact = false, logWasBuoyContact = false, logWasQuayContact = false, logWasGrounded = false;
    bool logWasCpaSounding = false, logWasGuardSounding = false;
    irr::f32 logCpaSoundingSince = 0;
    bool logReducedVisibility = false;
    void updateExerciseLog();
    irr::f32 collisionStartupGrace;     // Seconds of simulation after load during which proxy/collision are
    // suppressed, so the hull settling on spawn can't fire a false alarm
    irr::f32 collisionReleaseHold;
    //KYARA CONTACT: separate edge-detection for quay contact vs every other collision,
    //plus a cooldown so a hull rubbing along a wall doesn't machine-gun the sample.
    bool landContactWasTouching;
    bool otherCollisionWasTouching;
    irr::f32 contactSoundCooldown;

    //Offset position handling
    irr::core::vector3d<int64_t> offsetPosition;

    //store useful information
    std::string scenarioName;
    std::string worldName;
    std::string serialisedScenarioData;
    std::string worldModelReadmeText;

    //Structure to pass data to gui
    GUIData* guiData;
};
#endif