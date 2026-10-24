
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

#ifndef __CONTROLLERMODEL_HPP_INCLUDED__
#define __CONTROLLERMODEL_HPP_INCLUDED__

#include <vector>

#include "irrlicht.h"

#include "PositionDataStruct.hpp"
#include "../ScenarioDataStructure.hpp"

#include "GUI.hpp"
#include "../Lang.hpp"
#include "../chartView/ChartView.hpp"

class ControllerModel //Start of the 'Model' part of MVC
{

public:

    //ControllerModel(irr::IrrlichtDevice* dev, irr::scene::ISceneManager* scene, GUIMain* gui, std::string scenarioName);
    ControllerModel(irr::IrrlichtDevice* device, Lang* lang, GUIMain* gui, std::string worldName, ScenarioData* scenarioData, std::vector<PositionData>* buoysData);
    ~ControllerModel();
    irr::f32 longToX(irr::f32 longitude) const;
    irr::f32 latToZ(irr::f32 latitude) const;
    irr::f32 xToLong(irr::f32 x) const;
    irr::f32 zToLat(irr::f32 z) const;
    void update(); //Called once per loop from the main function.
    void resetOffset(); //Re-centre the map on the own-ship

    //Methods used to update the state, called by the event receiver:
    void setShipPosition(irr::s32 ship, irr::core::vector2df position); //To be called from eventReceiver
    void updateSelectedShip(irr::s32 index); //To be called from eventReceiver, where index is from the combo box
    void updateSelectedLeg(irr::s32 index); //To be called from eventReceiver, where index is from the combo box
    void setGeneralScenarioData(ScenarioData newData); //To be called from event receiver
    void checkName(); //Check if the scenario name chosen will mean that an existing scenario gets overwritten, and update flag in GeneralData

    void changeLeg(irr::s32 ship, irr::s32 index, irr::f32 legCourse, irr::f32 legSpeed, irr::f32 legDistance); //Change othership (or ownship) course, speed etc.
    void deleteLeg(irr::s32 ship, irr::s32 index);
    void addLeg(irr::s32 ship, irr::s32 afterLegNumber, irr::f32 legCourse, irr::f32 legSpeed, irr::f32 legDistance);
    void setMMSI(irr::s32 ship, int mmsi);
    void setDrifting(irr::s32 ship, bool drifting);
    void addShip(std::string name, irr::core::vector2df position);
	void deleteShip(irr::s32 ship);
    void recalculateLegTimes();

    void changeOwnShipName(std::string name);
    void changeOtherShipName(irr::s32 ship, std::string name);

    void save();

    // Chart interaction, from the event receiver. overGui: the cursor is over a window or button.
    // Left click on a ship selects it and dragging moves it; left drag elsewhere, or right drag, pans;
    // the wheel zooms about the cursor. Returns true if the event was used.
    bool onMouse(const irr::SEvent::SMouseInput& mouseInput, bool overGui);
    bool onKey(const irr::SEvent::SKeyInput& keyInput); // Home: back to own ship, left/right: heading of the selected ship
    void increaseZoom();
    void decreaseZoom();
    void nextChartStyle();
    irr::f32 chartWidth() const;  // metres
    irr::f32 chartHeight() const;

private:

    GUIMain* gui;
    Lang* lang;
    irr::IrrlichtDevice* device;
    irr::video::IVideoDriver* driver;

    //Data shared from main
    std::vector<PositionData>* buoysData;
    ScenarioData* scenarioData;
    std::string worldName;

    ChartView chart;
    bool viewInitialised;   // centred on the own ship at the first update

    irr::f32 terrainLong;
    irr::f32 terrainLat;
    irr::f32 terrainLongExtent;
    irr::f32 terrainLatExtent;
    irr::f32 terrainXWidth;
    irr::f32 terrainZWidth;

    irr::s32 dragShip;   // ship being dragged: 0 own ship, 1.. other ships, -1 none
    bool panning, rightDown;
    irr::core::position2di mouse, lastMouse, dragOffset;

    irr::s32 shipAt(irr::core::position2di screen) const; // 0 own ship, 1.. other ships, -1 none
    irr::core::position2di shipScreenPosition(irr::s32 ship) const;

    irr::s32 selectedShip; //Own ship as -1, other ships as 0 upwards
    irr::s32 selectedLeg; //No leg as -1, legs as 0 upwards

};

#endif // __CONTROLLERMODEL_HPP_INCLUDED__
