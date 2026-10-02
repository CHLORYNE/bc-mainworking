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

#ifndef __OTHERSHIP_HPP_INCLUDED__
#define __OTHERSHIP_HPP_INCLUDED__

#include "irrlicht.h"

#include "Ship.hpp"
#include "HullMotion.hpp" // KYARA HOULE

#include "NavLight.hpp"
#include "ShipLights.hpp" // KYARA FEUX
#include "Leg.hpp"

#include <cmath>
#include <vector>

     // Forward declarations
class SimulationModel;
struct RadarData;

class OtherShip : public Ship
{
public:
    OtherShip(const std::string& name, const std::string& internalName, const irr::u32& mmsi, const irr::core::vector3df& location, std::vector<Leg> legsLoaded, bool drifting, SimulationModel* model, irr::scene::ISceneManager* smgr, irr::IrrlichtDevice* dev);
    ~OtherShip();

    irr::f32 getHeight() const;
    irr::f32 getRCS() const;
    std::string getName() const;
    std::vector<Leg> getLegs() const;
    void changeLeg(int legNumber, irr::f32 bearing, irr::f32 speed, irr::f32 distance, irr::f32 scenarioTime);
    void addLeg(int afterLegNumber, irr::f32 bearing, irr::f32 speed, irr::f32 distance, irr::f32 scenarioTime);
    void deleteLeg(int legNumber, irr::f32 scenarioTime);
    void resetLegs(irr::f32 course, irr::f32 speedKts, irr::f32 distanceNm, irr::f32 scenarioTime);
    RadarData getRadarData(irr::core::vector3df scannerPosition) const;
    void update(irr::f32 deltaTime, irr::f32 scenarioTime, irr::f32 tideHeight, irr::u32 lightLevel);
    void enableTriangleSelector(bool selectorEnabled);
    void setRateOfTurn(irr::f32 rateOfTurn); // This could be moved to Ship.hpp
    void setTowState(bool active, irr::f32 x, irr::f32 z, irr::f32 hdg); // Kyara: remorquage
    // AUTO RESCUE
            // Kyara SAR: externally driven pose for the scripted rescue run. Same kinematic
    // override as tow, but on its own flag so the two solvers can never fight.
    void setScriptedPose(irr::f32 x, irr::f32 z, irr::f32 hdg, irr::f32 spd);
    bool isFireFightingVessel() const;   // Kyara FIRE: boat.ini FireFighting=1 (SAR craft / certified tug)
    bool getUnderTow() const;
    ShipLights& getLights(); // KYARA FEUX
    void setCasualty(bool active); // Kyara FIRE
    bool getCasualty() const;
    //FIREFIGHTING 
    void startSinking(irr::f32 secondsToGoUnder = 18.0f); // Kyara FIRE: begin foundering; fully under after secondsToGoUnder
    bool isSunk() const;     // Kyara FIRE: fully under

protected:
private:

    SimulationModel* model;
    std::string name;
    std::vector<Leg> legs;
    std::vector<NavLight*> navLights;
    ShipLights shipLights; // KYARA FEUX
    irr::f32 height; //For radar
    irr::f32 solidHeight; //For radar
    irr::f32 rcs;
    irr::f32 rateOfTurn;

    std::vector<Leg>::size_type findCurrentLeg(irr::f32 scenarioTime);

    // KYARA HOULE: heave / pitch / roll on the sea, same model as the own ship (3 x 3 samples)
    HullMotion hullMotion;
    // Apply heave + attitude and place the scene node. baseY = tide + heightCorrection (+sinking).
    // extraIrrRoll = extra roll in Irrlicht sign (the fire list), added on top.
    void placeOnSea(irr::f32 deltaTime, irr::f32 baseY, irr::f32 extraIrrRoll, bool withMotion);
    irr::scene::ITriangleSelector* selector;
    bool triangleSelectorEnabled;
    bool drifting;
    // Kyara FIRE: burning casualty - loses way, drifts, and lists as it floods/burns.
    bool onFire;
    irr::f32 fireListDeg;      // current list (deg), ramps up while onFire
    bool fireFightingVessel;             // Kyara FIRE: certified for fire-fighting (from boat.ini)
    irr::f32 fireListTarget;   // final list angle (deg)
    // Kyara REMORQUAGE: while under tow, position/heading are driven externally by the
    // tow solver each frame, overriding leg motion. Refreshed every frame while a taut
    // line-to-vessel exists; if it stops being asserted, normal leg behaviour resumes.
    bool underTow;
    //AUTO RESCUE
    // Kyara SAR: scripted-run override. Re-asserted every frame by the rescue solver;
// self-clears in update(), so if we stop asserting, leg behaviour resumes.
    bool scriptedPose;
    irr::f32 scrX, scrZ, scrHdg, scrSpd;
    irr::f32 towX, towZ, towHdg;
    //FIRE FIGHTING
            // Kyara FIRE: foundering. Left unfought past the deadline, the hull settles under and
    // heels hard over, then the wreck is left submerged.
    bool sinking;
    irr::f32 sinkDepth;        // metres settled below the floating waterline
    irr::f32 sinkTargetDepth;  // metres to fully submerge
    irr::f32 sinkSeconds;      // SCENARIO INCENDIE: time taken to go fully under
};

#endif