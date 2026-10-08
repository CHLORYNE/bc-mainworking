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

     //Extends from the general 'Ship' class
#include <limits>
#include "IniFile.hpp"
#include "Angles.hpp"
#include "RadarData.hpp"
#include "Constants.hpp"
#include "OtherShip.hpp"
#include "Utilities.hpp"
#include "SimulationModel.hpp"
#include "Terrain.hpp"

#include <iostream>
#include <algorithm>
#include <fstream>

//using namespace irr;

namespace
{
    //Seconds a leg lasts, as the scenario loader counts it: a stopped leg with distance to go lasts
    //for ever, a stopped leg without (a ship lying at a heading) takes no time. Dividing by the speed
    //as it was gave NaN or inf start times for a stopped leg, and negative ones going astern.
    irr::f32 legSeconds(irr::f32 distanceNm, irr::f32 speedKts)
    {
        if (fabs(speedKts) > 1e-6f) { return SECONDS_IN_HOUR * distanceNm / fabs(speedKts); }
        return distanceNm > 0 ? std::numeric_limits<irr::f32>::max() : 0.0f;
    }
}

OtherShip::OtherShip(const std::string& name, const std::string& internalName, const irr::u32& mmsi, const irr::core::vector3df& location, std::vector<Leg> legsLoaded, bool drifting, SimulationModel* model, irr::scene::ISceneManager* smgr, irr::IrrlichtDevice* dev)
{

    //Initialise speed and heading, normally updated from leg information
    axialSpd = 0;
    hdg = 0;
    rateOfTurn = 0; // Not normally used, but used to smooth behaviour in multiplayer
    underTow = false; towX = 0; towZ = 0; towHdg = 0;
    onFire = false; fireListDeg = 0.0f; fireListTarget = 12.0f; // Kyara FIRE
    sinking = false; sinkDepth = 0.0f; sinkTargetDepth = 0.0f; sinkSeconds = 18.0f; // Kyara FIRE
    scriptedPose = false; scrX = 0; scrZ = 0; scrHdg = 0; scrSpd = 0; //read by update() before any rescue sets it

    this->model = model;

    this->name = name;
    this->mmsi = mmsi;
    this->drifting = drifting;

    std::string basePath = "Models/Othership/" + name + "/";
    std::string userFolder = Utilities::getUserDir();
    //Read model from user dir if it exists there.
    if (Utilities::pathExists(userFolder + basePath)) {
        basePath = userFolder + basePath;
    }

    //Fall back to loading from own ship folder if it doesn't exist in Otherships (useful for multiplayer)
    if (!Utilities::pathExists(basePath)) {
        basePath = "Models/Ownship/" + name + "/";
        //Read model from user dir if it exists there.
        if (Utilities::pathExists(userFolder + basePath)) {
            basePath = userFolder + basePath;
        }
    }

    //Load from individual boat.ini file
    std::string iniFilename = basePath + "boat.ini";

    //load information about this model from its ini file
    std::string shipFileName = IniFile::iniFileToString(iniFilename, "FileName");

    //get scale factor from ini file (or zero if not set - assume 1)
    irr::f32 scaleFactor = IniFile::iniFileTof32(iniFilename, "Scalefactor", 1.f);

    irr::f32 yCorrection = IniFile::iniFileTof32(iniFilename, "YCorrection");
    boatIniFile = iniFilename;
    modelScale = scaleFactor;
    modelYCorrection = yCorrection;
    angleCorrection = IniFile::iniFileTof32(iniFilename, "AngleCorrection");
    // Kyara FIRE: certified rescue/fire-fighting vessel (SAR craft, fire-tug). FireFighting=1 in
// boat.ini. Such vessels are never set alight as the casualty and may carry a monitor.
    fireFightingVessel = (IniFile::iniFileTou32(iniFilename, "FireFighting") == 1);
    // DEE_DEC22 vvvv
    angleCorrectionPitch = IniFile::iniFileTof32(iniFilename, "AngleCorrectionPitch");
    angleCorrectionRoll = IniFile::iniFileTof32(iniFilename, "AngleCorrectionRoll");
    // DEE_DEC22 ^^^^


    std::string shipFullPath = basePath + shipFileName;

    //load mesh
    irr::scene::IAnimatedMesh* shipMesh = smgr->getMesh(shipFullPath.c_str());
    // --- kyara opt ---
    if (shipMesh) {
        shipMesh->setHardwareMappingHint(irr::scene::EHM_STATIC);
    }
    //Set mesh vertical correction (world units)
    heightCorrection = yCorrection * scaleFactor;

    //add to scene node
    if (shipMesh == 0) {
        //Failed to load mesh - load with dummy and continue
        dev->getLogger()->log("Failed to load other ship model:");
        dev->getLogger()->log(shipFullPath.c_str());
        shipMesh = smgr->addSphereMesh("Dummy");
    }
    ship = smgr->addAnimatedMeshSceneNode(shipMesh, 0, -1);
    ship->setScale(irr::core::vector3df(scaleFactor, scaleFactor, scaleFactor));
    ship->setPosition(irr::core::vector3df(0, heightCorrection, 0));

    ship->setMaterialFlag(irr::video::EMF_FOG_ENABLE, true);
    ship->setMaterialFlag(irr::video::EMF_NORMALIZE_NORMALS, true); //Normalise normals on scaled meshes, for correct lighting

    //store length and RCS information for radar etc
    ship->updateAbsolutePosition();
    length = ship->getTransformedBoundingBox().getExtent().Z;
    breadth = ship->getTransformedBoundingBox().getExtent().X;
    height = ship->getTransformedBoundingBox().getExtent().Y * 0.75; //Assume 3/4 of the mesh is above water
    draught = -1 * ship->getTransformedBoundingBox().MinEdge.Y;
    airDraught = ship->getTransformedBoundingBox().MaxEdge.Y;

    // KYARA HOULE: seakeeping for this vessel (periods estimated from its size; RollPeriod /
    // PitchPeriod / HeavePeriod / Freeboard in its boat.ini are used if present)
    initHullMotion();

    rcs = 0.005 * std::pow(length, 3); //Default RCS, base radar cross section on length^3 (following RCS table Ship_RCS_table.pdf)
    std::string logMessage = "Loading '";
    logMessage.append(shipFullPath);
    logMessage.append("' Length (m): ");
    logMessage.append(std::to_string(length));
    logMessage.append(", Breadth (m): ");          // NEW: Log the bounding box breadth
    logMessage.append(std::to_string(breadth));    // NEW: Log the bounding box breadth
    dev->getLogger()->log(logMessage.c_str());
    // Kyara SIZE-CHECK: one line per loaded other-ship, for comparing model sizes.
    {
        std::ofstream sizeCsv(Utilities::getUserDir() + "ship_sizes.csv", std::ios::app);
        if (sizeCsv.is_open()) {
            sizeCsv << name << ";" << shipFileName << ";" << scaleFactor << ";"
                << yCorrection << ";" << length << ";" << breadth << ";"
                << (length / (breadth > 0.01f ? breadth : 0.01f)) << ";"
                << draught << ";" << airDraught << "\n";
        }
    }

    //Add triangle selector and make pickable
    ship->setID(IDFlag_IsPickable);
    selector = smgr->createTriangleSelector(shipMesh, ship);
    //This is applied depending on distance to own ship, for speed
    triangleSelectorEnabled = false;

    ship->setName(internalName.c_str());

    // Todo: Note in documentation that to avoid blocking, use a value of 0.1, as 0 will go to default
    //FIXME: Note in documentation that this is height above waterline in model units
    solidHeight = scaleFactor * IniFile::iniFileTof32(iniFilename, "SolidHeight", .5f * height);

    //store initial x,y,z positions
    xPos = location.X;
    yPos = location.Y;
    zPos = location.Z;
    //speed and heading will come from leg data

    //Set lighting to use diffuse and ambient, so lighting of untextured models works
    if (ship->getMaterialCount() > 0) {
        for (irr::u32 mat = 0; mat < ship->getMaterialCount(); mat++) {
            if (ship->getMaterial(mat).AmbientColor.getAlpha() != 255 ||
                ship->getMaterial(mat).DiffuseColor.getAlpha() != 255) {
                // Only allow rendering with transparency if required to avoid Z order problems
                ship->getMaterial(mat).MaterialType = irr::video::EMT_TRANSPARENT_VERTEX_ALPHA;
            }
            ship->getMaterial(mat).ColorMaterial = irr::video::ECM_DIFFUSE_AND_AMBIENT;
        }
    }

    //KYARA FEUX: other ships show only the lamps their boat.ini declares - nothing is guessed.
  //Place them with "Placer les feux" in the Feux tab (GenerateLights=1 in a boat.ini opts back in).
    shipLights.load(smgr, ship, iniFilename, length,
        (scaleFactor > 0.0001f) ? (1.0f / scaleFactor) : 1.0f,
        ship->getBoundingBox(), false, //lamps only: the driver has few lights to give
        -yCorrection, false); //KYARA FEUX: lamps from boat.ini only (GenerateLights=1 opts in)

    //store leg information
    legs = legsLoaded;
}

void OtherShip::initHullMotion()
{
    HullMotion::Params hp;
    hp.length = length;
    hp.breadth = breadth;
    hp.draught = draught;
    hp.rollPeriod = IniFile::iniFileTof32(boatIniFile, "RollPeriod");
    hp.pitchPeriod = IniFile::iniFileTof32(boatIniFile, "PitchPeriod");
    hp.heavePeriod = IniFile::iniFileTof32(boatIniFile, "HeavePeriod");
    hp.freeboard = IniFile::iniFileTof32(boatIniFile, "Freeboard");
    hp.nLong = 3;
    hp.nTrans = 3;
    hullMotion.init(hp);
}

//Size and waterline editor ----------------------------------------------------------------------
const std::string& OtherShip::getBoatIniFile() const { return boatIniFile; }
irr::f32 OtherShip::getModelScale() const { return modelScale; }
irr::f32 OtherShip::getModelYCorrection() const { return modelYCorrection; }

//Same sums as loading: the box of the unrotated model, scaled and lifted by YCorrection.
void OtherShip::setModelSize(irr::f32 newScale, irr::f32 newYCorrection)
{
    if (!ship || newScale <= 0.000001f || modelScale <= 0.000001f) { return; }
    const irr::f32 factor = newScale / modelScale;
    modelScale = newScale;
    modelYCorrection = newYCorrection;
    heightCorrection = newYCorrection * newScale; //update() places her with this every frame
    ship->setScale(irr::core::vector3df(newScale, newScale, newScale));

    const irr::core::aabbox3df box = ship->getBoundingBox(); //model units
    length = box.getExtent().Z * newScale;
    breadth = box.getExtent().X * newScale;
    height = box.getExtent().Y * newScale * 0.75f;
    draught = -(box.MinEdge.Y * newScale + heightCorrection);
    airDraught = box.MaxEdge.Y * newScale + heightCorrection;
    solidHeight *= factor;
    rcs = 0.005 * std::pow(length, 3);
    initHullMotion();
    shipLights.rescale(length, 1.0f / newScale, -newYCorrection);
}

OtherShip::~OtherShip()
{
    //Drop navLights
    for (std::vector<NavLight*>::iterator it = navLights.begin(); it != navLights.end(); ++it) {
        delete (*it);
    }
    navLights.clear();
}

void OtherShip::update(irr::f32 deltaTime, irr::f32 scenarioTime, irr::f32 tideHeight, irr::u32 lightLevel)
{

    // Kyara FIRE: once she starts to settle, no kinematic override may pin her to the
    // surface. Drop tow/scripted pose every frame so the sinking block below runs.
    if (sinking) { underTow = false; scriptedPose = false; }


    // Kyara REMORQUAGE: if under tow, apply the solver's kinematic command and skip legs.
    if (underTow) {
        xPos = towX;
        zPos = towZ;
        hdg = towHdg;
        positionManuallyUpdated = false;
        placeOnSea(deltaTime, tideHeight + heightCorrection, fireListDeg, true); // KYARA HOULE
        shipLights.update(scenarioTime, lightLevel, fabs(axialSpd) > 0.2f); // KYARA FEUX
        underTow = false; // fail-safe: solver must re-assert next frame, else legs resume
        scriptedPose = false; scrX = 0; scrZ = 0; scrHdg = 0; scrSpd = 0; // Kyara SAR
        return;
    }
    //AUTO RESCUE
        // Kyara SAR: scripted rescue run. Pose is computed by SimulationModel::updateRescueRun
    // and applied here, bypassing legs. axialSpd is set too, so radar/ARPA/AIS see a vessel
    // actually making way rather than a target stopped in the water.
    if (scriptedPose) {
        xPos = scrX;
        zPos = scrZ;
        hdg = scrHdg;
        axialSpd = scrSpd;
        positionManuallyUpdated = false;
        placeOnSea(deltaTime, tideHeight + heightCorrection, 0.0f, true); // KYARA HOULE
        shipLights.update(scenarioTime, lightLevel, fabs(axialSpd) > 0.2f); // KYARA FEUX
        scriptedPose = false; // fail-safe: solver must re-assert next frame
        return;
    }
    //move according to leg information
    if (legs.empty()) {
        //Don't change speed and hdg - may be in secondary mode, where these are set externally
        //Except, use rateOfTurn to update hdg
        hdg += deltaTime * rateOfTurn; // rateOfTurn in deg/s
    }
    else {
        //Work out which leg we're on
        std::vector<Leg>::size_type currentLeg = findCurrentLeg(scenarioTime);

        axialSpd = legs[currentLeg].speed * KTS_TO_MPS;
        hdg = legs[currentLeg].bearing;
    }
    // Kyara FIRE: a burning casualty loses way and drifts; ignore leg speed, ramp the list.
    if (onFire) {
        axialSpd = 0.0f;
        if (fireListDeg < fireListTarget) {
            fireListDeg += deltaTime * 1.5f; // ~8 s to full list (TUNE)
            if (fireListDeg > fireListTarget) { fireListDeg = fireListTarget; }
        }
    }

    if (!positionManuallyUpdated) { //If the position has already been updated, skip (for this loop only)
        xPos = xPos + sin(hdg * irr::core::DEGTORAD) * axialSpd * deltaTime;
        zPos = zPos + cos(hdg * irr::core::DEGTORAD) * axialSpd * deltaTime;
    }
    else {
        positionManuallyUpdated = false;
    }
    yPos = tideHeight + heightCorrection;
    //FIRE FIGHTING  SINKING TIME TO CHANGE 
        // Kyara FIRE: founder - settle the hull under and stop all way.
    if (sinking) {
        axialSpd = 0.0f;
        sinkDepth += (sinkTargetDepth / sinkSeconds) * deltaTime;   // fully under after sinkSeconds
        if (sinkDepth > sinkTargetDepth) { sinkDepth = sinkTargetDepth; }
        yPos -= sinkDepth;
    }
    //------------------------

    if (drifting || onFire) {   // Kyara FIRE: casualties always drift {
        //Move with tidal stream (if not aground)
        irr::f32 depth = -1 * model->getTerrain()->getHeight(xPos, zPos) + yPos;
        irr::core::vector2df streamVector = model->getTidalStream(model->getTerrain()->xToLong(xPos), model->getTerrain()->zToLat(zPos), model->getTimestamp());

        // Add component from wind
        irr::f32 windSpeed = model->getWindSpeed() * KTS_TO_MPS;
        irr::f32 windDirection = model->getWindDirection();
        // Convert this into wind axial speed and wind lateral speed
        irr::f32 windFlowDirection = windDirection + 180; // Wind direction is where the wind is from. We want where it is flowing towards
        irr::f32 windX = windSpeed * sin(windFlowDirection * irr::core::DEGTORAD);
        irr::f32 windZ = windSpeed * cos(windFlowDirection * irr::core::DEGTORAD);
        // Assume that the drifting vessel moves at 1/10 of the wind speed
        streamVector.X += windX * 0.1;
        streamVector.Y += windZ * 0.1;

        // Apply movement vector
        if (depth > 0) {
            irr::f32 streamScaling = fmin(1, depth); //Reduce effect as water gets shallower
            xPos += streamVector.X * deltaTime * streamScaling;
            zPos += streamVector.Y * deltaTime * streamScaling;
        }
    }

    //Set position & speed by calling ship methods
    //setPosition(irr::core::vector3df(xPos,yPos,zPos));
    // KYARA HOULE: heave + pitch + roll on the sea. A foundering wreck no longer rides the waves.
    placeOnSea(deltaTime, yPos, fireListDeg, !sinking);

    //for each light, find range and angle
    shipLights.update(scenarioTime, lightLevel, fabs(axialSpd) > 0.2f); // KYARA FEUX

}
void OtherShip::startSinking(irr::f32 secondsToGoUnder)
{
    if (sinking) { return; }
    sinking = true;
    sinkSeconds = (secondsToGoUnder > 1.0f) ? secondsToGoUnder : 1.0f;
    onFire = true;               // stays a casualty
    fireListTarget = 35.0f;      // heel hard over as she founders
    sinkTargetDepth = airDraught + 4.0f;   // bury the whole hull, with margin
}
bool OtherShip::isSunk() const { return sinking && sinkDepth >= sinkTargetDepth - 0.05f; }


irr::f32 OtherShip::getHeight() const
{
    return height;
}

irr::f32 OtherShip::getRCS() const
{
    return rcs;
}

std::string OtherShip::getName() const
{
    return name;
}

std::vector<Leg> OtherShip::getLegs() const
{
    return legs;
}

void OtherShip::changeLeg(int legNumber, irr::f32 bearing, irr::f32 speed, irr::f32 distance, irr::f32 scenarioTime)
{

    //Check if leg exists, then if we are allowed to change this leg (current or future leg), and not the final 'stop' leg (hence legs.size()-1)
    if (legNumber >= 0 && legNumber < ((int)legs.size() - 1) && legNumber >= (int)findCurrentLeg(scenarioTime)) {

        //Store old information temporarily
        irr::f32 oldSpeed = legs.at(legNumber).speed;

        //Recalculate subsequent start times, only changing from the current point.
        //We can guarantee that there is a next leg, as we checked (legNumber < legs.size() - 1)

        irr::f32 newTimeRemaining;
        if (legNumber == (int)findCurrentLeg(scenarioTime)) {
            //On current leg - calculate from current point only
            irr::f32 oldTimeRemaining = legs.at(legNumber + 1).startTime - scenarioTime;
            if (distance < 0) { distance = fabs(oldSpeed) * oldTimeRemaining / SECONDS_IN_HOUR; } //If leg length is negative, ensure overall leg length doesn't change
            newTimeRemaining = legSeconds(distance, speed); //The adjusted leg distance starts from now
            legs.at(legNumber).startTime = scenarioTime; // New leg effectively starts now
        }
        else {
            //On subsequent leg - calculate for whole leg
            irr::f32 oldTimeRemaining = legs.at(legNumber + 1).startTime - legs.at(legNumber).startTime;
            if (distance < 0) { distance = fabs(oldSpeed) * oldTimeRemaining / SECONDS_IN_HOUR; } //If leg length is negative, ensure overall leg length doesn't change
            newTimeRemaining = legSeconds(distance, speed);
            //No need to change start time.
        }

        //Change this leg
        legs.at(legNumber).bearing = bearing;
        legs.at(legNumber).speed = speed;
        legs.at(legNumber).distance = distance; //Store for later reference

        //Set start time of the next leg (guaranteed to exist)
        legs.at(legNumber + 1).startTime = legs.at(legNumber).startTime + newTimeRemaining;
        //For the remaining legs (which may not exist)
        for (int i = legNumber + 2; i < (int)legs.size(); i++) {
            legs.at(i).startTime = legs.at(i - 1).startTime + legSeconds(legs.at(i - 1).distance, legs.at(i - 1).speed);
        }

    } //Check leg exists & can be changed

}

void OtherShip::addLeg(int afterLegNumber, irr::f32 bearing, irr::f32 speed, irr::f32 distance, irr::f32 scenarioTime)
{

    //Check if leg is reasonable, and is before the 'stop leg'
    //A special case allows afterLegNumber to equal -1, for when only a single 'stop leg' exists
    if (afterLegNumber >= -1 && afterLegNumber < ((int)legs.size() - 1)) {

        //if we're on the stop leg
        if (findCurrentLeg(scenarioTime) == (legs.size() - 1)) {

            //If the 'after' leg is the penultimate, add a leg before the stop one, starting now
            if (afterLegNumber == ((int)legs.size() - 2)) { //This also catches the special case where there is only the 'stop' leg, so the 'afterLegNumber value is -1

                Leg newLeg;
                newLeg.bearing = bearing;
                newLeg.speed = speed;
                newLeg.distance = distance;
                newLeg.startTime = scenarioTime;

                legs.insert(legs.end() - 1, newLeg); //Insert before final leg
            }
            //else check that the 'after' leg is current or future
        }
        else if (afterLegNumber >= 0 && afterLegNumber >= (int)findCurrentLeg(scenarioTime)) { //First check only required in case findCurrentLeg does not return a valid result (>=0)
            Leg newLeg;
            newLeg.bearing = bearing;
            newLeg.speed = speed;
            newLeg.distance = distance;
            newLeg.startTime = legs.at(afterLegNumber + 1).startTime; //This leg starts when the next leg would have started

            legs.insert(legs.begin() + afterLegNumber + 1, newLeg); //Insert leg
        }

        //set start time of subsequent legs
        //For the remaining legs (which may not exist)
        for (int i = afterLegNumber + 2; i < (int)legs.size(); i++) {
            legs.at(i).startTime = legs.at(i - 1).startTime + legSeconds(legs.at(i - 1).distance, legs.at(i - 1).speed);
        }


    } //Check leg exists & can be changed

}

void OtherShip::deleteLeg(int legNumber, irr::f32 scenarioTime)
{

    //Check if leg exists, then if we are allowed to change this leg (current or future leg), and not the final 'stop' leg (hence legs.size()-1)
    if (legNumber >= 0 && legNumber < ((int)legs.size() - 1) && legNumber >= (int)findCurrentLeg(scenarioTime)) {

        //We can guarantee that there is a next leg, as we checked (legNumber < legs.size() - 1)

        //Current or future leg?
        if (legNumber == (int)findCurrentLeg(scenarioTime)) {
            //Current leg
            //Set next leg start time to now: Set start time of the next leg (guaranteed to exist)
            legs.at(legNumber + 1).startTime = scenarioTime;

        }
        else {
            //Future leg
            //Set next leg start time to the start time of the leg we're removing
            legs.at(legNumber + 1).startTime = legs.at(legNumber).startTime;
        }

        //adjust start time of subsequent legs
        //For the remaining legs (which may not exist)
        for (int i = legNumber + 2; i < (int)legs.size(); i++) {
            legs.at(i).startTime = legs.at(i - 1).startTime + legSeconds(legs.at(i - 1).distance, legs.at(i - 1).speed);
        }

        //Remove this leg
        legs.erase(legs.begin() + legNumber);

    } //Check leg exists & can be changed

}

void OtherShip::resetLegs(irr::f32 course, irr::f32 speedKts, irr::f32 distanceNm, irr::f32 scenarioTime)
{
    legs.clear();

    Leg currentLeg;
    currentLeg.bearing = course;
    currentLeg.speed = speedKts;
    currentLeg.startTime = scenarioTime;
    currentLeg.distance = distanceNm;

    //Use distance to calculate startTime of next leg, and stored for later reference.
    currentLeg.distance = distanceNm;
    irr::f32 mainLegEndTime = scenarioTime + legSeconds(distanceNm, speedKts); // nm/kts -> hours, so convert to seconds

    legs.push_back(currentLeg);

    //Add a stop leg here
    Leg stopLeg;
    stopLeg.bearing = course;
    stopLeg.speed = 0;
    stopLeg.distance = 0;
    stopLeg.startTime = mainLegEndTime;
    legs.push_back(stopLeg);
}

void OtherShip::setRateOfTurn(irr::f32 rateOfTurn) //Sets the rate of turn (only used in multiplayer mode)
{
    this->rateOfTurn = rateOfTurn;
}

RadarData OtherShip::getRadarData(irr::core::vector3df scannerPosition) const
//Get data for OtherShip (number) relative to scannerPosition
//Similar code in Buoy.cpp
{
    RadarData radarData;

    irr::core::vector3df contactPosition = getPosition();
    irr::core::vector3df relativePosition = contactPosition - scannerPosition;

    radarData.relX = relativePosition.X;
    radarData.relZ = relativePosition.Z;
    radarData.angle = relativePosition.getHorizontalAngle().Y;
    radarData.range = relativePosition.getLength();
    radarData.heading = getHeading();

    radarData.height = getHeight();
    radarData.solidHeight = solidHeight;
    //radarData.radarHorizon=99999; //ToDo: Implement when ARPA is implemented
    radarData.length = getLength();
    radarData.width = getBreadth();
    radarData.rcs = getRCS();

    //Calculate angles and ranges to each end of the contact
    irr::f32 relAngle1 = Angles::normaliseAngle(irr::core::RADTODEG * std::atan2(radarData.relX + 0.5 * radarData.length * std::sin(irr::core::DEGTORAD * radarData.heading), radarData.relZ + 0.5 * radarData.length * std::cos(irr::core::DEGTORAD * radarData.heading)));
    irr::f32 relAngle2 = Angles::normaliseAngle(irr::core::RADTODEG * std::atan2(radarData.relX - 0.5 * radarData.length * std::sin(irr::core::DEGTORAD * radarData.heading), radarData.relZ - 0.5 * radarData.length * std::cos(irr::core::DEGTORAD * radarData.heading)));
    irr::f32 range1 = std::sqrt(std::pow(radarData.relX + 0.5 * radarData.length * std::sin(irr::core::DEGTORAD * radarData.heading), 2) + std::pow(radarData.relZ + 0.5 * radarData.length * std::cos(irr::core::DEGTORAD * radarData.heading), 2));
    irr::f32 range2 = std::sqrt(std::pow(radarData.relX - 0.5 * radarData.length * std::sin(irr::core::DEGTORAD * radarData.heading), 2) + std::pow(radarData.relZ - 0.5 * radarData.length * std::cos(irr::core::DEGTORAD * radarData.heading), 2));
    radarData.minRange = std::min(range1, range2);
    radarData.maxRange = std::max(range1, range2);
    radarData.minAngle = std::min(relAngle1, relAngle2);
    radarData.maxAngle = std::max(relAngle1, relAngle2);

    //Initial defaults: Fixme: Will need changing with full implementation
    radarData.hidden = false;
    radarData.racon = ""; //Racon code if set
    radarData.raconOffsetTime = 0.0;
    radarData.SART = false;

    radarData.contact = (void*)this;
    radarData.mmsi = this->getMMSI(); // NEW: Pass the real MMSI to the radar array

    return radarData;
}

std::vector<Leg>::size_type OtherShip::findCurrentLeg(irr::f32 scenarioTime)
{
    std::vector<Leg>::size_type currentLeg;

    for (currentLeg = 0; currentLeg < legs.size() - 1; currentLeg++) {
        if (legs[currentLeg].startTime <= scenarioTime && legs[currentLeg + 1].startTime > scenarioTime) {
            break;
        }
    }
    //currentLeg is now the correct leg, or the last leg, which is a 'stopped' leg. (true as we run currentLeg++ once after the check (currentLeg<legs.size()-1) if the 'break' isn't reached

    return currentLeg;
}

void OtherShip::enableTriangleSelector(bool selectorEnabled)
{

    //Only re-set if we need to change the state

    if (selectorEnabled && !triangleSelectorEnabled) {
        ship->setTriangleSelector(selector);
        triangleSelectorEnabled = true;
    }

    if (!selectorEnabled && triangleSelectorEnabled) {
        ship->setTriangleSelector(0);
        triangleSelectorEnabled = false;
    }

}
void OtherShip::setTowState(bool active, irr::f32 x, irr::f32 z, irr::f32 hdg)
{
    underTow = active;
    towX = x; towZ = z; towHdg = hdg;
}

bool OtherShip::getUnderTow() const
{
    return underTow;
}
void OtherShip::setCasualty(bool active)
{
    onFire = active;
    sinking = false; sinkDepth = 0.0f; sinkTargetDepth = 0.0f; sinkSeconds = 18.0f; // Kyara FIRE
    scriptedPose = false; scrX = 0; scrZ = 0; scrHdg = 0; scrSpd = 0; //read by update() before any rescue sets it
    if (!active) { fireListDeg = 0.0f; } // upright on extinguish/reset (persistent-list is a later option)
}

bool OtherShip::getCasualty() const { return onFire; }
bool OtherShip::isFireFightingVessel() const { return fireFightingVessel; }
//AUTO RESCUE
void OtherShip::setScriptedPose(irr::f32 x, irr::f32 z, irr::f32 hdg, irr::f32 spd)
{
    scriptedPose = true;
    scrX = x; scrZ = z; scrHdg = hdg; scrSpd = spd;
}

// KYARA HOULE -----------------------------------------------------------------------------------
void OtherShip::placeOnSea(irr::f32 deltaTime, irr::f32 baseY, irr::f32 extraIrrRoll, bool withMotion)
{
    irr::f32 heave = 0.0f;
    irr::f32 pitchUp = 0.0f;   // + bow up
    irr::f32 rollStbd = 0.0f;  // + heeled to starboard
    if (withMotion) {
        hullMotion.update(model, deltaTime, xPos, zPos, hdg, 1.0f); // other ships: always realistic
        heave = hullMotion.getHeave() + hullMotion.getAntiClipLift();
        pitchUp = hullMotion.getPitchDeg();
        rollStbd = hullMotion.getRollDeg();
    }
    yPos = baseY + heave;
    ship->setPosition(irr::core::vector3df(xPos, yPos, zPos));
    // Body-axis yaw, then pitch, then roll (the old Euler vector applied the fire list about the
    // WORLD Z axis, which turned it into a pitch on east/west headings).
    // Irrlicht signs: X + = bow down, Z + = starboard up.
    ship->setRotation(Angles::irrAnglesFromYawPitchRoll(hdg + angleCorrection,
        angleCorrectionPitch - pitchUp,
        angleCorrectionRoll + extraIrrRoll - rollStbd));
}

//KYARA FEUX
ShipLights& OtherShip::getLights()
{
    return shipLights;
}