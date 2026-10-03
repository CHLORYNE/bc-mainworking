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

#include "ControllerModel.hpp"
#include "../IniFile.hpp"
#include "../Constants.hpp"
#include "../Utilities.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#ifdef _WIN32
#include <direct.h> //for windows _mkdir
#else
#include <sys/stat.h>
#endif // _WIN32

//Constructor
ControllerModel::ControllerModel(irr::IrrlichtDevice* device, Lang* lang, GUIMain* gui, std::string worldName, ScenarioData* scenarioData, std::vector<PositionData>* buoysData)
{

    this->gui = gui;
    this->lang = lang;
    this->device = device;
    driver = device->getVideoDriver();

    this->buoysData = buoysData;
    this->scenarioData = scenarioData;
    this->worldName = worldName;

    checkName();//Check if the scenario name (preset in generalData) will cause an overwrite, and if so, set flag in generalData

    viewInitialised = false;
    dragShip = -1;
    panning = false;
    rightDown = false;

    selectedShip = -1; //Used to signify own ship selected
    selectedLeg = -1; //Used to signify no leg selected

    //Load the chart (shared with the fire scenario editor): world bounds, and the chart drawn from
    //the height map, the world's map image, or a high resolution image if the world has one.
    std::string error;
    if (!chart.load(device, worldName, error)) {
        std::cout << "Could not load map for " << worldName << ": " << error << std::endl;
        exit(EXIT_FAILURE);
    }
    terrainLong = chart.west;
    terrainLat = chart.south;
    terrainLongExtent = chart.longExtent;
    terrainLatExtent = chart.latExtent;
    terrainXWidth = chart.widthM;
    terrainZWidth = chart.heightM;

    std::cout << "Width m " << terrainXWidth << " Height m " << terrainZWidth << std::endl;

    chart.setViewport(gui->getMapViewport()); //beside the side panel
    chart.setMetresPerPixel(20.0); //About the old default zoom level
}

//Destructor
ControllerModel::~ControllerModel()
{
}

irr::f32 ControllerModel::longToX(irr::f32 longitude) const
{
    return ((longitude - terrainLong ) * (terrainXWidth)) / terrainLongExtent;
}

irr::f32 ControllerModel::latToZ(irr::f32 latitude) const
{
    return ((latitude - terrainLat ) * (terrainZWidth)) / terrainLatExtent;
}

irr::f32 ControllerModel::xToLong(irr::f32 x) const
{
    return terrainLong + x*terrainLongExtent/terrainXWidth;
}

irr::f32 ControllerModel::zToLat(irr::f32 z) const
{
    return terrainLat + z*terrainLatExtent/terrainZWidth;
}

void ControllerModel::update()
{
    chart.setViewport(gui->getMapViewport()); //beside the side panel

    //Start centred on the own ship (its position is only known once the scenario has been read)
    if (!viewInitialised) {
        chart.centreOnXZ(scenarioData->ownShipData.initialX, scenarioData->ownShipData.initialZ);
        viewInitialised = true;
    }

    //Ship under the cursor, highlighted when it can be picked up
    irr::s32 hoverShip = -1;
    if (dragShip < 0 && !panning) {
        irr::gui::IGUIElement* overElement = device->getGUIEnvironment()->getRootGUIElement()->getElementFromPoint(mouse);
        if (overElement == 0 || overElement == device->getGUIEnvironment()->getRootGUIElement()) {
            hoverShip = shipAt(mouse);
        }
    }

    //Send the current data to the gui, and update it
    gui->updateGuiData(*scenarioData, chart, *buoysData, selectedShip, selectedLeg, hoverShip, dragShip >= 0, mouse);
}

void ControllerModel::resetOffset()
{
    chart.centreOnXZ(scenarioData->ownShipData.initialX, scenarioData->ownShipData.initialZ);
}

void ControllerModel::increaseZoom()
{
    chart.zoomAt(chart.getViewport().getCenter(), 0.5f);
}

void ControllerModel::decreaseZoom()
{
    chart.zoomAt(chart.getViewport().getCenter(), 2.0f);
}

void ControllerModel::nextChartStyle()
{
    chart.nextStyle();
}

irr::f32 ControllerModel::chartWidth() const
{
    return terrainXWidth;
}

irr::f32 ControllerModel::chartHeight() const
{
    return terrainZWidth;
}

irr::core::position2di ControllerModel::shipScreenPosition(irr::s32 ship) const
{
    if (ship == 0) {
        return chart.toScreenXZ(scenarioData->ownShipData.initialX, scenarioData->ownShipData.initialZ);
    }
    const OtherShipData& other = scenarioData->otherShipsData.at(ship - 1);
    return chart.toScreenXZ(other.initialX, other.initialZ);
}

irr::s32 ControllerModel::shipAt(irr::core::position2di screen) const
{
    //Nearest ship symbol within a few pixels; the own ship wins a tie, as it is drawn on top.
    const irr::s32 pickRadius = 14;
    irr::s32 best = -1;
    irr::s32 bestDistSq = pickRadius * pickRadius + 1;
    for (irr::s32 ship = 0; ship <= (irr::s32)scenarioData->otherShipsData.size(); ship++) {
        irr::core::position2di p = shipScreenPosition(ship);
        irr::s32 dx = p.X - screen.X;
        irr::s32 dy = p.Y - screen.Y;
        irr::s32 distSq = dx * dx + dy * dy;
        if (distSq < bestDistSq) {
            best = ship;
            bestDistSq = distSq;
        }
    }
    return best;
}

bool ControllerModel::onMouse(const irr::SEvent::SMouseInput& mouseInput, bool overGui)
{
    irr::core::position2di at(mouseInput.X, mouseInput.Y);
    mouse = at;

    switch (mouseInput.Event) {
    case irr::EMIE_MOUSE_MOVED: {
        if (dragShip >= 0) {
            double x, z;
            chart.toXZ(at + dragOffset, x, z);
            setShipPosition(dragShip, irr::core::vector2df((irr::f32)x, (irr::f32)z));
        } else if (panning) {
            chart.panPixels(at.X - lastMouse.X, at.Y - lastMouse.Y);
        }
        lastMouse = at;
        return dragShip >= 0 || panning;
    }
    case irr::EMIE_MOUSE_WHEEL:
        if (overGui) {
            return false;
        }
        chart.zoomAt(at, mouseInput.Wheel > 0 ? 0.8f : 1.25f);
        return true;
    case irr::EMIE_LMOUSE_PRESSED_DOWN: {
        if (overGui) {
            return false;
        }
        device->getGUIEnvironment()->setFocus(0);
        lastMouse = at;
        irr::s32 ship = shipAt(at);
        if (ship >= 0) {
            gui->selectShip(ship); //Same as choosing it in the ship list
            dragShip = ship;
            dragOffset = shipScreenPosition(ship) - at;
        } else {
            panning = true;
        }
        return true;
    }
    case irr::EMIE_LMOUSE_LEFT_UP:
        dragShip = -1;
        panning = rightDown;
        return false;
    case irr::EMIE_RMOUSE_PRESSED_DOWN:
    case irr::EMIE_MMOUSE_PRESSED_DOWN:
        if (overGui) {
            return false;
        }
        rightDown = true;
        panning = true;
        lastMouse = at;
        return true;
    case irr::EMIE_RMOUSE_LEFT_UP:
    case irr::EMIE_MMOUSE_LEFT_UP:
        rightDown = false;
        panning = false;
        return false;
    default:
        return false;
    }
}

bool ControllerModel::onKey(const irr::SEvent::SKeyInput& keyInput)
{
    if (keyInput.Key == irr::KEY_HOME) {
        resetOffset();
        return true;
    }
    if (keyInput.Key == irr::KEY_LEFT || keyInput.Key == irr::KEY_RIGHT) {
        //Own ship: initial heading. Other ship: course of its first leg.
        irr::f32* heading = 0;
        if (selectedShip < 0) {
            heading = &scenarioData->ownShipData.initialBearing;
        } else if (selectedShip < (irr::s32)scenarioData->otherShipsData.size() && scenarioData->otherShipsData.at(selectedShip).legs.size() > 1) {
            heading = &scenarioData->otherShipsData.at(selectedShip).legs.at(0).bearing;
        }
        if (heading == 0) {
            return false;
        }
        irr::f32 step = keyInput.Shift ? 1.0f : 5.0f;
        *heading += (keyInput.Key == irr::KEY_LEFT) ? -step : step;
        while (*heading < 0) { *heading += 360; }
        while (*heading >= 360) { *heading -= 360; }
        gui->updateEditBoxes();
        return true;
    }
    return false;
}

void ControllerModel::setShipPosition(irr::s32 ship, irr::core::vector2df position)
{
    if (ship==0) {
        //Own ship
        scenarioData->ownShipData.initialX = position.X;
        scenarioData->ownShipData.initialZ = position.Y;
    } else if (ship>0) {
        //Other ship
        irr::s32 otherShipNumber = ship-1; //Ship number is minimum of 1, so subtract 1 to start at 0
        if (otherShipNumber < scenarioData->otherShipsData.size()) {
            scenarioData->otherShipsData.at(otherShipNumber).initialX = position.X;
            scenarioData->otherShipsData.at(otherShipNumber).initialZ = position.Y;
        }
    }
}

void ControllerModel::updateSelectedShip(irr::s32 index) //To be called from eventReceiver, where index is from the combo box
{
    if(index < 1) { //If 0 or -1
        selectedShip = -1; //Own ship
    } else {
        selectedShip = index-1; //Other ship number
    }

    //No guarantee from this that the selected ship is valid
}

void ControllerModel::updateSelectedLeg(irr::s32 index) //To be called from eventReceiver, where index is from the combo box. -1 if nothing selected, 0 upwards for leg
{
    selectedLeg = index;
    //No guarantee from this that the selected leg is valid
}

void ControllerModel::setGeneralScenarioData(ScenarioData newData)
{
    scenarioData->startTime = newData.startTime;
    scenarioData->startDay = newData.startDay;
    scenarioData->startMonth = newData.startMonth;
    scenarioData->startYear = newData.startYear;
    scenarioData->sunRise = newData.sunRise;
    scenarioData->sunSet = newData.sunSet;
    scenarioData->weather = newData.weather;
    scenarioData->rainIntensity = newData.rainIntensity;
    scenarioData->visibilityRange = newData.visibilityRange;
    scenarioData->windDirection = newData.windDirection;
    scenarioData->windSpeed = newData.windSpeed;
    scenarioData->scenarioName = newData.scenarioName;
    scenarioData->description = newData.description;
    
    recalculateLegTimes(); //These need to be updated to match new startTime.
    checkName(); //Check if the scenario name chosen will cause overwrite, and if so, set flag.
}

void ControllerModel::checkName() //Check if the scenario name chosen will mean that an existing scenario gets overwritten, and update flag in GeneralData
{
    //Find path to scenario folder
    std::string userFolder = Utilities::getUserDir();
    std::string scenarioPath = "Scenarios/";
    if (Utilities::pathExists(userFolder + scenarioPath)) {
        scenarioPath = userFolder + scenarioPath;
    }

    //Check if path exists already
    std::string fullScenarioPath = scenarioPath.append(scenarioData->scenarioName);

    if (Utilities::pathExists(fullScenarioPath)) {
        scenarioData->willOverwrite=true;
    } else {
        scenarioData->willOverwrite=false;
    }

    //Check if a valid multiplayer name
    scenarioData->multiplayerName=false;
    if (scenarioData->scenarioName.length() >= 3) {
        std::string endChars = scenarioData->scenarioName.substr(scenarioData->scenarioName.length()-3,3);
        if (endChars == "_mp" || endChars == "_MP") {
            scenarioData->multiplayerName = true;
        }
    }

}

void ControllerModel::changeLeg(irr::s32 ship, irr::s32 index, irr::f32 legCourse, irr::f32 legSpeed, irr::f32 legDistance)  //Change othership (or ownship) course, speed etc.
{
    //If ownship:
    if (ship==0) {
        scenarioData->ownShipData.initialBearing = legCourse;
        scenarioData->ownShipData.initialSpeed = legSpeed;
    }

    //If other ship:
    if (ship>0) {
        int otherShipIndex = ship-1;
        if (otherShipIndex < scenarioData->otherShipsData.size()) {
            if (index < scenarioData->otherShipsData.at(otherShipIndex).legs.size()) {
                scenarioData->otherShipsData.at(otherShipIndex).legs.at(index).bearing = legCourse;
                scenarioData->otherShipsData.at(otherShipIndex).legs.at(index).speed = legSpeed;
                scenarioData->otherShipsData.at(otherShipIndex).legs.at(index).distance = legDistance;
            }
        }
    }
    recalculateLegTimes(); //Subsequent leg start times may have changed, so recalculate these
}

void ControllerModel::deleteLeg(irr::s32 ship, irr::s32 index)
{
    //If other ship:
    if (ship>0) {
        int otherShipIndex = ship-1;
        if (otherShipIndex < scenarioData->otherShipsData.size()) {
            if (index < scenarioData->otherShipsData.at(otherShipIndex).legs.size()) {
                //Delete this leg
                scenarioData->otherShipsData.at(otherShipIndex).legs.erase(scenarioData->otherShipsData.at(otherShipIndex).legs.begin() + index);
                recalculateLegTimes(); //Subsequent leg start times may have changed, so recalculate these
            }
        }
    }
}

void ControllerModel::setMMSI(irr::s32 ship, int mmsi)
{
    //If other ship:
    if (ship>0) {
        int otherShipIndex = ship-1;
        if (otherShipIndex < scenarioData->otherShipsData.size()) {
            scenarioData->otherShipsData.at(otherShipIndex).mmsi = mmsi;
        }
    }   
}

void ControllerModel::setDrifting(irr::s32 ship, bool drifting)
{
    //If other ship:
    if (ship > 0) {
        int otherShipIndex = ship - 1;
        if (otherShipIndex < scenarioData->otherShipsData.size()) {
            scenarioData->otherShipsData.at(otherShipIndex).drifting = drifting;
        }
    }
}

void ControllerModel::addLeg(irr::s32 ship, irr::s32 afterLegNumber, irr::f32 legCourse, irr::f32 legSpeed, irr::f32 legDistance)
{
    //If other ship:
    if (ship>0) {
        int otherShipIndex = ship-1;
        if (otherShipIndex < scenarioData->otherShipsData.size()) {
            std::vector<LegData>* legs = &scenarioData->otherShipsData.at(otherShipIndex).legs;
            //Check if leg is reasonable, and is before the 'stop leg'
            //A special case allows afterLegNumber to equal -1, for when only a single 'stop leg' exists
            if (afterLegNumber >= -1 && afterLegNumber < ((int)legs->size() - 1)) {

                //If the 'after' leg is the penultimate, add a leg before the stop one, starting now
                if (afterLegNumber == ((int)legs->size()-2))  { //This also catches the special case where there is only the 'stop' leg, so the 'afterLegNumber value is -1

                    LegData newLeg;
                    newLeg.bearing = legCourse;
                    newLeg.speed = legSpeed;
                    newLeg.distance = legDistance;
                    legs->insert(legs->end()-1, newLeg); //Insert before final leg

                } else {

                    LegData newLeg;
                    newLeg.bearing = legCourse;
                    newLeg.speed = legSpeed;
                    newLeg.distance = legDistance;

                    //std::cout << "B" << std::endl;

                    legs->insert(legs->begin()+afterLegNumber+1, newLeg); //Insert leg
                }
            }
        }
    }
    recalculateLegTimes(); //Subsequent leg start times may have changed, so recalculate these
}

void ControllerModel::addShip(std::string name, irr::core::vector2df position)
{
    OtherShipData newShip;
    newShip.initialX = position.X;
    newShip.initialZ = position.Y;
    newShip.shipName = name;
    newShip.mmsi = 0;
    //Add a 'stop' leg
    LegData stopLeg;
    stopLeg.bearing=0;
    stopLeg.speed=0;
    stopLeg.distance=0;
    newShip.legs.push_back(stopLeg);

    //Add to list
    scenarioData->otherShipsData.push_back(newShip);

    recalculateLegTimes(); //Subsequent leg start times may have changed, so recalculate these
}

void ControllerModel::deleteShip(irr::s32 ship) 
{
	//If other ship:
	if (ship > 0) {
		int otherShipIndex = ship - 1;
		if (otherShipIndex < scenarioData->otherShipsData.size()) {
			scenarioData->otherShipsData.erase(scenarioData->otherShipsData.begin()+otherShipIndex);
		}
	}
}

void ControllerModel::recalculateLegTimes()
{
    //Run through all othership legs, recalculating leg stop times
    irr::f32 scenarioStartTime = scenarioData->startTime; //Legs start at the start of the scenario

    for (int thisShip = 0; thisShip < scenarioData->otherShipsData.size(); thisShip++) {

        irr::f32 legStartTime = scenarioStartTime; //Legs start at the start of the scenario
        for (int thisLeg = 0; thisLeg < scenarioData->otherShipsData.at(thisShip).legs.size(); thisLeg++) {
            scenarioData->otherShipsData.at(thisShip).legs.at(thisLeg).startTime = legStartTime;
            irr::f32 thisLegDistance = scenarioData->otherShipsData.at(thisShip).legs.at(thisLeg).distance;
            irr::f32 thisLegSpeed = scenarioData->otherShipsData.at(thisShip).legs.at(thisLeg).speed;
            //Update legStart time for start of next leg:
            legStartTime+= SECONDS_IN_HOUR*(thisLegDistance/fabs(thisLegSpeed)); // nm/kts -> hours, so convert to seconds
        }
    }

}

void ControllerModel::changeOwnShipName(std::string name)
{
    scenarioData->ownShipData.ownShipName = name;

}

void ControllerModel::changeOtherShipName(irr::s32 ship, std::string name)
{
    irr::s32 shipIndex = ship-1; //'ship' number starts at 1 for otherShips
    if (shipIndex < scenarioData->otherShipsData.size()) {
        scenarioData->otherShipsData.at(shipIndex).shipName = name;
    }
}

void ControllerModel::save()
{
    //Do save here
    std::cout << "About to save" << std::endl;

    //check there's a scenario name to save to
    if (scenarioData->scenarioName.empty()) {
        device->getGUIEnvironment()->addMessageBox(lang->translate("failed").c_str(), lang->translate("failedScenarioSave").c_str());
        return;
    }

    //Find path to scenario folder
    std::string userFolder = Utilities::getUserDir();
    std::string scenarioPath = "Scenarios/";
    if (Utilities::pathExists(userFolder + scenarioPath)) {
        scenarioPath = userFolder + scenarioPath;
    }

    //Check if path exists already
    std::string fullScenarioPath = scenarioPath.append(scenarioData->scenarioName);

    if (!Utilities::pathExists(fullScenarioPath)) {
        //Path does not exist, try and create it
        #ifdef _WIN32
        _mkdir(fullScenarioPath.c_str());
        #else
        mkdir(fullScenarioPath.c_str(),0755);
        #endif // _WIN32

    } else {
        std::cout << "Overwriting scenario at " << fullScenarioPath << std::endl;
    }

    //Try and create files
    bool successOfFar = true;

    //environment.ini
    std::string envPath = fullScenarioPath + "/environment.ini";
    std::ofstream envFile;

    envFile.open(envPath.c_str());
    envFile << "Setting=\"" << worldName << "\"" << std::endl;
    envFile << "StartTime=" << scenarioData->startTime/SECONDS_IN_HOUR << std::endl;
    envFile << "StartDay=" << scenarioData->startDay << std::endl;
    envFile << "StartMonth=" << scenarioData->startMonth << std::endl;
    envFile << "StartYear=" << scenarioData->startYear << std::endl;
    envFile << "SunRise=" << scenarioData->sunRise << std::endl;
    envFile << "SunSet=" << scenarioData->sunSet << std::endl;
    //envFile << "Variation=0" << std::endl;
    envFile << "VisibilityRange=" << scenarioData->visibilityRange << std::endl;
    envFile << "Weather=" << scenarioData->weather << std::endl;
    envFile << "WindDirection=" << scenarioData->windDirection << std::endl;
    envFile << "WindSpeed=" << scenarioData->windSpeed << std::endl;
    envFile << "Rain=" << scenarioData->rainIntensity << std::endl;
    
    envFile.close();
    if (!envFile.good()) {successOfFar=false;}

    //othership.ini
    std::string otherPath = fullScenarioPath + "/othership.ini";
    std::ofstream otherFile;

    otherFile.open(otherPath.c_str());
    otherFile << "Number=" << scenarioData->otherShipsData.size() << std::endl;
    for (int i = 1; i<=scenarioData->otherShipsData.size(); i++) {
        otherFile << "Type(" << i << ")=\"" << scenarioData->otherShipsData.at(i-1).shipName << "\"" << std::endl;
        otherFile << "InitLong(" << i << ")=" << std::setprecision(8) << xToLong(scenarioData->otherShipsData.at(i-1).initialX) << std::endl;
        otherFile << "InitLat(" << i << ")=" << std::setprecision(8) << zToLat(scenarioData->otherShipsData.at(i-1).initialZ) << std::endl;
        otherFile << "mmsi(" << i << ")=" << scenarioData->otherShipsData.at(i-1).mmsi << std::endl;
        if (scenarioData->otherShipsData.at(i - 1).drifting) {
            otherFile << "Drifting(" << i << ")=1" << std::endl;
        }
        //Don't save last leg, as this is an automatically added 'stop' leg.
        otherFile << "Legs(" << i << ")=" << scenarioData->otherShipsData.at(i-1).legs.size() - 1 << std::endl;

        for (int j = 1; j<=scenarioData->otherShipsData.at(i-1).legs.size() - 1; j++) {
            otherFile << "Bearing(" << i << "," << j << ")=" << std::setprecision(8) << scenarioData->otherShipsData.at(i-1).legs.at(j-1).bearing << std::endl;
            otherFile << "Speed(" << i << "," << j << ")=" << std::setprecision(8) << scenarioData->otherShipsData.at(i-1).legs.at(j-1).speed << std::endl;
            otherFile << "Distance(" << i << "," << j << ")=" << std::setprecision(8) << scenarioData->otherShipsData.at(i-1).legs.at(j-1).distance << std::endl;
        }
    }
    otherFile.close();
    if (!otherFile.good()) {successOfFar=false;}

    //ownship.ini
    std::string ownPath = fullScenarioPath + "/ownship.ini";
    std::ofstream ownFile;

    ownFile.open(ownPath.c_str());
    ownFile << "ShipName=\"" << scenarioData->ownShipData.ownShipName << "\"" << std::endl;
    ownFile << "InitialLong=" << std::setprecision(8) << xToLong(scenarioData->ownShipData.initialX) << std::endl;
    ownFile << "InitialLat=" << std::setprecision(8) << zToLat(scenarioData->ownShipData.initialZ) << std::endl;
    ownFile << "InitialBearing=" << std::setprecision(8) << scenarioData->ownShipData.initialBearing << std::endl;
    ownFile << "InitialSpeed=" << std::setprecision(8) << scenarioData->ownShipData.initialSpeed << std::endl;
    ownFile.close();
    if (!ownFile.good()) {successOfFar=false;}

    //description.ini
    std::string descriptionPath = fullScenarioPath + "/description.ini";
    std::ofstream descriptionFile;

    descriptionFile.open(descriptionPath.c_str());
    descriptionFile << scenarioData->description;
    descriptionFile.close();
    if (!descriptionFile.good()) {successOfFar=false;}

    if (successOfFar) {
        device->getGUIEnvironment()->addMessageBox(lang->translate("saved").c_str(), lang->translate("scenarioSaved").c_str());
    } else {
        device->getGUIEnvironment()->addMessageBox(lang->translate("failed").c_str(), lang->translate("failedScenarioSave").c_str());
    }


}
