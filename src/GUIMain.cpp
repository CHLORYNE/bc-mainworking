/*
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

     // DEE What does this do ... draws the GUI screen



#include "GUIMain.hpp"
#include "BridgeSkin.hpp"
#include "CentreScreen.hpp"
#include "WeatherPanel.hpp"
#include <cctype>

#include "Constants.hpp"
#include "Utilities.hpp"
//#include "OutlineScrollBar.h"
#include "ScrollDial.h"
#include "AzimuthDial.h"

#include "SimulationModel.hpp"
#include "RadarScreen.hpp"   //kyara: for RADAR_FIT_MARGIN, shared with the radar picture scaling
#include "Lines.hpp"

#include <iostream> //for debugging
#include <cmath> //For fmod
#include <fstream>
#include "IniFile.hpp"
#include "ConsoleWindow.hpp"

namespace {
//Parent of the console's elements while the console is in its own window. It has no parent itself,
//so the console is out of the main GUI tree: the main window's clicks can never reach it, and its
//clicks (routed by GUIMain::dispatchConsoleWindowInput) never reach the main window's elements.
//GUI events from the console's buttons go to the device's receiver, exactly as from the root.
class ConsoleHostElement : public irr::gui::IGUIElement
{
public:
    ConsoleHostElement(irr::gui::IGUIEnvironment* env, irr::IrrlichtDevice* dev, const irr::core::rect<irr::s32>& r)
        : irr::gui::IGUIElement(irr::gui::EGUIET_ELEMENT, env, 0, -1, r), device(dev) {}
    virtual bool OnEvent(const irr::SEvent& event)
    {
        if (event.EventType == irr::EET_GUI_EVENT && device->getEventReceiver()) {
            return device->getEventReceiver()->OnEvent(event);
        }
        return false;
    }
private:
    irr::IrrlichtDevice* device;
};

std::string consolePlacementFile(irr::u32 instance)
{
    if (instance > 1) { return Utilities::getUserDir() + "consoleWindow-" + std::to_string(instance) + ".ini"; }
    return Utilities::getUserDir() + "consoleWindow.ini";
}
} // namespace

//using namespace irr;
//KYARA: maritime formatting helpers for the instructor console readouts.
namespace {

    //16-point compass. Wind uses this for the direction it blows FROM; current for the
    //direction it sets TOWARDS. Opposite conventions - deliberate, and taught that way.
    irr::core::stringw compass16(irr::f32 deg)
    {
        static const wchar_t* pts[16] = { L"N", L"NNE", L"NE", L"ENE", L"E", L"ESE", L"SE", L"SSE",
                                          L"S", L"SSW", L"SW", L"WSW", L"W", L"WNW", L"NW", L"NNW" };
        if (!std::isfinite(deg)) { deg = 0.f; }
        deg = fmodf(deg, 360.f); //(a loop adding 360 never ends on a huge value)
        if (deg < 0.f) { deg += 360.f; }
        return irr::core::stringw(pts[(int)((deg + 11.25f) / 22.5f) % 16]);
    }

    //Beaufort force from wind speed in knots (WMO scale).
    int beaufort(irr::f32 knots)
    {
        static const irr::f32 upper[12] = { 1, 3, 6, 10, 16, 21, 27, 33, 40, 47, 55, 63 };
        for (int f = 0; f < 12; f++) { if (knots < upper[f]) { return f; } }
        return 12;
    }

    //Three-digit degrees true, as written on a bridge: 007, 090, 270.
    irr::core::stringw degrees3(irr::f32 deg)
    {
        int d = (int)(deg + 0.5f) % 360;
        if (d < 0) { d += 360; }
        wchar_t buf[8];
        swprintf(buf, 8, L"%03d", d);
        return irr::core::stringw(buf);
    }
}
GUIMain::GUIMain()
{

}

void GUIMain::load(irr::IrrlichtDevice* device, Lang* language, std::vector<std::string>* logMessages, SimulationModel* model, bool singleEngine, bool azimuthDrive, bool controlsHidden, bool hasDepthSounder, irr::f32 maxSounderDepth, bool hasGPS, bool showTideHeight, bool hasBowThruster, bool hasSternThruster, bool hasRateOfTurnIndicator, bool showCollided, bool vr3dMode)
{
    this->device = device;
    collisionWarningTexture = device->getVideoDriver()->getTexture("media/warning_triangle.png"); // Collision kyara
    //KYARA: optional. If media/alarm_muted.png is missing, getTexture() returns 0 and the indicator
    //falls back to a drawn badge - so the feature works with no new asset, and looks nicer with one.
    alarmMutedTexture = device->getVideoDriver()->getTexture("media/alarm_muted.png");
    guiProxyAlarmMuted = false;
    this->model = model;
    this->hasDepthSounder = hasDepthSounder;
    this->maxSounderDepth = maxSounderDepth;
    this->hasGPS = hasGPS;
    this->showTideHeight = showTideHeight;
    this->showCollided = showCollided;

    this->hasBowThruster = hasBowThruster;
    this->hasRateOfTurnIndicator = hasRateOfTurnIndicator;
    this->controlsHidden = controlsHidden;

    this->hasSternThruster = hasSternThruster;
    guienv = device->getGUIEnvironment();


    irr::video::IVideoDriver* driver = device->getVideoDriver();
    su = driver->getScreenSize().Width;
    sh = driver->getScreenSize().Height;

    this->language = language;
    this->logMessages = logMessages;

    //UI COLOR CHANGE 2025-12-30 -Kyara
    //FOR ENGINE COLORS

    std::string userFolder = Utilities::getUserDir();
    std::string iniFilename = "bc5.ini";
    if (Utilities::pathExists(userFolder + iniFilename)) {
        iniFilename = userFolder + iniFilename;
    }

    // helper to parse "R,G,B" SColor -Kyara
    auto parseColor = [&](const char* key, const irr::video::SColor& def) {
        std::string s = IniFile::iniFileToString(iniFilename, key);
        int r = 0, g = 0, b = 0;
        if (sscanf_s(s.c_str(), "%d,%d,%d", &r, &g, &b) == 3) {
            return irr::video::SColor(255, r, g, b);
        }
        return def;
        };



    irr::video::SColor defTop = irr::video::SColor(128, 0, 255, 0);
    irr::video::SColor defBottom = irr::video::SColor(128, 255, 0, 0);

    engineTopColor = parseColor("enginePanelTopColor", defTop);
    engineBottomColor = parseColor("enginePanelBottomColor", defBottom);

    //KYARA: read the console flag here, before anything is created - the engine levers are built
    //further down and need to know which style to use.
    //  classic_panel=1       everything reverts to the original Bridge Command layout
    //  instrument_extra=...  which optional dials to add: rpm, wind, both (default), none
    instrumentsEnabled = (IniFile::iniFileTou32(iniFilename, "classic_panel") != 1);
    std::string extraInstruments = IniFile::iniFileToString(iniFilename, "instrument_extra");
    for (size_t i = 0; i < extraInstruments.size(); i++) { extraInstruments[i] = tolower(extraInstruments[i]); }
    if (extraInstruments.empty()) { extraInstruments = "wind"; } //one extra dial by default - six gauges crowd a single screen
    const bool showRPMDial = instrumentsEnabled && (extraInstruments == "rpm" || extraInstruments == "both");
    const bool showWindDial = instrumentsEnabled && (extraInstruments == "wind" || extraInstruments == "both");




    /*Set gui skin less transparent
    irr::video::SColor col = guienv->getSkin()->getColor(irr::gui::EGDC_3D_SHADOW);
    col.setAlpha(200);
    guienv->getSkin()->setColor(irr::gui::EGDC_3D_SHADOW, col);

    col = guienv->getSkin()->getColor(irr::gui::EGDC_3D_FACE);
    col.setAlpha(200);
    guienv->getSkin()->setColor(irr::gui::EGDC_3D_FACE, col);*/

    //The bridge look: one flat style for every widget, in day / dusk / night colours (BridgeSkin.hpp).
    //It wraps the skin main.cpp made, which keeps the fonts, sizes and icons.
    bridgeSkin = new bridge::BridgeSkin(guienv, guienv->getSkin());
    guienv->setSkin(bridgeSkin);
    bridgeSkin->drop(); //the environment holds it now


    //default to double engine in gui
    this->singleEngine = singleEngine;

    //set if we have azimuth controls, instead of engine and rudder
    this->azimuthDrive = azimuthDrive;

    // GUI position modifications if in azimuth drive mode
    if (azimuthDrive) {
        //azimuthGUIOffset = 0.05*su;
        azimuthGUIOffsetL = -0.02 * su;
        azimuthGUIOffsetR = -0.07 * su;
    }
    else {
        azimuthGUIOffsetL = 0;
        azimuthGUIOffsetR = 0;
    }

    //Initial settings for NFU buttons
    nfuPortDown = false;
    nfuStbdDown = false;

    //Default to small radar display
    radarLarge = false;
    //Find available 4:3 rectangle to fit in area for large radar display
    irr::s32 availableWidth;
    irr::s32 availableHeight = (0.95 - 0.01) * sh;
    if (azimuthDrive) {
        // leave 0.9*su on both sides
        availableWidth = (0.91 - 0.09) * su;
    }
    else {
        // leave 0.9*su on left, 0.01*su on right
        availableWidth = (0.99 - 0.09) * su;
    }
    if (availableWidth / (float)availableHeight > 4.0 / 3.0) {
        // Wider than 4:3
        irr::s32 activeWidth = availableHeight * 4.0 / 3.0;
        irr::s32 activeHeight = availableHeight;
        radarLargeRect = irr::core::rect<irr::s32>(0.09 * su + (availableWidth - activeWidth) / 2, 0.01 * sh, 0.09 * su + activeWidth + (availableWidth - activeWidth) / 2, 0.01 + activeHeight);
    }
    else {
        // 4:3 or narrower
        irr::s32 activeWidth = availableWidth;
        irr::s32 activeHeight = availableWidth * 3.0 / 4.0;
        radarLargeRect = irr::core::rect<irr::s32>(0.09 * su, 0.01 * sh + (availableHeight - activeHeight) / 2, 0.09 * su + activeWidth, 0.01 + activeHeight + (availableHeight - activeHeight) / 2);
    }
    //For brevity, store large radar window width and top left corner.
    irr::s32 radarSu = radarLargeRect.getWidth();
    irr::core::vector2d<irr::s32> radarTL = radarLargeRect.UpperLeftCorner;
    //Find radar screen centre X, Y and radius
    largeRadarScreenRadius = (radarLargeRect.LowerRightCorner.Y - radarTL.Y) / 2;
    largeRadarScreenCentreX = radarTL.X + largeRadarScreenRadius;
    largeRadarScreenCentreY = (radarLargeRect.LowerRightCorner.Y + radarTL.Y) / 2;
    //Make display slightly smaller, keeping the centre in the same place
    largeRadarScreenRadius *= 0.90; //Make display slightly smaller, keeping the centre in the same place
    // RADAR SCREEN 
    const irr::s32 boxL = largeRadarScreenCentreX - largeRadarScreenRadius;
    const irr::s32 boxT = largeRadarScreenCentreY - largeRadarScreenRadius;
    const irr::s32 boxB = largeRadarScreenCentreY + largeRadarScreenRadius;
    const irr::f32 R = (irr::f32)largeRadarScreenRadius;

    // Top-left: own-ship heading/speed
    radarLeftInfoTop = guienv->addStaticText(L"", irr::core::rect<irr::s32>(
        boxL, boxT, boxL + (irr::s32)(0.55f * R), boxT + (irr::s32)(0.28f * R)),
        false, true, 0, -1);
    radarLeftInfoTop->setOverrideColor(irr::video::SColor(255, 255, 220, 0));
    radarLeftInfoTop->setVisible(false); // Will be managed by updateVisibility

    // Bottom-left: selected-target readout
    radarLeftInfoBottom = guienv->addStaticText(L"", irr::core::rect<irr::s32>(
        boxL, boxB - (irr::s32)(0.30f * R), boxL + (irr::s32)(0.55f * R), boxB),
        false, true, 0, -1);
    radarLeftInfoBottom->setOverrideColor(irr::video::SColor(255, 255, 220, 0));
    radarLeftInfoBottom->setVisible(false); // Will be managed by updateVisibility
    //----------------------------END
    smallRadarScreenCentreX = su - 0.2 * sh + azimuthGUIOffsetR;
    smallRadarScreenCentreY = 0.8 * sh;
    smallRadarScreenRadius = 0.2 * sh;

    //gui - add scroll bars for speed and heading control directly
    hdgScrollbar = new irr::gui::OutlineScrollBar(false, guienv, guienv->getRootGUIElement(), GUI_ID_HEADING_SCROLL_BAR, irr::core::rect<irr::s32>(0.01 * su, 0.61 * sh, 0.04 * su, 0.99 * sh));
    hdgScrollbar->setMax(360);
    spdScrollbar = new irr::gui::OutlineScrollBar(false, guienv, guienv->getRootGUIElement(), GUI_ID_SPEED_SCROLL_BAR, irr::core::rect<irr::s32>(0.05 * su, 0.61 * sh, 0.08 * su, 0.99 * sh));
    spdScrollbar->setMax(20.f * 1852.f / 3600.f); //20 knots in m/s
    //Hide speed/heading bars normally
    hdgScrollbar->setVisible(false);
    spdScrollbar->setVisible(false);

    //Add engine, rudder and thruster bars
    irr::core::array<irr::s32> rudderTics; rudderTics.push_back(-25); rudderTics.push_back(-20); rudderTics.push_back(-15); rudderTics.push_back(-10); rudderTics.push_back(-5);
    rudderTics.push_back(5); rudderTics.push_back(10); rudderTics.push_back(15); rudderTics.push_back(20); rudderTics.push_back(25);

    //Values to show on wheel control (should be same size as rudderTics, but we probably want to show an unsigned version in the GUI
    irr::core::array<irr::s32> rudderIndicatorTics; rudderIndicatorTics.push_back(25); rudderIndicatorTics.push_back(20); rudderIndicatorTics.push_back(15); rudderIndicatorTics.push_back(10); rudderIndicatorTics.push_back(5);
    rudderIndicatorTics.push_back(5); rudderIndicatorTics.push_back(10); rudderIndicatorTics.push_back(15); rudderIndicatorTics.push_back(20); rudderIndicatorTics.push_back(25);


    irr::core::array<irr::s32> engineTics; engineTics.push_back(-80); engineTics.push_back(-60); engineTics.push_back(-40); engineTics.push_back(-20);
    engineTics.push_back(20); engineTics.push_back(40); engineTics.push_back(60); engineTics.push_back(80);

    irr::core::array<irr::s32> centreTic; centreTic.push_back(0);

    if (hasBowThruster) {
        irr::f32 verticalScreenPos;
        if (hasSternThruster) {
            verticalScreenPos = 0.99 - 2 * 0.04;
        }
        else {
            verticalScreenPos = 0.99 - 1 * 0.04;
        }

        // DEE bowthruster position
        bowThrusterScrollbar = new irr::gui::OutlineScrollBar(true, guienv, guienv->getRootGUIElement(), GUI_ID_BOWTHRUSTER_SCROLL_BAR, irr::core::rect<irr::s32>(0.01 * su, verticalScreenPos * sh, 0.08 * su, (verticalScreenPos + 0.04) * sh), engineTics, centreTic);
        bowThrusterScrollbar->setMax(100);
        bowThrusterScrollbar->setMin(-100);
        bowThrusterScrollbar->setPos(0);
        bowThrusterScrollbar->setToolTipText(language->translate("bowThruster").c_str());
    }
    else {
        bowThrusterScrollbar = 0;
    }

    if (hasSternThruster) {
        irr::f32 verticalScreenPos = 0.99 - 1 * 0.04;
        sternThrusterScrollbar = new irr::gui::OutlineScrollBar(true, guienv, guienv->getRootGUIElement(), GUI_ID_STERNTHRUSTER_SCROLL_BAR, irr::core::rect<irr::s32>(0.01 * su, verticalScreenPos * sh, 0.08 * su, (verticalScreenPos + 0.04) * sh), engineTics, centreTic);
        sternThrusterScrollbar->setMax(100);
        sternThrusterScrollbar->setMin(-100);
        sternThrusterScrollbar->setPos(0);
        sternThrusterScrollbar->setToolTipText(language->translate("sternThruster").c_str());
    }
    else {
        sternThrusterScrollbar = 0;
    }

    if (azimuthDrive) {
        // Azimuth drive
        portText = 0;
        portScrollbar = 0;
        stbdText = 0;
        stbdScrollbar = 0;
        wheelScrollbar = 0;
        nonFollowUpPortButton = 0;
        nonFollowUpStbdButton = 0;
        clickForRudderText = 0;
        clickForEngineText = 0;


        // DEE_NOV22 defines new objects

        //DEE_NOV22 comment below code by others.  they combine control and indication,  I'm just going to move them up a little to make room for other
        //	    indicators.  I'd prefer it to be a simple thrust direction indicator to be honest, 1 is port 2 is stbd
//            azimuth1Control = new irr::gui::AzimuthDial(irr::core::vector2d<irr::s32>(0.035*su,0.8*sh),0.03*su,guienv,guienv->getRootGUIElement(),GUI_ID_AZIMUTH_1); 
//            azimuth2Control = new irr::gui::AzimuthDial(irr::core::vector2d<irr::s32>(0.105*su,0.8*sh),0.03*su,guienv,guienv->getRootGUIElement(),GUI_ID_AZIMUTH_2); 

        azimuth1Control = new irr::gui::AzimuthDial(irr::core::vector2d<irr::s32>(0.035 * su, 0.77 * sh), 0.04 * sh, guienv, guienv->getRootGUIElement(), GUI_ID_AZIMUTH_2);
        azimuth2Control = new irr::gui::AzimuthDial(irr::core::vector2d<irr::s32>(0.965 * su, 0.77 * sh), 0.04 * sh, guienv, guienv->getRootGUIElement(), GUI_ID_AZIMUTH_2);

        azimuth1Control->setMax(360); // DEE_NOV22 comment sets maximum value port azimuth indicator
        azimuth2Control->setMax(360); // DEE_NOV22 comment sets maximum value stbd azimuth indicator

        // DEE_NOV22 vvvv change position of these
        //            azimuth1Master = guienv->addCheckBox(false,irr::core::rect<irr::s32>(0.025*su,0.88*sh,0.045*su,0.90*sh),0,GUI_ID_AZIMUTH_1_MASTER_CHECKBOX);
        //            azimuth2Master = guienv->addCheckBox(false,irr::core::rect<irr::s32>(0.095*su,0.88*sh,0.115*su,0.90*sh),0,GUI_ID_AZIMUTH_2_MASTER_CHECKBOX);

        azimuth1Master = guienv->addCheckBox(false, irr::core::rect<irr::s32>(0.025 * su, 0.82 * sh, 0.045 * su, 0.84 * sh), 0, GUI_ID_AZIMUTH_1_MASTER_CHECKBOX);
        azimuth2Master = guienv->addCheckBox(false, irr::core::rect<irr::s32>(0.955 * su, 0.82 * sh, 0.975 * su, 0.84 * sh), 0, GUI_ID_AZIMUTH_2_MASTER_CHECKBOX);

        // DEE_NOV22 ^^^^

        azimuth1Master->setToolTipText(language->translate("azimuthMaster").c_str());
        azimuth2Master->setToolTipText(language->translate("azimuthMaster").c_str());

        // DEE_NOV22 defines how and where indicators are displayed

                // DEE_NOV22 the schottels ... the bottom most pair of dial

        schottelPort = new irr::gui::AzimuthDial(irr::core::vector2d<irr::s32>(0.035 * su, 0.89 * sh), 0.04 * sh, guienv, guienv->getRootGUIElement(), GUI_ID_SCHOTTEL_PORT); // DEE_NOV22 visual representation of the physical schottel control todo in time, make it look like a schottel wheel
        schottelPort->setToolTipText(language->translate("Schottel Port").c_str());
        schottelPort->setMax(360); // DEE_NOV22 sets maximum value port schottel

        schottelStbd = new irr::gui::AzimuthDial(irr::core::vector2d<irr::s32>(0.965 * su, 0.89 * sh), 0.04 * sh, guienv, guienv->getRootGUIElement(), GUI_ID_SCHOTTEL_STBD); // DEE_NOV22 visual representation of the physical schottel control todo in time, make it look like a schottel wheel
        schottelStbd->setToolTipText(language->translate("Schottel Starboard").c_str());
        schottelStbd->setMax(360); // DEE_NOV22 sets maximum value stbd schottel

        // DEE_NOV22 added emergency steering checkox todo background code for this

        emergencySteering = guienv->addCheckBox(false, irr::core::rect<irr::s32>(0.955 * su, 0.94 * sh, 0.975 * su, 0.96 * sh), 0, GUI_ID_EMERGENCY_STEERING);
        emergencySteering->setToolTipText(language->translate("Emergency Steering").c_str());


        // DEE_NOV22 the engine rpm indicators (0..1) the top most pair

        azimuthEnginePort = new irr::gui::AzimuthDial(irr::core::vector2d<irr::s32>(0.035 * su, 0.65 * sh), 0.04 * sh, guienv, guienv->getRootGUIElement(), GUI_ID_AZIMUTH_ENGINE_PORT); // DEE_NOV22 visual representation of the port engine rpm as a proportion of max revs so 0..1, there is no reverse engine
        azimuthEnginePort->setToolTipText(language->translate("Engine Port").c_str());
        azimuthEnginePort->setMax(360); // DEE_NOV22 sets maximum value port engine indicator

        azimuthEngineStbd = new irr::gui::AzimuthDial(irr::core::vector2d<irr::s32>(0.965 * su, 0.65 * sh), 0.04 * sh, guienv, guienv->getRootGUIElement(), GUI_ID_AZIMUTH_ENGINE_STBD); // DEE_NOV22 visual representation of the starboard engine rpm as a proportion of max revs so 0..1, there is no reverse engine
        azimuthEngineStbd->setToolTipText(language->translate("Engine Starboard").c_str());
        azimuthEngineStbd->setMax(360); // DEE_NOV22 sets maximum value stbd engine indicator

        azimuthClutchPort = guienv->addCheckBox(false, irr::core::rect<irr::s32>(0.025 * su, 0.70 * sh, 0.045 * su, 0.72 * sh), 0, GUI_ID_AZIMUTH_CLUTCH_PORT);
        azimuthClutchStbd = guienv->addCheckBox(false, irr::core::rect<irr::s32>(0.955 * su, 0.70 * sh, 0.975 * su, 0.72 * sh), 0, GUI_ID_AZIMUTH_CLUTCH_STBD);
        azimuthClutchPort->setToolTipText(language->translate("Port Clutch").c_str());
        azimuthClutchStbd->setToolTipText(language->translate("Starboard Clutch").c_str());



        // DEE_NOV22 ^^^^

    }
    else {  // is Not azimuth drive
        azimuth1Control = 0;
        azimuth2Control = 0;
        azimuth1Master = 0;
        azimuth2Master = 0;
        // DEE_NOV22 vvvv hide the azimuth drive controls
        schottelPort = 0;
        schottelStbd = 0;
        azimuthClutchPort = 0; // DEE_NOV22 not sure about this, I think perhaps clutch should be on some non azi engines like CPP vessels
        azimuthClutchStbd = 0; // DEE_NOV22 as above
        azimuthEnginePort = 0;
        azimuthEngineStbd = 0;
        emergencySteering = 0; // though perhaps this would be useful for conventional ships too
        // DEE_NOV22 ^^^^


        portText = guienv->addStaticText(language->translate("portEngine").c_str(), irr::core::rect<irr::s32>(0.005 * su, 0.61 * sh, 0.045 * su, 0.67 * sh));
        portText->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);

        //COLORCHANGE TITLE ENGINE ONE -KYARA
        portText->setOverrideColor(irr::video::SColor(255, 255, 0, 0));
        //KYARA: telegraph-style lever in the console layout (same -100..100 range, same event, so
        //MyEventReceiver is untouched); the original slider when classic_panel=1.
        const irr::core::rect<irr::s32> portLeverPos(0.01 * su, 0.675 * sh, 0.04 * su, (0.99 - 0.04 * hasBowThruster - 0.04 * hasSternThruster) * sh);
        if (instrumentsEnabled) {
            portScrollbar = new irr::gui::GUIEngineLever(guienv, guienv->getRootGUIElement(), GUI_ID_PORT_SCROLL_BAR, portLeverPos, engineTopColor, engineBottomColor);
        }
        else {
            portScrollbar = new irr::gui::OutlineScrollBar(false, guienv, guienv->getRootGUIElement(), GUI_ID_PORT_SCROLL_BAR, portLeverPos, engineTics, centreTic);
        }
        portScrollbar->setMax(100);
        portScrollbar->setMin(-100);
        portScrollbar->setPos(0);
        stbdText = guienv->addStaticText(language->translate("stbdEngine").c_str(), irr::core::rect<irr::s32>(0.045 * su, 0.61 * sh, 0.085 * su, 0.67 * sh));
        stbdText->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
        //COLORCHANGE TITLE ENGINE TWO   - KYARA
        stbdText->setOverrideColor(irr::video::SColor(255, 30, 227, 53));
        const irr::core::rect<irr::s32> stbdLeverPos(0.05 * su, 0.675 * sh, 0.08 * su, (0.99 - 0.04 * hasBowThruster - 0.04 * hasSternThruster) * sh);
        if (instrumentsEnabled) {
            stbdScrollbar = new irr::gui::GUIEngineLever(guienv, guienv->getRootGUIElement(), GUI_ID_STBD_SCROLL_BAR, stbdLeverPos, engineTopColor, engineBottomColor);
        }
        else {
            stbdScrollbar = new irr::gui::OutlineScrollBar(false, guienv, guienv->getRootGUIElement(), GUI_ID_STBD_SCROLL_BAR, stbdLeverPos, engineTics, centreTic);
        }
        stbdScrollbar->setMax(100);
        stbdScrollbar->setMin(-100);
        stbdScrollbar->setPos(0);

        /*Original code
         portText->setOverrideColor(irr::video::SColor(255, 128, 0, 0));
         portScrollbar = new irr::gui::OutlineScrollBar(false,guienv,guienv->getRootGUIElement(),GUI_ID_PORT_SCROLL_BAR,irr::core::rect<irr::s32>(0.01*su, 0.675*sh, 0.04*su, (0.99-0.04*hasBowThruster-0.04*hasSternThruster)*sh),engineTics,centreTic);
         portScrollbar->setMax(100);
         portScrollbar->setMin(-100);
         portScrollbar->setPos(0);
         stbdText = guienv->addStaticText(language->translate("stbdEngine").c_str(),irr::core::rect<irr::s32>(0.045*su, 0.61*sh, 0.085*su, 0.67*sh));
         stbdText->setTextAlignment(irr::gui::EGUIA_CENTER,irr::gui::EGUIA_CENTER);
         stbdText->setOverrideColor(irr::video::SColor(255,0,128,0));
         stbdScrollbar = new irr::gui::OutlineScrollBar(false,guienv,guienv->getRootGUIElement(),GUI_ID_STBD_SCROLL_BAR,irr::core::rect<irr::s32>(0.05*su, 0.675*sh, 0.08*su, (0.99-0.04*hasBowThruster-0.04*hasSternThruster)*sh),engineTics,centreTic);
         stbdScrollbar->setMax(100);
         stbdScrollbar->setMin(-100);
         stbdScrollbar->setPos(0);*/

        wheelScrollbar = new irr::gui::OutlineScrollBar(true, guienv, guienv->getRootGUIElement(), GUI_ID_WHEEL_SCROLL_BAR, irr::core::rect<irr::s32>(0.13 * su, 0.96 * sh, 0.45 * su, 0.99 * sh), rudderTics, centreTic, true, rudderIndicatorTics);
        wheelScrollbar->setMax(30);
        wheelScrollbar->setMin(-30);
        wheelScrollbar->setPos(0);


        nonFollowUpPortButton = guienv->addButton(irr::core::rect<irr::s32>(0.09 * su, 0.96 * sh, 0.11 * su, 0.99 * sh), 0, GUI_ID_NFU_PORT_BUTTON, language->translate("NFUPort").c_str());
        nonFollowUpStbdButton = guienv->addButton(irr::core::rect<irr::s32>(0.11 * su, 0.96 * sh, 0.13 * su, 0.99 * sh), 0, GUI_ID_NFU_STBD_BUTTON, language->translate("NFUStbd").c_str());

        //Adapt if single engine:
        if (singleEngine) {
            stbdScrollbar->setVisible(false);
            stbdText->setVisible(false);

            //Get max extent of both engine scroll bars
            irr::core::vector2d<irr::s32> lowerRight = stbdScrollbar->getRelativePosition().LowerRightCorner;
            irr::core::vector2d<irr::s32> upperLeft = portScrollbar->getRelativePosition().UpperLeftCorner;
            portScrollbar->setRelativePosition(irr::core::rect<irr::s32>(upperLeft, lowerRight));

            //Change text from 'portEngine' to 'engine', and use all space
            portText->setText(language->translate("engine").c_str());
            portText->enableOverrideColor(false);
            lowerRight = stbdText->getRelativePosition().LowerRightCorner;
            upperLeft = portText->getRelativePosition().UpperLeftCorner;
            portText->setRelativePosition(irr::core::rect<irr::s32>(upperLeft, lowerRight));
        }

        //Add 'hint' text to click on the rudder and wheel controls
        clickForRudderText = guienv->addStaticText(language->translate("startupHelpRudder").c_str(), wheelScrollbar->getAbsolutePosition());
        clickForRudderText->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
        clickForRudderText->setOverrideColor(irr::video::SColor(255, 255, 0, 0));

        irr::core::rect<irr::s32> engineHintPos = irr::core::rect<irr::s32>(
            portScrollbar->getRelativePosition().UpperLeftCorner,
            stbdScrollbar->getRelativePosition().LowerRightCorner);

        clickForEngineText = guienv->addStaticText(language->translate("startupHelpEngine").c_str(), engineHintPos);
        clickForEngineText->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
        clickForEngineText->setOverrideColor(irr::video::SColor(255, 255, 0, 0));
    }

    //add data display:
    stdDataDisplayPos = irr::core::rect<irr::s32>(0.09 * su + azimuthGUIOffsetL, 0.71 * sh, 0.45 * su + azimuthGUIOffsetR, 0.95 * sh); //In normal view
    //radDataDisplayPos = irr::core::rect<irr::s32>(0.83 * su, 0.96 * sh, 0.99 * su, 0.99 * sh); //In maximised 3d view
   // altDataDisplayPos = irr::core::rect<irr::s32>(0.83 * su, 0.96 * sh, 0.99 * su, 0.99 * sh); //In maximised 3d view
    radDataDisplayPos = irr::core::rect<irr::s32>(0.83 * su, 0.96 * sh, 0.90 * su, 0.99 * sh); //In maximised 3d view
    altDataDisplayPos = irr::core::rect<irr::s32>(0.83 * su, 0.96 * sh, 0.90 * su, 0.99 * sh); //In maximised 3d view
    dataDisplay = guienv->addStaticText(L"", stdDataDisplayPos, true, false, 0, -1, true); //Actual text set later
    stdDataDisplayBG = dataDisplay->getBackgroundColor();
    altDataDisplayBG = irr::video::SColor(200 / 4, 255, 255, 255);
    radDataDisplayBG = irr::video::SColor(200 / 4, 255, 255, 255);
    //CHANGES color for UI -KYARA
    stdDataDisplayBG = dataDisplay->getBackgroundColor();
    altDataDisplayBG = irr::video::SColor(255, 144, 0, 1);
    radDataDisplayBG = irr::video::SColor(255, 144, 0, 1);

    //=============================================================================================
    //KYARA: INSTRUMENT CONSOLE
    //In the normal view this replaces the text data box, the heading tape, the small radar picture
    //and the small radar's control tabs. The radar CALCULATION keeps running (ARPA, guard alarm,
    //sync with the radar station) - only the small picture is gone. The RADAR button in the
    //console's status column still opens the full-screen radar.
    //bc5.ini keys (all optional, read near the top of this function):
    //  classic_panel=1          old layout back, no rebuild needed
    //  instrument_extra=wind    optional dials: rpm | wind | both | none
    //  instrument_speed_max=20  top of the speed log, knots
    //  instrument_rudder=1      put the BARRE dial back in the console (hidden by default)
    //  instrument_rot_max=60    rate-of-turn full scale each side, deg/min
    //Which dials appear follows the ship: no rudder dial on azimuth drives, rate of turn only if
    //the ship has one, GPS lines only with GPS, depth only with a sounder.
    instrumentPanel = 0;
    if (instrumentsEnabled) {
        const irr::core::rect<irr::s32> panelPos(
            (irr::s32)(0.09 * su + azimuthGUIOffsetL), (irr::s32)(0.608 * sh),
            (irr::s32)(0.995 * su + azimuthGUIOffsetR), (irr::s32)(0.910 * sh));
        instrumentPanel = new irr::gui::GUIInstrumentPanel(guienv, guienv->getRootGUIElement(), -1, panelPos);
        //The rudder angle is read on the bridge itself, so the BARRE dial is off unless asked for.
        const bool rudderDial = !azimuthDrive && IniFile::iniFileTou32(iniFilename, "instrument_rudder") == 1;
        instrumentPanel->setFit(rudderDial, hasRateOfTurnIndicator, hasGPS, hasDepthSounder, maxSounderDepth, showTideHeight);

        //Shaft tachometer and true wind dial, in the space beside the other gauges.
        instrumentExtraRPM = showRPMDial;
        instrumentExtraWind = showWindDial;
        instrumentPanel->setExtraInstruments(showRPMDial, showWindDial, 1000.0f, singleEngine);

        //KYARA HOULE: TANGAGE + GITE dials. bc5.ini instrument_motion=0 hides them (default shown).
        {
            const std::string motionKey = IniFile::iniFileToString(iniFilename, "instrument_motion");
            instrumentPanel->setMotionInstruments(motionKey != "0");
        }

        //KYARA: the rudder dial doubles as the helm - drag inside it to order a wheel angle. That
        //is what replaces the slider that used to run along the bottom of the screen.
        if (!azimuthDrive) { instrumentPanel->setHelmControl(true, 30.0f); }

        const irr::f32 iniSpeedMax = IniFile::iniFileTof32(iniFilename, "instrument_speed_max");
        const irr::f32 iniRotMax = IniFile::iniFileTof32(iniFilename, "instrument_rot_max");
        instrumentPanel->setScales(iniSpeedMax > 0 ? iniSpeedMax : 20.0f, iniRotMax > 0 ? iniRotMax : 60.0f, 35.0f);

        //Status column: as wide as the widest label it holds (same rule as the Amarres window -
        //size from the font, never from su, so it is right on Eyefinity too).
        irr::gui::IGUIFont* scFont = guienv->getSkin() ? guienv->getSkin()->getFont() : 0;
        const irr::s32 scCh = scFont ? (irr::s32)scFont->getDimension(L"X").Height : 16;
        irr::s32 scW = scCh * 7;
        if (scFont) {
            const irr::core::stringw scLabels[4] = { language->translate("pump1").c_str(), language->translate("pump2").c_str(),
                                                     language->translate("ackAlarms").c_str(), L"RADAR" };
            for (int i = 0; i < 4; i++) {
                const irr::s32 w = (irr::s32)scFont->getDimension(scLabels[i].c_str()).Width + 2 * scCh;
                if (w > scW) { scW = w; }
            }
        }
        instrumentPanel->setStatusColumnWidth(scW);
        consolePanelAttachedRect = panelPos;
        consoleBaseStatusW = scW;
        consoleAttachedGaugeD = instrumentPanel->getGaugeDiameter();
        consoleFontName = IniFile::iniFileToString(iniFilename, "font");
        const irr::f32 fontScale = IniFile::iniFileTof32(iniFilename, "font_scale");
        if (fontScale > 0) { consoleBaseFontSize = (irr::s32)(12 * fontScale + 0.5f); }
    }
    //=============================================================================================
    //Slim console layout: the band under the 3D view is only as tall as the dials need, with the
    //command bar along the bottom; the 3D view gets the rest of the screen (viewProportion3D).
    //Azimuth-drive ships keep their own arrangement of dials round the console.
    //Screens side by side seen as one (Surround / Eyefinity): everything in the band goes on the
    //middle screen. bc5.ini menu_screens (0 = from the window's shape), triple_screen=1 = three.
    {
        int forced = (int)IniFile::iniFileTou32(iniFilename, "menu_screens");
        const bool tripleView = IniFile::iniFileTou32(iniFilename, "triple_screen") == 1;
        if (forced == 0 && tripleView) { forced = 3; }
        centre::state().forcedScreens = forced;
        const irr::core::dimension2du window((irr::u32)su, (irr::u32)sh);
        const int screens = centre::screensAcross(window);
        consoleArea = centre::middleArea(window, screens);
        //The three camera columns are exactly the three screens: the side ones can run to the bottom
        sideScreensFree = tripleView && screens == 3;
    }
    //x at fraction f of the console's width (the middle screen on Surround, else the window)
    auto X = [&](irr::f32 f) { return consoleArea.UpperLeftCorner.X + (irr::s32)(f * consoleArea.getWidth()); };
    if (instrumentsEnabled && instrumentPanel) {
        irr::gui::IGUIFont* barFont = guienv->getSkin() ? guienv->getSkin()->getFont() : 0;
        const irr::s32 fh = barFont ? (irr::s32)barFont->getDimension(L"Ag").Height : 16;
        const irr::s32 m = irr::core::max_(4, (irr::s32)(0.006 * sh));
        const irr::s32 barH = irr::core::max_(fh * 2 + 10, (irr::s32)(0.042 * sh));
        if (!azimuthDrive) {
            //The dial size the width allows (the panel is still at its old height here), at most a
            //quarter of the screen
            instrumentPanel->setRelativePosition(irr::core::rect<irr::s32>(X(0.09f), (irr::s32)(0.608 * sh), X(0.995f), (irr::s32)(0.910 * sh)));
            irr::f32 D = instrumentPanel->getGaugeDiameter();
            if (D > 0.24f * sh) { D = 0.24f * sh; }
            const irr::s32 consoleH = (irr::s32)(D * 1.12f) + 4;
            irr::s32 bandTop = (irr::s32)sh - (m + consoleH + m + barH + m);
            if (bandTop < (irr::s32)(0.6 * sh)) { bandTop = (irr::s32)(0.6 * sh); }
            viewProportion3D() = (irr::f32)bandTop / (irr::f32)sh;
            const irr::s32 top = bandTop + m;
            const irr::s32 consoleBottom = (irr::s32)sh - m - barH - m;
            const irr::core::rect<irr::s32> consoleRect(X(0.09f), top, X(0.995f), consoleBottom);
            instrumentPanel->setRelativePosition(consoleRect);
            consolePanelAttachedRect = consoleRect;
            consoleAttachedGaugeD = instrumentPanel->getGaugeDiameter();

            //Engine levers beside it, their labels on top, the thrusters (if any) underneath
            const irr::s32 labelH = fh + 4;
            const irr::s32 thrusterH = (irr::s32)(0.034 * sh);
            const irr::s32 thrusters = (bowThrusterScrollbar ? 1 : 0) + (sternThrusterScrollbar ? 1 : 0);
            const irr::s32 leverTop = top + labelH;
            const irr::s32 leverBottom = consoleBottom - thrusters * (thrusterH + 2);
            const irr::s32 xa = X(0.008f), xb = X(0.045f), xc = X(0.085f);
            if (portText) {
                portText->setRelativePosition(singleEngine ? irr::core::rect<irr::s32>(xa, top, xc, top + labelH)
                                                           : irr::core::rect<irr::s32>(xa, top, xb, top + labelH));
            }
            // One-line labels: the translations carry a line break for the classic tall layout
            auto oneLine = [](irr::gui::IGUIStaticText* t) {
                if (!t) { return; }
                irr::core::stringw text = t->getText();
                text.replace(L'\n', L' ');
                t->setText(text.c_str());
                t->setWordWrap(false);
            };
            if (stbdText) { stbdText->setRelativePosition(irr::core::rect<irr::s32>(xb, top, xc, top + labelH)); }
            oneLine(portText);
            oneLine(stbdText);
            if (portScrollbar) {
                portScrollbar->setRelativePosition(singleEngine ? irr::core::rect<irr::s32>(xa + 2, leverTop, xc - 2, leverBottom)
                                                                : irr::core::rect<irr::s32>(xa + 2, leverTop, xb - 2, leverBottom));
            }
            if (stbdScrollbar) { stbdScrollbar->setRelativePosition(irr::core::rect<irr::s32>(xb + 2, leverTop, xc - 2, leverBottom)); }
            irr::s32 ty = leverBottom + 2;
            if (bowThrusterScrollbar) {
                bowThrusterScrollbar->setRelativePosition(irr::core::rect<irr::s32>(xa, ty, xc, ty + thrusterH));
                ty += thrusterH + 2;
            }
            if (sternThrusterScrollbar) {
                sternThrusterScrollbar->setRelativePosition(irr::core::rect<irr::s32>(xa, ty, xc, ty + thrusterH));
            }
            if (clickForEngineText) { clickForEngineText->setRelativePosition(irr::core::rect<irr::s32>(xa, leverTop, xc, leverBottom)); }
            commandBarRect = irr::core::rect<irr::s32>(X(0.005f), (irr::s32)sh - m - barH, X(0.995f), (irr::s32)sh - m);
            if (emergencySteering) {
                emergencySteering->setRelativePosition(irr::core::rect<irr::s32>(X(0.955f), (irr::s32)(0.94 * sh), X(0.975f), (irr::s32)(0.96 * sh)));
            }
        }
        else {
            commandBarRect = irr::core::rect<irr::s32>((irr::s32)(0.09 * su + azimuthGUIOffsetL), (irr::s32)(0.915 * sh),
                (irr::s32)(0.995 * su + azimuthGUIOffsetR), (irr::s32)(0.915 * sh) + barH);
        }
        //Made before the buttons, so they are drawn on top of it
        commandBar = new bridge::CommandBar(guienv, guienv->getRootGUIElement(), commandBarRect);
        commandBar->drop();
    }

    //Row i (0..3) of the status column: 0 = RADAR, 1 = pump 1, 2 = pump 2, 3 = Acquitter.
    //Only called when instrumentPanel exists.
    auto statusRow = [&](int i) -> irr::core::rect<irr::s32> {
        const irr::core::rect<irr::s32> col = instrumentPanel->getStatusColumnRect();
        const irr::s32 gapY = col.getHeight() / 20;
        const irr::s32 rowH = (col.getHeight() - 3 * gapY) / 4;
        const irr::s32 top = col.UpperLeftCorner.Y + i * (rowH + gapY);
        return irr::core::rect<irr::s32>(col.UpperLeftCorner.X, top, col.LowerRightCorner.X, top + rowH);
        };
    //=============================================================================================


    guiHeading = 0;
    guiSpeed = 0;

    //Add heading indicator
    stdHdgIndicatorPos = irr::core::rect<irr::s32>(0.09 * su + azimuthGUIOffsetL, 0.630 * sh, 0.45 * su + azimuthGUIOffsetR, 0.680 * sh); //In normal view
    radHdgIndicatorPos = irr::core::rect<irr::s32>(0.46 * su, 0.96 * sh, 0.82 * su, 0.99 * sh); //In maximised radar view
    maxHdgIndicatorPos = irr::core::rect<irr::s32>(0.46 * su, 0.96 * sh, 0.82 * su, 0.99 * sh); //In maximised 3d view
    //Slim layout: the command bar fills the bottom strip, so the heading tape and the data box of the
    //full-screen views sit just above it instead of on top of its keys.
    if (commandBar && commandBarRect.LowerRightCorner.Y > (irr::s32)(0.95 * sh)) {
        const irr::s32 bottom = commandBarRect.UpperLeftCorner.Y - irr::core::max_(4, (irr::s32)(0.006 * sh));
        const irr::s32 top = bottom - (irr::s32)(0.03 * sh);
        radHdgIndicatorPos = irr::core::rect<irr::s32>(X(0.46f), top, X(0.82f), bottom);
        maxHdgIndicatorPos = radHdgIndicatorPos;
        radDataDisplayPos = irr::core::rect<irr::s32>(X(0.83f), top, X(0.90f), bottom);
        altDataDisplayPos = radDataDisplayPos;
    }
    headingIndicator = new irr::gui::HeadingIndicator(guienv, guienv->getRootGUIElement(), stdHdgIndicatorPos);

    // DEE vvvvv add very basic rate of turn indicator
// rewrite this with its own class so that it is more realistic i.e. either a dial or a conning display

    rateofturnScrollbar = new irr::gui::OutlineScrollBar(true, guienv, guienv->getRootGUIElement(), GUI_ID_RATE_OF_TURN_SCROLL_BAR, irr::core::rect<irr::s32>(0.10 * su + azimuthGUIOffsetL, 0.87 * sh, 0.20 * su + azimuthGUIOffsetL, 0.91 * sh), rudderTics, centreTic);

    rateofturnScrollbar->setMax(50);
    rateofturnScrollbar->setMin(-50);
    rateofturnScrollbar->setSmallStep(1);
    rateofturnScrollbar->setPos(0);
    rateofturnScrollbar->setToolTipText(language->translate("rotText").c_str());
    if (!hasRateOfTurnIndicator) {
        rateofturnScrollbar->setVisible(false);
    }

    // DEE ^^^^^

            // add indicators for whether the rudder pumps are working
    pump1On = guienv->addStaticText(language->translate("pump1").c_str(), irr::core::rect<irr::s32>(0.35 * su + azimuthGUIOffsetR, 0.72 * sh, 0.44 * su + azimuthGUIOffsetR, 0.745 * sh), true, false, 0, -1, true);
    pump2On = guienv->addStaticText(language->translate("pump2").c_str(), irr::core::rect<irr::s32>(0.35 * su + azimuthGUIOffsetR, 0.75 * sh, 0.44 * su + azimuthGUIOffsetR, 0.775 * sh), true, false, 0, -1, true);
    ackAlarms = guienv->addButton(irr::core::rect<irr::s32>(0.35 * su + azimuthGUIOffsetR, 0.78 * sh, 0.44 * su + azimuthGUIOffsetR, 0.805 * sh), 0, GUI_ID_ACK_ALARMS_BUTTON, language->translate("ackAlarms").c_str());
    pump1On->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
    pump2On->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
    if (instrumentPanel) { //kyara: annunciator lamps + ack in the console's status column
        pump1On->setRelativePosition(statusRow(1));
        pump2On->setRelativePosition(statusRow(2));
        ackAlarms->setRelativePosition(statusRow(3));
    }

    //Add an additional window for controls (will normally be hidden)
    //=============================================================================================
    //KYARA: EYEFINITY-SAFE WINDOW SIZING
    //On the Eyefinity canvas, 'su' is the width of ALL THREE screens (~5760px), not one. Any window
    //sized as a FRACTION of su therefore gets stretched to ~2000px wide. The Amarres window does not
    //suffer from this, and that is not luck: it caps its size in CHARACTER units and uses su only to
    //POSITION itself. Same pattern here - place with su/sh, size with the font. That makes the window
    //immune to however many screens are attached.
    //('sh' is safe on its own - Eyefinity only stretches the width - but capping both keeps the
    //window proportions stable if the TVs are ever a different aspect.)
    irr::gui::IGUIFont* ecFont = guienv->getSkin() ? guienv->getSkin()->getFont() : 0;
    irr::core::dimension2du ecSample = ecFont ? ecFont->getDimension(L"X")
        : irr::core::dimension2du(8, 16);
    const irr::s32 ecCh = (irr::s32)ecSample.Height; //one character height - the unit for everything below

    const irr::s32 ecVO = guienv->getSkin()
        ? guienv->getSkin()->getSize(irr::gui::EGDS_WINDOW_BUTTON_WIDTH) + 5
        : 20; //title bar height - same idea as lwVO in the lines window below

    //KYARA: ROW GRID FIRST, WINDOW SECOND.
    //Previously the rows were a fraction of the window and the window was an arbitrary 24 lines tall,
    //so the shorter tabs left a big empty strip at the bottom. Now the row grid is sized from the
    //FONT (so rows stay readable at any resolution), and the WINDOW is sized to fit exactly the
    //tallest tab. No dead space, by construction.
    const irr::s32 tabHeaderH = (irr::s32)(ecCh * 1.9f);
    const irr::s32 rowH = (irr::s32)(ecCh * 1.7f);
    const irr::s32 rowPitch = (irr::s32)(ecCh * 2.4f);
    const irr::s32 row0Y = (irr::s32)(ecCh * 0.7f);
    const int      ecMaxRows = 7; //Feux is the tallest tab: 8 situations, deck lights, then the answer key

    //Height the tab body needs, then what the whole window needs to hold it.
    const irr::s32 tabBodyH = row0Y + (ecMaxRows - 1) * rowPitch + rowH + (irr::s32)(ecCh * 0.7f);
    const irr::u32 ecTargetHeight = (irr::u32)(ecVO + tabHeaderH + tabBodyH + (irr::s32)(ecCh * 0.8f));
    const irr::u32 ecTargetWidth = ecSample.Width * 82; //~82 characters. Wider = longer slider travel.

    irr::core::rect<irr::s32> extraControlsPos = irr::core::rect<irr::s32>(
        0.09 * su + azimuthGUIOffsetL, 0.56 * sh,
        0.45 * su + azimuthGUIOffsetR, 0.95 * sh);

    if ((irr::u32)extraControlsPos.getWidth() > ecTargetWidth) {
        extraControlsPos.LowerRightCorner.X = extraControlsPos.UpperLeftCorner.X + ecTargetWidth;
    }
    //Height is now set outright, not just clamped, so the window shrinks to fit the content.
    extraControlsPos.LowerRightCorner.Y = extraControlsPos.UpperLeftCorner.Y + ecTargetHeight;

    extraControlsWindow = guienv->addWindow(extraControlsPos);
    extraControlsWindow->getCloseButton()->setVisible(false);
    extraControlsWindow->setText(language->translate("extraControls").c_str());
    guienv->addButton(extraControlsWindow->getCloseButton()->getRelativePosition(), extraControlsWindow, GUI_ID_HIDE_EXTRA_CONTROLS_BUTTON, L"X");
    extraControlsWindow->setVisible(false);

    //KYARA: from here on, EVERYTHING inside this window is measured against the WINDOW, never
    //against su/sh. This is the rule that was being broken. Any child of a window or tab must be
    //sized from its parent, or it will shoot straight through the frame on a multi-screen canvas.
    const irr::s32 ecW = extraControlsPos.getWidth();
    const irr::s32 ecH = extraControlsPos.getHeight();

    // Add tab control for extra settings like weather and machinery failure
    irr::core::rect<irr::s32> extraControlsTabPosition = irr::core::rect<irr::s32>(
        (irr::s32)(ecCh * 0.4f),
        ecVO,
        ecW - (irr::s32)(ecCh * 0.4f),
        ecH - (irr::s32)(ecCh * 0.4f)
    );
    irr::gui::IGUITabControl* extraControlsTabControl = guienv->addTabControl(extraControlsTabPosition, extraControlsWindow);
    extraControlsTabControl->setTabHeight(tabHeaderH);

    // Weather tab
    irr::gui::IGUITab* extraControlsTabWeather = extraControlsTabControl->addTab(language->translate("weather").c_str());

    //KYARA: dials -> horizontal sliders. The members are IGUIScrollBar*, and both ScrollDial and
     //Irrlicht's own scrollbar derive from it, so nothing downstream changes: getPos()/setPos() and
     //EGET_SCROLL_BAR_CHANGED in MyEventReceiver work exactly as before. The old setOverrideColor()/
     //setLineThickness() blocks MUST go with the dials - they static_cast to ScrollDial*, and doing
     //that to a plain CGUIScrollBar is UB.
     //Note the radar gain/clutter/rain dials are NOT touched, so ScrollDial.h stays included.
    //---------------------------------------------------------------------------------------------
    //KYARA: EXTRA CONTROLS SLIDERS
    //A child of a TAB is positioned relative to the TAB, not the screen. Everything below is a
    //fraction of the tab's own size, so it fits by construction and needs no retuning however many
    //screens are attached. (The old slLabelX0..slHeight constants that used to live here were dead
    //code from an earlier revision - nothing read them, and they were all su-based.)
    const irr::s32 tabW = extraControlsTabPosition.getWidth();
    const irr::s32 tabH = tabBodyH; //rowH / rowPitch / row0Y are already defined above, from the font

    //KYARA: columns. The track is now ~46% of a wider window (was ~39% of a narrower one), so the
    //slider has noticeably more travel - finer control over wind direction in particular.
    const irr::s32 chipX0 = (irr::s32)(tabW * 0.012f);
    const irr::s32 chipX1 = (irr::s32)(tabW * 0.030f);
    const irr::s32 labelX0 = (irr::s32)(tabW * 0.045f);
    const irr::s32 labelX1 = (irr::s32)(tabW * 0.330f);
    const irr::s32 trackX0 = (irr::s32)(tabW * 0.345f);
    const irr::s32 trackX1 = (irr::s32)(tabW * 0.845f);
    const irr::s32 valueX0 = (irr::s32)(tabW * 0.855f);
    const irr::s32 valueX1 = (irr::s32)(tabW * 0.988f);

    //KYARA: rebalanced for the lighter Nord panel. On the old near-black navy these could be dim;
    //against a mid-tone grey-blue they need more saturation or the label text goes muddy. The two
    //that changed most are VIS (was near-white, which vanishes on a light panel) and STRM_SPD (was
    //too dark to read). Hue logic unchanged: blue = from the sky, amber = wind, green = water.
    const irr::video::SColor COL_SEASTATE(255, 34, 211, 195); //bright teal
    const irr::video::SColor COL_RAIN(255, 120, 190, 255); //sky blue
    const irr::video::SColor COL_VIS(255, 236, 242, 248); //fog white
    const irr::video::SColor COL_WIND_DIR(255, 245, 190, 66); //amber
    const irr::video::SColor COL_WIND_SPD(255, 250, 145, 50); //deep amber
    const irr::video::SColor COL_STRM_DIR(255, 110, 220, 145); //tidal green
    const irr::video::SColor COL_STRM_SPD(255, 60, 190, 115); //deep tidal green

    struct SliderRow { irr::gui::IGUIScrollBar* bar; irr::gui::IGUIStaticText* value; };

    auto addSliderRow = [&](irr::gui::IGUIElement* tab, int row, const wchar_t* label,
        irr::s32 id, irr::s32 minV, irr::s32 maxV,
        irr::s32 smallStep, irr::s32 largeStep,
        irr::video::SColor colour, const wchar_t* tip) -> SliderRow
        {
            const irr::s32 y = row0Y + row * rowPitch;

            //An empty static text with a drawn background is the cheapest reliable colour chip in
            //Irrlicht - no custom element, no skin surgery.
            irr::gui::IGUIStaticText* chip = guienv->addStaticText(L"",
                irr::core::rect<irr::s32>(chipX0, y, chipX1, y + rowH), false, false, tab);
            chip->setBackgroundColor(colour);
            chip->setDrawBackground(true);

            //KYARA: the tooltip lives on the LABEL, not on the slider. Irrlicht draws tooltips next
            //to the cursor, so a tooltip on the track was popping up right on top of the value
            //readout at the exact moment you were dragging and wanted to read it. Hovering the label
            //(on the left, where you are NOT dragging) still gives you the explanation; hovering the
            //track gives you an unobstructed view of the number. Same information, no collision.
            irr::gui::IGUIStaticText* lab = guienv->addStaticText(label,
                irr::core::rect<irr::s32>(labelX0, y, labelX1, y + rowH), false, true, tab);
            lab->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_CENTER);
            lab->setToolTipText(tip);

            irr::gui::IGUIScrollBar* bar = guienv->addScrollBar(true, //horizontal
                irr::core::rect<irr::s32>(trackX0, y, trackX1, y + rowH), tab, id);
            bar->setMin(minV);
            bar->setMax(maxV);
            bar->setSmallStep(smallStep);
            bar->setLargeStep(largeStep);

            irr::gui::IGUIStaticText* val = guienv->addStaticText(L"",
                irr::core::rect<irr::s32>(valueX0, y, valueX1, y + rowH), false, false, tab);
            val->setTextAlignment(irr::gui::EGUIA_LOWERRIGHT, irr::gui::EGUIA_CENTER);

            SliderRow r; r.bar = bar; r.value = val;
            return r;
        };

    //--- Weather tab (weather and visibility are /10 downstream) ---
    //KYARA METEO: the sea-state slider stops at SIM_MAX_WEATHER (SimulationModel.hpp) instead of 12
    {
        SliderRow r = addSliderRow(extraControlsTabWeather, 0,
            language->translate("weather").c_str(), GUI_ID_WEATHER_SCROLL_BAR,
            0, (irr::s32)(SIM_MAX_WEATHER * 10.0f + 0.5f), 5, 10, COL_SEASTATE, language->translate("weatherHelp").c_str());
        weatherScrollbar = r.bar; weatherValue = r.value;
    }
    {
        SliderRow r = addSliderRow(extraControlsTabWeather, 1,
            language->translate("rain").c_str(), GUI_ID_RAIN_SCROLL_BAR,
            0, 100, 5, 5, COL_RAIN, language->translate("rainHelp").c_str());
        rainScrollbar = r.bar; rainValue = r.value;
    }
    {
        SliderRow r = addSliderRow(extraControlsTabWeather, 2,
            language->translate("visibility").c_str(), GUI_ID_VISIBILITY_SCROLL_BAR,
            0, 100, 1, 5, COL_VIS, language->translate("visibilityHelp").c_str());
        visibilityScrollbar = r.bar; visibilityValue = r.value;
    }
    // KYARA: one-tap "mauvais temps" preset, row 3 of the weather tab (under the three sliders).
      // MUST use the tab-LOCAL coords (row0Y/rowPitch/rowH/labelX0/trackX1) the sliders use - screen
      // fractions (su/sh) push the button off the little window and it gets clipped => invisible.
    {
        const irr::s32 by = row0Y + 3 * rowPitch;
        guienv->addButton(
            irr::core::rect<irr::s32>(labelX0, by, trackX1, by + rowH),
            extraControlsTabWeather, GUI_ID_STORM_PRESET_BUTTON,
            language->translate("badWeather").c_str());
    }
    // KYARA: thunder on/off, row 4 of the weather tab.
    {
        const irr::s32 by = row0Y + 4 * rowPitch;
        guienv->addCheckBox(true,   // checked = thunder enabled (default)
            irr::core::rect<irr::s32>(labelX0, by, trackX1, by + rowH),
            extraControlsTabWeather, GUI_ID_THUNDER_CHECKBOX,
            language->translate("thunder").c_str());
    }
    // KYARA: lightning on/off, row 5 of the weather tab.
    {
        const irr::s32 by = row0Y + 5 * rowPitch;
        guienv->addCheckBox(true,
            irr::core::rect<irr::s32>(labelX0, by, trackX1, by + rowH),
            extraControlsTabWeather, GUI_ID_LIGHTNING_CHECKBOX,
            language->translate("lightning").c_str());
    }
    // KYARA HOULE: instructor motion scale, row 6 (0-150 %, 100 % = realistic). Scales the pitch and
    // roll the trainee sees (comfort vs realism); heave, speed loss and yaw stay physical.
    {
        SliderRow r = addSliderRow(extraControlsTabWeather, 6,
            L"Mouvement navire", GUI_ID_MOTION_SCALE_SCROLL_BAR,
            0, 150, 5, 25, COL_SEASTATE,
            L"Amplitude du tangage et du roulis affich\u00E9s. 100 % = r\u00E9aliste, moins = plus confortable.");
        motionScaleScrollbar = r.bar; motionScaleValue = r.value;
        motionScaleScrollbar->setPos(100);
    }
    // KYARA HOULE: read-only line describing the sea the weather has produced, row 7.
    {
        const irr::s32 by = row0Y + 7 * rowPitch;
        swellInfoText = guienv->addStaticText(L"",
            irr::core::rect<irr::s32>(labelX0, by, valueX1, by + rowH), false, false, extraControlsTabWeather);
        swellInfoText->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_CENTER);
        swellInfoText->setOverrideColor(COL_SEASTATE);
    }
    //--- Wind & Current tab ---
    irr::gui::IGUITab* windStreamTab =
        extraControlsTabControl->addTab(language->translate("windStreamTab").c_str());

    {
        SliderRow r = addSliderRow(windStreamTab, 0,
            language->translate("windDirection").c_str(), GUI_ID_WINDDIRECTION_SCROLL_BAR,
            0, 360, 5, 45, COL_WIND_DIR, language->translate("windDirectionHelp").c_str());
        windDirectionScrollbar = r.bar; windDirectionValue = r.value;
    }
    {
        SliderRow r = addSliderRow(windStreamTab, 1,
            language->translate("windSpeed").c_str(), GUI_ID_WINDSPEED_SCROLL_BAR,
            0, 50, 1, 5, COL_WIND_SPD, language->translate("windSpeedHelp").c_str());
        windSpeedScrollbar = r.bar; windSpeedValue = r.value;
    }
    {
        SliderRow r = addSliderRow(windStreamTab, 2,
            language->translate("streamDirection").c_str(), GUI_ID_STREAMDIRECTION_SCROLL_BAR,
            0, 360, 5, 45, COL_STRM_DIR, language->translate("streamDirectionHelp").c_str());
        streamDirectionScrollbar = r.bar; streamDirectionValue = r.value;
    }
    {
        SliderRow r = addSliderRow(windStreamTab, 3,
            language->translate("streamSpeed").c_str(), GUI_ID_STREAMSPEED_SCROLL_BAR,
            0, 10, 1, 5, COL_STRM_SPD, language->translate("streamSpeedHelp").c_str());
        streamSpeedScrollbar = r.bar; streamSpeedValue = r.value;
    }

    //Stream override moves here, where it belongs, on row 4.
    {
        const irr::s32 y = row0Y + 4 * rowPitch;
        irr::gui::IGUIStaticText* ovLab = guienv->addStaticText(
            language->translate("streamOverride").c_str(),
            irr::core::rect<irr::s32>(labelX0, y, labelX1, y + rowH), false, true, windStreamTab);
        ovLab->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_CENTER);
        streamOverride = guienv->addCheckBox(false,
            irr::core::rect<irr::s32>(trackX0, y, trackX0 + rowH, y + rowH),
            windStreamTab, GUI_ID_STREAMOVERRIDE_BOX);
        streamOverride->setToolTipText(language->translate("streamOverride").c_str());
    }


    /* original code Wind direction and speed
    windDirectionScrollbar = new irr::gui::ScrollDial(irr::core::vector2d<irr::s32>(0.26*su,0.06*sh),0.0175*su,guienv,extraControlsTabWeather,GUI_ID_WINDDIRECTION_SCROLL_BAR, 360, true);
    windDirectionScrollbar->setMax(360);
    windDirectionScrollbar->setMin(0);
    windDirectionScrollbar->setLargeStep(45);
    windDirectionScrollbar->setSmallStep(5);
    windDirectionScrollbar->setToolTipText(language->translate("windDirection").c_str());

    windSpeedScrollbar = new irr::gui::ScrollDial(irr::core::vector2d<irr::s32>(0.26*su,0.12*sh),0.0175*su,guienv,extraControlsTabWeather,GUI_ID_WINDSPEED_SCROLL_BAR,315, true);
    windSpeedScrollbar->setMax(50);
    windSpeedScrollbar->setMin(0);
    windSpeedScrollbar->setLargeStep(5);
    windSpeedScrollbar->setSmallStep(1);
    windSpeedScrollbar->setToolTipText(language->translate("windSpeed").c_str());

    // Tidal stream override
    streamDirectionScrollbar = new irr::gui::ScrollDial(irr::core::vector2d<irr::s32>(0.30*su,0.06*sh),0.0175*su,guienv,extraControlsTabWeather,GUI_ID_STREAMDIRECTION_SCROLL_BAR, 360, true);
    streamDirectionScrollbar->setMax(360);
    streamDirectionScrollbar->setMin(0);
    streamDirectionScrollbar->setLargeStep(45);
    streamDirectionScrollbar->setSmallStep(5);
    streamDirectionScrollbar->setToolTipText(language->translate("streamDirection").c_str());

    streamSpeedScrollbar = new irr::gui::ScrollDial(irr::core::vector2d<irr::s32>(0.30*su,0.12*sh),0.0175*su,guienv,extraControlsTabWeather,GUI_ID_STREAMSPEED_SCROLL_BAR, 315, true);
    streamSpeedScrollbar->setMax(10);
    streamSpeedScrollbar->setMin(0);
    streamSpeedScrollbar->setLargeStep(5);
    streamSpeedScrollbar->setSmallStep(1);
    streamSpeedScrollbar->setToolTipText(language->translate("streamSpeed").c_str());

    streamOverride = guienv->addCheckBox(false, irr::core::rect<irr::s32>(0.29*su,0.01*sh,0.31*su,0.03*sh),extraControlsTabWeather,GUI_ID_STREAMOVERRIDE_BOX);
    streamOverride->setToolTipText(language->translate("streamOverride").c_str());*/

    //Add buttons to control rudder failures etc.
    irr::gui::IGUITab* extraControlsTabRudder = extraControlsTabControl->addTab(language->translate("rudderFailure").c_str());

    //KYARA: these were su/sh based and would have burst out of the window on Eyefinity exactly like
    //the sliders did. Reuse the same row grid so all four tabs line up.
    {
        const irr::s32 colLx0 = (irr::s32)(tabW * 0.020f);
        const irr::s32 colLx1 = (irr::s32)(tabW * 0.470f);
        const irr::s32 colRx0 = (irr::s32)(tabW * 0.520f);
        const irr::s32 colRx1 = (irr::s32)(tabW * 0.980f);

        const irr::s32 rY0 = row0Y + 0 * rowPitch;
        const irr::s32 rY1 = row0Y + 1 * rowPitch;
        const irr::s32 rY2 = row0Y + 2 * rowPitch;
        const irr::s32 rY3 = row0Y + 3 * rowPitch;

        guienv->addButton(irr::core::rect<irr::s32>(colLx0, rY0, colLx1, rY0 + rowH), extraControlsTabRudder, GUI_ID_RUDDERPUMP_1_WORKING_BUTTON, language->translate("pump1Working").c_str());
        guienv->addButton(irr::core::rect<irr::s32>(colLx0, rY1, colLx1, rY1 + rowH), extraControlsTabRudder, GUI_ID_RUDDERPUMP_1_FAILED_BUTTON, language->translate("pump1Failed").c_str());
        guienv->addButton(irr::core::rect<irr::s32>(colLx0, rY2, colLx1, rY2 + rowH), extraControlsTabRudder, GUI_ID_RUDDERPUMP_2_WORKING_BUTTON, language->translate("pump2Working").c_str());
        guienv->addButton(irr::core::rect<irr::s32>(colLx0, rY3, colLx1, rY3 + rowH), extraControlsTabRudder, GUI_ID_RUDDERPUMP_2_FAILED_BUTTON, language->translate("pump2Failed").c_str());

        guienv->addButton(irr::core::rect<irr::s32>(colRx0, rY0, colRx1, rY0 + rowH), extraControlsTabRudder, GUI_ID_FOLLOWUP_WORKING_BUTTON, language->translate("followUpWorking").c_str());
        guienv->addButton(irr::core::rect<irr::s32>(colRx0, rY1, colRx1, rY1 + rowH), extraControlsTabRudder, GUI_ID_FOLLOWUP_FAILED_BUTTON, language->translate("followUpFailed").c_str());
    }


    //Add extra controls for view (zoom etc) kyara
    irr::gui::IGUITab* extraControlsTabView = extraControlsTabControl->addTab(language->translate("view").c_str());

    //KYARA: tab-relative, same grid as the other tabs (was su/sh).
    {
        const irr::s32 rY0 = row0Y + 0 * rowPitch;
        guienv->addStaticText(language->translate("3dView").c_str(),
            irr::core::rect<irr::s32>((irr::s32)(tabW * 0.055f), rY0, (irr::s32)(tabW * 0.380f), rY0 + rowH),
            false, true, extraControlsTabView)->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_CENTER);
        show3d = guienv->addCheckBox(true,
            irr::core::rect<irr::s32>((irr::s32)(tabW * 0.400f), rY0, (irr::s32)(tabW * 0.400f) + rowH, rY0 + rowH),
            extraControlsTabView);
    }

#if KYARA_COLREG_ENABLED
    //=== KYARA FEUX TAB: the COLREG situation of any vessel ======================================
    // Row 0      : vessel                              | "Feu masque (erreur volontaire)"
    // Rows 1..4  : 8 situations (2 cols)                | 7 lamps to hide + "Retablir" (2 cols)
    // Row 5      : deck lights
    // Row 6      : what the rules want her to show right now
    // Hidden from trainees with show_colreg_tab=0 in bc5.ini.
    if (showColregTab) {
        irr::gui::IGUITab* tabFeux = extraControlsTabControl->addTab(L"Feux");

        const irr::s32 cA0 = (irr::s32)(tabW * 0.020f), cA1 = (irr::s32)(tabW * 0.285f);
        const irr::s32 cB0 = (irr::s32)(tabW * 0.295f), cB1 = (irr::s32)(tabW * 0.560f);
        const irr::s32 cC0 = (irr::s32)(tabW * 0.590f), cC1 = (irr::s32)(tabW * 0.785f);
        const irr::s32 cD0 = (irr::s32)(tabW * 0.795f), cD1 = (irr::s32)(tabW * 0.985f);
        auto rowY = [&](int r) -> irr::s32 { return row0Y + r * rowPitch; };
        auto cell = [&](int r, irr::s32 x0, irr::s32 x1) {
            return irr::core::rect<irr::s32>(x0, rowY(r), x1, rowY(r) + rowH);
            };

        //Row 0: which vessel. Own ship first, then the scenario's other ships in file order - the
        //same numbering the instructor sees everywhere else.
        const irr::s32 comboX0 = cA0 + (irr::s32)(tabW * 0.090f);
        irr::gui::IGUIStaticText* vesselLab = guienv->addStaticText(L"Navire :",
            cell(0, cA0, comboX0), false, false, tabFeux);
        vesselLab->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_CENTER);

        lightsVesselBox = guienv->addComboBox(cell(0, comboX0, cB1), tabFeux, GUI_ID_LIGHTS_VESSEL_COMBO);
        lightsVesselBox->addItem(L"Navire propre");
        if (model) {
            for (irr::u32 i = 0; i < model->getNumberOfOtherShips(); i++) {
                irr::core::stringw item(i + 1);
                item += L" - ";
                item += irr::core::stringw(model->getOtherShipName((int)i).c_str());
                lightsVesselBox->addItem(item.c_str());
            }
        }
        lightsVesselBox->setSelected(0);

        irr::gui::IGUIStaticText* ovrHeader = guienv->addStaticText(
            L"Feu masqu\u00E9 (erreur volontaire)", cell(0, cC0, cD1), false, false, tabFeux);
        ovrHeader->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_CENTER);
        ovrHeader->setToolTipText(L"Cocher pour \u00E9teindre un feu que les r\u00E8gles exigent : "
            L"le stagiaire doit trouver ce qui ne va pas.");

        //Rows 1..4, left: one button per situation. Slot k -> row 1 + k/2, column k%2.
        for (int k = 0; k < ShipLights::SIT_COUNT; k++) {
            const int r = 1 + k / 2;
            const bool right = (k % 2) == 1;
            lightsSitButton[k] = guienv->addButton(
                cell(r, right ? cB0 : cA0, right ? cB1 : cA1), tabFeux, GUI_ID_LIGHTS_SIT_0 + k,
                ShipLights::getSituationShortFr((ShipLights::Situation)k),
                ShipLights::getSituationNameFr((ShipLights::Situation)k));
        }
        //Under the situations: working lights of the same vessel.
        lightsDeckBox = guienv->addCheckBox(false, cell(5, cA0, cB1), tabFeux,
            GUI_ID_LIGHTS_DECK_CHECKBOX, L"Feux de pont");
        lightsDeckBox->setToolTipText(L"Feux de travail / de pont de ce navire (pas des feux de navigation)");

        //Rows 1..4, right: the lamps that can be hidden on purpose, then "Retablir".
        for (int s = 0; s < ShipLights::OVERRIDE_SLOTS; s++) {
            const int r = 1 + s / 2;
            const bool right = (s % 2) == 1;
            lightsOverrideBox[s] = guienv->addCheckBox(false,
                cell(r, right ? cD0 : cC0, right ? cD1 : cC1), tabFeux, GUI_ID_LIGHTS_OVR_0 + s,
                ShipLights::overrideLabelFr(s));
            lightsOverrideBox[s]->setToolTipText(ShipLights::overrideTipFr(s));
        }
        guienv->addButton(cell(4, cD0, cD1), tabFeux, GUI_ID_LIGHTS_OVR_RESET,
            L"R\u00E9tablir", L"Rallumer tous les feux masqu\u00E9s de ce navire");

        //Row 6: the answer key, for the instructor - and the way into the placement editor.
        lightsStatusText = guienv->addStaticText(L"", cell(6, cA0, cC1), false, false, tabFeux);
        lightsStatusText->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_CENTER);
        guienv->addButton(cell(6, cD0, cD1), tabFeux, GUI_ID_LIGHTS_EDIT_BUTTON,
            L"Placer les feux", L"Placer les feux de ce navire \u00E0 la main et les enregistrer dans son boat.ini");
    }
#endif //KYARA_COLREG_ENABLED - the "Eclairage" tab below is NOT part of COLREG and stays

    //=== KYARA FEUX TAB: lighting inside the own ship ============================================
    // Same column grid as the Weather tab (labelX0..valueX1), so the two read alike.
    {
        irr::gui::IGUITab* tabBord = extraControlsTabControl->addTab(L"\u00C9clairage");
        auto rowY = [&](int r) -> irr::s32 { return row0Y + r * rowPitch; };

        //Row 0: bridge instruments - off / dim / bright
        irr::gui::IGUIStaticText* instrLab = guienv->addStaticText(L"\u00C9crans et cadrans",
            irr::core::rect<irr::s32>(labelX0, rowY(0), labelX1, rowY(0) + rowH), false, false, tabBord);
        instrLab->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_CENTER);
        instrLab->setToolTipText(L"Auto-\u00E9clairage des \u00E9crans et instruments de passerelle");

        const wchar_t* instrNames[3] = { L"\u00C9teints", L"Tamis\u00E9s", L"Pleins feux" };
        const wchar_t* instrTips[3] = {
            L"Instruments \u00E9teints",
            L"R\u00E9glage de nuit : lisible sans \u00E9blouir la veille",
            L"R\u00E9glage de jour" };
        const irr::s32 bw = (valueX1 - trackX0) / 3;
        for (int i = 0; i < 3; i++) {
            const irr::s32 x0 = trackX0 + i * bw;
            const irr::s32 x1 = (i == 2) ? valueX1 : x0 + bw - (irr::s32)(tabW * 0.008f);
            instrLightsButton[i] = guienv->addButton(
                irr::core::rect<irr::s32>(x0, rowY(0), x1, rowY(0) + rowH),
                tabBord, GUI_ID_INSTR_LIGHTS_0 + i, instrNames[i], instrTips[i]);
        }

        //Row 1: working lights of the own ship (same flag as "Feux de pont" in the Feux tab
        //when the own ship is selected - both boxes follow the model).
        irr::gui::IGUIStaticText* deckLab = guienv->addStaticText(L"Feux de pont / travail",
            irr::core::rect<irr::s32>(labelX0, rowY(1), labelX1, rowY(1) + rowH), false, false, tabBord);
        deckLab->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_CENTER);
        ownDeckLightsBox = guienv->addCheckBox(false,
            irr::core::rect<irr::s32>(trackX0, rowY(1), trackX0 + rowH, rowY(1) + rowH),
            tabBord, GUI_ID_OWN_DECK_LIGHTS_CHECKBOX);
        ownDeckLightsBox->setToolTipText(L"Projecteurs de pont avant, arri\u00E8re et de coup\u00E9e");

        //Rows 2..4: what is actually happening, and why nothing may seem to change.
        interiorStatusText = guienv->addStaticText(L"",
            irr::core::rect<irr::s32>(labelX0, rowY(2), valueX1, rowY(4) + rowH), false, true, tabBord);
        interiorStatusText->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_UPPERLEFT);

        //Row 5: which screens and gauges glow (hidden from trainees with show_instrument_tool=0)
        if (showInstrumentTool) {
            guienv->addButton(irr::core::rect<irr::s32>(labelX0, rowY(5), valueX1, rowY(5) + rowH),
                tabBord, GUI_ID_INSTR_EDIT_BUTTON, L"Choisir les \u00E9crans \u00E9clair\u00E9s",
                L"Cliquer sur les \u00E9crans et cadrans de la passerelle pour choisir ceux qui s'\u00E9clairent la nuit, "
                L"puis les enregistrer dans le boat.ini du navire");
        }
    }

    //=== Taille tab: size and waterline of any vessel, saved to her boat.ini =====================
    // Hidden from trainees with show_size_tool=0 in bc5.ini.
    if (showSizeTool) {
        irr::gui::IGUITab* tabTaille = extraControlsTabControl->addTab(L"Taille");
        auto rowY = [&](int r) -> irr::s32 { return row0Y + r * rowPitch; };
        const irr::s32 x0 = (irr::s32)(tabW * 0.020f);
        const irr::s32 comboX0 = x0 + (irr::s32)(tabW * 0.090f);
        const irr::s32 x1 = (irr::s32)(tabW * 0.560f);
        const irr::s32 xEnd = (irr::s32)(tabW * 0.985f);

        //Row 0: which vessel - own ship first, then the scenario's other ships in file order
        irr::gui::IGUIStaticText* vesselLab = guienv->addStaticText(L"Navire :",
            irr::core::rect<irr::s32>(x0, rowY(0), comboX0, rowY(0) + rowH), false, false, tabTaille);
        vesselLab->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_CENTER);
        sizeVesselBox = guienv->addComboBox(
            irr::core::rect<irr::s32>(comboX0, rowY(0), x1, rowY(0) + rowH), tabTaille);
        sizeVesselBox->addItem(L"Navire propre");
        if (model) {
            for (irr::u32 i = 0; i < model->getNumberOfOtherShips(); i++) {
                irr::core::stringw item(i + 1);
                item += L" - ";
                item += irr::core::stringw(model->getOtherShipName((int)i).c_str());
                sizeVesselBox->addItem(item.c_str());
            }
        }
        sizeVesselBox->setSelected(0);

        //Row 1: into the tool
        guienv->addButton(irr::core::rect<irr::s32>(x0, rowY(1), x1, rowY(1) + rowH), tabTaille,
            GUI_ID_SIZE_EDIT_BUTTON, L"Taille et flottaison",
            L"R\u00E9gler la longueur et la ligne de flottaison de ce navire en direct, puis les enregistrer dans son boat.ini");

        //Rows 2..5: what it does
        irr::gui::IGUIStaticText* help = guienv->addStaticText(
            L"Choisir un navire, puis \u00AB Taille et flottaison \u00BB : sa longueur et sa ligne de flottaison "
            L"changent en direct, et \u00AB Enregistrer \u00BB les \u00E9crit dans son boat.ini.\n"
            L"Les navires du sc\u00E9nario qui utilisent le m\u00EAme boat.ini changent ensemble.\n"
            L"Navire propre : vues, radar et commandes suivent tout de suite ; sa man\u0153uvrabilit\u00E9 "
            L"(masse, inertie) suit au prochain lancement du sc\u00E9nario.",
            irr::core::rect<irr::s32>(x0, rowY(2), xEnd, rowY(5) + rowH), false, true, tabTaille);
        help->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_UPPERLEFT);
    }

    refreshLightsTab();



    // Positioned to the right -kyara 
    //KYARA: in the classic layout this bar sits at 0.92 sh, where the row of buttons overlaps it -
    //only its right-hand end was ever visible. With the console, the helm slider along the bottom
    //is gone, so the zoom bar moves into that free row and is fully visible and usable.
    const irr::core::rect<irr::s32> magnificationPos = instrumentsEnabled
        ? irr::core::rect<irr::s32>(0.13 * su + azimuthGUIOffsetL, 0.96 * sh, 0.45 * su + azimuthGUIOffsetL, 0.99 * sh)
        : irr::core::rect<irr::s32>(0.24 * su + azimuthGUIOffsetL, 0.92 * sh, 0.45 * su + azimuthGUIOffsetL, 0.95 * sh);
    if (commandBar) {
        //A plain slider in the command bar (placed by layoutCommandBar)
        magnificationScrollbar = guienv->addScrollBar(true, magnificationPos, 0, GUI_ID_MAGNIFICATION_SCROLL_BAR);
    }
    else {
        magnificationScrollbar = new irr::gui::OutlineScrollBar(true, guienv, guienv->getRootGUIElement(), GUI_ID_MAGNIFICATION_SCROLL_BAR, magnificationPos);
    }
    magnificationScrollbar->setToolTipText(language->translate("magnification").c_str());

    magnificationScrollbar->setMax(200); //Divide by 10 to get magnification
    magnificationScrollbar->setMin(10);
    magnificationScrollbar->setSmallStep(5);
    magnificationScrollbar->setPos(1.0 * 10); // Initialise as 1x zoom
    //KYARA LIGHTING TIME: type an hour (HH:MM) and press Enter. Lighting only - the scenario
    //clock, traffic and tide are untouched.
    lightingTimeBox = guienv->addEditBox(L"12:00",
        irr::core::rect<irr::s32>(0.46 * su + azimuthGUIOffsetL, 0.92 * sh,
            0.52 * su + azimuthGUIOffsetL, 0.95 * sh),
        true, 0, GUI_ID_LIGHTING_TIME_BOX);
    lightingTimeBox->setToolTipText(L"Heure d'\u00E9clairage (HH:MM)");
    //Add an additional window for lines (will normally be hidden)
 //=============================================================================================
 //KYARA: AMARRES WINDOW - SIZED FROM THE TRANSLATED TEXT, NOT FROM su/sh.
 //The old version inherited the data-display rectangle and then gave each control a fixed
 //FRACTION of it - addLine got 29% of the width. That is wide enough for "Add line" and far too
 //narrow for "Ajouter ligne d'amarrage". Irrlicht buttons do NOT wrap, they clip, so no amount of
 //alignment tweaking fixes it. The only correct fix is to measure the strings we are actually
 //going to draw and build the window around them.
 //Same rule as the extra-controls window: POSITION with su/sh, SIZE with the font.
    irr::gui::IGUIFont* lwFont = guienv->getSkin() ? guienv->getSkin()->getFont() : 0;
    const irr::s32 lwCh = lwFont ? (irr::s32)lwFont->getDimension(L"Xg").Height : 16;

    //Strings fetched once so we can measure exactly what will be rendered.
    const irr::core::stringw lwAddTxt = language->translate("addLine").c_str();
    const irr::core::stringw lwRemoveTxt = language->translate("removeLine").c_str();
    const irr::core::stringw lwSlackTxt = language->translate("keepLineSlack").c_str();
    const irr::core::stringw lwHaulTxt = language->translate("haulLineIn").c_str();
    const irr::core::stringw lwAnchorTxt = language->translate("anchorLine").c_str();

    auto lwTextW = [lwFont, lwCh](const irr::core::stringw& s) -> irr::s32 {
        return lwFont ? (irr::s32)lwFont->getDimension(s.c_str()).Width
            : (irr::s32)(s.size() * (lwCh / 2));
        };
    auto lwGrow = [](irr::s32& target, irr::s32 candidate) { if (candidate > target) target = candidate; };

    const irr::s32 lwPad = (irr::s32)(lwCh * 0.55f);  //outer margin
    const irr::s32 lwGap = (irr::s32)(lwCh * 0.40f);  //gap between controls
    const irr::s32 lwRowH = (irr::s32)(lwCh * 1.70f);  //one control row
    const irr::s32 lwBoxW = (irr::s32)(lwCh * 1.10f);  //checkbox square
    const irr::s32 lwBtnPad = (irr::s32)(lwCh * 1.40f);  //breathing room inside a button

    //--- Right-hand column: as wide as the widest thing that has to fit in it -------------------
    irr::s32 lwRightW = 0;
    lwGrow(lwRightW, lwBoxW + lwGap + lwTextW(lwSlackTxt));
    lwGrow(lwRightW, lwBoxW + lwGap + lwTextW(lwHaulTxt));
    lwGrow(lwRightW, lwBoxW + lwGap + lwTextW(lwAnchorTxt));
    lwGrow(lwRightW, lwTextW(lwAddTxt) + lwTextW(lwRemoveTxt) + 2 * lwBtnPad + lwGap);
    lwGrow(lwRightW, lwTextW(L"Longueur 000 m  Tension 000 t")); //min useful width for linesText

    //--- Left-hand column: the list of lines ----------------------------------------------------
    irr::s32 lwListW = 0;
    lwGrow(lwListW, lwTextW(L"Amarrage 8 - Navire") + lwCh); //+1 char for the scrollbar
    lwGrow(lwListW, (irr::s32)(lwCh * 8));

    //--- Vertical layout: status block, three checkbox rows, one button row ---------------------
    irr::s32 lwVO = guienv->getSkin() ? guienv->getSkin()->getSize(irr::gui::EGDS_WINDOW_BUTTON_WIDTH) + 5 : 20;
    const irr::s32 lwStatusH = (irr::s32)(lwCh * 1.35f * 3); //3 lines of status text
    const irr::s32 lwClientH = lwPad + lwStatusH + lwGap
        + 3 * (lwRowH + lwGap)
        + lwRowH + lwPad;

    irr::core::rect<irr::s32> linesWindowPos = stdDataDisplayPos;
    linesWindowPos.LowerRightCorner.X = linesWindowPos.UpperLeftCorner.X + lwPad + lwListW + lwPad + lwRightW + lwPad;
    linesWindowPos.LowerRightCorner.Y = linesWindowPos.UpperLeftCorner.Y + lwVO + lwClientH;

    //Keep it on screen if the translated strings turn out to be very long.
    if (linesWindowPos.LowerRightCorner.X > (irr::s32)su) {
        irr::s32 overshoot = linesWindowPos.LowerRightCorner.X - (irr::s32)su;
        linesWindowPos.UpperLeftCorner.X -= overshoot;
        linesWindowPos.LowerRightCorner.X -= overshoot;
        if (linesWindowPos.UpperLeftCorner.X < 0) linesWindowPos.UpperLeftCorner.X = 0;
    }

    linesControlsWindow = guienv->addWindow(linesWindowPos);
    linesControlsWindow->getCloseButton()->setVisible(false);
    linesControlsWindow->setText(language->translate("lines").c_str());
    guienv->addButton(linesControlsWindow->getCloseButton()->getRelativePosition(), linesControlsWindow, GUI_ID_HIDE_LINES_CONTROLS_BUTTON, L"X");
    linesControlsWindow->setVisible(false);

    //--- Children. All coordinates are relative to the WINDOW, in pixels, not fractions. ---------
    const irr::s32 lwListX0 = lwPad;
    const irr::s32 lwListX1 = lwListX0 + lwListW;
    const irr::s32 lwRightX0 = lwListX1 + lwPad;
    const irr::s32 lwRightX1 = lwRightX0 + lwRightW;
    irr::s32 lwY = lwVO + lwPad;

    linesList = guienv->addListBox(
        irr::core::rect<irr::s32>(lwListX0, lwY, lwListX1, lwVO + lwClientH - lwPad),
        linesControlsWindow, GUI_ID_LINES_LIST);

    linesText = guienv->addStaticText(L"",
        irr::core::rect<irr::s32>(lwRightX0, lwY, lwRightX1, lwY + lwStatusH),
        true, true, linesControlsWindow);
    lwY += lwStatusH + lwGap;

    //Checkbox first, label immediately to its right - one row each, so nothing needs to wrap.
    auto lwAddCheckRow = [&](irr::s32 id, const irr::core::stringw& label,
        irr::gui::IGUIStaticText** labelOut) -> irr::gui::IGUICheckBox*
        {
            irr::gui::IGUICheckBox* cb = guienv->addCheckBox(false,
                irr::core::rect<irr::s32>(lwRightX0, lwY, lwRightX0 + lwBoxW, lwY + lwRowH),
                linesControlsWindow, id);
            irr::gui::IGUIStaticText* txt = guienv->addStaticText(label.c_str(),
                irr::core::rect<irr::s32>(lwRightX0 + lwBoxW + lwGap, lwY, lwRightX1, lwY + lwRowH),
                false, false, linesControlsWindow);
            txt->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_CENTER);
            if (labelOut) *labelOut = txt;
            lwY += lwRowH + lwGap;
            return cb;
        };

    keepLineSlack = lwAddCheckRow(GUI_ID_KEEP_SLACK_LINE_CHECKBOX, lwSlackTxt, 0);
    heaveLineIn = lwAddCheckRow(GUI_ID_HAUL_IN_LINE_CHECKBOX, lwHaulTxt, 0);
    anchorLine = lwAddCheckRow(GUI_ID_ANCHOR_LINE_CHECKBOX, lwAnchorTxt, 0);

    //Buttons last, side by side, each sized to its own text.
    {
        const irr::s32 addW = lwTextW(lwAddTxt) + lwBtnPad;
        addLine = guienv->addButton(
            irr::core::rect<irr::s32>(lwRightX0, lwY, lwRightX0 + addW, lwY + lwRowH),
            linesControlsWindow, GUI_ID_ADD_LINE_BUTTON, lwAddTxt.c_str());
        removeLine = guienv->addButton(
            irr::core::rect<irr::s32>(lwRightX0 + addW + lwGap, lwY, lwRightX1, lwY + lwRowH),
            linesControlsWindow, GUI_ID_REMOVE_LINE_BUTTON, lwRemoveTxt.c_str());
        //Full text as a tooltip, so the meaning survives even if the skin font changes.
        addLine->setToolTipText(lwAddTxt.c_str());
        removeLine->setToolTipText(lwRemoveTxt.c_str());
    }

    //add radar buttons
    //add tab control for radar
    radarTabControl = guienv->addTabControl(irr::core::rect<irr::s32>(0.455 * su + azimuthGUIOffsetR, 0.695 * sh, 0.697 * su + azimuthGUIOffsetR, 0.990 * sh), 0, true);
    radarTabControl->setTabHeight(0.03 * sh);
    irr::gui::IGUITab* mainRadarTab = radarTabControl->addTab(language->translate("radarMainTab").c_str(), 0);
    //irr::gui::IGUITab* radarEBLTab = radarTabControl->addTab(language->translate("radarEBLVRMTab").c_str(),0);
    irr::gui::IGUITab* radarPITab = radarTabControl->addTab(language->translate("radarPITab").c_str(), 0);
    //irr::gui::IGUITab* radarGZoneTab = radarTabControl->addTab(language->translate("radarGuardZoneTab").c_str(),0);
    irr::gui::IGUITab* radarARPATab = radarTabControl->addTab(language->translate("radarARPATab").c_str(), 0);
    //irr::gui::IGUITab* radarTrackTab = radarTabControl->addTab(language->translate("radarTrackTab").c_str(),0);
    //irr::gui::IGUITab* radarARPAVectorTab = radarTabControl->addTab(language->translate("radarARPAVectorTab").c_str(),0);
    //irr::gui::IGUITab* radarARPAAlarmTab = radarTabControl->addTab(language->translate("radarARPAAlarmTab").c_str(),0);
    //irr::gui::IGUITab* radarARPATrialTab = radarTabControl->addTab(language->translate("radarARPATrialTab").c_str(),0);

    radarText = guienv->addStaticText(L"", irr::core::rect<irr::s32>(0.460 * su + azimuthGUIOffsetR, 0.610 * sh, 0.690 * su + azimuthGUIOffsetR, 0.690 * sh), true, true, 0, -1, true);
    //CHANGE COLOR OF DATA MAIN IN TOP NEXT TO RADAR    -Kyara   
    radarText->setOverrideColor(irr::video::SColor(255, 150, 200, 255)); //kyara: panneau radar en bleu clair
    //Buttons for radar on/off
    radarOnOffButton = guienv->addButton(irr::core::rect<irr::s32>(0.005 * su, 0.010 * sh, 0.055 * su, 0.040 * sh), mainRadarTab, GUI_ID_RADAR_ONOFF_BUTTON, language->translate("onoff").c_str());
    //TODO: Complete this: To go where radar zoom + is, and squash these down a bit

    //Buttons for full or small radar
    bigRadarButton = guienv->addButton(irr::core::rect<irr::s32>(0.700 * su + azimuthGUIOffsetR, 0.610 * sh, 0.720 * su + azimuthGUIOffsetR, 0.640 * sh), 0, GUI_ID_BIG_RADAR_BUTTON, language->translate("bigRadar").c_str());
    irr::s32 smallRadarButtonLeft = radarTL.X + 0.01 * su;
    irr::s32 smallRadarButtonTop = radarTL.Y + 0.01 * sh;
    smallRadarButton = guienv->addButton(irr::core::rect<irr::s32>(smallRadarButtonLeft, smallRadarButtonTop, smallRadarButtonLeft + 0.020 * su, smallRadarButtonTop + 0.030 * sh), 0, GUI_ID_SMALL_RADAR_BUTTON, language->translate("smallRadar").c_str());
    bigRadarButton->setToolTipText(language->translate("fullScreenRadar").c_str());
    if (instrumentPanel) { //kyara: the only radar control left on the main view
        bigRadarButton->setRelativePosition(statusRow(0));
        bigRadarButton->setText(L"RADAR");
    }
    smallRadarButton->setToolTipText(language->translate("minimiseRadar").c_str());

    // Radar cursor buttons
    radarCursorLeftButton = guienv->addButton(irr::core::rect<irr::s32>(0.700 * su + azimuthGUIOffsetR, 0.950 * sh, 0.715 * su + azimuthGUIOffsetR, 0.970 * sh), 0, GUI_ID_RADAR_DECREASE_X_BUTTON, L"<");
    radarCursorRightButton = guienv->addButton(irr::core::rect<irr::s32>(0.730 * su + azimuthGUIOffsetR, 0.950 * sh, 0.745 * su + azimuthGUIOffsetR, 0.970 * sh), 0, GUI_ID_RADAR_INCREASE_X_BUTTON, L">");
    radarCursorUpButton = guienv->addButton(irr::core::rect<irr::s32>(0.715 * su + azimuthGUIOffsetR, 0.930 * sh, 0.730 * su + azimuthGUIOffsetR, 0.950 * sh), 0, GUI_ID_RADAR_INCREASE_Y_BUTTON, L"^");
    radarCursorDownButton = guienv->addButton(irr::core::rect<irr::s32>(0.715 * su + azimuthGUIOffsetR, 0.970 * sh, 0.730 * su + azimuthGUIOffsetR, 0.990 * sh), 0, GUI_ID_RADAR_DECREASE_Y_BUTTON, L"v");

    guienv->addButton(irr::core::rect<irr::s32>(0.005 * su, 0.045 * sh, 0.055 * su, 0.085 * sh), mainRadarTab, GUI_ID_RADAR_INCREASE_BUTTON, language->translate("increaserange").c_str());
    guienv->addButton(irr::core::rect<irr::s32>(0.005 * su, 0.085 * sh, 0.055 * su, 0.125 * sh), mainRadarTab, GUI_ID_RADAR_DECREASE_BUTTON, language->translate("decreaserange").c_str());
    northUpButton = guienv->addButton(irr::core::rect<irr::s32>(0.005 * su, 0.130 * sh, 0.055 * su, 0.160 * sh), mainRadarTab, GUI_ID_RADAR_NORTH_BUTTON, language->translate("northUp").c_str());
    courseUpButton = guienv->addButton(irr::core::rect<irr::s32>(0.005 * su, 0.160 * sh, 0.055 * su, 0.190 * sh), mainRadarTab, GUI_ID_RADAR_COURSE_BUTTON, language->translate("courseUp").c_str());
    headUpButton = guienv->addButton(irr::core::rect<irr::s32>(0.005 * su, 0.190 * sh, 0.055 * su, 0.220 * sh), mainRadarTab, GUI_ID_RADAR_HEAD_BUTTON, language->translate("headUp").c_str());
    //COLORS FOR RADAR 
        //Controls for small radar window
    //=============================================================================================
    //KYARA: RADAR DIAL SIZING
    //Everything else in this tab was already safe, because each x fraction is measured against su
    //and the tab's WIDTH is also an su fraction - they stretch together and cancel out. The dial
    //RADIUS did not: it was 0.02 * su, i.e. a LENGTH scaled by the canvas WIDTH. On Eyefinity su is
    //the width of all THREE screens, so the radius tripled - while the vertical room it has to fit
    //into is an sh fraction, and sh did not change. Hence the circles bursting out of the top.
    //The radius is now bounded by BOTH axes of the tab, and the centre is pushed down far enough to
    //clear the top edge whatever the radius works out to be. On a single 1920x1080 screen this
    //reproduces the old size almost exactly, so nothing changes on the test machine.
    const irr::core::rect<irr::s32> rtRect = radarTabControl->getRelativePosition();
    const irr::s32 rtW = rtRect.getWidth();
    const irr::s32 rtH = rtRect.getHeight() - (irr::s32)(0.03 * sh); //minus the tab header row
    const irr::s32 dialR = irr::core::min_((irr::s32)(rtW * 0.085f), (irr::s32)(rtH * 0.220f));
    const irr::s32 dialCY = dialR + (irr::s32)(rtH * 0.030f); //centre always at least one radius down
    //=============================================================================================

    radarGainScrollbar = new irr::gui::ScrollDial(irr::core::vector2d<irr::s32>(0.0850 * su, dialCY), dialR, guienv, mainRadarTab, GUI_ID_RADAR_GAIN_SCROLL_BAR);
    radarClutterScrollbar = new irr::gui::ScrollDial(irr::core::vector2d<irr::s32>(0.1425 * su, dialCY), dialR, guienv, mainRadarTab, GUI_ID_RADAR_CLUTTER_SCROLL_BAR);
    radarRainScrollbar = new irr::gui::ScrollDial(irr::core::vector2d<irr::s32>(0.2000 * su, dialCY), dialR, guienv, mainRadarTab, GUI_ID_RADAR_RAIN_SCROLL_BAR);
    {
        //KYARA: labels anchored to the DIAL now, not to a fixed sh fraction - so they stay put
        //inside the face whatever radius the dial resolves to.
        irr::gui::IGUIStaticText* t = guienv->addStaticText(language->translate("gain").c_str(),
            irr::core::rect<irr::s32>((irr::s32)(0.0850 * su) - dialR, dialCY + (irr::s32)(dialR * 0.15f),
                (irr::s32)(0.0850 * su) + dialR, dialCY + (irr::s32)(dialR * 0.85f)),
            false, true, mainRadarTab);
        t->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
        t->setOverrideColor(irr::video::SColor(255, 255, 220, 0)); // amber, reads on dark
    }
    {
        irr::gui::IGUIStaticText* t = guienv->addStaticText(language->translate("clutter").c_str(),
            irr::core::rect<irr::s32>((irr::s32)(0.1425 * su) - dialR, dialCY + (irr::s32)(dialR * 0.15f),
                (irr::s32)(0.1425 * su) + dialR, dialCY + (irr::s32)(dialR * 0.85f)),
            false, true, mainRadarTab);
        t->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
        t->setOverrideColor(irr::video::SColor(255, 255, 220, 0));
    }
    {
        irr::gui::IGUIStaticText* t = guienv->addStaticText(language->translate("rain").c_str(),
            irr::core::rect<irr::s32>((irr::s32)(0.2000 * su) - dialR, dialCY + (irr::s32)(dialR * 0.15f),
                (irr::s32)(0.2000 * su) + dialR, dialCY + (irr::s32)(dialR * 0.85f)),
            false, true, mainRadarTab);
        t->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
        t->setOverrideColor(irr::video::SColor(255, 255, 220, 0));
    }
    radarGainScrollbar->setSmallStep(2);
    radarClutterScrollbar->setSmallStep(2);
    radarRainScrollbar->setSmallStep(2);
    //CHANGES FROM HERE    - kyara    
    {
        auto dial = static_cast<irr::gui::ScrollDial*>(radarGainScrollbar);
        dial->setOverrideColor(irr::video::SColor(255, 255, 255, 0));
        dial->setLineThickness(3);
    }
    {
        auto dial = static_cast<irr::gui::ScrollDial*>(radarClutterScrollbar);
        dial->setOverrideColor(irr::video::SColor(255, 255, 255, 0));
        dial->setLineThickness(3);
    }

    {
        auto dial = static_cast<irr::gui::ScrollDial*>(radarRainScrollbar);
        dial->setOverrideColor(irr::video::SColor(255, 255, 255, 0));
        dial->setLineThickness(3);
    }
    //ENDS HERE 

    eblLeftButton = guienv->addButton(irr::core::rect<irr::s32>(0.060 * su, 0.160 * sh, 0.115 * su, 0.190 * sh), mainRadarTab, GUI_ID_RADAR_EBL_LEFT_BUTTON, language->translate("eblLeft").c_str());
    eblRightButton = guienv->addButton(irr::core::rect<irr::s32>(0.170 * su, 0.160 * sh, 0.225 * su, 0.190 * sh), mainRadarTab, GUI_ID_RADAR_EBL_RIGHT_BUTTON, language->translate("eblRight").c_str());
    eblUpButton = guienv->addButton(irr::core::rect<irr::s32>(0.115 * su, 0.130 * sh, 0.170 * su, 0.160 * sh), mainRadarTab, GUI_ID_RADAR_EBL_UP_BUTTON, language->translate("eblUp").c_str());
    eblDownButton = guienv->addButton(irr::core::rect<irr::s32>(0.115 * su, 0.190 * sh, 0.170 * su, 0.220 * sh), mainRadarTab, GUI_ID_RADAR_EBL_DOWN_BUTTON, language->translate("eblDown").c_str());


    //kyara: EBL/VRM selector toggles, in the free corners of the EBL cross
    eblSelectButton = guienv->addButton(irr::core::rect<irr::s32>(0.060 * su, 0.130 * sh, 0.115 * su, 0.160 * sh), mainRadarTab, GUI_ID_RADAR_EBL_SELECT_BUTTON, L"EBL 1/2");
    vrmSelectButton = guienv->addButton(irr::core::rect<irr::s32>(0.170 * su, 0.130 * sh, 0.225 * su, 0.160 * sh), mainRadarTab, GUI_ID_RADAR_VRM_SELECT_BUTTON, L"VRM 1/2");
    radarColourButton = guienv->addButton(irr::core::rect<irr::s32>(0.115 * su, 0.160 * sh, 0.170 * su, 0.190 * sh), mainRadarTab, GUI_ID_RADAR_COLOUR_BUTTON, language->translate("radarColour").c_str());
    //kyara: EBL/VRM colour cycle buttons, below the selector buttons
    eblColourButton = guienv->addButton(irr::core::rect<irr::s32>(0.060 * su, 0.190 * sh, 0.115 * su, 0.220 * sh), mainRadarTab, GUI_ID_RADAR_EBL_COLOUR_BUTTON, L"Coul. EBL");
    vrmColourButton = guienv->addButton(irr::core::rect<irr::s32>(0.170 * su, 0.190 * sh, 0.225 * su, 0.220 * sh), mainRadarTab, GUI_ID_RADAR_VRM_COLOUR_BUTTON, L"Coul. VRM");
    //Controls for large radar window
    largeRadarControls = new irr::gui::IGUIRectangle(guienv, guienv->getRootGUIElement(), irr::core::rect<irr::s32>(radarTL.X + 0.770 * radarSu, radarTL.Y + 0.020 * radarSu, radarTL.X + 0.980 * radarSu, radarTL.Y + 0.730 * radarSu));
    largeRadarPIControls = new irr::gui::IGUIRectangle(guienv, guienv->getRootGUIElement(), irr::core::rect<irr::s32>(radarTL.X + 0.550 * radarSu, radarTL.Y + 0.020 * radarSu, radarTL.X + 0.770 * radarSu, radarTL.Y + 0.200 * radarSu), false);
    // SCROLL DIAL SIZE CHANGE
    radarGainScrollbar2 = new irr::gui::ScrollDial(irr::core::vector2d<irr::s32>(0.040 * radarSu, 0.040 * radarSu), 0.030 * radarSu, guienv, largeRadarControls, GUI_ID_RADAR_GAIN_SCROLL_BAR);
    radarClutterScrollbar2 = new irr::gui::ScrollDial(irr::core::vector2d<irr::s32>(0.105 * radarSu, 0.040 * radarSu), 0.030 * radarSu, guienv, largeRadarControls, GUI_ID_RADAR_CLUTTER_SCROLL_BAR);
    radarRainScrollbar2 = new irr::gui::ScrollDial(irr::core::vector2d<irr::s32>(0.170 * radarSu, 0.040 * radarSu), 0.030 * radarSu, guienv, largeRadarControls, GUI_ID_RADAR_RAIN_SCROLL_BAR);
    //Console rotary knobs (grab and turn, or mouse wheel), amber like the radar picture.
    static_cast<irr::gui::ScrollDial*>(radarGainScrollbar2)->setKnobStyle(true);
    static_cast<irr::gui::ScrollDial*>(radarClutterScrollbar2)->setKnobStyle(true);
    static_cast<irr::gui::ScrollDial*>(radarRainScrollbar2)->setKnobStyle(true);
    radarGainScrollbar2->setToolTipText(L"Gain : saisir et tourner, ou molette");
    radarClutterScrollbar2->setToolTipText(L"Anti-clutter mer : saisir et tourner, ou molette");
    radarRainScrollbar2->setToolTipText(L"Anti-clutter pluie : saisir et tourner, ou molette");

    static_cast<irr::gui::ScrollDial*>(radarGainScrollbar2)->setShowScale(true);
    static_cast<irr::gui::ScrollDial*>(radarClutterScrollbar2)->setShowScale(true);
    static_cast<irr::gui::ScrollDial*>(radarRainScrollbar2)->setShowScale(true);
    //----------------------------------END
    radarGainScrollbar2->setSmallStep(2);
    radarClutterScrollbar2->setSmallStep(2);
    radarRainScrollbar2->setSmallStep(2);
    //CHANGE COLOR OF RADAR COLOR
        //THE TEXT
    irr::gui::IGUIStaticText* gainLabel = guienv->addStaticText(
        L"GAIN",
        irr::core::rect<irr::s32>(
            0.010f * radarSu, 0.071f * radarSu,
            0.070f * radarSu, 0.093f * radarSu
        ),
        false,   // border
        true,    // background
        largeRadarControls
    );

    gainLabel->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
    gainLabel->setOverrideColor(irr::video::SColor(255, 214, 218, 224));


    //CLUTTER LABEL COLOR
    irr::gui::IGUIStaticText* clutterLabel = guienv->addStaticText(
        L"MER",
        irr::core::rect<irr::s32>(
            0.075 * radarSu, 0.071 * radarSu,
            0.135 * radarSu, 0.093 * radarSu),
        false, true,
        largeRadarControls);
    clutterLabel->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
    clutterLabel->setOverrideColor(irr::video::SColor(255, 214, 218, 224));

    //RAIN LABEL COLOR
    irr::gui::IGUIStaticText* rainLabel = guienv->addStaticText(L"PLUIE",
        irr::core::rect<irr::s32>(
            0.140 * radarSu, 0.071 * radarSu,
            0.200 * radarSu, 0.093 * radarSu),
        false, true,
        largeRadarControls);
    rainLabel->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
    rainLabel->setOverrideColor(irr::video::SColor(255, 214, 218, 224));



    // THE SCROLLS

    {
        auto dial = static_cast<irr::gui::ScrollDial*>(radarGainScrollbar2);
        dial->setOverrideColor(irr::video::SColor(255, 255, 255, 0));
        dial->setLineThickness(3);
    }
    {
        auto dial = static_cast<irr::gui::ScrollDial*>(radarClutterScrollbar2);
        dial->setOverrideColor(irr::video::SColor(255, 255, 255, 0));
        dial->setLineThickness(3);
    }

    {
        auto dial = static_cast<irr::gui::ScrollDial*>(radarRainScrollbar2);
        dial->setOverrideColor(irr::video::SColor(255, 255, 255, 0));
        dial->setLineThickness(3);
    }


    // ============ kyara: bandeau grand radar — grille mathématique ============
 // Repère local (enfant de largeRadarControls), unités radarSu: x 0..0.210, y 0..0.710.
    const irr::f32 GM = 0.010f;                    // marge gauche/droite
    const irr::f32 GG = 0.006f;                    // gouttière colonnes
    const irr::f32 GWc = 0.190f;                    // largeur de contenu
    const irr::f32 GC = (GWc - 2.0f * GG) / 3.0f;  // largeur d'une colonne
    const irr::f32 GRH = 0.024f;                    // hauteur d'un bouton
    auto cell = [&](int c, int span, irr::f32 y, irr::f32 h) -> irr::core::rect<irr::s32> {
        irr::f32 x0 = GM + c * (GC + GG);
        irr::f32 x1 = x0 + span * GC + (span - 1) * GG;
        return irr::core::rect<irr::s32>((irr::s32)(x0 * radarSu), (irr::s32)(y * radarSu),
            (irr::s32)(x1 * radarSu), (irr::s32)((y + h) * radarSu));
        };
    const irr::video::SColor accBtn(60, 61, 75, 97);
    const irr::video::SColor accSoft(60, 40, 90, 70); // touches à bascule: teinte distincte
    auto addStrip = [&](irr::gui::IGUIButton* b, irr::video::SColor c) {
        if (!b) return;
        irr::core::rect<irr::s32> br = b->getRelativePosition();
        irr::gui::IGUIRectangle* s = new irr::gui::IGUIRectangle(guienv, b,
            irr::core::rect<irr::s32>(0, 0, br.getWidth(), br.getHeight()), false);
        s->setFillColour(irr::video::SColor(80, c.getRed(), c.getGreen(), c.getBlue()));
        s->setClickThrough(true);
        };
    auto addRadarBtn = [&](irr::core::rect<irr::s32> r, irr::s32 id, const wchar_t* label, irr::video::SColor c) -> irr::gui::IGUIButton* {
        irr::gui::IGUIButton* b = guienv->addButton(r, largeRadarControls, id, label);
        addStrip(b, c);
        return b;
        };

    // Rangée A — orientation
    northUpButton2 = addRadarBtn(cell(0, 1, 0.104f, GRH), GUI_ID_RADAR_NORTH_BUTTON, language->translate("northUp").c_str(), accBtn);
    courseUpButton2 = addRadarBtn(cell(1, 1, 0.104f, GRH), GUI_ID_RADAR_COURSE_BUTTON, language->translate("courseUp").c_str(), accBtn);
    headUpButton2 = addRadarBtn(cell(2, 1, 0.104f, GRH), GUI_ID_RADAR_HEAD_BUTTON, language->translate("headUp").c_str(), accBtn);

    // Rangée B — portée + anneaux
    addRadarBtn(cell(0, 1, 0.132f, GRH), GUI_ID_RADAR_INCREASE_BUTTON, language->translate("increaserange").c_str(), accBtn);
    addRadarBtn(cell(1, 1, 0.132f, GRH), GUI_ID_RADAR_DECREASE_BUTTON, language->translate("decreaserange").c_str(), accBtn);
    rangeRingsButton2 = addRadarBtn(cell(2, 1, 0.132f, GRH), GUI_ID_RADAR_RANGE_RINGS_BUTTON, L"Anneaux", accBtn);

    // Rangée C — touches à bascule (remplacent les listes déroulantes)
    arpaModeButton2 = addRadarBtn(cell(0, 1, 0.160f, GRH), GUI_ID_BIG_ARPA_MODE_BUTTON, L"ARPA: Man", accSoft);
    arpaVectorButton2 = addRadarBtn(cell(1, 1, 0.160f, GRH), GUI_ID_BIG_ARPA_VECTOR_BUTTON, L"Vect: Vrai", accSoft);
    headingModeButton2 = addRadarBtn(cell(2, 1, 0.160f, GRH), GUI_ID_RADAR_HEADING_MODE_BUTTON, L"Cap: ARPA", accSoft);

    // Bloc EBL/VRM/couleur/alarme — 4 rangées x 3 colonnes (croix conservée)
    const irr::f32 EY = 0.192f, EP = 0.028f;
    eblSelectButton2 = addRadarBtn(cell(0, 1, EY + 0 * EP, GRH), GUI_ID_RADAR_EBL_SELECT_BUTTON, L"EBL 1/2", accBtn);
    eblUpButton2 = addRadarBtn(cell(1, 1, EY + 0 * EP, GRH), GUI_ID_RADAR_EBL_UP_BUTTON, language->translate("eblUp").c_str(), accBtn);
    vrmSelectButton2 = addRadarBtn(cell(2, 1, EY + 0 * EP, GRH), GUI_ID_RADAR_VRM_SELECT_BUTTON, L"VRM 1/2", accBtn);
    eblLeftButton2 = addRadarBtn(cell(0, 1, EY + 1 * EP, GRH), GUI_ID_RADAR_EBL_LEFT_BUTTON, language->translate("eblLeft").c_str(), accBtn);
    radarColourButton2 = addRadarBtn(cell(1, 1, EY + 1 * EP, GRH), GUI_ID_RADAR_COLOUR_BUTTON, language->translate("radarColour").c_str(), accBtn);
    eblRightButton2 = addRadarBtn(cell(2, 1, EY + 1 * EP, GRH), GUI_ID_RADAR_EBL_RIGHT_BUTTON, language->translate("eblRight").c_str(), accBtn);
    eblColourButton2 = addRadarBtn(cell(0, 1, EY + 2 * EP, GRH), GUI_ID_RADAR_EBL_COLOUR_BUTTON, L"Coul. EBL", accBtn);
    eblDownButton2 = addRadarBtn(cell(1, 1, EY + 2 * EP, GRH), GUI_ID_RADAR_EBL_DOWN_BUTTON, language->translate("eblDown").c_str(), accBtn);
    vrmColourButton2 = addRadarBtn(cell(2, 1, EY + 2 * EP, GRH), GUI_ID_RADAR_VRM_COLOUR_BUTTON, L"Coul. VRM", accBtn);
    guardAlarmButton2 = addRadarBtn(cell(0, 2, EY + 3 * EP, GRH), GUI_ID_RADAR_GUARD_ALARM_BUTTON, L"Alarme", accBtn);
    echoStretchButton2 = addRadarBtn(cell(2, 1, EY + 3 * EP, GRH), GUI_ID_RADAR_ECHO_STRETCH_BUTTON, L"\u00C9cho", accBtn); //kyara: cellule libre de la grille
    //offCentreButton2 = addRadarBtn(cell(2, 1, EY + 4 * EP, GRH), GUI_ID_RADAR_OFFCENTRE_BUTTON, L"D\u00E9centr.", accBtn); //kyara: décentrage
    offCentreButton2 = 0; //not shown (line above); updateGuiData still highlights it, so it must be null, not garbage

    // Radar cursor buttons
    radarCursorLeftButton2 = guienv->addButton(irr::core::rect<irr::s32>(radarTL.X + 0.670 * radarSu, radarTL.Y + 0.640 * radarSu, radarTL.X + 0.700 * radarSu, radarTL.Y + 0.670 * radarSu), 0, GUI_ID_RADAR_DECREASE_X_BUTTON, L"<");
    radarCursorRightButton2 = guienv->addButton(irr::core::rect<irr::s32>(radarTL.X + 0.730 * radarSu, radarTL.Y + 0.640 * radarSu, radarTL.X + 0.760 * radarSu, radarTL.Y + 0.670 * radarSu), 0, GUI_ID_RADAR_INCREASE_X_BUTTON, L">");
    radarCursorUpButton2 = guienv->addButton(irr::core::rect<irr::s32>(radarTL.X + 0.700 * radarSu, radarTL.Y + 0.610 * radarSu, radarTL.X + 0.730 * radarSu, radarTL.Y + 0.640 * radarSu), 0, GUI_ID_RADAR_INCREASE_Y_BUTTON, L"^");
    radarCursorDownButton2 = guienv->addButton(irr::core::rect<irr::s32>(radarTL.X + 0.700 * radarSu, radarTL.Y + 0.670 * radarSu, radarTL.X + 0.730 * radarSu, radarTL.Y + 0.700 * radarSu), 0, GUI_ID_RADAR_DECREASE_Y_BUTTON, L"v");

    // Bloc info + position + curseur (pleine largeur)
    auto fullRow = [&](irr::f32 yTop, irr::f32 yBot) -> irr::core::rect<irr::s32> {
        return irr::core::rect<irr::s32>((irr::s32)(GM * radarSu), (irr::s32)(yTop * radarSu),
            (irr::s32)((GM + GWc) * radarSu), (irr::s32)(yBot * radarSu));
        };
    //kyara: bloc Echelle/VRM/EBL/Alarme -> bas-gauche, style Furuno (sans cadre)
    {
        irr::s32 vX = radarTL.X + (irr::s32)(0.015f * radarSu);
        irr::s32 vH = (irr::s32)(0.13f * sh);
        //EBL BLOCK LOWER vH
        irr::s32 vTop = radarTL.Y + (irr::s32)(radarLargeRect.getHeight()) - vH - (irr::s32)(0.03f * sh);
        irr::s32 vW = (irr::s32)(0.22f * radarSu);
        radarText2 = guienv->addStaticText(L"", irr::core::rect<irr::s32>(vX, vTop, vX + vW, vTop + vH), false, true, 0, -1, false);
        radarText2->setOverrideColor(irr::video::SColor(255, 0, 255, 0));
    }
    //kyara: bloc OWN SHIP / CURSOR -> haut-droite (zone verte), style Furuno (sans cadre)
    radarPosText2 = guienv->addStaticText(L"",
        irr::core::rect<irr::s32>(
            //MOVING BLOC TO THE RIGHT
            radarTL.X + (irr::s32)(0.650f * radarSu), radarTL.Y + (irr::s32)(0.020f * radarSu),
            radarTL.X + (irr::s32)(0.780f * radarSu), radarTL.Y + (irr::s32)(0.360f * radarSu)),
        false, true, 0, -1, false);
    radarPosText2->setOverrideColor(irr::video::SColor(255, 0, 255, 0));
    //kyara: infos curseur repliées dans radarPosText2 -> ces 3 éléments restent masqués (compat refs)
    radarCursorPosText2 = guienv->addStaticText(L"", irr::core::rect<irr::s32>(0, 0, 1, 1), false, false, 0, -1, false);
    radarCursorRangeText2 = guienv->addStaticText(L"", irr::core::rect<irr::s32>(0, 0, 1, 1), false, false, 0, -1, false);
    radarCursorBrgText2 = guienv->addStaticText(L"", irr::core::rect<irr::s32>(0, 0, 1, 1), false, false, 0, -1, false);
    radarCursorPosText2->setVisible(false);
    radarCursorRangeText2->setVisible(false);
    radarCursorBrgText2->setVisible(false);
    //Radar PI tab
    //Drop down box to select PI 1-10
//CHANGE PI TAB TEXT 
    irr::gui::IGUIStaticText* IndexLabel = guienv->addStaticText(
        language->translate("parallelIndex").c_str(),
        irr::core::rect<irr::s32>(
            0.055 * su, 0.040 * sh,
            0.205 * su, 0.080 * sh), false,
        true
        , radarPITab);
    IndexLabel->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_CENTER);
    IndexLabel->setOverrideColor(irr::video::SColor(255, 255, 255, 255));

    (guienv->addStaticText(language->translate("parallelIndex").c_str(), irr::core::rect<irr::s32>(0.055 * su, 0.040 * sh, 0.205 * su, 0.080 * sh), false, true, radarPITab))->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_CENTER);
    irr::gui::IGUIComboBox* piSelected = guienv->addComboBox(irr::core::rect<irr::s32>(0.005 * su, 0.040 * sh, 0.050 * su, 0.080 * sh), radarPITab, GUI_ID_PI_SELECT_BOX);
    piSelected->addItem(L"1");
    piSelected->addItem(L"2");
    piSelected->addItem(L"3");
    piSelected->addItem(L"4");
    piSelected->addItem(L"5");
    piSelected->addItem(L"6");
    piSelected->addItem(L"7");
    piSelected->addItem(L"8");
    piSelected->addItem(L"9");
    piSelected->addItem(L"10");
    //Edit boxes for bearing and range (+ve/-ve)
    (guienv->addStaticText(language->translate("piRange").c_str(), irr::core::rect<irr::s32>(0.055 * su, 0.100 * sh, 0.205 * su, 0.140 * sh), false, true, radarPITab))->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_CENTER);;
    guienv->addEditBox(L"0", irr::core::rect<irr::s32>(0.005 * su, 0.100 * sh, 0.050 * su, 0.140 * sh), true, radarPITab, GUI_ID_PI_RANGE_BOX);
    (guienv->addStaticText(language->translate("piBearing").c_str(), irr::core::rect<irr::s32>(0.055 * su, 0.160 * sh, 0.205 * su, 0.200 * sh), false, true, radarPITab))->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_CENTER);;
    guienv->addEditBox(L"0", irr::core::rect<irr::s32>(0.005 * su, 0.160 * sh, 0.050 * su, 0.200 * sh), true, radarPITab, GUI_ID_PI_BEARING_BOX);

    //PI on big radar screen
    (guienv->addStaticText(language->translate("parallelIndex").c_str(), irr::core::rect<irr::s32>(0.005 * radarSu, 0.010 * radarSu, 0.075 * radarSu, 0.070 * radarSu), false, true, largeRadarPIControls))->setTextAlignment(irr::gui::EGUIA_LOWERRIGHT, irr::gui::EGUIA_UPPERLEFT);
    irr::gui::IGUIComboBox* piSelectedBig = guienv->addComboBox(irr::core::rect<irr::s32>(0.080 * radarSu, 0.010 * radarSu, 0.195 * radarSu, 0.035 * radarSu), largeRadarPIControls, GUI_ID_BIG_PI_SELECT_BOX);
    piSelectedBig->addItem(L"1");
    piSelectedBig->addItem(L"2");
    piSelectedBig->addItem(L"3");
    piSelectedBig->addItem(L"4");
    piSelectedBig->addItem(L"5");
    piSelectedBig->addItem(L"6");
    piSelectedBig->addItem(L"7");
    piSelectedBig->addItem(L"8");
    piSelectedBig->addItem(L"9");
    piSelectedBig->addItem(L"10");

    guienv->addStaticText(language->translate("PIrange").c_str(), irr::core::rect<irr::s32>(0.130 * radarSu, 0.045 * radarSu, 0.215 * radarSu, 0.070 * radarSu), false, false, largeRadarPIControls);
    guienv->addEditBox(L"0", irr::core::rect<irr::s32>(0.080 * radarSu, 0.045 * radarSu, 0.125 * radarSu, 0.070 * radarSu), true, largeRadarPIControls, GUI_ID_BIG_PI_RANGE_BOX);

    guienv->addStaticText(language->translate("PIbearing").c_str(), irr::core::rect<irr::s32>(0.130 * radarSu, 0.080 * radarSu, 0.215 * radarSu, 0.105 * radarSu), false, false, largeRadarPIControls);
    guienv->addEditBox(L"0", irr::core::rect<irr::s32>(0.080 * radarSu, 0.080 * radarSu, 0.125 * radarSu, 0.105 * radarSu), true, largeRadarPIControls, GUI_ID_BIG_PI_BEARING_BOX);
    //CHANGES  Range Nm box
    irr::gui::IGUIEditBox* piRangeBox = guienv->addEditBox(
        L"0",
        irr::core::rect<irr::s32>(0.005 * su, 0.100 * sh, 0.050 * su, 0.140 * sh),
        true, radarPITab, GUI_ID_PI_RANGE_BOX
    );
    piRangeBox->setDrawBackground(true);

    //CHANGES Bearing box
    irr::gui::IGUIEditBox* piBearingBox = guienv->addEditBox(
        L"0",
        irr::core::rect<irr::s32>(0.005 * su, 0.160 * sh, 0.050 * su, 0.200 * sh),
        true, radarPITab, GUI_ID_PI_BEARING_BOX
    );
    piBearingBox->setDrawBackground(true);

    //Radar ARPA tab
    irr::gui::IGUIComboBox* arpaMode = guienv->addComboBox(irr::core::rect<irr::s32>(0.005 * su, 0.005 * sh, 0.150 * su, 0.035 * sh), radarARPATab, GUI_ID_ARPA_ON_BOX);
    arpaMode->addItem(language->translate("arpaManual").c_str());
    arpaMode->addItem(language->translate("marpaOn").c_str());
    arpaMode->addItem(language->translate("arpaOn").c_str());
    irr::gui::IGUIComboBox* arpaVectorMode = guienv->addComboBox(irr::core::rect<irr::s32>(0.005 * su, 0.040 * sh, 0.150 * su, 0.070 * sh), radarARPATab, GUI_ID_ARPA_TRUE_REL_BOX);
    arpaVectorMode->addItem(language->translate("trueArpa").c_str());
    arpaVectorMode->addItem(language->translate("relArpa").c_str());
    guienv->addEditBox(L"6", irr::core::rect<irr::s32>(0.155 * su, 0.040 * sh, 0.195 * su, 0.070 * sh), true, radarARPATab, GUI_ID_ARPA_VECTOR_TIME_BOX);
    (guienv->addStaticText(language->translate("minsARPA").c_str(), irr::core::rect<irr::s32>(0.200 * su, 0.040 * sh, 0.237 * su, 0.070 * sh), false, true, radarARPATab))->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
    arpaList = guienv->addListBox(irr::core::rect<irr::s32>(0.005 * su, 0.075 * sh, 0.121 * su, 0.190 * sh), radarARPATab, GUI_ID_ARPA_LIST);
    arpaText = guienv->addListBox(irr::core::rect<irr::s32>(0.121 * su, 0.075 * sh, 0.237 * su, 0.190 * sh), radarARPATab);
    arpaList->setItemHeight(20);
    if (arpaList->getVerticalScrollBar()) {
        arpaList->getVerticalScrollBar()->setSmallStep(1);
        arpaList->getVerticalScrollBar()->setLargeStep(5);
    }
    // Same for arpaList2
    // Manual/MARPA buttons
    (guienv->addStaticText(language->translate("manualOrMarpa").c_str(), irr::core::rect<irr::s32>(0.005 * su, 0.190 * sh, 0.237 * su, 0.215 * sh), false, true, radarARPATab))->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
    guienv->addButton(irr::core::rect<irr::s32>(0.005 * su, 0.215 * sh, 0.082 * su, 0.240 * sh), radarARPATab, GUI_ID_MANUAL_NEW_BUTTON, language->translate("new").c_str());
    guienv->addButton(irr::core::rect<irr::s32>(0.082 * su, 0.215 * sh, 0.159 * su, 0.240 * sh), radarARPATab, GUI_ID_MANUAL_SCAN_BUTTON, language->translate("manualLog").c_str());
    guienv->addButton(irr::core::rect<irr::s32>(0.159 * su, 0.215 * sh, 0.237 * su, 0.240 * sh), radarARPATab, GUI_ID_MANUAL_CLEAR_BUTTON, language->translate("clear").c_str());



    //kyara: listes déroulantes CONSERVÉES mais MASQUÉES (mémoire d'état + synchro).
 //Les touches à bascule de la rangée C les pilotent.
    arpaMode = guienv->addComboBox(irr::core::rect<irr::s32>(0, 0, 1, 1), largeRadarControls, GUI_ID_BIG_ARPA_ON_BOX);
    arpaMode->addItem(language->translate("arpaManual").c_str());
    arpaMode->addItem(language->translate("marpaOn").c_str());
    arpaMode->addItem(language->translate("arpaOn").c_str());
    arpaMode->setVisible(false);
    arpaVectorMode = guienv->addComboBox(irr::core::rect<irr::s32>(0, 0, 1, 1), largeRadarControls, GUI_ID_BIG_ARPA_TRUE_REL_BOX);
    arpaVectorMode->addItem(language->translate("trueArpa").c_str());
    arpaVectorMode->addItem(language->translate("relArpa").c_str());
    arpaVectorMode->setVisible(false);

    // Temps de vecteur [6][min]
    //guienv->addEditBox(L"6", cell(0, 1, 0.456f, GRH), true, largeRadarControls, GUI_ID_BIG_ARPA_VECTOR_TIME_BOX);
    (guienv->addStaticText(language->translate("minsARPA").c_str(), cell(1, 1, 0.456f, GRH), false, true, largeRadarControls))->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);

    // Liste ARPA (deux colonnes)
    const irr::f32 splitX = GM + GC + GG * 0.5f;
    arpaList2 = guienv->addListBox(irr::core::rect<irr::s32>((irr::s32)(GM * radarSu), (irr::s32)(0.486f * radarSu), (irr::s32)(splitX * radarSu), (irr::s32)(0.640f * radarSu)), largeRadarControls, GUI_ID_BIG_ARPA_LIST);

    arpaText2 = guienv->addListBox(irr::core::rect<irr::s32>((irr::s32)(splitX * radarSu), (irr::s32)(0.486f * radarSu), (irr::s32)((GM + GWc) * radarSu), (irr::s32)(0.640f * radarSu)), largeRadarControls);
    //kyara: onglets ARPA / AIS au-dessus de la boîte de données + liste AIS superposée
    aisDataMode = false;
    irr::f32 midX = (splitX + (GM + GWc)) * 0.5f;
    arpaTabButton2 = guienv->addButton(irr::core::rect<irr::s32>((irr::s32)(splitX * radarSu), (irr::s32)(0.462f * radarSu), (irr::s32)(midX * radarSu), (irr::s32)(0.484f * radarSu)), largeRadarControls, GUI_ID_RADAR_ARPA_TAB_BUTTON, L"ARPA");
    aisTabButton2 = guienv->addButton(irr::core::rect<irr::s32>((irr::s32)(midX * radarSu), (irr::s32)(0.462f * radarSu), (irr::s32)((GM + GWc) * radarSu), (irr::s32)(0.484f * radarSu)), largeRadarControls, GUI_ID_RADAR_AIS_TAB_BUTTON, L"AIS");
    aisText2 = guienv->addListBox(irr::core::rect<irr::s32>((irr::s32)(splitX * radarSu), (irr::s32)(0.486f * radarSu), (irr::s32)((GM + GWc) * radarSu), (irr::s32)(0.640f * radarSu)), largeRadarControls);
    aisText2->setVisible(false);
    arpaList2->setItemHeight(20);
    if (arpaList2->getVerticalScrollBar()) {
        arpaList2->getVerticalScrollBar()->setSmallStep(1);
        arpaList2->getVerticalScrollBar()->setLargeStep(5);
    }

    // Manuel/MARPA


    // NOTE: the ship/buoy colour combo boxes live ONLY in the bottom-left global panel
    // (created below). Do NOT create duplicates here with the same GUI IDs, or
    // getElementFromId() finds the wrong one and selections stop applying.
    //KYARA: the big "paused - click to start" page listing the keys is gone. The key mapping now
    //lives in the launcher tab, and main.cpp starts the clock as soon as loading has finished.
    pausedButton = 0;

    //show/hide interface
    showInterface = true; //If we start with the 2d interface shown
    showPrimaryControls = true;
    togglePrimaryControlsButton = guienv->addButton(irr::core::rect<irr::s32>(0.01 * su, 0.965 * sh, 0.08 * su, 0.995 * sh), 0, GUI_ID_TOGGLE_PRIMARY_CONTROLS_BUTTON, L"Hide Controls");

    // KYARA DAY/NIGHT toggle. Starts in day mode, so the button offers "Nuit".


    showInterfaceButton = guienv->addButton(irr::core::rect<irr::s32>(0.09 * su + azimuthGUIOffsetL, 0.92 * sh, 0.125 * su + azimuthGUIOffsetL, 0.95 * sh), 0, GUI_ID_SHOW_INTERFACE_BUTTON, language->translate("showinterface").c_str());
    hideInterfaceButton = guienv->addButton(irr::core::rect<irr::s32>(0.09 * su + azimuthGUIOffsetL, 0.92 * sh, 0.125 * su + azimuthGUIOffsetL, 0.95 * sh), 0, GUI_ID_HIDE_INTERFACE_BUTTON, language->translate("hideinterface").c_str());
    showInterfaceButton->setVisible(false);

    //binoculars button
    binosButton = guienv->addButton(irr::core::rect<irr::s32>(0.125 * su + azimuthGUIOffsetL, 0.92 * sh, 0.16 * su + azimuthGUIOffsetL, 0.95 * sh), 0, GUI_ID_BINOS_INTERFACE_BUTTON, language->translate("zoom").c_str());
    binosButton->setIsPushButton(true);

    //Take bearing button
    bearingButton = guienv->addButton(irr::core::rect<irr::s32>(0.16 * su + azimuthGUIOffsetL, 0.92 * sh, 0.195 * su + azimuthGUIOffsetL, 0.95 * sh), 0, GUI_ID_BEARING_INTERFACE_BUTTON, language->translate("bearing").c_str());
    bearingButton->setIsPushButton(true);

    // Change view button
    changeViewButton = guienv->addButton(irr::core::rect<irr::s32>(0.195 * su + azimuthGUIOffsetL, 0.92 * sh, 0.23 * su + azimuthGUIOffsetL, 0.95 * sh), 0, GUI_ID_CHANGE_VIEW_BUTTON, language->translate("changeView").c_str());

    //Exit button
    exitButton = guienv->addButton(irr::core::rect<irr::s32>(0.23 * su + azimuthGUIOffsetL, 0.92 * sh, 0.265 * su + azimuthGUIOffsetL, 0.95 * sh), 0, GUI_ID_EXIT_BUTTON, language->translate("exit").c_str());

    //Show button to display extra controls window
    showExtraControlsButton = guienv->addButton(irr::core::rect<irr::s32>(0.265 * su + azimuthGUIOffsetL, 0.92 * sh, 0.34 * su + azimuthGUIOffsetL, 0.95 * sh), 0, GUI_ID_SHOW_EXTRA_CONTROLS_BUTTON, language->translate("extraControls").c_str());

    //Show button to display lines control window
    showLinesControlsButton = guienv->addButton(irr::core::rect<irr::s32>(0.34 * su + azimuthGUIOffsetL, 0.92 * sh, 0.375 * su + azimuthGUIOffsetL, 0.95 * sh), 0, GUI_ID_SHOW_LINES_CONTROLS_BUTTON, language->translate("lines").c_str());

    //Show internal log window button
    pcLogButton = guienv->addButton(irr::core::rect<irr::s32>(0.375 * su + azimuthGUIOffsetL, 0.92 * sh, 0.39 * su + azimuthGUIOffsetL, 0.95 * sh), 0, GUI_ID_SHOW_LOG_BUTTON, language->translate("log").c_str());

    //Instrument console in its own window (second screen). Sized from its label, in the gap before
    //the lighting time box.
    if (instrumentPanel) {
        irr::gui::IGUIFont* dcFont = guienv->getSkin() ? guienv->getSkin()->getFont() : 0;
        const irr::s32 dcW = dcFont ? (irr::s32)dcFont->getDimension(L"Rattacher").Width + 24 : (irr::s32)(0.06 * su);
        const irr::s32 dcX = (irr::s32)(0.39 * su + azimuthGUIOffsetL) + 6;
        detachConsoleButton = guienv->addButton(irr::core::rect<irr::s32>(dcX, (irr::s32)(0.92 * sh), dcX + dcW, (irr::s32)(0.95 * sh)),
            0, GUI_ID_DETACH_CONSOLE_BUTTON, L"D\u00E9tacher");
        detachConsoleButton->setToolTipText(L"Instruments dans une fen\u00EAtre s\u00E9par\u00E9e, \u00E0 placer sur un autre \u00E9cran");
    }

    //Command bar: the day / dusk / night switch, then every key in its place
    if (commandBar) {
        const wchar_t* paletteNames[PALETTE_KEYS] = { L"JOUR", L"CR\u00C9P.", L"NUIT", L"DIGITAL", L"AUTO" };
        const wchar_t* paletteTips[PALETTE_KEYS] = {
            L"Couleurs de jour : cadrans clairs, lisibles au soleil",
            L"Couleurs de cr\u00E9puscule : cadrans sombres, graduations blanches",
            L"Couleurs de nuit : noir et ambre att\u00E9nu\u00E9, pour garder la vision de nuit",
            L"Couleurs digitales : noir et vert, comme l'\u00E9cran radar",
            L"Suit la lumi\u00E8re du sc\u00E9nario (jour, cr\u00E9puscule, nuit)" };
        for (int i = 0; i < PALETTE_KEYS; i++) {
            paletteButton[i] = guienv->addButton(irr::core::rect<irr::s32>(0, 0, 10, 10), 0, GUI_ID_PALETTE_DAY + i,
                paletteNames[i], paletteTips[i]);
            paletteButton[i]->setIsPushButton(true);
            bridgeSkin->setKeyStyle(GUI_ID_PALETTE_DAY + i, bridge::BridgeSkin::KEY_BAR);
        }
        //The weather window, and its key (bc5.ini show_weather_tool=0 hides them from trainees)
        if (IniFile::iniFileTou32(iniFilename, "show_weather_tool", 1) == 1) {
            weatherButton = guienv->addButton(irr::core::rect<irr::s32>(0, 0, 10, 10), 0, GUI_ID_WEATHER_BUTTON, L"M\u00C9T\u00C9O",
                L"M\u00E9t\u00E9o : pr\u00E9r\u00E9glages, vent, mer, visibilit\u00E9, pluie, neige, front, heure");
            weatherPanel = new WeatherPanel(guienv, guienv->getRootGUIElement(), model, irr::core::rect<irr::s32>(0, 0, 10, 10));
            weatherPanel->drop();
            weatherPanel->setLightingTimeBox(lightingTimeBox);
        }
        //Glow of the lit instruments
        glowScrollbar = guienv->addScrollBar(true, irr::core::rect<irr::s32>(0, 0, 10, 10), 0, GUI_ID_GLOW_SCROLL_BAR);
        glowScrollbar->setMin(0);
        glowScrollbar->setMax(100);
        glowScrollbar->setSmallStep(5);
        glowScrollbar->setLargeStep(10);
        layoutCommandBar();

        //Start-up colours and glow from bc5.ini: palette=auto|jour|crepuscule|nuit|digital, instrument_glow=0..100
        std::string startPalette = IniFile::iniFileToString(iniFilename, "palette");
        for (size_t k = 0; k < startPalette.size(); k++) { startPalette[k] = (char)tolower((unsigned char)startPalette[k]); }
        int choice = -1;
        if (startPalette == "jour" || startPalette == "day") { choice = bridge::MODE_DAY; }
        else if (startPalette == "crepuscule" || startPalette == "dusk") { choice = bridge::MODE_DUSK; }
        else if (startPalette == "nuit" || startPalette == "night") { choice = bridge::MODE_NIGHT; }
        else if (startPalette == "digital") { choice = bridge::MODE_DIGITAL; }
        setPaletteChoice(choice);
        const std::string glowKey = IniFile::iniFileToString(iniFilename, "instrument_glow");
        setGlowLevel(glowKey.empty() ? 50 : (int)IniFile::iniFileTou32(iniFilename, "instrument_glow"));
    }

    // --- TOGGLE CONTROLS: top-left of large radar screen (clear of the circular display) ---
    // ARPA Buoys / Buoy Trails moved here from the bottom block below, since at the
    // bottom of the radar box the circle's curve extends far enough left to sit underneath
    // them; near the top the circle is narrow enough that this column is clear of it.
    {
        irr::s32 cbX = radarTL.X + (irr::s32)(0.015f * radarSu);
        irr::s32 cbW = (irr::s32)(0.17f * radarSu);
        irr::s32 cbH = (irr::s32)(0.025f * sh);
        irr::s32 cbPad = (irr::s32)(0.032f * sh);
        //kyara - to control position for toggle controls
        irr::s32 cbY = radarTL.Y + (irr::s32)(0.040f * radarSu);

        guienv->addCheckBox(true, irr::core::rect<irr::s32>(cbX, cbY, cbX + cbW, cbY + cbH), 0, GUI_ID_RADAR_ARPA_BUOYS_CHECKBOX, L"Bou\u00E9es ARPA");

        //kyara: cases + combos couleur déplacés ici depuis le bas
        guienv->addCheckBox(false, irr::core::rect<irr::s32>(cbX, cbY + cbPad, cbX + cbW, cbY + cbPad + cbH), 0, GUI_ID_RADAR_SHIP_TRAILS_CHECKBOX, L"Traces navires");
        guienv->addCheckBox(false, irr::core::rect<irr::s32>(cbX, cbY + 2 * cbPad, cbX + cbW, cbY + 2 * cbPad + cbH), 0, GUI_ID_RADAR_OWN_SHIP_TRAILS_CHECKBOX, L"Trace navire propre");
        guienv->addCheckBox(true, irr::core::rect<irr::s32>(cbX, cbY + 3 * cbPad, cbX + cbW, cbY + 3 * cbPad + cbH), 0, GUI_ID_RADAR_MMSI_CHECKBOX, L"Num\u00E9ros MMSI");
        irr::s32 cbWcol = (irr::s32)(0.080f * radarSu);
        irr::s32 cbHcol = (irr::s32)(0.020f * sh);

        irr::gui::IGUIComboBox* buoyColourBoxGlobal =
            guienv->addComboBox(irr::core::rect<irr::s32>((irr::s32)(0.010f * radarSu), (irr::s32)(0.655f * radarSu), (irr::s32)(0.150f * radarSu), (irr::s32)(0.678f * radarSu)), largeRadarControls, GUI_ID_RADAR_BUOY_COLOUR_BOX);

        buoyColourBoxGlobal->addItem(L"Bou\u00E9es : jaune"); buoyColourBoxGlobal->addItem(L"Bou\u00E9es : cyan"); buoyColourBoxGlobal->addItem(L"Bou\u00E9es : vert"); buoyColourBoxGlobal->addItem(L"Bou\u00E9es : rouge"); buoyColourBoxGlobal->addItem(L"Bou\u00E9es : magenta"); buoyColourBoxGlobal->addItem(L"Bou\u00E9es : blanc"); buoyColourBoxGlobal->addItem(L"Bou\u00E9es : orange");
        buoyColourBoxGlobal->setSelected(1);
        irr::gui::IGUIComboBox* shipColourBoxGlobal = guienv->addComboBox(irr::core::rect<irr::s32>((irr::s32)(0.010f * radarSu), (irr::s32)(0.682f * radarSu), (irr::s32)(0.150f * radarSu), (irr::s32)(0.705f * radarSu)), largeRadarControls, GUI_ID_RADAR_SHIP_COLOUR_BOX);
        shipColourBoxGlobal->addItem(L"Navires : jaune"); shipColourBoxGlobal->addItem(L"Navires : cyan"); shipColourBoxGlobal->addItem(L"Navires : vert"); shipColourBoxGlobal->addItem(L"Navires : rouge"); shipColourBoxGlobal->addItem(L"Navires : magenta"); shipColourBoxGlobal->addItem(L"Navires : blanc"); shipColourBoxGlobal->addItem(L"Navires : orange");
        shipColourBoxGlobal->setSelected(5);
    }



    //Set initial visibility
    updateVisibility();
    // Determine if primary controls should be visible

    //Console window: put the console back where it was last session (its own window, if detached).
    if (instrumentPanel) {
        consoleStatusAttachedRect[0] = bigRadarButton->getRelativePosition();
        consoleStatusAttachedRect[1] = pump1On->getRelativePosition();
        consoleStatusAttachedRect[2] = pump2On->getRelativePosition();
        consoleStatusAttachedRect[3] = ackAlarms->getRelativePosition();
        const irr::u32 fps = IniFile::iniFileTou32(iniFilename, "console_window_fps");
        if (fps > 0) { consoleFrameMs = 1000 / fps; }
        const std::string placement = consolePlacementFile(consoleInstance);
        consolePlaceX = IniFile::iniFileTos32(placement, "X", consolePlaceX);
        consolePlaceY = IniFile::iniFileTos32(placement, "Y", consolePlaceY);
        consolePlaceW = IniFile::iniFileTou32(placement, "Width");
        consolePlaceH = IniFile::iniFileTou32(placement, "Height");
        if (consoleOnScreen || IniFile::iniFileTou32(placement, "Detached") == 1) {
            setConsoleDetached(true);
        }
    }
}

GUIMain::~GUIMain()
{
    //Drop scroll bars created with 'new'
    if (portScrollbar) { portScrollbar->drop(); }
    if (stbdScrollbar) { stbdScrollbar->drop(); }
    if (wheelScrollbar) { wheelScrollbar->drop(); }

    if (rateofturnScrollbar) { rateofturnScrollbar->drop(); }

    if (bowThrusterScrollbar) { bowThrusterScrollbar->drop(); }
    if (sternThrusterScrollbar) { sternThrusterScrollbar->drop(); }

    if (azimuth1Control) { azimuth1Control->drop(); }
    if (azimuth2Control) { azimuth2Control->drop(); }


    // DEE_NOV22 vvvv
    if (schottelPort) { schottelPort->drop(); }
    if (schottelStbd) { schottelStbd->drop(); }
    if (azimuthEnginePort) { azimuthEnginePort->drop(); }
    if (azimuthEngineStbd) { azimuthEngineStbd->drop(); }
    // DEE_NOV22 ^^^^

    //KYARA: the weather/wind/stream sliders are created with guienv->addScrollBar(), which already
    //hands ownership to their tab. Dropping them here freed them while the GUI tree still pointed
    //at them -> use-after-free when the environment is destroyed (crash on exit). Only elements
    //made with 'new' (below) are ours to drop.

    if (instrumentPanel) { instrumentPanel->drop(); }

    radarGainScrollbar->drop();
    radarClutterScrollbar->drop();
    radarRainScrollbar->drop();

    radarGainScrollbar2->drop();
    radarClutterScrollbar2->drop();
    radarRainScrollbar2->drop();

    //largeRadarControls->drop();

    hdgScrollbar->drop();
    spdScrollbar->drop();

    headingIndicator->drop();

    magnificationScrollbar->drop();
}

bool GUIMain::getSmallRadarEnabled() const
{
    return !instrumentsEnabled; //kyara: the console replaces the small radar
}

bool GUIMain::getShowInterface() const
{
    return showInterface;
}

// Roll round between normal view, full view with limited gui, and full view with no GUI
void GUIMain::toggleShow2dInterface()
{
    if (!getLargeRadar()) {
        if (!showInterface) {
            if (guienv->getRootGUIElement()->isVisible()) {
                guienv->getRootGUIElement()->setVisible(false);
            }
            else {
                showInterface = true;
                guienv->getRootGUIElement()->setVisible(true);
            }

        }
        else {
            showInterface = false;
        }
        updateVisibility();
    }
}

void GUIMain::show2dInterface()
{
    showInterface = true;
    updateVisibility();
}

void GUIMain::hide2dInterface()
{
    showInterface = false;
    updateVisibility();
}

bool GUIMain::getShow3d() const
{
    return show3d->isChecked();
}

void GUIMain::showBearings()
{
    bearingButton->setPressed(true);
}

void GUIMain::hideBearings()
{
    bearingButton->setPressed(false);
}

void GUIMain::toggleBearings()
{
    bearingButton->setPressed(!bearingButton->isPressed());
}

void GUIMain::zoomOn()
{
    binosButton->setPressed(true);
}

void GUIMain::zoomOff()
{
    binosButton->setPressed(false);
}

void GUIMain::setLargeRadar(bool radarState)
{
    radarLarge = radarState;
    updateVisibility();
}

bool GUIMain::getLargeRadar() const
{
    return radarLarge;
}

bool GUIMain::getAnchorLine() const
{
    return anchorLine->isChecked();
}

void GUIMain::setARPAComboboxes(irr::s32 arpaState)
{
    //Set both linked inputs - brute force
    irr::gui::IGUIElement* arpaCheckbox = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(GUIMain::GUI_ID_ARPA_ON_BOX, true);
    if (arpaCheckbox != 0) {
        ((irr::gui::IGUIComboBox*)arpaCheckbox)->setSelected(arpaState);
    }
    arpaCheckbox = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(GUIMain::GUI_ID_BIG_ARPA_ON_BOX, true);
    if (arpaCheckbox != 0) {
        ((irr::gui::IGUIComboBox*)arpaCheckbox)->setSelected(arpaState);
    }
}

void GUIMain::setARPAList(int arpaSelected)
{
    //Set both linked inputs - brute force
    if (arpaList != 0) {
        arpaList->setSelected(arpaSelected);
    }

    if (arpaList2 != 0) {
        arpaList2->setSelected(arpaSelected);
    }
}

irr::u32 GUIMain::getRadarPixelRadius() const
{
    //kyara: the picture is drawn RADAR_FIT_MARGIN smaller than its viewport (see RadarScreen.hpp),
    //so report the real on-screen circle radius. This keeps the bitmap resolution and the
    //cursor/range mapping matched to what's actually displayed.
    if (radarLarge) {
        return (irr::u32)((irr::f32)largeRadarScreenRadius / RADAR_FIT_MARGIN);
    }
    else {
        return (irr::u32)((irr::f32)smallRadarScreenRadius / RADAR_FIT_MARGIN);
    }
}

irr::core::vector2di GUIMain::getCursorPositionRadar() const
{
    //Basic mouse position
    irr::core::vector2di cursorPosition = device->getCursorControl()->getPosition();

    //KYARA: no small radar on screen -> report the cursor far outside any scope. RadarCalculation
    //ignores clicks beyond the range, so a click on the console can never move the radar cursor.
    if (!radarLarge && !getSmallRadarEnabled()) {
        return irr::core::vector2di(10000, 10000);
    }

    //Radar screen centre position
    irr::core::vector2di radarScreenCentre;
    if (radarLarge) {
        radarScreenCentre.X = largeRadarScreenCentreX;
        radarScreenCentre.Y = largeRadarScreenCentreY;
        radarScreenCentre.X += (irr::s32)(guiRadarOffsetX * largeRadarScreenRadius); //kyara
        radarScreenCentre.Y -= (irr::s32)(guiRadarOffsetY * largeRadarScreenRadius);
    }
    else {
        radarScreenCentre.X = smallRadarScreenCentreX;
        radarScreenCentre.Y = smallRadarScreenCentreY;
    }

    //Return the difference
    return (cursorPosition - radarScreenCentre);
}

irr::core::rect<irr::s32> GUIMain::getSmallRadarRect() const
{
    irr::u32 graphicsWidth3d = su;
    irr::u32 graphicsHeight3d = sh * viewProportion3D();
    return irr::core::rect<irr::s32>(su - (sh - graphicsHeight3d) + azimuthGUIOffsetR, graphicsHeight3d, su + azimuthGUIOffsetR, sh);
}

irr::core::rect<irr::s32> GUIMain::getLargeRadarRect() const
{
    return irr::core::rect<irr::s32>(largeRadarScreenCentreX - largeRadarScreenRadius, largeRadarScreenCentreY - largeRadarScreenRadius, largeRadarScreenCentreX + largeRadarScreenRadius, largeRadarScreenCentreY + largeRadarScreenRadius);
}

bool GUIMain::isNFUActive() const
{
    return (nfuPortDown || nfuStbdDown);
}

void GUIMain::updateVisibility()
{
    //Items to show if we're showing interface
    //KYARA: the small radar's tabs, text and cursor pad only exist in the classic layout
    const bool smallRadarUI = showInterface && getSmallRadarEnabled();
    radarTabControl->setVisible(smallRadarUI);
    radarText->setVisible(smallRadarUI);

    radarCursorLeftButton->setVisible(smallRadarUI && !radarLarge);
    radarCursorRightButton->setVisible(smallRadarUI && !radarLarge);
    radarCursorUpButton->setVisible(smallRadarUI && !radarLarge);
    radarCursorDownButton->setVisible(smallRadarUI && !radarLarge);

    radarCursorLeftButton2->setVisible(false);
    radarCursorRightButton2->setVisible(false);
    radarCursorUpButton2->setVisible(false);
    radarCursorDownButton2->setVisible(false);

    //weatherScrollbar->setVisible(showInterface);
    //rainScrollbar->setVisible(showInterface);
    //visibilityScrollbar->setVisible(showInterface);
    pcLogButton->setVisible(showInterface);
    showExtraControlsButton->setVisible(showInterface);
    showLinesControlsButton->setVisible(showInterface);

    exitButton->setVisible(showInterface);



    pump1On->setVisible(showInterface);
    pump2On->setVisible(showInterface);
    ackAlarms->setVisible(showInterface);

    //Items not to show if we're on full screen radar
    //dataDisplay->setVisible(!radarLarge);
    binosButton->setVisible(!radarLarge);
    bearingButton->setVisible(!radarLarge);
    changeViewButton->setVisible(!radarLarge);
    //kyara
    if (magnificationScrollbar) { magnificationScrollbar->setVisible(!radarLarge); }
    rateofturnScrollbar->setVisible(!radarLarge && hasRateOfTurnIndicator && !instrumentsEnabled); //kyara: replaced by the GIRATION dial
    hideInterfaceButton->setVisible(showInterface && !radarLarge);
    showInterfaceButton->setVisible(!showInterface && !radarLarge);

    bigRadarButton->setVisible(showInterface && !radarLarge);

    smallRadarButton->setVisible(radarLarge);
    largeRadarControls->setVisible(radarLarge);
    largeRadarPIControls->setVisible(false); //kyara: PI retiré de l'affichage
    if (radarPosText2) radarPosText2->setVisible(radarLarge); //kyara
    if (radarText2) radarText2->setVisible(radarLarge); //kyara
    if (radarCursorPosText2) radarCursorPosText2->setVisible(false);
    if (radarCursorRangeText2) radarCursorRangeText2->setVisible(false);
    if (radarCursorBrgText2) radarCursorBrgText2->setVisible(false);
    // IGUIRectangle does not auto-propagate setVisible to children — do it manually
    for (irr::core::list<irr::gui::IGUIElement*>::ConstIterator it = largeRadarControls->getChildren().begin();
        it != largeRadarControls->getChildren().end(); ++it) {
        (*it)->setVisible(radarLarge);
    }
    for (irr::core::list<irr::gui::IGUIElement*>::ConstIterator it = largeRadarPIControls->getChildren().begin();
        it != largeRadarPIControls->getChildren().end(); ++it) {
        (*it)->setVisible(false);
    }

    // Toggle the new custom checkboxes dynamically so they only appear on the large radar
    irr::gui::IGUIElement* cb1 = guienv->getRootGUIElement()->getElementFromId(GUI_ID_RADAR_ARPA_BUOYS_CHECKBOX, true);
    if (cb1) cb1->setVisible(radarLarge);
    /*irr::gui::IGUIElement* cb2 = guienv->getRootGUIElement()->getElementFromId(GUI_ID_RADAR_BUOY_TRAILS_CHECKBOX, true);
    if (cb2) cb2->setVisible(radarLarge);*/
    irr::gui::IGUIElement* cb3 = guienv->getRootGUIElement()->getElementFromId(GUI_ID_RADAR_SHIP_TRAILS_CHECKBOX, true);
    if (cb3) cb3->setVisible(radarLarge);
    irr::gui::IGUIElement* cb3b = guienv->getRootGUIElement()->getElementFromId(GUI_ID_RADAR_OWN_SHIP_TRAILS_CHECKBOX, true);
    if (cb3b) cb3b->setVisible(radarLarge);
    irr::gui::IGUIElement* cb4 = guienv->getRootGUIElement()->getElementFromId(GUI_ID_RADAR_MMSI_CHECKBOX, true);
    if (cb4) cb4->setVisible(radarLarge);
    irr::gui::IGUIElement* cb5 = guienv->getRootGUIElement()->getElementFromId(GUI_ID_RADAR_BUOY_COLOUR_BOX, true);
    if (cb5) cb5->setVisible(radarLarge);
    irr::gui::IGUIElement* cb6 = guienv->getRootGUIElement()->getElementFromId(GUI_ID_RADAR_SHIP_COLOUR_BOX, true);
    if (cb6) cb6->setVisible(radarLarge);

    //Move gui elements if on largescreen radar

    //Move gui elements if on largescreen radar
    //Heading
    if (radarLarge) {
        headingIndicator->setRelativePosition(radHdgIndicatorPos);
    }
    else if (!showInterface) {
        headingIndicator->setRelativePosition(maxHdgIndicatorPos);
    }
    else {
        headingIndicator->setRelativePosition(stdHdgIndicatorPos);
    }
    //Set position of data display
    if (radarLarge) {
        dataDisplay->setRelativePosition(radDataDisplayPos);
        dataDisplay->setBackgroundColor(radDataDisplayBG);
    }
    else if (!showInterface) {
        dataDisplay->setRelativePosition(altDataDisplayPos);
        dataDisplay->setBackgroundColor(altDataDisplayBG);
    }
    else {
        dataDisplay->setRelativePosition(stdDataDisplayPos);
        dataDisplay->setBackgroundColor(stdDataDisplayBG);
    }
    // --- HIDE CONTROLS LOGIC ---
     // Determine if primary controls should be visible
    bool showPrimary = showPrimaryControls && !radarLarge && !controlsHidden;
    //KYARA: Masquer / Grand radar are DISPLAY controls, not ship controls. A secondary station
//still needs to be able to hide its own 2D overlay and go full-screen radar, so these must
//NOT be gated on controlsHidden.
    bool showDisplayControls = showPrimaryControls && !radarLarge;
    // Apply to all hardware sliders and indicators
    if (portScrollbar) { portScrollbar->setVisible(showPrimary); }
    if (stbdScrollbar) { stbdScrollbar->setVisible(showPrimary && !singleEngine); }
    //KYARA: the helm slider is hidden in the console layout - the rudder dial is the helm now.
    //The element itself stays alive: it still holds the ordered wheel angle and the NFU buttons
    //and the dial both drive the simulation through it.
    if (wheelScrollbar) { wheelScrollbar->setVisible(showPrimary && !instrumentsEnabled); }
    if (bowThrusterScrollbar) { bowThrusterScrollbar->setVisible(showPrimary); }
    if (sternThrusterScrollbar) { sternThrusterScrollbar->setVisible(showPrimary); }
    //(They go with the helm slider, which the console layout does not show; they used to sit hidden
    //under the command bar, and come out from under it when the bar is on the middle of a Surround canvas.)
    if (nonFollowUpPortButton) { nonFollowUpPortButton->setVisible(showPrimary && showInterface && !commandBar); }
    if (nonFollowUpStbdButton) { nonFollowUpStbdButton->setVisible(showPrimary && showInterface && !commandBar); }
    // REMOVED dayNightButton HERE. Added lightingTimeBox and expanded UI logic:
    if (lightingTimeBox) { lightingTimeBox->setVisible(showPrimary); }
    if (binosButton) { binosButton->setVisible(showPrimary); }
    if (bearingButton) { bearingButton->setVisible(showPrimary); }
    if (changeViewButton) { changeViewButton->setVisible(showPrimary); }
    if (magnificationScrollbar) { magnificationScrollbar->setVisible(showPrimary); }
    if (rateofturnScrollbar) { rateofturnScrollbar->setVisible(showPrimary && hasRateOfTurnIndicator && !instrumentsEnabled); }

    // Afficher / Masquer - must still respect which of the pair is the ACTIVE one
    if (hideInterfaceButton) { hideInterfaceButton->setVisible(showDisplayControls && showInterface); }
    if (showInterfaceButton) { showInterfaceButton->setVisible(showDisplayControls && !showInterface); }

    // The rest of the bottom strip
    if (pcLogButton) { pcLogButton->setVisible(showPrimary && showInterface); }
    if (showExtraControlsButton) { showExtraControlsButton->setVisible(showPrimary && showInterface); }
    if (showLinesControlsButton) { showLinesControlsButton->setVisible(showPrimary && showInterface); }
    if (pump1On) { pump1On->setVisible(showPrimary && showInterface); }
    if (pump2On) { pump2On->setVisible(showPrimary && showInterface); }
    if (ackAlarms) { ackAlarms->setVisible(showPrimary && showInterface); }
    if (bigRadarButton) { bigRadarButton->setVisible(showDisplayControls && showInterface); }

    //KYARA: in the normal view the console replaces the heading tape and the text box. In the
    //maximised 3D view (no console) they come back as the small strip along the bottom.
    const bool consoleShown = instrumentsEnabled && showInterface;
    if (instrumentPanel) { instrumentPanel->setVisible(showPrimary && showInterface); }

    // Apply to the heading indicator
    if (headingIndicator) { headingIndicator->setVisible(showPrimary && !consoleShown); }
    if (portText) { portText->setVisible(showPrimary); }
    if (stbdText) { stbdText->setVisible(showPrimary && !singleEngine); }
    if (dataDisplay) { dataDisplay->setVisible(showPrimary && !consoleShown); }

    //If we're in secondary mode, make sure things are hidden if they shouldn't be shown on the secondary screen
    if (controlsHidden) {
        hideInSecondary();
    }

    if (detachConsoleButton) {
        detachConsoleButton->setVisible(showDisplayControls && showInterface);
        if (commandBar) { detachConsoleButton->setText((iconSpace + (consoleDetached ? L"RATTACHER" : L"D\u00C9TACHER")).c_str()); }
        else { detachConsoleButton->setText(consoleDetached ? L"Rattacher" : L"D\u00E9tacher"); }
    }
    //The command bar shows with its keys; with the controls hidden only "Afficher" stays
    if (commandBar) {
        commandBar->setVisible(showDisplayControls);
        commandBar->setCaptionsVisible(showPrimary);
        for (int i = 0; i < PALETTE_KEYS; i++) { if (paletteButton[i]) { paletteButton[i]->setVisible(showDisplayControls); } }
        if (glowScrollbar) { glowScrollbar->setVisible(showDisplayControls); }
        if (weatherButton) { weatherButton->setVisible(showDisplayControls); }
    }
    if (consoleDetached) {
        applyDetachedConsoleVisibility();
    }
}

void GUIMain::hideInSecondary() {
    //Hide user inputs if in secondary mode
    if (weatherButton) { weatherButton->setVisible(false); }
    if (weatherPanel) { weatherPanel->setVisible(false); }
    if (stbdScrollbar) { stbdScrollbar->setVisible(false); }
    if (portScrollbar) { portScrollbar->setVisible(false); }
    if (azimuth1Control) { azimuth1Control->setVisible(false); }
    if (azimuth2Control) { azimuth2Control->setVisible(false); }
    if (azimuth1Master) { azimuth1Master->setVisible(false); }
    if (azimuth2Master) { azimuth2Master->setVisible(false); }
    if (nonFollowUpPortButton) { nonFollowUpPortButton->setVisible(false); }
    if (nonFollowUpStbdButton) { nonFollowUpStbdButton->setVisible(false); }
    // REMOVED dayNightButton HERE. Added new elements to hide in secondary displays:
    if (lightingTimeBox) { lightingTimeBox->setVisible(false); }
    if (binosButton) { binosButton->setVisible(false); }
    if (bearingButton) { bearingButton->setVisible(false); }
    if (changeViewButton) { changeViewButton->setVisible(false); }

    if (pcLogButton) { pcLogButton->setVisible(false); }
    if (pump1On) { pump1On->setVisible(false); }
    if (pump2On) { pump2On->setVisible(false); }
    if (ackAlarms) { ackAlarms->setVisible(false); }


    //kyara
    if (magnificationScrollbar) { magnificationScrollbar->setVisible(false); }
    if (stbdScrollbar) { stbdScrollbar->setVisible(false); }

    if (showLinesControlsButton) { showLinesControlsButton->setVisible(false); }
    if (showExtraControlsButton) { showExtraControlsButton->setVisible(false); }

    // DEE_NOV22 vvvv hide these in secondary displays
    if (azimuthEnginePort) { azimuthEnginePort->setVisible(false); }
    if (azimuthEngineStbd) { azimuthEngineStbd->setVisible(false); }
    if (schottelPort) { schottelPort->setVisible(false); }
    if (schottelStbd) { schottelStbd->setVisible(false); }
    if (azimuthClutchPort) { azimuthClutchPort->setVisible(false); }
    if (azimuthClutchStbd) { azimuthClutchStbd->setVisible(false); }
    if (emergencySteering) { emergencySteering->setVisible(false); }

    // DEE_NOV22 ^^^^

    if (stbdText) { stbdText->setVisible(false); }
    if (portText) { portText->setVisible(false); }
    if (wheelScrollbar) { wheelScrollbar->setVisible(false); }
    if (nonFollowUpPortButton) { nonFollowUpPortButton->setVisible(false); }
    if (nonFollowUpStbdButton) { nonFollowUpStbdButton->setVisible(false); }
    //rateofturnScrollbar->setVisible(false); // hides rate of turn indicator in full screen
    if (bowThrusterScrollbar) { bowThrusterScrollbar->setVisible(false); }
    if (sternThrusterScrollbar) { sternThrusterScrollbar->setVisible(false); }
}

std::wstring GUIMain::f32To1dp(irr::f32 value)
{
    //Convert a floating point value to a wstring, with 1dp
    char tempStr[100];
    snprintf(tempStr, 100, "%.1f", value);
    return std::wstring(tempStr, tempStr + strlen(tempStr));
}

std::wstring GUIMain::f32To2dp(irr::f32 value)
{
    //Convert a floating point value to a wstring, with 2dp
    char tempStr[100];
    snprintf(tempStr, 100, "%.2f", value);
    return std::wstring(tempStr, tempStr + strlen(tempStr));
}

std::wstring GUIMain::f32To3dp(irr::f32 value)
{
    //Convert a floating point value to a wstring, with 3dp
    char tempStr[100];
    snprintf(tempStr, 100, "%.3f", value);
    return std::wstring(tempStr, tempStr + strlen(tempStr));
}

bool GUIMain::manuallyTriggerClick(irr::gui::IGUIButton* button)
{
    irr::SEvent triggerUpdateEvent;
    triggerUpdateEvent.EventType = irr::EET_GUI_EVENT;
    triggerUpdateEvent.GUIEvent.Caller = button;
    triggerUpdateEvent.GUIEvent.Element = 0;
    triggerUpdateEvent.GUIEvent.EventType = irr::gui::EGET_BUTTON_CLICKED;
    return device->postEventFromUser(triggerUpdateEvent);
}

bool GUIMain::manuallyTriggerScroll(irr::gui::IGUIScrollBar* bar)
{
    irr::SEvent triggerUpdateEvent;
    triggerUpdateEvent.EventType = irr::EET_GUI_EVENT;
    triggerUpdateEvent.GUIEvent.Caller = bar;
    triggerUpdateEvent.GUIEvent.Element = 0;
    triggerUpdateEvent.GUIEvent.EventType = irr::gui::EGET_SCROLL_BAR_CHANGED;
    return device->postEventFromUser(triggerUpdateEvent);
}

//kyara: surligne un bouton skinné + gère un libellé dont on peut forcer la couleur (noir sur fond vif).
void GUIMain::setButtonHighlight(irr::gui::IGUIButton* button, irr::video::SColor colour)
{
    if (!button) { return; }
    const irr::s32 HL_ID = 0x6B590001; //calque de couleur
    const irr::s32 LBL_ID = 0x6B590002; //libellé recolorable

    irr::core::rect<irr::s32> br = button->getRelativePosition();
    const bool bright = (colour.getAlpha() > 0);

    // 1) Calque de couleur (créé une fois), click-through pour ne pas bloquer le bouton.
    irr::gui::IGUIRectangle* hl =
        static_cast<irr::gui::IGUIRectangle*>(button->getElementFromId(HL_ID, false));
    if (!hl) {
        hl = new irr::gui::IGUIRectangle(guienv, button,
            irr::core::rect<irr::s32>(0, 0, br.getWidth(), br.getHeight()), false);
        hl->setID(HL_ID);
        hl->setClickThrough(true);
    }
    hl->setFillColour(colour);

    // 2) Libellé recolorable, imbriqué DANS le calque click-through (donc ne vole pas le clic).
    irr::gui::IGUIStaticText* lbl =
        static_cast<irr::gui::IGUIStaticText*>(hl->getElementFromId(LBL_ID, false));
    if (!lbl) {
        lbl = guienv->addStaticText(button->getText(),
            irr::core::rect<irr::s32>(0, 0, br.getWidth(), br.getHeight()),
            false, false, hl, LBL_ID, false);
        lbl->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
        button->setText(L"");   // masque le texte natif (clair uniquement) pour éviter le doublon
    }
    lbl->setOverrideColor(bright ? irr::video::SColor(255, 0, 0, 0)         // noir sur orange/rouge
        : irr::video::SColor(255, 236, 239, 244)); // clair sinon
    lbl->enableOverrideColor(true);
}
//kyara: variante qui rafraîchit aussi le libellé (boutons dont le texte change, ex. Écho)
void GUIMain::setButtonHighlight(irr::gui::IGUIButton* button, irr::video::SColor colour, const wchar_t* labelText)
{
    setButtonHighlight(button, colour);
    if (!button) { return; }
    irr::gui::IGUIElement* hl = button->getElementFromId(0x6B590001, false);
    if (hl) {
        irr::gui::IGUIStaticText* lbl = static_cast<irr::gui::IGUIStaticText*>(hl->getElementFromId(0x6B590002, false));
        if (lbl) { lbl->setText(labelText); }
    }
}
void GUIMain::updateGuiData(GUIData* guiData)
{
    //KYARA: apply any wheel order dragged on the rudder dial. Routed through the wheel scrollbar so
    //the event MyEventReceiver sees is identical to the one the old slider sent.
    if (instrumentPanel && wheelScrollbar) {
        irr::f32 helmDeg = 0;
        if (instrumentPanel->consumeHelmRequest(helmDeg)) {
            wheelScrollbar->setPos(Utilities::round(helmDeg));
            manuallyTriggerScroll(wheelScrollbar);
        }
    }

    guiDistressTimer = guiData->distressTimer;

    //Day / dusk / night: "auto" follows the scene's light, with a margin so it does not flicker
    {
        const irr::u32 L = guiData->lightLevel;
        int m = paletteAutoMode;
        if (m == bridge::MODE_DAY && L < 160) { m = bridge::MODE_DUSK; }
        if (m == bridge::MODE_NIGHT && L >= 80) { m = bridge::MODE_DUSK; }
        if (m == bridge::MODE_DUSK) {
            if (L >= 175) { m = bridge::MODE_DAY; }
            else if (L < 65) { m = bridge::MODE_NIGHT; }
        }
        paletteAutoMode = m;
        if (paletteChoice < 0) { applyPaletteMode(m); }
    }
    if (commandBar && magnificationScrollbar) {
        wchar_t zoomText[16];
        std::swprintf(zoomText, 16, L"\u00D7%.1f", (irr::f32)magnificationScrollbar->getPos() / 10.0f);
        commandBar->setCaptionText(1, zoomText);
    }

    // TODO: Check the scroll bars exist!

    //Hide the 'hint' bars
    if (device->getTimer()->getTime() > 3000) {
        if (clickForEngineText) { clickForEngineText->setVisible(false); }
        if (clickForRudderText) { clickForRudderText->setVisible(false); }
    }

    //Update scroll bars
    hdgScrollbar->setPos(Utilities::round(guiData->hdg));
    spdScrollbar->setPos(Utilities::round(guiData->spd));
    if (portScrollbar) { portScrollbar->setPos(Utilities::round(guiData->portEng * -100)); }//Engine units are +- 1, scale to -+100, inverted as astern is at bottom of scroll bar
    if (stbdScrollbar) { stbdScrollbar->setPos(Utilities::round(guiData->stbdEng * -100)); }

    if (azimuth1Control) {
        // DEE_NOV22 should be read only with the needle showing direction only
//            azimuth1Control->setMag(Utilities::round(guiData->portEng * 100));
        azimuth1Control->setMag(Utilities::round(90));
        azimuth1Control->setPos(Utilities::round(guiData->portAzimuthAngle));
    }

    if (azimuth2Control) {
        // DEE_NOV22 should be read only with the needle showing direction only
//            azimuth2Control->setMag(Utilities::round(guiData->stbdEng * 100));
        azimuth2Control->setMag(Utilities::round(90));
        azimuth2Control->setPos(Utilities::round(guiData->stbdAzimuthAngle));
    }

    // DEE_NOV22 vvvv sets the displayed data in the GUI it does not receive mouse clicks

    if (schottelPort)
    { // refers to the GUI object schottelPort as opposed to the value
        schottelPort->setMag(Utilities::round(90)); // this is because I want a needle of fixed size
        schottelPort->setPos(Utilities::round(guiData->schottelPort));
    } // end if schottelPort 

    if (schottelStbd)
    { // refers to the GUI object schottelPort as opposed to the value
        schottelStbd->setMag(Utilities::round(90)); // this is because I want a needle of fixed size
        schottelStbd->setPos(Utilities::round(guiData->schottelStbd));
    } // end if schottelStbd

    if (azimuthEnginePort)
    { // refers to the GUI object that represents the engine rpm
        azimuthEnginePort->setMag(Utilities::round(90)); // fixed length needle
        azimuthEnginePort->setPos(Utilities::round(guiData->azimuthEnginePort));  // i think this has already been adjusted to be
        // * 360, the engine proportion 0..1 
    }

    if (azimuthEngineStbd)
    { // refers to the GUI object that represents the engine rpm
        azimuthEngineStbd->setMag(Utilities::round(90)); // fixed length needle
        azimuthEngineStbd->setPos(Utilities::round(guiData->azimuthEngineStbd));  // i think this has already been adjusted to be
        // * 360, the engine proportion 0..1 
    }


    if (azimuthClutchPort)
    {
        azimuthClutchPort->setChecked(guiData->azimuthClutchPort);
    }

    if (azimuthClutchStbd)
    {
        azimuthClutchStbd->setChecked(guiData->azimuthClutchStbd);
    }

    // DEE_NOV22 ^^^^


// DEE_NOV22 would prefer to get rid of all this "master" business never seen it on a ship
//           as you can steer with one azi dead ahead, steering input with the other

    if (azimuth1Master) {
        azimuth1Master->setChecked(guiData->azimuth1Master);
    }

    if (azimuth2Master) {
        azimuth2Master->setChecked(guiData->azimuth2Master);
    }

    //rudderScrollbar->setPos(Utilities::round(guiData->rudder));
    if (wheelScrollbar) {
        wheelScrollbar->setSecondary(Utilities::round(guiData->rudder));
        wheelScrollbar->setPos(Utilities::round(guiData->wheel));
    }
    if (bowThrusterScrollbar) { bowThrusterScrollbar->setPos(Utilities::round(guiData->bowThruster * 100)); }
    if (sternThrusterScrollbar) { sternThrusterScrollbar->setPos(Utilities::round(guiData->sternThruster * 100)); }

    // TODO: is the 'round' needed here?
    radarGainScrollbar->setPos(Utilities::round(guiData->radarGain));
    radarClutterScrollbar->setPos(Utilities::round(guiData->radarClutter));
    radarRainScrollbar->setPos(Utilities::round(guiData->radarRain));

    radarGainScrollbar2->setPos(Utilities::round(guiData->radarGain));
    radarClutterScrollbar2->setPos(Utilities::round(guiData->radarClutter));
    radarRainScrollbar2->setPos(Utilities::round(guiData->radarRain));

    weatherScrollbar->setPos(Utilities::round(guiData->weather * 10.0)); //(Weather scroll bar is 0-SIM_MAX_WEATHER*10)
    rainScrollbar->setPos(Utilities::round(guiData->rain * 10.0)); //(Rain scroll bar is 0-100, rain is 0-10)
    visibilityScrollbar->setPos(Utilities::round(guiData->visibility * 10.0)); //Visibility scroll bar is 0-100, visibility is near 0 to 10 Nm
    if (motionScaleScrollbar) { motionScaleScrollbar->setPos(Utilities::round(guiData->motionScale * 100.0f)); } // KYARA HOULE

    windDirectionScrollbar->setPos(Utilities::round(guiData->windDirection));
    windSpeedScrollbar->setPos(Utilities::round(guiData->windSpeed));

    streamDirectionScrollbar->setPos(Utilities::round(guiData->streamDirection));
    streamSpeedScrollbar->setPos(Utilities::round(guiData->streamSpeed));

    //KYARA: live readouts, in the units a mariner actually uses. Note the two OPPOSITE conventions,
    //which the tooltips spell out: wind is named for the direction it blows FROM, current for the
    //direction it sets TOWARDS. Getting that backwards is a classic trainee error - the sim should
    //be teaching it, not blurring it.
    {
        wchar_t buf[64];

        swprintf(buf, 64, L"%.1f (F%d)", guiData->weather, (int)(guiData->weather + 0.5f));
        weatherValue->setText(buf);

        // KYARA HOULE
        if (motionScaleValue) {
            swprintf(buf, 64, L"%d %%", (int)(guiData->motionScale * 100.0f + 0.5f));
            motionScaleValue->setText(buf);
        }
        if (swellInfoText) {
            wchar_t sbuf[128];
            swprintf(sbuf, 128, L"Mer %.1f m  |  Houle %.1f m  %.0f s  du %ls\u00B0",
                guiData->seaHs, guiData->swellHs, guiData->swellTp, degrees3(guiData->swellDirFrom).c_str());
            swellInfoText->setText(sbuf);
        }

        swprintf(buf, 64, L"%.1f / 10", guiData->rain);
        rainValue->setText(buf);

        swprintf(buf, 64, L"%.1f NM", guiData->visibility);
        visibilityValue->setText(buf);

        //Wind: FROM. e.g. "270 W"
        swprintf(buf, 64, L"%ls\u00B0 %ls",
            degrees3(guiData->windDirection).c_str(),
            compass16(guiData->windDirection).c_str());
        windDirectionValue->setText(buf);

        swprintf(buf, 64, L"%d kt (F%d)",
            (int)(guiData->windSpeed + 0.5f), beaufort(guiData->windSpeed));
        windSpeedValue->setText(buf);

        //Current: TOWARDS (set)
        swprintf(buf, 64, L"%ls\u00B0 %ls",
            degrees3(guiData->streamDirection).c_str(),
            compass16(guiData->streamDirection).c_str());
        streamDirectionValue->setText(buf);

        swprintf(buf, 64, L"%.1f kt", guiData->streamSpeed);
        streamSpeedValue->setText(buf);
    }


    // DEE vvvvv  this should display the rate of turn data on the screen
    // DEE        since internalrate of turn is in rads per second then for deg per min x 3438
    rateofturnScrollbar->setPos(Utilities::round(3438 * guiData->RateOfTurn));
    // DEE ^^^^

            //Update text display data
    guiLat = guiData->lat;
    guiRadarOffsetX = guiData->radarOffsetX; //kyara
    guiRadarOffsetY = guiData->radarOffsetY;
    guiSpd = guiData->spd; //kyara
    guiLightningFlash = guiData->lightningFlash;
    guiDistressActive = guiData->distressActive;   // Inc 3 (comms)
    guiDistressTimer = guiData->distressTimer;
    guiMaydayLines = guiData->maydayLines;
    guiCommsLog = guiData->commsLog;
    void drawCommsOverlay();                       // Inc 3 (comms)
    bool guiDistressActive = false;                // Inc 3 (comms)
    std::vector<std::wstring> guiMaydayLines;      // Inc 3 (comms)
    std::vector<std::wstring> guiCommsLog;         // Inc 3 (comms)
    guiLong = guiData->longitude;
    guiCursorLat = guiData->cursorLat;   //kyara
    guiCursorLong = guiData->cursorLong;  //kyara
    guiHeading = guiData->hdg; //Heading in degrees
    headingIndicator->setHeading(guiHeading);
    viewHdg = guiData->viewAngle;
    viewElev = guiData->viewElevationAngle;
    while (viewHdg >= 360) { viewHdg -= 360; }
    while (viewHdg < 0) { viewHdg += 360; }
    guiSpeed = guiData->spd * MPS_TO_KTS; //Speed in knots
    //kyara: instrument console inputs, converted once here into the units the dials show
    guiCOG = guiData->cog;
    guiSOGKts = guiData->sog * MPS_TO_KTS;
    guiRudder = guiData->rudder;
    guiWheel = guiData->wheel;
    guiRateOfTurnDegMin = guiData->RateOfTurn * irr::core::RADTODEG * 60.0f; //rad/s -> deg/min
    guiPortRPM = guiData->portRPM;
    guiStbdRPM = guiData->stbdRPM;
    guiWindDirection = guiData->windDirectionNow; //the dial shows the wind as it blows, gusts and all
    guiWindSpeed = guiData->windSpeedNow; //already in knots
    if (guiData->maxRPM > 0 && instrumentPanel && guiMaxRPM != guiData->maxRPM) {
        //The ship's MaxRevs only arrives with the first update, so set the tachometer scale once.
        guiMaxRPM = guiData->maxRPM;
        instrumentPanel->setExtraInstruments(instrumentExtraRPM, instrumentExtraWind, guiMaxRPM, singleEngine);
    }
    guiDepth = guiData->depth;
    guiRadarOn = guiData->radarOn;
    guiRadarRangeNm = guiData->radarRangeNm;
    guiRadarRangeRingBrightness = guiData->radarRangeRingBrightness; //kyara
    guiTime = guiData->currentTime;
    guiPaused = guiData->paused;
    guiCollided = guiData->collided;
    guiProxyAlarmMuted = guiData->proxyAlarmMuted;   //KYARA
    // DEE Feb 23 vvvv height of tide
    guiTideHeight = guiData->tideHeight;
    guiPitch = guiData->pitch; // KYARA HOULE
    guiRoll = guiData->roll;   // KYARA HOULE


    radarHeadUp = guiData->headUp;

    //update EBL Data
   //update EBL/VRM Data (kyara: two sets)
    for (int i = 0; i < 2; i++) {
        this->guiRadarEBLBrg[i] = guiData->guiRadarEBLBrg[i];
        if (radarHeadUp) {
            this->guiRadarEBLBrg[i] -= guiHeading;
        }
        this->guiRadarVRMNm[i] = guiData->guiRadarVRMNm[i];
    }
    this->guiRadarActiveEBL = guiData->guiRadarActiveEBL;
    this->guiRadarActiveVRM = guiData->guiRadarActiveVRM;
    this->guiRadarGuardAlarmMode = guiData->guiRadarGuardAlarmMode;

    this->guiRadarStabilised = guiData->radarStabilised; //kyara

    // ---- kyara : surlignage des boutons de contrôle radar --------------------
    {
        const irr::video::SColor HL_NONE(0, 0, 0, 0);   // transparent = aspect normal
        const irr::video::SColor HL_RED(150, 220, 40, 40);  // IN
        const irr::video::SColor HL_ORANGE(150, 255, 140, 0);   // OUT / sélectionné

        // Alarme de garde : OFF -> normal, IN -> rouge, OUT -> orange
        irr::video::SColor alarmCol = (guiRadarGuardAlarmMode == 1) ? HL_RED
            : (guiRadarGuardAlarmMode == 2) ? HL_ORANGE
            : HL_NONE;
        setButtonHighlight(guardAlarmButton2, alarmCol);

        // Sélecteur EBL : surligné quand EBL2 est l'actif
        irr::video::SColor eblCol = (guiRadarActiveEBL == 1) ? HL_ORANGE : HL_NONE;
        setButtonHighlight(eblSelectButton, eblCol);
        setButtonHighlight(eblSelectButton2, eblCol);

        // Sélecteur VRM : surligné quand VRM2 est l'actif
        irr::video::SColor vrmCol = (guiRadarActiveVRM == 1) ? HL_ORANGE : HL_NONE;
        setButtonHighlight(vrmSelectButton, vrmCol);
        setButtonHighlight(vrmSelectButton2, vrmCol);

        // Orientation (exclusive) : 0=Nord, 1=Route(stabilisé), 2=Cap
        int orientation = (!radarHeadUp) ? 0 : (guiRadarStabilised ? 1 : 2);
        setButtonHighlight(northUpButton, orientation == 0 ? HL_ORANGE : HL_NONE);
        setButtonHighlight(courseUpButton, orientation == 1 ? HL_ORANGE : HL_NONE);
        setButtonHighlight(headUpButton, orientation == 2 ? HL_ORANGE : HL_NONE);
        setButtonHighlight(northUpButton2, orientation == 0 ? HL_ORANGE : HL_NONE);
        setButtonHighlight(courseUpButton2, orientation == 1 ? HL_ORANGE : HL_NONE);
        setButtonHighlight(headUpButton2, orientation == 2 ? HL_ORANGE : HL_NONE);
        // Anneaux de portée : clair=normal, faible=orange, off=rouge (kyara)
        irr::video::SColor ringCol = (guiData->guiRadarRingLevel == 2) ? HL_RED
            : (guiData->guiRadarRingLevel == 1) ? HL_ORANGE
            : HL_NONE;
        setButtonHighlight(rangeRingsButton2, ringCol);
        // Échostretch (kyara): Echo 1/2 orange, Echo 3 rouge, OFF normal
        {
            int es = guiData->guiRadarEchoStretch;
            irr::video::SColor esCol = (es == 3) ? HL_RED : (es >= 1) ? HL_ORANGE : HL_NONE;
            const wchar_t* esLbl = (es == 1) ? L"Echo 1" : (es == 2) ? L"Echo 2" : (es == 3) ? L"Echo 3" : L"\u00C9cho";
            setButtonHighlight(echoStretchButton2, esCol, esLbl);
            // Décentrage actif -> bouton orange (kyara)
            setButtonHighlight(offCentreButton2, (fabs(guiRadarOffsetX) > 0.001f || fabs(guiRadarOffsetY) > 0.001f) ? HL_ORANGE : HL_NONE);
        }

    }
    // --------------------------------------------------------------------------r

    //update cursor data
    this->guiRadarCursorBrg = guiData->guiRadarCursorBrg;
    if (radarHeadUp) {
        this->guiRadarCursorBrg -= guiHeading;
    }
    this->guiRadarCursorRangeNm = guiData->guiRadarCursorRangeNm;

    //Update ARPA data
    arpaContactStates = guiData->arpaContactStates;
    setARPAList(guiData->arpaListSelection);

    //Update rudder pump indicators
    if (guiData->pump1On == true) {
        pump1On->setBackgroundColor(irr::video::SColor(255, 0, 128, 0));
    }
    else {
        pump1On->setBackgroundColor(irr::video::SColor(255, 128, 0, 0));
    }
    if (guiData->pump2On == true) {
        pump2On->setBackgroundColor(irr::video::SColor(255, 0, 128, 0));
    }
    else {
        pump2On->setBackgroundColor(irr::video::SColor(255, 128, 0, 0));
    }
    // Light text on the green/red lamps, whatever the palette
    pump1On->setOverrideColor(irr::video::SColor(255, 235, 240, 235));
    pump2On->setOverrideColor(irr::video::SColor(255, 235, 240, 235));
}

void GUIMain::showLogWindow()
{

    irr::gui::IGUIWindow* logWindow = guienv->addWindow(irr::core::rect<irr::s32>(0.01 * su, 0.01 * sh, 0.99 * su, 0.99 * sh));
    irr::gui::IGUIListBox* logText = guienv->addListBox(irr::core::rect<irr::s32>(0.03 * su, 0.05 * sh, 0.95 * su, 0.95 * sh), logWindow);

    if (logWindow && logText && logMessages) {

        logText->setDrawBackground(true);
        logText->clear();

        for (unsigned int i = 0; i < logMessages->size(); i++) {
            std::string logTextString = logMessages->at(i);
            logText->addItem(irr::core::stringw(logTextString.c_str()).c_str());
        }
    }

}

void GUIMain::drawGUI()
{
    //CHANGES paint the engine panels behind the scrollbars -KYARA
    irr::video::IVideoDriver* driver = device->getVideoDriver();

    // --- DEEP GRAY-BLUE UI BACKGROUND ---
// Fill the empty areas with a deep green, but LEAVE A HOLE for the radar so it remains visible!
    irr::video::SColor uiBgColor(255, 0, 0, 0); // deep green

    if (radarLarge) {
        // Large radar mode: paint 4 boxes around the radar area
        irr::core::rect<irr::s32> r = radarLargeRect;
        driver->draw2DRectangle(uiBgColor, irr::core::rect<irr::s32>(0, 0, r.UpperLeftCorner.X, sh)); // Left side
        driver->draw2DRectangle(uiBgColor, irr::core::rect<irr::s32>(r.LowerRightCorner.X, 0, su, sh)); // Right side
        driver->draw2DRectangle(uiBgColor, irr::core::rect<irr::s32>(r.UpperLeftCorner.X, 0, r.LowerRightCorner.X, r.UpperLeftCorner.Y)); // Top
        driver->draw2DRectangle(uiBgColor, irr::core::rect<irr::s32>(r.UpperLeftCorner.X, r.LowerRightCorner.Y, r.LowerRightCorner.X, sh)); // Bottom

        // --- NEW: Fix the gray camera bleed ---

        //RADAR BOX BACKGROUND 2----------------------------
        // Kyara: this fills the gaps between the radar mesh and the edges of the radar panel.
        // It was deep blue (0,18,105), which showed as a blue band above/below the scope even
        // though the radar bitmap's own surround is black - so the panel looked two-tone.
        // Keep it matching the radar surround colour in RadarCalculation::load (black).
        irr::video::SColor deepGreen(255, 0, 0, 0);
        //--------------------------------------------------
        irr::s32 meshLeft = largeRadarScreenCentreX - largeRadarScreenRadius;
        irr::s32 meshRight = largeRadarScreenCentreX + largeRadarScreenRadius;
        irr::s32 meshTop = largeRadarScreenCentreY - largeRadarScreenRadius;
        irr::s32 meshBottom = largeRadarScreenCentreY + largeRadarScreenRadius;

        // Fill gap to the left of the radar mesh
        if (meshLeft > r.UpperLeftCorner.X)
            driver->draw2DRectangle(deepGreen, irr::core::rect<irr::s32>(r.UpperLeftCorner.X, r.UpperLeftCorner.Y, meshLeft, r.LowerRightCorner.Y));

        // Fill gap to the right of the mesh (This covers the UI controls background!)
        if (meshRight < r.LowerRightCorner.X)
            driver->draw2DRectangle(deepGreen, irr::core::rect<irr::s32>(meshRight, r.UpperLeftCorner.Y, r.LowerRightCorner.X, r.LowerRightCorner.Y));

        // Fill gap above the mesh
        if (meshTop > r.UpperLeftCorner.Y)
            driver->draw2DRectangle(deepGreen, irr::core::rect<irr::s32>(meshLeft, r.UpperLeftCorner.Y, meshRight, meshTop));

        // Fill gap below the mesh
        if (meshBottom < r.LowerRightCorner.Y)
            driver->draw2DRectangle(deepGreen, irr::core::rect<irr::s32>(meshLeft, meshBottom, meshRight, r.LowerRightCorner.Y));
    }
    else if (showInterface && !getSmallRadarEnabled()) {
        //KYARA: console layout - no small radar, so paint the whole lower band. (Without this the
        //old radar hole shows the scene clear colour, since that viewport is no longer rendered.)
        //With the console in its own window the bridge view fills the screen: nothing to paint.
        if (!consoleDetached) {
            //(Surround with three camera columns: only under the middle screen, the side ones show the view)
            const irr::s32 bandL = sideScreensFree ? consoleArea.UpperLeftCorner.X : 0;
            const irr::s32 bandR = sideScreensFree ? consoleArea.LowerRightCorner.X : (irr::s32)su;
            driver->draw2DRectangle(bridge::palette().band, irr::core::rect<irr::s32>(bandL, (irr::s32)(sh * viewProportion3D()), bandR, sh));
        }
    }
    else if (showInterface) {
        // Normal view mode: paint boxes around the small radar area on the bottom panel
        irr::core::rect<irr::s32> sr = getSmallRadarRect();
        irr::s32 uiTop = sr.UpperLeftCorner.Y;

        driver->draw2DRectangle(uiBgColor, irr::core::rect<irr::s32>(0, uiTop, sr.UpperLeftCorner.X, sh)); // Left of radar

        if (sr.LowerRightCorner.X < (irr::s32)su) {
            driver->draw2DRectangle(uiBgColor, irr::core::rect<irr::s32>(sr.LowerRightCorner.X, uiTop, su, sh)); // Right of radar
            // Décentrage actif -> bouton orange (kyara)

        }

    }
    // -----------------------------------
    // Only draw the colors if the scrollbar is actually visible!
    if (!instrumentsEnabled && portScrollbar && portScrollbar->isVisible()) { //kyara: the new levers colour themselves
        auto r = portScrollbar->getAbsolutePosition();
        int midY = (r.UpperLeftCorner.Y + r.LowerRightCorner.Y) / 2;

        // Set colors to be very faint (Alpha 45 out of 255 is ~18% opacity)
        irr::video::SColor faintTop = engineTopColor;
        faintTop.setAlpha(45);
        irr::video::SColor faintBottom = engineBottomColor;
        faintBottom.setAlpha(45);

        driver->draw2DRectangle(faintTop, irr::core::rect<irr::s32>(r.UpperLeftCorner.X, r.UpperLeftCorner.Y, r.LowerRightCorner.X, midY));
        driver->draw2DRectangle(faintBottom, irr::core::rect<irr::s32>(r.UpperLeftCorner.X, midY, r.LowerRightCorner.X, r.LowerRightCorner.Y));
    }

    if (!instrumentsEnabled && stbdScrollbar && stbdScrollbar->isVisible()) {
        auto r = stbdScrollbar->getAbsolutePosition();
        int midY = (r.UpperLeftCorner.Y + r.LowerRightCorner.Y) / 2;

        irr::video::SColor faintTop = engineTopColor;
        faintTop.setAlpha(45);
        irr::video::SColor faintBottom = engineBottomColor;
        faintBottom.setAlpha(45);


        driver->draw2DRectangle(faintTop, irr::core::rect<irr::s32>(r.UpperLeftCorner.X, r.UpperLeftCorner.Y, r.LowerRightCorner.X, midY));
        driver->draw2DRectangle(faintBottom, irr::core::rect<irr::s32>(r.UpperLeftCorner.X, midY, r.LowerRightCorner.X, r.LowerRightCorner.Y));
    }
    //Remove big paused button when the simulation is started.
    if (pausedButton) {
        if (!guiPaused) {
            pausedButton->remove();
            pausedButton = 0;
        }
    }

    //Convert lat/long into a readable format
    wchar_t eastWest;
    wchar_t northSouth;
    if (guiLat >= 0) {
        northSouth = 'N';
    }
    else {
        northSouth = 'S';
    }
    if (guiLong >= 0) {
        eastWest = 'E';
    }
    else {
        eastWest = 'W';
    }
    irr::f32 displayLat = fabs(guiLat);
    irr::f32 displayLong = fabs(guiLong);

    irr::f32 latMinutes = (displayLat - (int)displayLat) * 60;
    irr::f32 lonMinutes = (displayLong - (int)displayLong) * 60;
    irr::u8 latDegrees = (int)displayLat;
    irr::u8 lonDegrees = (int)displayLong;

    //update heading display element
    irr::core::stringw displayText;

    displayText.append(language->translate("spd"));
    displayText.append(f32To1dp(guiSpeed).c_str());
    displayText.append(L" ");
    displayText.append(language->translate("kts"));
    displayText.append(L" ");

    if (showInterface) { //Only show speed in minimal 2d interface
        displayText.append(L"\n");
        if (hasDepthSounder) {
            displayText.append(language->translate("depth"));
            if (guiDepth <= maxSounderDepth) {
                displayText.append(f32To1dp(guiDepth).c_str());
            }
            else {
                displayText.append(L"-");
            }
            displayText.append(L" m \n");
        }

        displayText.append(irr::core::stringw(guiTime.c_str()));
        displayText.append(L"\n");

        if (hasGPS) {
            displayText.append(language->translate("pos"));
            displayText.append(irr::core::stringw(latDegrees));
            displayText.append(language->translate("deg"));
            displayText.append(f32To3dp(latMinutes).c_str());
            displayText.append(language->translate("minSymbol"));
            displayText.append(northSouth);
            displayText.append(L" ");

            displayText.append(irr::core::stringw(lonDegrees));
            displayText.append(language->translate("deg"));
            displayText.append(f32To3dp(lonMinutes).c_str());
            displayText.append(language->translate("minSymbol"));
            displayText.append(eastWest);
            displayText.append(L"\n");
        }

        displayText.append(language->translate("fps"));
        displayText.append(irr::core::stringw(mainWindowFPS()).c_str());
        displayText.append(L"\n");

        if (showTideHeight) {
            // DEE FEB 23 vvv add height of tide to the display
            displayText.append(language->translate("hot"));
            displayText.append(f32To1dp(guiTideHeight).c_str());
            displayText.append(L"\n");
            // DEE FEB 23 ^^^
        }

    }
    if (guiPaused) {
        displayText.append(language->translate("paused"));
        displayText.append(L"\n");
    }
    dataDisplay->setText(displayText.c_str());

    //add radar text (reuse the displayText)
    irr::f32 displayEBLBearing[2];
    irr::f32 displayCursorBearing = guiRadarCursorBrg;
    for (int i = 0; i < 2; i++) {
        displayEBLBearing[i] = guiRadarEBLBrg[i];
        if (radarHeadUp) {
            displayEBLBearing[i] += guiHeading;
        }
        while (displayEBLBearing[i] >= 360) { displayEBLBearing[i] -= 360; }
        while (displayEBLBearing[i] < 0) { displayEBLBearing[i] += 360; }
    }
    if (radarHeadUp) {
        displayCursorBearing += guiHeading;
    }

    displayText = language->translate("range");
    displayText.append(f32To1dp(guiRadarRangeNm).c_str());
    displayText.append(language->translate("nm"));
    displayText.append(L"\n");

    displayText.append((guiRadarActiveVRM == 0) ? L">VRM1 " : L" VRM1 ");
    displayText.append(f32To2dp(guiRadarVRMNm[0]).c_str());
    displayText.append(language->translate("nm"));
    displayText.append((guiRadarActiveVRM == 1) ? L" >VRM2 " : L"  VRM2 ");
    displayText.append(f32To2dp(guiRadarVRMNm[1]).c_str());
    displayText.append(language->translate("nm"));
    if (guiRadarCursorRangeNm > 0) {
        displayText.append(" ");
        displayText.append(language->translate("cursor"));
        displayText.append(f32To2dp(guiRadarCursorRangeNm).c_str());
        displayText.append(language->translate("nm"));
    }
    displayText.append(L"\n");

    displayText.append((guiRadarActiveEBL == 0) ? L">EBL1 " : L" EBL1 ");
    displayText.append(f32To1dp(displayEBLBearing[0]).c_str());
    displayText.append(language->translate("deg"));
    displayText.append((guiRadarActiveEBL == 1) ? L" >EBL2 " : L"  EBL2 ");
    displayText.append(f32To1dp(displayEBLBearing[1]).c_str());
    displayText.append(language->translate("deg"));
    if (guiRadarCursorRangeNm > 0) {
        displayText.append(" ");
        displayText.append(language->translate("cursor"));
        displayText.append(f32To2dp(displayCursorBearing).c_str());
        displayText.append(language->translate("deg"));
    }
    displayText.append(L"\nAlarme : ");
    displayText.append((guiRadarGuardAlarmMode == 0) ? L"OFF" : (guiRadarGuardAlarmMode == 1) ? L"IN" : L"OUT");
    radarText->setText(displayText.c_str());
    //kyara: grand radar - chaîne SANS curseur inline (le petit radar reste inchangé),
 //le curseur s'affiche sur ses deux lignes colorées dédiées.

    {
        irr::core::stringw t2 = language->translate("range");
        t2.append(f32To1dp(guiRadarRangeNm).c_str());
        t2.append(language->translate("nm"));
        t2.append(L"\n");
        t2.append((guiRadarActiveVRM == 0) ? L">VRM1 " : L" VRM1 ");
        t2.append(f32To2dp(guiRadarVRMNm[0]).c_str());
        t2.append(language->translate("nm"));
        t2.append((guiRadarActiveVRM == 1) ? L" >VRM2 " : L"  VRM2 ");
        t2.append(f32To2dp(guiRadarVRMNm[1]).c_str());
        t2.append(language->translate("nm"));
        t2.append(L"\n");
        t2.append((guiRadarActiveEBL == 0) ? L">EBL1 " : L" EBL1 ");
        t2.append(f32To1dp(displayEBLBearing[0]).c_str());
        t2.append(language->translate("deg"));
        t2.append((guiRadarActiveEBL == 1) ? L" >EBL2 " : L"  EBL2 ");
        t2.append(f32To1dp(displayEBLBearing[1]).c_str());
        t2.append(language->translate("deg"));
        t2.append(L"\nAlarme : ");
        t2.append((guiRadarGuardAlarmMode == 0) ? L"OFF" : (guiRadarGuardAlarmMode == 1) ? L"IN" : L"OUT");
        radarText2->setText(t2.c_str());
        //kyara: bloc données style Furuno (navire + curseur) dans radarPosText2
        {
            wchar_t ns[2] = { northSouth, 0 };
            wchar_t ew[2] = { eastWest, 0 };
            //Own ship data the way a radar shows it: heading, position, then course and speed over ground.
            wchar_t hdgBuf[16], cogBuf[16];
            swprintf(hdgBuf, 16, L"%05.1f\u00B0", guiHeading < 0 ? guiHeading + 360.0f : guiHeading);
            swprintf(cogBuf, 16, L"%03d\u00B0", ((int)(guiCOG + 0.5f) % 360 + 360) % 360);
            irr::core::stringw s = L"OWN SHIP\n";
            s.append(L"HDG  "); s.append(hdgBuf); s.append(L"\n");
            s.append(L"LAT  "); s.append(irr::core::stringw((irr::s32)latDegrees)); s.append(L"\u00B0");
            s.append(f32To3dp(latMinutes).c_str()); s.append(L"'"); s.append(ns); s.append(L"\n");
            s.append(L"LON  "); s.append(irr::core::stringw((irr::s32)lonDegrees)); s.append(L"\u00B0");
            s.append(f32To3dp(lonMinutes).c_str()); s.append(L"'"); s.append(ew); s.append(L"\n");
            s.append(L"COG  "); s.append(cogBuf); s.append(L"   SOG  "); s.append(f32To1dp(guiSOGKts).c_str()); s.append(L" kn");

            if (guiRadarCursorRangeNm > 0) {
                wchar_t cns[2] = { (guiCursorLat >= 0) ? L'N' : L'S', 0 };
                wchar_t cew[2] = { (guiCursorLong >= 0) ? L'E' : L'W', 0 };
                irr::f32 clat = fabs(guiCursorLat);
                irr::f32 clon = fabs(guiCursorLong);
                s.append(L"\n\nCURSOR\n");
                s.append(L"LAT  "); s.append(irr::core::stringw((irr::s32)clat)); s.append(L"\u00B0");
                s.append(f32To3dp((clat - (int)clat) * 60.0f).c_str()); s.append(L"'"); s.append(cns); s.append(L"\n");
                s.append(L"LON  "); s.append(irr::core::stringw((irr::s32)clon)); s.append(L"\u00B0");
                s.append(f32To3dp((clon - (int)clon) * 60.0f).c_str()); s.append(L"'"); s.append(cew); s.append(L"\n");
                s.append(irr::core::stringw((irr::s32)(displayCursorBearing + 0.5f))); s.append(L"\u00B0  ");
                s.append(f32To2dp(guiRadarCursorRangeNm).c_str()); s.append(L" NM");
            }
            radarPosText2->setText(s.c_str());
        }
    }
    //Use guiCPAs and guiTCPAs to display ARPA data
    //Todo: Store current position and reset here
    irr::s32 selectedItem = arpaList->getSelected();
    irr::s32 selectedItem2 = arpaList2->getSelected();
    irr::s32 selectedPosition = 0;
    irr::s32 selectedPosition2 = 0;
    if (arpaList->getVerticalScrollBar()) { selectedPosition = arpaList->getVerticalScrollBar()->getPos(); }
    if (arpaList2->getVerticalScrollBar()) { selectedPosition2 = arpaList2->getVerticalScrollBar()->getPos(); }
    arpaList->clear();
    arpaList2->clear();
    arpaText->clear();
    arpaText2->clear();

    //if (guiCPAs.size() == guiTCPAs.size() && guiCPAs.size() == guiARPAspeeds.size() && guiCPAs.size() == guiARPAheadings.size()) {
    for (unsigned int i = 0; i < arpaContactStates.size(); i++) {

        //Convert TCPA from decimal minutes into minutes and seconds.
        //TODO: Filter list based on risk?

        // If stationary, show placeholder only
        if (arpaContactStates.at(i).stationary) {
            displayText = language->translate("untracked");
            arpaList->addItem(displayText.c_str());
            arpaList2->addItem(displayText.c_str());
            continue;
        }

        displayText = L"";

        irr::f32 tcpa = arpaContactStates.at(i).tcpa;
        irr::f32 cpa = arpaContactStates.at(i).cpa;
        irr::u32 arpahdg = round(arpaContactStates.at(i).absHeading);
        irr::u32 arpaspd = round(arpaContactStates.at(i).speed);

        irr::u32 tcpaMins = floor(tcpa);
        irr::u32 tcpaSecs = floor(60 * (tcpa - tcpaMins));

        irr::core::stringw tcpaDisplayMins = irr::core::stringw(tcpaMins);
        if (tcpaDisplayMins.size() == 1) {
            irr::core::stringw zeroPadded = L"0";
            zeroPadded.append(tcpaDisplayMins);
            tcpaDisplayMins = zeroPadded;
        }

        irr::core::stringw tcpaDisplaySecs = irr::core::stringw(tcpaSecs);
        if (tcpaDisplaySecs.size() == 1) {
            irr::core::stringw zeroPadded = L"0";
            zeroPadded.append(tcpaDisplaySecs);
            tcpaDisplaySecs = zeroPadded;
        }

        if (arpaContactStates.at(i).contactType == CONTACT_MANUAL) {
            displayText.append(language->translate("manualContact"));
        }
        else {
            displayText.append(language->translate("arpaContact"));
        }
        displayText.append(L" ");
        displayText.append(irr::core::stringw(i + 1)); //Contact ID (1,2,...)
        displayText.append(L":");

        if (!arpaContactStates.at(i).isBuoy && arpaContactStates.at(i).mmsi > 0) {
            displayText.append(L" MMSI:");
            displayText.append(irr::core::stringw(arpaContactStates.at(i).mmsi));
        }

        arpaList->addItem(displayText.c_str());
        arpaList2->addItem(displayText.c_str());

        if (i == selectedItem || i == selectedItem2) {
            //Show arpa details

            if (arpaContactStates.at(i).range == 0) {
                // Exactly 0 means untracked
                displayText = language->translate("untracked");
                if (i == selectedItem) {
                    arpaText->addItem(displayText.c_str());
                }
                if (i == selectedItem2) {
                    arpaText2->addItem(displayText.c_str());

                }
            }
            else {
                // Show MMSI as the first detail line for ship contacts (when a target is clicked)
                if (!arpaContactStates.at(i).isBuoy && arpaContactStates.at(i).mmsi > 0) {
                    displayText = L"MMSI:";
                    displayText.append(irr::core::stringw(arpaContactStates.at(i).mmsi));
                    if (i == selectedItem) { arpaText->addItem(displayText.c_str()); }
                    if (i == selectedItem2) { arpaText2->addItem(displayText.c_str()); }
                }
                //CPA
                displayText = L"";
                displayText.append(language->translate("cpa"));
                displayText.append(L":");
                displayText.append(f32To2dp(cpa).c_str());
                displayText.append(language->translate("nm"));
                //Add to the correct box
                if (i == selectedItem) {
                    arpaText->addItem(displayText.c_str());
                }
                if (i == selectedItem2) {
                    arpaText2->addItem(displayText.c_str());
                }

                //TCPA
                displayText = L"";
                displayText.append(language->translate("tcpa"));
                displayText.append(L":");
                if (tcpa >= 0) {
                    displayText.append(tcpaDisplayMins);
                    displayText.append(L":");
                    displayText.append(tcpaDisplaySecs);
                }
                else {
                    displayText.append(L" ");
                    displayText.append(language->translate("past"));
                }
                //Add to the correct box
                if (i == selectedItem) {
                    arpaText->addItem(displayText.c_str());
                }
                if (i == selectedItem2) {
                    arpaText2->addItem(displayText.c_str());
                }

                //Heading and speed
                //Pad heading to three decimals
                irr::core::stringw headingText = irr::core::stringw(arpahdg);
                if (headingText.size() == 1) {
                    irr::core::stringw zeroPadded = L"00";
                    zeroPadded.append(headingText);
                    headingText = zeroPadded;
                }
                else if (headingText.size() == 2) {
                    irr::core::stringw zeroPadded = L"0";
                    zeroPadded.append(headingText);
                    headingText = zeroPadded;
                }
                displayText = L"";
                displayText.append(headingText);
                displayText.append(language->translate("deg"));
                displayText.append(L" ");
                displayText.append(irr::core::stringw(arpaspd));
                displayText.append(L" kts");
                //Add to the correct box
                if (i == selectedItem) {
                    arpaText->addItem(displayText.c_str());
                }
                if (i == selectedItem2) {
                    arpaText2->addItem(displayText.c_str());
                }
            }

        }

    }
    //}
    if (selectedItem > -1 && (irr::s32)arpaList->getItemCount() > selectedItem) {
        arpaList->setSelected(selectedItem);
    }
    if (selectedItem2 > -1 && (irr::s32)arpaList2->getItemCount() > selectedItem2) {
        arpaList2->setSelected(selectedItem2);
    }
    if (arpaList->getVerticalScrollBar()) {
        arpaList->getVerticalScrollBar()->setPos(selectedPosition);
    }
    if (arpaList2->getVerticalScrollBar()) {
        arpaList2->getVerticalScrollBar()->setPos(selectedPosition2);
    }

    //kyara: onglet AIS — rempli une fois par frame (contact sélectionné), hors de la boucle
    aisText2->clear();
    if (aisDataMode && selectedItem2 >= 0 && selectedItem2 < (irr::s32)arpaContactStates.size()) {
        const ARPAEstimatedState& c = arpaContactStates.at(selectedItem2);
        irr::core::stringw l;
        l = L"MMSI : "; l.append(irr::core::stringw(c.mmsi)); aisText2->addItem(l.c_str());
        l = L"SOG  : "; l.append(f32To1dp(c.speed).c_str()); l.append(L" kts"); aisText2->addItem(l.c_str());
        l = L"COG  : "; l.append(irr::core::stringw((irr::s32)round(c.absHeading))); l.append(L"\u00B0"); aisText2->addItem(l.c_str());
        l = L"Dist : "; l.append(f32To2dp(c.range).c_str()); l.append(L" NM"); aisText2->addItem(l.c_str());
        l = L"Brg  : "; l.append(irr::core::stringw((irr::s32)round(c.bearing))); l.append(L"\u00B0"); aisText2->addItem(l.c_str());
    }
    arpaText2->setVisible(!aisDataMode);
    aisText2->setVisible(aisDataMode);
    setButtonHighlight(arpaTabButton2, aisDataMode ? irr::video::SColor(0, 0, 0, 0) : irr::video::SColor(150, 255, 140, 0));
    setButtonHighlight(aisTabButton2, aisDataMode ? irr::video::SColor(150, 255, 140, 0) : irr::video::SColor(0, 0, 0, 0));
    //KYARA: lightning screen-flash (driven by thunder; synced from the primary on secondaries)
    // The drawn line-bolt is retired - the sky uses textured tender/hero billboards instead.
    // Only the soft full-scene flash glow is kept here.
    guiPrevLightningFlash = guiLightningFlash;
    if (guiLightningFlash > 0.02f && !getLargeRadar()) {   // KYARA: never paint over the full radar
        drawLightningFlash();
    }

    //add a collision warning. KYARA: hidden while the proximity alarm is silenced with 'P'
      //(one keypress silences the alarm AND clears the sign; pressing 'P' again restores both).
    if (guiCollided && showCollided && !guiProxyAlarmMuted) {
        drawCollisionWarning();
    }

    //KYARA: proximity alarm silenced indicator. Standard bridge practice: silencing an alarm must
    //never be invisible - the panel keeps a lit "BUZZER SILENCED" lamp so the OOW cannot forget the
    //alarm is off. Same principle here.
    if (guiProxyAlarmMuted) {
        drawAlarmMutedIndicator();
    }
    // Inc 3 (comms): distress/MAYDAY prompt + communications log while a casualty is active.
    if (guiDistressActive) { drawCommsOverlay(); }
    else if (commsMinButton) { commsMinButton->setVisible(false); }   // hide the toggle when the panel is gone
    //manually trigger gui event if buttons are held down
    if (eblUpButton->isPressed()) { manuallyTriggerClick(eblUpButton); }
    if (eblDownButton->isPressed()) { manuallyTriggerClick(eblDownButton); }
    if (eblLeftButton->isPressed()) { manuallyTriggerClick(eblLeftButton); }
    if (eblRightButton->isPressed()) { manuallyTriggerClick(eblRightButton); }

    if (eblUpButton2->isPressed()) { manuallyTriggerClick(eblUpButton2); }
    if (eblDownButton2->isPressed()) { manuallyTriggerClick(eblDownButton2); }
    if (eblLeftButton2->isPressed()) { manuallyTriggerClick(eblLeftButton2); }
    if (eblRightButton2->isPressed()) { manuallyTriggerClick(eblRightButton2); }

    if (radarCursorLeftButton->isPressed()) { manuallyTriggerClick(radarCursorLeftButton); }
    if (radarCursorRightButton->isPressed()) { manuallyTriggerClick(radarCursorRightButton); }
    if (radarCursorUpButton->isPressed()) { manuallyTriggerClick(radarCursorUpButton); }
    if (radarCursorDownButton->isPressed()) { manuallyTriggerClick(radarCursorDownButton); }

    if (radarCursorLeftButton2->isPressed()) { manuallyTriggerClick(radarCursorLeftButton2); }
    if (radarCursorRightButton2->isPressed()) { manuallyTriggerClick(radarCursorRightButton2); }
    if (radarCursorUpButton2->isPressed()) { manuallyTriggerClick(radarCursorUpButton2); }
    if (radarCursorDownButton2->isPressed()) { manuallyTriggerClick(radarCursorDownButton2); }

    if (nonFollowUpPortButton && wheelScrollbar) {
        //Handle port NFU rudder button
        if (nonFollowUpPortButton->isPressed() && !nfuPortDown) {
            nfuPortDown = true; //Set this before we trigger the event, as this will be checked for override
            wheelScrollbar->setPos(-30);
            manuallyTriggerScroll(wheelScrollbar);
        }
        if (!nonFollowUpPortButton->isPressed() && nfuPortDown) {
            wheelScrollbar->setPos(wheelScrollbar->getSecondary());
            manuallyTriggerScroll(wheelScrollbar);
            nfuPortDown = false; //Set this after we trigger the event, as this will be checked for override
        }
    }

    if (nonFollowUpStbdButton && wheelScrollbar) {
        //Handle stbd NFU rudder button
        if (nonFollowUpStbdButton->isPressed() && !nfuStbdDown) {
            nfuStbdDown = true; //Set this before we trigger the event, as this will be checked for override
            wheelScrollbar->setPos(30);
            manuallyTriggerScroll(wheelScrollbar);
        }
        if (!nonFollowUpStbdButton->isPressed() && nfuStbdDown) {
            wheelScrollbar->setPos(wheelScrollbar->getSecondary());
            manuallyTriggerScroll(wheelScrollbar);
            nfuStbdDown = false; //Set this after we trigger the event, as this will be checked for override
        }
    }

    //KYARA FEUX TAB: only while the window is open. Her making-way state changes the expected
    //lights, so the answer line has to follow her as she stops or gets under way.
    if (extraControlsWindow && extraControlsWindow->isVisible()) {
        refreshLightsTab();
    }
    //KYARA FEUX EDIT: the readout follows the lamp as it moves
    if (lightEditWindow) {
        refreshLightEditor();
    }
    //The readout follows every change of size
    if (sizeEditWindow) {
        refreshSizeEditor();
    }
    if (instrEditWindow) {
        refreshInstrumentEditor();
    }

    // Update lines display
    if (model && model->getLines()) {
        std::vector<std::string> linesNames = model->getLines()->getLineNames();

        // remove excess lines if required
        while (linesList->getItemCount() > linesNames.size()) {
            linesList->removeItem(linesList->getItemCount() - 1);
            linesList->setSelected(-1);
        }

        // Update text for existing lines
        for (unsigned int i = 0; i < linesList->getItemCount(); i++) {
            linesList->setItem(i, irr::core::stringw(linesNames.at(i).c_str()).c_str(), -1);
        }

        // Add additional lines if required
        for (unsigned int i = linesList->getItemCount(); i < linesNames.size(); i++) {
            linesList->addItem(irr::core::stringw(linesNames.at(i).c_str()).c_str());
        }

        // Get 'keepSlack' & 'heaveIn' status of current line
        if (linesList->getSelected() > -1) {
            keepLineSlack->setChecked(model->getLines()->getKeepSlack(linesList->getSelected()));
            heaveLineIn->setChecked(model->getLines()->getHeaveIn(linesList->getSelected()));
        }
        else {
            keepLineSlack->setChecked(false);
            heaveLineIn->setChecked(false);
        }


    }

    //KYARA: feed the instrument console (display only - it never writes back to the model)
    if (instrumentPanel) {
        irr::gui::InstrumentData d;
        d.heading = guiHeading;
        d.cog = guiCOG;
        d.stwKn = guiSpeed;
        d.sogKn = guiSOGKts;
        d.rudder = guiRudder;
        d.rudderOrder = guiWheel;
        d.rotDegMin = guiRateOfTurnDegMin;
        d.portRPM = guiPortRPM;
        d.stbdRPM = guiStbdRPM;
        d.windDirTrue = guiWindDirection;
        d.windSpeedKn = guiWindSpeed;
        d.depth = guiDepth;
        d.lat = guiLat;
        d.lon = guiLong;
        d.tideHeight = guiTideHeight;
        d.pitchDeg = guiPitch; // KYARA HOULE
        d.rollDeg = guiRoll;   // KYARA HOULE
        d.timeMs = device->getTimer()->getTime(); // KYARA HOULE: peak markers
        d.timeText = irr::core::stringw(guiTime.c_str());
        d.fps = mainWindowFPS();
        d.paused = guiPaused;
        instrumentPanel->setData(d);
    }

    guienv->drawAll();

    //draw the heading line on the radar
    if ((showInterface && getSmallRadarEnabled()) || radarLarge) { //kyara: no small radar overlay in the console layout
        if (guiRadarOn) {
            draw2dRadar();
        }
    }

    //draw view bearing if needed
    if (bearingButton->isPressed()) {
        draw2dBearing();
    }
}
//kyara: shared palette for EBL/VRM colour cycling
static irr::video::SColor eblVrmPaletteColour(irr::u32 index) {
    switch (index % 6) {
    case 0:  return irr::video::SColor(255, 255, 0, 0); //red
    case 1:  return irr::video::SColor(255, 255, 165, 0); //orange
    case 2:  return irr::video::SColor(255, 0, 255, 0); //green
    case 3:  return irr::video::SColor(255, 0, 255, 255); //cyan
    case 4:  return irr::video::SColor(255, 255, 255, 255); //white
    default: return irr::video::SColor(255, 255, 0, 255); //magenta
    }
}
void GUIMain::draw2dRadar()
{
    irr::s32 centreX;
    irr::s32 centreY;
    irr::s32 radius;

    if (radarLarge) {
        centreX = largeRadarScreenCentreX;
        centreY = largeRadarScreenCentreY;
        //kyara: use the ACTUAL on-screen circle radius (picture is drawn RADAR_FIT_MARGIN smaller
        //than its viewport), so ring labels, bearing numbers and the heading marker sit on the
        //picture rather than a few percent outside it.
        radius = (irr::s32)((irr::f32)largeRadarScreenRadius / RADAR_FIT_MARGIN);   // must come first
        centreX += (irr::s32)(guiRadarOffsetX * radius); //kyara: décentrage
        centreY -= (irr::s32)(guiRadarOffsetY * radius);
    }
    else {
        centreX = smallRadarScreenCentreX;
        centreY = smallRadarScreenCentreY;
        radius = (irr::s32)((irr::f32)smallRadarScreenRadius / RADAR_FIT_MARGIN);
    }
    //kyara: numéros des anneaux de distance (comme un vrai radar)
    //These belong to the range-ring scale, so they follow the same clair / faible / off cycle
    //as the rings themselves - otherwise the numbers stayed on over an empty scope.
    if (guiRadarRangeRingBrightness < 2) {
        irr::gui::IGUIFont* rf = guienv->getSkin()->getFont();
        if (rf) {
            const irr::f32 lblScale = (guiRadarRangeRingBrightness == 0) ? 1.0f : 0.4f;
            const irr::video::SColor ringLabelColour(255,
                (irr::u32)(255 * lblScale), (irr::u32)(200 * lblScale), (irr::u32)(60 * lblScale));
            const int numRings = 6; //doit correspondre à numberOfRings dans RadarCalculation
            for (int rr = 1; rr <= numRings; rr++) {
                irr::s32 ringR = (irr::s32)((irr::f32)radius * (irr::f32)rr / (irr::f32)numRings);
                irr::f32 ringRange = guiRadarRangeNm * (irr::f32)rr / (irr::f32)numRings;
                irr::core::stringw lbl = f32To1dp(ringRange).c_str();
                rf->draw(lbl.c_str(),
                    irr::core::rect<irr::s32>(centreX + 3, centreY - ringR - 7, centreX + 60, centreY - ringR + 9),
                    ringLabelColour);
            }
        }
    }
    //std::cout << radius*2 << std::endl;

    //If full screen radar, draw a 4:3 box around the radar display area
    if (radarLarge) {
        device->getVideoDriver()->draw2DRectangleOutline(radarLargeRect, irr::video::SColor(255, 0, 0, 0));
    }
    //RADAR BEARING NUMBERS
    // ==== Compass bearing numbers around the scope (large radar only) ====
    // kyara: enabled - a real marine radar carries a bearing scale (000..330) around the rim.
    // Part of the same scale as the rings/ticks, so it follows the clair / faible / off cycle.
    if (radarLarge && guiRadarRangeRingBrightness < 2) {
        irr::gui::IGUIFont* font = guienv->getSkin()->getFont();
        if (font) {
            const irr::f32 brgScale = (guiRadarRangeRingBrightness == 0) ? 1.0f : 0.4f;
            const irr::video::SColor labelColour(255,
                (irr::u32)(255 * brgScale), (irr::u32)(220 * brgScale), 0); // amber, matches the ring
            const irr::f32 labelR = (irr::f32)radius * 1.055f;      // just outside the sweep

            for (int brg = 0; brg < 360; brg += 30) {
                // 0 = up on screen, clockwise (same convention as the sweep and blips)
                irr::f32 ang = brg * irr::core::DEGTORAD;
                irr::s32 lx = centreX + (irr::s32)(labelR * sin(ang));
                irr::s32 ly = centreY - (irr::s32)(labelR * cos(ang));

                // 3-digit maritime format: 000, 030, 060 ... 330
                wchar_t buf[8];
                swprintf(buf, 8, L"%03d", brg);

                irr::core::dimension2du ts = font->getDimension(buf);
                irr::core::rect<irr::s32> r(lx - (irr::s32)ts.Width / 2,
                    ly - (irr::s32)ts.Height / 2,
                    lx + (irr::s32)ts.Width / 2,
                    ly + (irr::s32)ts.Height / 2);
                font->draw(buf, r, labelColour, true, true);
            }
        }
    }
    //-----------------------------------END
    irr::f32 radarHeadingIndicator;
    if (radarHeadUp) {
        radarHeadingIndicator = 0;
    }
    else {
        radarHeadingIndicator = guiHeading;
    }
    irr::s32 deltaX = radius * sin(irr::core::DEGTORAD * radarHeadingIndicator);
    irr::s32 deltaY = -1 * radius * cos(irr::core::DEGTORAD * radarHeadingIndicator);
    irr::core::position2d<irr::s32> radarCentre(centreX, centreY);
    irr::core::position2d<irr::s32> radarHeading(centreX + deltaX, centreY + deltaY);
    //kyara: ligne de foi (cap navire) en blanc, du navire vers le pourtour
    device->getVideoDriver()->draw2DLine(radarCentre, radarHeading, irr::video::SColor(255, 255, 255, 255));
    //kyara: forme de coque orientée au cap, au centre de l'écran radar (remplace le disque).
  //Proue pointue vers le cap (même angle que la ligne de foi), poupe plate.
    {
        const irr::video::SColor hullColour(255, 0, 255, 0); //vert vif
        const irr::f32 th = irr::core::DEGTORAD * radarHeadingIndicator; //même cap que la ligne de foi
        const irr::f32 cs = cos(th), sn = sin(th);
        const irr::f32 L = radarLarge ? 12.0f : 7.0f; //demi-longueur (px)
        const irr::f32 W = L * 0.42f;                  //demi-largeur (px)
        //repère navire: f = vers l'avant (proue +), s = tribord +
        const irr::f32 hull[7][2] = {
            {  L,       0.0f   }, //proue
            {  0.4f * L,  W      }, //épaule tribord
            { -0.6f * L,  W      }, //hanche tribord
            { -L,       0.5f * W }, //poupe tribord
            { -L,      -0.5f * W }, //poupe bâbord
            { -0.6f * L, -W      }, //hanche bâbord
            {  0.4f * L, -W      }  //épaule bâbord
        };
        irr::core::position2d<irr::s32> pts[7];
        for (int k = 0; k < 7; ++k) {
            irr::f32 f = hull[k][0], s = hull[k][1];
            pts[k] = irr::core::position2d<irr::s32>(
                centreX + (irr::s32)(s * cs + f * sn),
                centreY + (irr::s32)(s * sn - f * cs));
        }
        for (int k = 0; k < 7; ++k)
            device->getVideoDriver()->draw2DLine(pts[k], pts[(k + 1) % 7], hullColour);
    }
    //draw a look direction line
    if (radarHeadUp) {
        radarHeadingIndicator = viewHdg - guiHeading;
    }
    else {
        radarHeadingIndicator = viewHdg;
    }
    irr::s32 deltaXView = radius * sin(irr::core::DEGTORAD * radarHeadingIndicator);
    irr::s32 deltaYView = -1 * radius * cos(irr::core::DEGTORAD * radarHeadingIndicator);
    irr::core::position2d<irr::s32> lookInner(centreX + 0.9 * deltaXView, centreY + 0.9 * deltaYView);
    irr::core::position2d<irr::s32> lookOuter(centreX + deltaXView, centreY + deltaYView);
    device->getVideoDriver()->draw2DLine(lookInner, lookOuter, irr::video::SColor(255, 255, 0, 0)); //Todo: Make these colours configurable

    //draw an EBL line
   //kyara: draw EBL1 (solid red) and EBL2 (dashed orange)
    for (int i = 0; i < 2; i++) {
        irr::f32 sinB = sin(irr::core::DEGTORAD * guiRadarEBLBrg[i]);
        irr::f32 cosB = cos(irr::core::DEGTORAD * guiRadarEBLBrg[i]);
        irr::video::SColor eblColour = eblVrmPaletteColour(eblColourIndex);
        if (i == 0) {
            irr::core::position2d<irr::s32> eblOuter(centreX + (irr::s32)(radius * sinB), centreY - (irr::s32)(radius * cosB));
            device->getVideoDriver()->draw2DLine(radarCentre, eblOuter, eblColour);
        }
        else {
            //dashed: 20 radial segments, draw every other one
            for (int seg = 0; seg < 20; seg += 2) {
                irr::f32 r1 = radius * seg / 20.0f;
                irr::f32 r2 = radius * (seg + 1) / 20.0f;
                device->getVideoDriver()->draw2DLine(
                    irr::core::position2d<irr::s32>(centreX + (irr::s32)(r1 * sinB), centreY - (irr::s32)(r1 * cosB)),
                    irr::core::position2d<irr::s32>(centreX + (irr::s32)(r2 * sinB), centreY - (irr::s32)(r2 * cosB)),
                    eblColour);
            }
        }
    }
    //kyara: draw VRM1 (solid red circle) and VRM2 (dashed orange circle)
    for (int i = 0; i < 2; i++) {
        if (guiRadarVRMNm[i] > 0 && guiRadarRangeNm >= guiRadarVRMNm[i]) {
            irr::f32 vrmRangePx = radius * guiRadarVRMNm[i] / guiRadarRangeNm;
            irr::video::SColor vrmColour = eblVrmPaletteColour(vrmColourIndex);
            if (i == 0) {
                irr::u8 noSegments = vrmRangePx / 2;
                if (noSegments < 10) { noSegments = 10; }
                device->getVideoDriver()->draw2DPolygon(radarCentre, vrmRangePx, vrmColour, noSegments);
            }
            else {
                //dashed circle: 6 degree arcs with 6 degree gaps
                for (int a = 0; a < 360; a += 12) {
                    irr::f32 a1 = a * irr::core::DEGTORAD;
                    irr::f32 a2 = (a + 6) * irr::core::DEGTORAD;
                    device->getVideoDriver()->draw2DLine(
                        irr::core::position2d<irr::s32>(centreX + (irr::s32)(vrmRangePx * sin(a1)), centreY - (irr::s32)(vrmRangePx * cos(a1))),
                        irr::core::position2d<irr::s32>(centreX + (irr::s32)(vrmRangePx * sin(a2)), centreY - (irr::s32)(vrmRangePx * cos(a2))),
                        vrmColour);
                }
            }
        }
    }

    //draw radar cursor
    irr::s32 cursorPixelRadius = radius * guiRadarCursorRangeNm / guiRadarRangeNm;
    irr::s32 deltaXCursor = cursorPixelRadius * sin(irr::core::DEGTORAD * guiRadarCursorBrg);
    irr::s32 deltaYCursor = -1 * cursorPixelRadius * cos(irr::core::DEGTORAD * guiRadarCursorBrg);
    //Plot if within the display and not at zero range
    if (cursorPixelRadius <= radius && guiRadarCursorRangeNm > 0) {
        irr::core::position2d<irr::s32> cursorCentre(centreX + deltaXCursor, centreY + deltaYCursor);
        device->getVideoDriver()->draw2DPolygon(cursorCentre, radius / 20, irr::video::SColor(255, 255, 0, 0), 4); //a 4 segment polygon, i.e. a square!
    }

}

void GUIMain::draw2dBearing()
{

    //make cross hairs
    irr::s32 screenCentreX = 0.5 * su;
    irr::s32 screenCentreY;
    if (showInterface) {
        screenCentreY = 0.3 * sh;
    }
    else {
        screenCentreY = 0.5 * sh;
    }
    irr::s32 lineLength = 0.1 * sh;
    irr::core::position2d<irr::s32> left(screenCentreX - lineLength, screenCentreY);
    irr::core::position2d<irr::s32> right(screenCentreX + lineLength, screenCentreY);
    irr::core::position2d<irr::s32> top(screenCentreX, screenCentreY - lineLength);
    irr::core::position2d<irr::s32> bottom(screenCentreX, screenCentreY + lineLength);
    irr::core::position2d<irr::s32> centre(screenCentreX, screenCentreY);
    device->getVideoDriver()->draw2DLine(left, right, irr::video::SColor(255, 255, 0, 0));
    device->getVideoDriver()->draw2DLine(top, bottom, irr::video::SColor(255, 255, 0, 0));

    //show view bearing
    guienv->getSkin()->getFont()->draw(f32To1dp(viewHdg).c_str(), irr::core::rect<irr::s32>(screenCentreX - lineLength, screenCentreY - lineLength, screenCentreX, screenCentreY), irr::video::SColor(255, 255, 0, 0), true, true);
    guienv->getSkin()->getFont()->draw(f32To1dp(viewElev).c_str(), irr::core::rect<irr::s32>(screenCentreX - lineLength, screenCentreY, screenCentreX, screenCentreY + lineLength), irr::video::SColor(255, 255, 0, 0), true, true);


    //show angle (from horizon)

}
// kyara collision logic
void GUIMain::drawCollisionWarning()
{
    irr::s32 screenCentreX = 0.5 * su;
    irr::s32 screenCentreY = 0.05 * sh;

    irr::core::rect<irr::s32> boxRect(
        screenCentreX - 0.25 * su, screenCentreY - 0.025 * sh,
        screenCentreX + 0.25 * su, screenCentreY + 0.025 * sh);



    // Warning triangle, centred just below the box
    if (collisionWarningTexture) {
        irr::core::dimension2d<irr::u32> texSize =
            collisionWarningTexture->getOriginalSize();
        irr::s32 imgW = 0.07 * su;   // was 0.12 — smaller collision triangle
        irr::s32 imgH = (texSize.Width > 0)
            ? imgW * ((irr::f32)texSize.Height / (irr::f32)texSize.Width)
            : imgW;
        irr::s32 imgTop = screenCentreY + 0.03 * sh;
        irr::core::rect<irr::s32> destRect(
            screenCentreX - imgW / 2, imgTop,
            screenCentreX + imgW / 2, imgTop + imgH);
        irr::core::rect<irr::s32> srcRect(0, 0, texSize.Width, texSize.Height);
        device->getVideoDriver()->draw2DImage(
            collisionWarningTexture, destRect, srcRect,
            0, 0, true);   // true = use the PNG's alpha channel
    }
}
//KYARA: brief white flash over the whole view, alpha tracks the decaying flash level.
void GUIMain::drawLightningFlash()
{
    irr::f32 a = guiLightningFlash;
    if (a > 1.0f) a = 1.0f;
    irr::u32 alpha = (irr::u32)(a * 150.0f);   // peak ~150/255; lower if it's too strong
    irr::core::rect<irr::s32> full(0, 0, (irr::s32)su, (irr::s32)sh);
    device->getVideoDriver()->draw2DRectangle(
        irr::video::SColor(alpha, 255, 255, 255), full);
}
void GUIMain::generateLightningBolt()
{
    boltStrokes.clear();
    irr::s32 W = (irr::s32)su, H = (irr::s32)sh;
    irr::s32 topY = (irr::s32)(0.02f * H);                 //tweak: how high in the sky bolts start (0 = very top)

    int nBolts = 2 + rand() % 3;                           //tweak: 2..4 separate bolts across the sky at once
    for (int b = 0; b < nBolts; b++) {
        irr::s32 botY = (irr::s32)((0.35f + 0.20f * ((rand() % 100) / 100.0f)) * H); //tweak: how low a bolt reaches
        std::vector<irr::core::vector2di> stroke;
        irr::s32 x = (irr::s32)((0.05f + 0.90f * ((rand() % 100) / 100.0f)) * W);    //tweak: horizontal spread (0.05..0.95 = whole width)
        int segs = 12 + rand() % 8;                        //tweak: zigzag detail (more = finer jags)
        irr::s32 jmax = (irr::s32)(0.035f * W) + 1;        //tweak: how wide the zigzag swings
        for (int s = 0; s <= segs; s++) {
            irr::f32 t = (irr::f32)s / (irr::f32)segs;
            irr::s32 y = topY + (irr::s32)((botY - topY) * t);
            if (s > 0) { x += ((rand() % 2) ? 1 : -1) * (rand() % jmax); }
            stroke.push_back(irr::core::vector2di(x, y));
        }
        boltStrokes.push_back(stroke);

        int nBranches = 2 + rand() % 3;                    //tweak: 2..4 branches per bolt
        for (int br = 0; br < nBranches && (int)stroke.size() > 5; br++) {
            int mid = 3 + rand() % ((int)stroke.size() - 4);
            std::vector<irr::core::vector2di> branch;
            irr::core::vector2di p = stroke[mid];
            branch.push_back(p);
            int bsegs = 4 + rand() % 5;                    //tweak: branch length
            irr::s32 dir = (rand() % 2) ? 1 : -1;
            for (int s = 1; s <= bsegs; s++) {
                p.X += dir * (irr::s32)(0.025f * W) + ((rand() % 2) ? 1 : -1) * (rand() % ((irr::s32)(0.02f * W) + 1));
                p.Y += (irr::s32)(0.03f * H);
                branch.push_back(p);
            }
            boltStrokes.push_back(branch);
        }
    }


}

void GUIMain::drawLightningBolt()
{
    if (boltStrokes.empty()) { return; }
    irr::f32 a = guiLightningFlash; if (a > 1.0f) { a = 1.0f; }
    irr::u32 al = (irr::u32)(a * 255.0f);
    irr::video::SColor core(al, 255, 255, 255);            //tweak: bolt core colour (white)
    irr::video::SColor halo((irr::u32)(al * 0.5f), 170, 195, 255); //tweak: bolt glow colour + the 0.5f halo strength
    irr::video::IVideoDriver* drv = device->getVideoDriver();
    for (size_t s = 0; s < boltStrokes.size(); s++) {
        const std::vector<irr::core::vector2di>& st = boltStrokes[s];
        for (size_t i = 1; i < st.size(); i++) {
            drv->draw2DLine(st[i - 1] + irr::core::vector2di(1, 0), st[i] + irr::core::vector2di(1, 0), halo);
            drv->draw2DLine(st[i - 1] - irr::core::vector2di(1, 0), st[i] - irr::core::vector2di(1, 0), halo);
            drv->draw2DLine(st[i - 1], st[i], core);
        }
    }
}
//KYARA: PROXIMITY ALARM SILENCED INDICATOR
//Drawn top-right of the 3D view, clear of the collision warning (top-centre).
//If media/alarm_muted.png exists it is used; otherwise a badge is drawn from primitives, so this
//works with no new asset at all. A muted alarm that leaves no trace on screen is a safety problem,
//not a convenience - hence the amber, and hence it never fades out.
void GUIMain::drawAlarmMutedIndicator()
{

    irr::video::IVideoDriver* driver = device->getVideoDriver();
    if (!driver) { return; }

    const irr::video::SColor AMBER(255, 255, 176, 0);
    const irr::video::SColor BACKING(200, 28, 32, 40);

    //Size from the FONT, not from su - su is the whole Eyefinity canvas, and this is a LENGTH.
    irr::gui::IGUIFont* font = guienv->getSkin() ? guienv->getSkin()->getFont() : 0;
    const irr::s32 ch = font ? (irr::s32)font->getDimension(L"X").Height : 16;

    const irr::s32 iconSize = ch * 3;
    const irr::s32 margin = ch;
    //Anchor to the RIGHT of the 3D view. On Eyefinity that is the right edge of the canvas; if you
    //would rather it sat on the centre TV, replace su with (su/3)*2 here.
    const irr::s32 right = su - margin;
    const irr::s32 top = margin;

    if (alarmMutedTexture) {
        irr::core::dimension2d<irr::u32> texSize = alarmMutedTexture->getOriginalSize();
        irr::s32 imgW = iconSize;
        irr::s32 imgH = (texSize.Width > 0)
            ? (irr::s32)(imgW * ((irr::f32)texSize.Height / (irr::f32)texSize.Width))
            : imgW;
        irr::core::rect<irr::s32> destRect(right - imgW, top, right, top + imgH);
        irr::core::rect<irr::s32> srcRect(0, 0, texSize.Width, texSize.Height);
        driver->draw2DImage(alarmMutedTexture, destRect, srcRect, 0, 0, true);
        return;
    }

    //---- Fallback badge, drawn from primitives: a speaker with a slash through it. ----
    const irr::s32 boxL = right - iconSize;
    const irr::s32 boxT = top;
    const irr::s32 boxR = right;
    const irr::s32 boxB = top + iconSize;

    driver->draw2DRectangle(BACKING, irr::core::rect<irr::s32>(boxL, boxT, boxR, boxB));
    driver->draw2DRectangleOutline(irr::core::rect<irr::s32>(boxL, boxT, boxR, boxB), AMBER);

    const irr::s32 cx = (boxL + boxR) / 2;
    const irr::s32 cy = (boxT + boxB) / 2;
    const irr::s32 u = iconSize / 8; //unit

    driver->draw2DRectangle(AMBER, irr::core::rect<irr::s32>(cx - 2 * u, cy - u, cx - u, cy + u));
    driver->draw2DLine(irr::core::vector2d<irr::s32>(cx - u, cy - u),
        irr::core::vector2d<irr::s32>(cx + u, cy - 2 * u), AMBER);
    driver->draw2DLine(irr::core::vector2d<irr::s32>(cx - u, cy + u),
        irr::core::vector2d<irr::s32>(cx + u, cy + 2 * u), AMBER);
    driver->draw2DLine(irr::core::vector2d<irr::s32>(cx + u, cy - 2 * u),
        irr::core::vector2d<irr::s32>(cx + u, cy + 2 * u), AMBER);

    //The slash. Drawn three times with a 1px offset so it is thick enough to read at a glance.
    for (irr::s32 d = -1; d <= 1; d++) {
        driver->draw2DLine(irr::core::vector2d<irr::s32>(boxL + u + d, boxT + u),
            irr::core::vector2d<irr::s32>(boxR - u + d, boxB - u), AMBER);
    }
}

void GUIMain::drawCommsOverlay()
{
    irr::video::IVideoDriver* driver = device->getVideoDriver();    if (!driver) { return; }
    irr::gui::IGUIFont* font = guienv->getSkin() ? guienv->getSkin()->getFont() : 0;
    if (!font) { return; }

    // --- palette -------------------------------------------------------------------------
    // Deep navy ground with a signal-red frame. Amber for the running clock, red for the
    // distress header, warm off-white for the MAYDAY body, steel blue for the log heading,
    // cool grey for log entries. Reads as an emergency panel without shouting.
    const irr::video::SColor colPanel(232, 8, 18, 38);
    const irr::video::SColor colFrame(255, 198, 58, 52);
    const irr::video::SColor colRule(255, 38, 70, 112);
    const irr::video::SColor colClock(255, 255, 194, 92);
    const irr::video::SColor colDistress(255, 238, 92, 82);
    const irr::video::SColor colMayday(255, 240, 228, 204);
    const irr::video::SColor colLogHead(255, 128, 176, 228);
    const irr::video::SColor colLogText(255, 202, 214, 228);
    const irr::video::SColor colHint(255, 118, 198, 168);

    const irr::s32 lh = (irr::s32)font->getDimension(L"Xg").Height + 2;   // line height
    const irr::s32 pad = lh / 2;

    // --- panel frame ---------------------------------------------------------------------
    // KYARA: the panel anchors flush to the very top edge (panelT = 0) and, when open, is given a
    // fixed tall height so it fills the empty space across the top of the view. Its WIDTH is now
    // measured from the widest line actually drawn, so there's no dead space on the right; it is
    // still capped at the old su*0.28 ceiling, so nothing that fitted before can start clipping.
    const irr::s32 panelL = pad;
    const irr::s32 panelT = 0;                            // flush to the very top edge
    const irr::s32 panelWCap = (irr::s32)(su * 0.28f);    // never wider than the original
    const irr::s32 btnSize = lh + pad;

    // How many log lines are visible depends only on the (fixed) height, so we can work it out
    // before deciding the width.
    const irr::s32 panelB = panelT + (irr::s32)(sh * 0.45f);   // fill the top of the view
    const irr::s32 nMayday = (irr::s32)guiMaydayLines.size();
    const irr::s32 yLog = panelT + pad + (3 + nMayday) * lh + lh / 2;  // timer+distress+heading + mayday + rule
    const irr::s32 hintY = panelB - pad - lh;
    irr::s32 logCap = (hintY - yLog > 0) ? (hintY - yLog) / lh : 0;
    if (logCap > 30) { logCap = 30; }
    const irr::u32 nLog = (irr::u32)guiCommsLog.size();
    const irr::u32 showLog = (nLog < (irr::u32)logCap) ? nLog : (irr::u32)logCap;

    // Widest line among everything we draw. The timer line also has to clear the button.
    auto textW = [&](const wchar_t* s) -> irr::s32 {
        return (irr::s32)font->getDimension(s).Width;
        };
    irr::s32 contentW = textW(guiDistressTimer.c_str()) + pad + btnSize;   // timer + button
    auto grow = [&](irr::s32 w) { if (w > contentW) { contentW = w; } };
    grow(textW(L"\u2014 D\u00C9TRESSE / COMMUNICATIONS \u2014"));
    for (irr::u32 i = 0; i < guiMaydayLines.size(); ++i) { grow(textW(guiMaydayLines[i].c_str())); }
    grow(textW(L"JOURNAL DES COMMUNICATIONS"));
    for (irr::u32 i = nLog - showLog; i < nLog; ++i) { grow(textW(guiCommsLog[i].c_str())); }
    grow(textW(L"[Ctrl+A] action de comm. suivante"));

    irr::s32 panelW = contentW + 2 * pad;
    if (panelW > panelWCap) { panelW = panelWCap; }
    const irr::s32 panelR = panelL + panelW;

    // --- minimise / restore toggle (top-right of the panel) ------------------------------
    // A push button, created once and polled with isPressed() - exactly how binosButton and the
    // EBL buttons already work in this file, so no EventReceiver wiring is needed. Pressed == minimised.
    const irr::s32 btnR = panelR - pad / 2;
    const irr::s32 btnL = btnR - btnSize;
    const irr::s32 btnT = panelT + pad / 2;
    const irr::s32 btnB = btnT + btnSize;
    const irr::core::rect<irr::s32> btnRect(btnL, btnT, btnR, btnB);
    if (!commsMinButton) {
        commsMinButton = guienv->addButton(btnRect, 0, GUI_ID_COMMS_MINIMISE_BUTTON, L"\u2013");
        commsMinButton->setIsPushButton(true);
    }
    else {
        commsMinButton->setRelativePosition(btnRect);
    }
    commsMinButton->setVisible(true);
    const bool minimised = commsMinButton->isPressed();
    commsMinButton->setText(minimised ? L"+" : L"\u2013");   // "+" restores, "\u2013" minimises

    // --- minimised: collapse to a slim header bar (just the running clock) ----------------
    if (minimised) {
        const irr::s32 panelBmin = btnB + pad / 2;
        driver->draw2DRectangle(colPanel,
            irr::core::rect<irr::s32>(panelL, panelT, panelR, panelBmin));
        driver->draw2DRectangleOutline(
            irr::core::rect<irr::s32>(panelL, panelT, panelR, panelBmin), colFrame);
        font->draw(guiDistressTimer.c_str(),
            irr::core::rect<irr::s32>(panelL + pad, panelT + pad, btnL - pad, panelT + pad + lh),
            colClock);
        return;   // button itself is drawn on top by guienv->drawAll()
    }

    // --- expanded: tall panel covering the top-left space --------------------------------
    driver->draw2DRectangle(colPanel,
        irr::core::rect<irr::s32>(panelL, panelT, panelR, panelB));
    driver->draw2DRectangleOutline(
        irr::core::rect<irr::s32>(panelL, panelT, panelR, panelB), colFrame);

    irr::s32 x = panelL + pad;
    irr::s32 y = panelT + pad;

    // Timer shares its line with the minimise button, so stop its text short of the button.
    font->draw(guiDistressTimer.c_str(),
        irr::core::rect<irr::s32>(x, y, btnL - pad, y + lh), colClock); y += lh;
    font->draw(L"\u2014 D\u00C9TRESSE / COMMUNICATIONS \u2014",
        irr::core::rect<irr::s32>(x, y, panelR - pad, y + lh), colDistress); y += lh;

    for (irr::u32 i = 0; i < guiMaydayLines.size(); ++i) {
        font->draw(guiMaydayLines[i].c_str(),
            irr::core::rect<irr::s32>(x, y, panelR - pad, y + lh), colMayday); y += lh;
    }

    // Hairline rule separating the traffic from the running log.
    y += lh / 4;
    driver->draw2DRectangle(colRule,
        irr::core::rect<irr::s32>(x, y, panelR - pad, y + 1));
    y += lh / 4;

    font->draw(L"JOURNAL DES COMMUNICATIONS",
        irr::core::rect<irr::s32>(x, y, panelR - pad, y + lh), colLogHead); y += lh;

    // The hint is pinned to the bottom of the (now taller) panel; the log fills the gap between
    // the heading and the hint, showing as many of the most recent entries as fit.
    for (irr::u32 i = nLog - showLog; i < nLog; ++i) {
        font->draw(guiCommsLog[i].c_str(),
            irr::core::rect<irr::s32>(x, y, panelR - pad, y + lh), colLogText); y += lh;
    }

    font->draw(L"[Ctrl+A] action de comm. suivante",
        irr::core::rect<irr::s32>(x, hintY, panelR - pad, hintY + lh), colHint);
}
void GUIMain::setExtraControlsWindowVisible(bool windowVisible)
{
    extraControlsWindow->setVisible(windowVisible);
    if (windowVisible) {
        guienv->setFocus(extraControlsWindow);
    }
}

void GUIMain::setLinesControlsWindowVisible(bool windowVisible)
{
    linesControlsWindow->setVisible(windowVisible);
    if (windowVisible) {
        guienv->setFocus(linesControlsWindow);
    }
}
void GUIMain::setAisDataMode(bool on) { aisDataMode = on; }


//KYARA FEUX TAB ----------------------------------------------------------------------------------
int GUIMain::getLightsVessel() const
{
    if (!lightsVesselBox) { return -1; }
    return lightsVesselBox->getSelected() - 1; //item 0 is the own ship
}

//The model is the only truth: every control is re-read from it, so the tabs, the Ctrl+Shift keys
//and the two "Feux de pont" boxes can never drift apart.
void GUIMain::refreshLightsTab()
{
    if (!model) { return; }

    const irr::video::SColor SELECTED(255, 245, 190, 66); //amber, like the other toggles
    const irr::video::SColor NONE(0, 0, 0, 0);
    const irr::video::SColor TEXT_OK = bridge::palette().text;
    const irr::video::SColor TEXT_ERR = bridge::palette().error;

    //--- Feux tab: selected vessel (absent when the feature is switched off) ---
    ShipLights* lights = lightsStatusText ? model->getShipLights(getLightsVessel()) : 0;
    if (lights) {
        const int sit = (int)lights->getSituation();
        for (int k = 0; k < ShipLights::SIT_COUNT; k++) {
            setButtonHighlight(lightsSitButton[k], (k == sit) ? SELECTED : NONE);
        }

        //A lamp this vessel does not have (a second masthead light under 50 m, say) cannot be
        //hidden, so its box is greyed out rather than silently doing nothing.
        for (int s = 0; s < ShipLights::OVERRIDE_SLOTS; s++) {
            if (!lightsOverrideBox[s]) { continue; }
            const ShipLights::Role role = ShipLights::overrideRole(s);
            const bool present = lights->countRole(role) > 0;
            lightsOverrideBox[s]->setEnabled(present);
            lightsOverrideBox[s]->setChecked(present && lights->isRoleOverridden(role));
        }

        if (lightsDeckBox) {
            const bool hasDeck = lights->countRole(ShipLights::ROLE_DECK) +
                lights->countRole(ShipLights::ROLE_ACCOMMODATION) > 0;
            lightsDeckBox->setEnabled(hasDeck);
            lightsDeckBox->setChecked(hasDeck && lights->getDeckLights());
        }

        std::wstring status = L"Attendu : ";
        status += lights->describeExpectedFr();
        if (lights->hasOverrides()) {
            status += L"  -  FEU MASQU\u00C9";
        }
        lightsStatusText->setText(status.c_str());
        lightsStatusText->setOverrideColor(lights->hasOverrides() ? TEXT_ERR : TEXT_OK);
    }

    //--- Eclairage tab: always the own ship ---
    const int instr = model->getOwnShipInstrumentLights();
    for (int i = 0; i < 3; i++) {
        setButtonHighlight(instrLightsButton[i], (i == instr) ? SELECTED : NONE);
    }
    ShipLights* own = model->getShipLights(-1);
    if (own && ownDeckLightsBox) {
        ownDeckLightsBox->setChecked(own->getDeckLights());
    }
    if (interiorStatusText) {
        std::wstring info;
        if (model->getOwnShipInstrumentMaterialCount() == 0) {
            //Without this line the buttons look broken on a model whose textures match none of
            //the keywords - which is a data problem, not a code one.
            info = showInstrumentTool
                ? L"Aucun \u00E9cran \u00E9clair\u00E9 sur ce mod\u00E8le : les choisir avec "
                  L"\u00AB Choisir les \u00E9crans \u00E9clair\u00E9s \u00BB ci-dessous."
                : L"Aucun \u00E9cran d\u00E9tect\u00E9 sur ce mod\u00E8le : renseigner "
                  L"InstrumentMaterials= dans boat.ini (indices list\u00E9s dans le journal).";
        }
        else {
            info = std::to_wstring(model->getOwnShipInstrumentMaterialCount());
            info += L" surface(s) d'instrument sur ce mod\u00E8le.";
        }
        info += L" Les feux de pont n'\u00E9clairent le pont que la nuit ; de jour ils sont sans effet visible.";
        interiorStatusText->setText(info.c_str());
    }
}

void GUIMain::setLinesControlsText(std::string textToShow)
{
    linesText->setText(irr::core::stringw(textToShow.c_str()).c_str());
}
void GUIMain::togglePrimaryControls()
{
    showPrimaryControls = !showPrimaryControls;

    // Change the button text depending on the state
    if (commandBar) {
        togglePrimaryControlsButton->setText((iconSpace + (showPrimaryControls ? L"MASQUER" : L"AFFICHER")).c_str());
    }
    else if (showPrimaryControls) {
        togglePrimaryControlsButton->setText(L"Hide Controls");
    }
    else {
        togglePrimaryControlsButton->setText(L"Show Controls");
    }

    updateVisibility();
}

//KYARA: nudge the zoom bar (mouse wheel). Returns the new raw position so the caller can
//convert it to a magnification and drive the model.
irr::s32 GUIMain::adjustMagnification(irr::s32 delta)
{
    if (!magnificationScrollbar) { return 10; }
    //setPos() clamps to the bar's min/max (10..200) for us.
    magnificationScrollbar->setPos(magnificationScrollbar->getPos() + delta);
    return magnificationScrollbar->getPos();
}
//=================================================================================================
//Detachable instrument console
//=================================================================================================

bool GUIMain::getCompact3dView() const
{
    return showInterface && !consoleDetached;
}

const irr::video::SExposedVideoData& GUIMain::getMainVideoData() const
{
#ifdef _WIN32
    return noVideoData;   //Irrlicht's WGL manager switches back to the main window by itself
#else
    return (consoleWindowUsed && consoleWindow) ? consoleWindow->mainVideoData() : noVideoData;
#endif
}

irr::u32 GUIMain::mainWindowFPS() const
{
    //Every endScene() counts as a frame for the driver, the console window's included.
    const irr::s32 fps = device->getVideoDriver()->getFPS();
    const irr::s32 own = consoleDetached ? (irr::s32)consoleRenderRate : 0;
    return (irr::u32)((fps > own) ? fps - own : fps);
}

void GUIMain::toggleConsoleDetached()
{
    setConsoleDetached(!consoleDetached);
}

void GUIMain::saveConsolePlacement(bool detached)
{
    if (consoleOnScreen) { return; } //placed by the launcher or bc5.ini, not by the user
    if (consoleWindow && consoleWindow->isOpen()) {
        consoleWindow->getPlacement(consolePlaceX, consolePlaceY, consolePlaceW, consolePlaceH);
    }
    std::ofstream f(consolePlacementFile(consoleInstance).c_str());
    if (!f) { return; }
    f << "Detached=" << (detached ? 1 : 0) << std::endl;
    f << "X=" << consolePlaceX << std::endl;
    f << "Y=" << consolePlaceY << std::endl;
    if (consolePlaceW > 0 && consolePlaceH > 0) {
        f << "Width=" << consolePlaceW << std::endl;
        f << "Height=" << consolePlaceH << std::endl;
    }
}

void GUIMain::layoutConsoleStatusColumn()
{
    //Same rows as in load(): 0 = RADAR, 1 = pump 1, 2 = pump 2, 3 = Acquitter.
    const irr::core::rect<irr::s32> col = instrumentPanel->getStatusColumnRect();
    irr::core::position2di origin(0, 0);
    if (instrumentPanel->getParent()) { origin = instrumentPanel->getParent()->getAbsolutePosition().UpperLeftCorner; }
    const irr::s32 gapY = col.getHeight() / 20;
    const irr::s32 rowH = (col.getHeight() - 3 * gapY) / 4;
    irr::gui::IGUIElement* rows[4] = { bigRadarButton, pump1On, pump2On, ackAlarms };
    for (int i = 0; i < 4; i++) {
        if (!rows[i]) { continue; }
        const irr::s32 top = col.UpperLeftCorner.Y + i * (rowH + gapY);
        rows[i]->setRelativePosition(irr::core::rect<irr::s32>(col.UpperLeftCorner.X, top, col.LowerRightCorner.X, top + rowH) - origin);
    }
}

irr::s32 GUIMain::consoleStatusWidthFor(irr::gui::IGUIFont* font) const
{
    //As in load(): as wide as the widest label, at least seven characters.
    if (!font) { return consoleBaseStatusW; }
    const irr::s32 ch = (irr::s32)font->getDimension(L"X").Height;
    irr::s32 w = ch * 7;
    irr::gui::IGUIElement* labels[4] = { bigRadarButton, pump1On, pump2On, ackAlarms };
    for (int i = 0; i < 4; i++) {
        if (!labels[i]) { continue; }
        const irr::s32 lw = (irr::s32)font->getDimension(labels[i]->getText()).Width + 2 * ch;
        if (lw > w) { w = lw; }
    }
    return w;
}

void GUIMain::layoutDetachedConsole(const irr::core::dimension2du& size)
{
    const irr::core::rect<irr::s32> all(0, 0, (irr::s32)size.Width, (irr::s32)size.Height);
    consoleHost->setRelativePosition(all);

    //First pass with the normal lettering: as many rows as make the dials biggest.
    instrumentPanel->setOverrideFont(0);
    instrumentPanel->setMaxRows(4);
    instrumentPanel->setStatusColumnWidth(consoleBaseStatusW);
    instrumentPanel->setRelativePosition(all);

    //Lettering in proportion to the dials (from the main-screen size), within the fonts on disk.
    irr::gui::IGUIFont* font = 0;
    if (consoleAttachedGaugeD > 1.0f && !consoleFontName.empty()) {
        //A little slower than the dials, so the big readouts (depth, heading) keep their room.
        const irr::f32 ratio = instrumentPanel->getGaugeDiameter() / consoleAttachedGaugeD;
        irr::s32 fontSize = (irr::s32)(consoleBaseFontSize * powf(ratio, 0.85f) + 0.5f);
        if (fontSize > 30) { fontSize = 30; }
        if (fontSize > consoleBaseFontSize) {
            const std::string path = "media/fonts/" + consoleFontName + "/" + consoleFontName + "-" + std::to_string(fontSize) + ".xml";
            font = guienv->getFont(path.c_str());
        }
    }
    if (font) {
        instrumentPanel->setOverrideFont(font);
        instrumentPanel->setStatusColumnWidth(consoleStatusWidthFor(font));
    }
    bigRadarButton->setOverrideFont(font);
    pump1On->setOverrideFont(font);
    pump2On->setOverrideFont(font);
    ackAlarms->setOverrideFont(font);
    layoutConsoleStatusColumn();
    consoleLaidOutSize = size;
}

void GUIMain::applyDetachedConsoleVisibility()
{
    //In the console window the panel and RADAR are always there; pumps and ack follow the station's
    //role (a secondary station shows no ship controls).
    const bool controls = !controlsHidden;
    if (instrumentPanel) { instrumentPanel->setVisible(true); }
    if (bigRadarButton) { bigRadarButton->setVisible(true); }
    if (pump1On) { pump1On->setVisible(controls); }
    if (pump2On) { pump2On->setVisible(controls); }
    if (ackAlarms) { ackAlarms->setVisible(controls); }
    //The console window shows heading and the ship's data: no need for the strip versions on the
    //bridge view (they stay on the full-screen radar, which hides the rest).
    if (!radarLarge) {
        if (headingIndicator) { headingIndicator->setVisible(false); }
        if (dataDisplay) { dataDisplay->setVisible(false); }
    }
}

void GUIMain::setConsoleDetached(bool detached)
{
    if (!instrumentPanel || detached == consoleDetached) { return; }
    irr::gui::IGUIElement* consoleElements[5] = { instrumentPanel, bigRadarButton, pump1On, pump2On, ackAlarms };

    if (detached) {
        if (!consoleWindow) { consoleWindow = new ConsoleWindow(); }
        //Its own screen, filled; or the last place and size; or the console's own size on the main screen.
        irr::u32 w = consolePlaceW;
        irr::u32 h = consolePlaceH;
        irr::s32 x = consolePlaceX;
        irr::s32 y = consolePlaceY;
        if (consoleOnScreen) {
            x = consoleScreen.UpperLeftCorner.X;
            y = consoleScreen.UpperLeftCorner.Y;
            w = (irr::u32)consoleScreen.getWidth();
            h = (irr::u32)consoleScreen.getHeight();
        }
        else if (w < 200 || h < 80) {
            w = (irr::u32)consolePanelAttachedRect.getWidth();
            h = (irr::u32)consolePanelAttachedRect.getHeight();
        }
        if (!consoleWindow->open(device, L"NAUTITECH - Instruments", x, y, w, h, consoleOnScreen)) {
            std::cerr << "Could not open the instrument console window (OpenGL driver needed)." << std::endl;
            return;
        }
        consoleWindowUsed = true;
        consoleHost = new ConsoleHostElement(guienv, device, irr::core::rect<irr::s32>(0, 0, (irr::s32)w, (irr::s32)h));
        for (int i = 0; i < 5; i++) {
            if (consoleElements[i]) { consoleHost->addChild(consoleElements[i]); }
        }
        consoleDetached = true;
        layoutDetachedConsole(irr::core::dimension2du(w, h));
        consoleInputTarget = 0;
        consoleRenderCount = 0;
        consoleRenderRate = 0;
        consoleRateStartMs = device->getTimer()->getRealTime();
        saveConsolePlacement(true);
    }
    else {
        saveConsolePlacement(false);
        if (consoleInputTarget) { guienv->removeFocus(consoleInputTarget); }
        consoleInputTarget = 0;
        //Back into the main GUI tree, where they were.
        irr::gui::IGUIElement* root = guienv->getRootGUIElement();
        for (int i = 0; i < 5; i++) {
            if (consoleElements[i]) { root->addChild(consoleElements[i]); }
        }
        //Main-screen layout: one strip, normal lettering.
        instrumentPanel->setOverrideFont(0);
        bigRadarButton->setOverrideFont(0);
        pump1On->setOverrideFont(0);
        pump2On->setOverrideFont(0);
        ackAlarms->setOverrideFont(0);
        instrumentPanel->setMaxRows(1);
        instrumentPanel->setStatusColumnWidth(consoleBaseStatusW);
        instrumentPanel->setRelativePosition(consolePanelAttachedRect);
        bigRadarButton->setRelativePosition(consoleStatusAttachedRect[0]);
        pump1On->setRelativePosition(consoleStatusAttachedRect[1]);
        pump2On->setRelativePosition(consoleStatusAttachedRect[2]);
        ackAlarms->setRelativePosition(consoleStatusAttachedRect[3]);
        if (consoleHost) {
            consoleHost->drop();
            consoleHost = 0;
        }
        if (consoleWindow) { consoleWindow->close(); }
        consoleDetached = false;
    }
    updateVisibility();
}

void GUIMain::dispatchConsoleWindowInput()
{
    std::vector<irr::SEvent> events;
    bool closeRequested = false;
    consoleWindow->poll(events, closeRequested);
    if (closeRequested) {
        setConsoleDetached(false);   //closing the window puts the console back on the main screen
        return;
    }
    //Straight to the element under the press - no hit testing by the GUI environment, which only
    //knows the main window's elements.
    for (size_t i = 0; i < events.size() && consoleDetached; i++) {
        const irr::SEvent& e = events[i];
        const irr::core::position2di p(e.MouseInput.X, e.MouseInput.Y);
        switch (e.MouseInput.Event) {
        case irr::EMIE_LMOUSE_PRESSED_DOWN:
        case irr::EMIE_RMOUSE_PRESSED_DOWN: {
            const bool left = (e.MouseInput.Event == irr::EMIE_LMOUSE_PRESSED_DOWN);
            irr::gui::IGUIElement* target = 0;
            irr::gui::IGUIButton* buttons[2] = { bigRadarButton, ackAlarms };
            for (int b = 0; b < 2 && left; b++) {
                if (buttons[b] && buttons[b]->isVisible() && buttons[b]->isEnabled() && buttons[b]->getAbsolutePosition().isPointInside(p)) {
                    target = buttons[b];
                }
            }
            if (!target && instrumentPanel->isPointInside(p)) { target = instrumentPanel; }   //rudder dial = helm
            consoleInputTarget = target;
            if (target) { target->OnEvent(e); }
            break;
        }
        case irr::EMIE_MOUSE_MOVED:
            if (consoleInputTarget) { consoleInputTarget->OnEvent(e); }
            break;
        case irr::EMIE_LMOUSE_LEFT_UP:
        case irr::EMIE_RMOUSE_LEFT_UP:
            if (consoleInputTarget) {
                irr::gui::IGUIElement* target = consoleInputTarget;
                consoleInputTarget = 0;
                target->OnEvent(e);
                //No keyboard focus left on the console: Space/Enter in the bridge view must not
                //press its buttons.
                if (consoleDetached) { guienv->removeFocus(target); }
            }
            break;
        default:
            break;
        }
    }
}

void GUIMain::renderDetachedConsole()
{
    if (!consoleDetached || !consoleWindow) { return; }
    dispatchConsoleWindowInput();
    if (!consoleDetached) { return; }
    consoleWindow->keepAboveSimulator();

    const irr::u32 now = device->getTimer()->getRealTime();
    if (now - consoleRateStartMs >= 1000) {
        consoleRenderRate = consoleRenderCount * 1000 / (now - consoleRateStartMs);
        consoleRenderCount = 0;
        consoleRateStartMs = now;
    }
    if (now - consoleLastRenderMs < consoleFrameMs) { return; }

    const irr::core::dimension2du size = consoleWindow->getClientSize();
    if (size.Width < 64 || size.Height < 32) { return; }   //minimised
    if (size != consoleLaidOutSize) {
        layoutDetachedConsole(size);
    }
    consoleLastRenderMs = now;
    consoleRenderCount++;

    irr::video::IVideoDriver* driver = device->getVideoDriver();
    const irr::core::dimension2du mainSize = driver->getScreenSize();
    driver->OnResize(size);
    driver->beginScene(irr::video::ECBF_COLOR, irr::video::SColor(255, 24, 27, 33), 1.0f, 0, consoleWindow->videoData());
    consoleHost->draw();
    driver->endScene();
    driver->OnResize(mainSize);
}

void GUIMain::shutdownConsoleWindow()
{
    if (!consoleWindow) { return; }
    if (consoleDetached) {
        saveConsolePlacement(true);   //detached again next session
        //Put the elements back (they belong to the GUI tree for clean-up) without forgetting the state.
        irr::gui::IGUIElement* consoleElements[5] = { instrumentPanel, bigRadarButton, pump1On, pump2On, ackAlarms };
        irr::gui::IGUIElement* root = guienv->getRootGUIElement();
        for (int i = 0; i < 5; i++) {
            if (consoleElements[i]) { root->addChild(consoleElements[i]); }
        }
        if (consoleHost) {
            consoleHost->drop();
            consoleHost = 0;
        }
        consoleDetached = false;
    }
    consoleWindow->close();
    delete consoleWindow;
    consoleWindow = 0;
}


#include <cwchar> //KYARA FEUX EDIT: std::swprintf
//=================================================================================================
//KYARA FEUX EDIT - the "Placement des feux" window
//=================================================================================================
void GUIMain::openLightEditor(int vessel)
{
    closeLightEditor();
    closeSizeEditor(); //one editor at a time: they share the camera
    closeInstrumentEditor();
    if (!model) { return; }
    ShipLights* lights = model->getShipLights(vessel);
    if (!lights) { return; }

    //Sized from the font, so it reads the same at every screen resolution.
    irr::s32 fh = 16;
    if (guienv->getSkin() && guienv->getSkin()->getFont()) {
        fh = (irr::s32)guienv->getSkin()->getFont()->getDimension(L"Ag").Height;
    }
    const irr::s32 pad = fh / 2;
    const irr::s32 rowH = fh + 10;
    const irr::s32 w = fh * 26;
    const irr::s32 listH = rowH * 7;
    const irr::s32 readH = fh * 5;
    const irr::s32 helpH = fh * 9;
    const irr::s32 h = 2 * fh + listH + readH + 6 * (rowH + pad) + fh * 3 + helpH + 3 * pad;
    const irr::s32 sw = (irr::s32)device->getVideoDriver()->getScreenSize().Width;
    const irr::s32 sh = (irr::s32)device->getVideoDriver()->getScreenSize().Height;
    const irr::s32 x = sw - w - fh;
    //Low on the right, clear of the instrument console along the top
    irr::s32 y = sh - h - fh * 4;
    if (y < fh * 3) { y = fh * 3; }

    std::wstring title = L"Placement des feux - ";
    if (vessel < 0) { title += L"navire propre"; }
    else {
        const std::string n = model->getOtherShipName(vessel);
        title += std::wstring(n.begin(), n.end());
    }
    lightEditWindow = guienv->addWindow(irr::core::rect<irr::s32>(x, y, x + w, y + h), false, title.c_str());
    if (lightEditWindow->getCloseButton()) { lightEditWindow->getCloseButton()->setVisible(false); }

    irr::s32 cy = 2 * fh;
    lightEditList = guienv->addListBox(irr::core::rect<irr::s32>(pad, cy, w - pad, cy + listH),
        lightEditWindow, GUI_ID_LEDIT_LIST, true);
    for (int i = 0; i < lights->getEditItemCount(); i++) {
        lightEditList->addItem(lights->getEditItemLabel(i).c_str());
    }
    lightEditList->setSelected(lights->getSelectedEditItem());
    lightEditRevision = lights->getEditRevision();
    cy += listH + pad;

    lightEditReadout = guienv->addStaticText(L"", irr::core::rect<irr::s32>(pad, cy, w - pad, cy + readH),
        true, true, lightEditWindow);
    cy += readH + pad;

    lightEditMirror = guienv->addCheckBox(lights->getMirror(),
        irr::core::rect<irr::s32>(pad, cy, w - pad, cy + rowH), lightEditWindow, GUI_ID_LEDIT_MIRROR,
        L"Miroir : d\u00E9placer un feu de c\u00F4t\u00E9 d\u00E9place l'autre");
    cy += rowH + pad;

    const irr::s32 bw = fh * 2;
    lightEditSpacingText = guienv->addStaticText(L"", irr::core::rect<irr::s32>(pad, cy, w - pad - 2 * bw - pad, cy + rowH),
        false, false, lightEditWindow);
    lightEditSpacingText->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_CENTER);
    guienv->addButton(irr::core::rect<irr::s32>(w - pad - 2 * bw - pad / 2, cy, w - pad - bw - pad / 2, cy + rowH),
        lightEditWindow, GUI_ID_LEDIT_SPACING_DOWN, L"-", L"Rapprocher les feux de signal (0,5 m)");
    guienv->addButton(irr::core::rect<irr::s32>(w - pad - bw, cy, w - pad, cy + rowH),
        lightEditWindow, GUI_ID_LEDIT_SPACING_UP, L"+", L"\u00C9carter les feux de signal (0,5 m)");
    cy += rowH + pad;

    const irr::s32 half = (w - 3 * pad) / 2;
    guienv->addButton(irr::core::rect<irr::s32>(pad, cy, pad + half, cy + rowH), lightEditWindow,
        GUI_ID_LEDIT_ROLE, L"R\u00F4le suivant (R)",
        L"Corriger le r\u00F4le de ce feu s'il a \u00E9t\u00E9 mal devin\u00E9 (Maj+R : pr\u00E9c\u00E9dent)");
    guienv->addButton(irr::core::rect<irr::s32>(2 * pad + half, cy, w - pad, cy + rowH), lightEditWindow,
        GUI_ID_LEDIT_DELETE, L"Supprimer ce feu (Suppr)",
        L"Retirer un feu en trop - prend effet \u00E0 l'enregistrement");
    cy += rowH + pad;

    const irr::s32 third = (w - 4 * pad) / 3;
    guienv->addButton(irr::core::rect<irr::s32>(pad, cy, pad + third, cy + rowH), lightEditWindow,
        GUI_ID_LEDIT_SAVE, L"Enregistrer", L"\u00C9crire les positions dans le boat.ini du navire (copie .bak / .prev)");
    guienv->addButton(irr::core::rect<irr::s32>(2 * pad + third, cy, 2 * pad + 2 * third, cy + rowH), lightEditWindow,
        GUI_ID_LEDIT_REVERT, L"Annuler", L"Remettre les feux o\u00F9 ils \u00E9taient \u00E0 l'ouverture");
    guienv->addButton(irr::core::rect<irr::s32>(3 * pad + 2 * third, cy, w - pad, cy + rowH), lightEditWindow,
        GUI_ID_LEDIT_CLOSE, L"Terminer", L"Fermer l'\u00E9diteur et revenir \u00E0 la passerelle");
    cy += rowH + pad;

    lightEditStatus = guienv->addStaticText(L"Non enregistr\u00E9.", irr::core::rect<irr::s32>(pad, cy, w - pad, cy + fh * 3),
        false, true, lightEditWindow);
    cy += fh * 3 + pad;

    guienv->addStaticText(
        L"Fl\u00E8ches : vers l'avant / l'arri\u00E8re, vers b\u00E2bord / tribord\n"
        L"Pg.Pr\u00E9c / Pg.Suiv : monter / descendre\n"
        L"Pas : 25 cm - Maj : 5 cm - Ctrl : 1 m\n"
        L"Tab : feu suivant - Maj+Tab : pr\u00E9c\u00E9dent\n"
        L"R : changer le r\u00F4le - Suppr : supprimer le feu\n"
        L"Tourner autour : glisser dans la vue, ou A / D et W / S\n"
        L"Zoom : molette, ou Q / E\n"
        L"Le feu s\u00E9lectionn\u00E9 clignote.",
        irr::core::rect<irr::s32>(pad, cy, w - pad, cy + helpH), false, true, lightEditWindow);

    refreshLightEditor();
}

void GUIMain::closeLightEditor()
{
    if (lightEditWindow) { lightEditWindow->remove(); }
    lightEditWindow = 0;
    lightEditList = 0;
    lightEditReadout = 0;
    lightEditMirror = 0;
    lightEditSpacingText = 0;
    lightEditStatus = 0;
}

void GUIMain::refreshLightEditor()
{
    if (!lightEditWindow || !model || !model->isLightEditing()) { return; }
    ShipLights* lights = model->getShipLights(model->getLightEditVessel());
    if (!lights) { return; }

    //A role changed or a lamp was deleted: the labels are stale, rebuild the list.
    if (lightEditList && lightEditRevision != lights->getEditRevision()) {
        lightEditList->clear();
        for (int i = 0; i < lights->getEditItemCount(); i++) {
            lightEditList->addItem(lights->getEditItemLabel(i).c_str());
        }
        lightEditRevision = lights->getEditRevision();
        lightEditList->setSelected(-1); //forces the selection below to be re-applied
    }
    //Tab on the keyboard changes the selection behind the list's back; follow it.
    if (lightEditList && lightEditList->getSelected() != lights->getSelectedEditItem()) {
        lightEditList->setSelected(lights->getSelectedEditItem());
    }
    if (lightEditReadout) {
        lightEditReadout->setText(lights->describeSelectedFr().c_str());
    }
    if (lightEditSpacingText) {
        wchar_t buf[96];
        std::swprintf(buf, 96, L"Espacement des feux de signal : %.1f m", lights->getSignalSpacingMetres());
        lightEditSpacingText->setText(buf);
    }
}

void GUIMain::setLightEditorStatus(const std::wstring& text, bool isError)
{
    if (!lightEditStatus) { return; }
    lightEditStatus->setText(text.c_str());
    lightEditStatus->setOverrideColor(isError ? bridge::palette().error : bridge::palette().ok);
}

//=================================================================================================
//The "Taille et flottaison" window
//=================================================================================================
namespace
{
    //French decimals for the readout: 12,50
    std::wstring metresFr(irr::f32 v, int decimals)
    {
        wchar_t buf[48];
        std::swprintf(buf, 48, L"%.*f", decimals, v);
        std::wstring t(buf);
        for (size_t i = 0; i < t.size(); i++) { if (t[i] == L'.') { t[i] = L','; } }
        return t;
    }
}

void GUIMain::openSizeEditor(int vessel)
{
    closeSizeEditor();
    closeLightEditor(); //one editor at a time: they share the camera
    closeInstrumentEditor();
    if (!model) { return; }
    SimulationModel::VesselSize size;
    if (!model->getVesselSize(vessel, size)) { return; }

    //Sized from the font, like the lamp editor
    irr::s32 fh = 16;
    if (guienv->getSkin() && guienv->getSkin()->getFont()) {
        fh = (irr::s32)guienv->getSkin()->getFont()->getDimension(L"Ag").Height;
    }
    const irr::s32 pad = fh / 2;
    const irr::s32 rowH = fh + 10;
    const irr::s32 w = fh * 28;
    const irr::s32 readH = fh * 8;
    const irr::s32 statusH = fh * 3;
    const irr::s32 helpH = fh * 6;
    const irr::s32 h = 2 * fh + readH + pad + 5 * (rowH + pad) + statusH + pad + helpH + pad;
    const irr::s32 sw = (irr::s32)device->getVideoDriver()->getScreenSize().Width;
    const irr::s32 sh = (irr::s32)device->getVideoDriver()->getScreenSize().Height;
    const irr::s32 x = sw - w - fh;
    //Low on the right, clear of the instrument console along the top
    irr::s32 y = sh - h - fh * 4;
    if (y < fh * 3) { y = fh * 3; }

    std::wstring title = L"Taille et flottaison - ";
    if (vessel < 0) { title += L"navire propre"; }
    else {
        const std::string n = model->getOtherShipName(vessel);
        title += std::wstring(n.begin(), n.end());
    }
    sizeEditWindow = guienv->addWindow(irr::core::rect<irr::s32>(x, y, x + w, y + h), false, title.c_str());
    if (sizeEditWindow->getCloseButton()) { sizeEditWindow->getCloseButton()->setVisible(false); }

    irr::s32 cy = 2 * fh;
    sizeEditReadout = guienv->addStaticText(L"", irr::core::rect<irr::s32>(pad, cy, w - pad, cy + readH),
        true, true, sizeEditWindow);
    cy += readH + pad;

    //A value row: label, box, OK
    const irr::s32 labelX1 = pad + (irr::s32)(w * 0.42f);
    const irr::s32 okW = fh * 3;
    const irr::s32 boxX1 = w - pad - okW - pad;
    auto valueRow = [&](const wchar_t* label, int boxId, int okId, const wchar_t* tip) -> irr::gui::IGUIEditBox* {
        irr::gui::IGUIStaticText* lab = guienv->addStaticText(label,
            irr::core::rect<irr::s32>(pad, cy, labelX1, cy + rowH), false, false, sizeEditWindow);
        lab->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_CENTER);
        irr::gui::IGUIEditBox* box = guienv->addEditBox(L"",
            irr::core::rect<irr::s32>(labelX1 + pad, cy, boxX1, cy + rowH), true, sizeEditWindow, boxId);
        box->setToolTipText(tip);
        guienv->addButton(irr::core::rect<irr::s32>(w - pad - okW, cy, w - pad, cy + rowH), sizeEditWindow,
            okId, L"OK", tip);
        cy += rowH + pad;
        return box;
    };
    const irr::s32 half = (w - 3 * pad) / 2;
    auto buttonPair = [&](int idA, const wchar_t* a, const wchar_t* tipA, int idB, const wchar_t* b, const wchar_t* tipB) {
        guienv->addButton(irr::core::rect<irr::s32>(pad, cy, pad + half, cy + rowH), sizeEditWindow, idA, a, tipA);
        guienv->addButton(irr::core::rect<irr::s32>(2 * pad + half, cy, w - pad, cy + rowH), sizeEditWindow, idB, b, tipB);
        cy += rowH + pad;
    };

    sizeEditLengthBox = valueRow(L"Longueur hors tout (m) :", GUI_ID_SEDIT_LENGTH_BOX, GUI_ID_SEDIT_LENGTH_APPLY,
        L"Taper la vraie longueur du navire, puis Entr\u00E9e ou OK");
    buttonPair(GUI_ID_SEDIT_LENGTH_MINUS, L"- 1 m", L"Raccourcir de 1 m (fl\u00E8che bas)",
        GUI_ID_SEDIT_LENGTH_PLUS, L"+ 1 m", L"Allonger de 1 m (fl\u00E8che haut)");
    sizeEditDraughtBox = valueRow(L"Tirant d'eau (m) :", GUI_ID_SEDIT_DRAUGHT_BOX, GUI_ID_SEDIT_DRAUGHT_APPLY,
        L"De la ligne de flottaison au point le plus bas du mod\u00E8le, puis Entr\u00E9e ou OK");
    buttonPair(GUI_ID_SEDIT_RAISE, L"Monter 10 cm", L"Sortir le navire de l'eau de 10 cm (Pg.Pr\u00E9c : 5 cm)",
        GUI_ID_SEDIT_LOWER, L"Descendre 10 cm", L"Enfoncer le navire de 10 cm (Pg.Suiv : 5 cm)");

    const irr::s32 third = (w - 4 * pad) / 3;
    guienv->addButton(irr::core::rect<irr::s32>(pad, cy, pad + third, cy + rowH), sizeEditWindow,
        GUI_ID_SEDIT_SAVE, L"Enregistrer", L"\u00C9crire ScaleFactor et YCorrection dans le boat.ini du navire (copie .bak / .prev)");
    guienv->addButton(irr::core::rect<irr::s32>(2 * pad + third, cy, 2 * pad + 2 * third, cy + rowH), sizeEditWindow,
        GUI_ID_SEDIT_REVERT, L"Annuler", L"Revenir \u00E0 la taille et \u00E0 la flottaison de l'ouverture");
    guienv->addButton(irr::core::rect<irr::s32>(3 * pad + 2 * third, cy, w - pad, cy + rowH), sizeEditWindow,
        GUI_ID_SEDIT_CLOSE, L"Terminer", L"Fermer et revenir \u00E0 la passerelle (ce qui n'est pas enregistr\u00E9 reste jusqu'\u00E0 la fin du sc\u00E9nario)");
    cy += rowH + pad;

    sizeEditStatus = guienv->addStaticText(L"", irr::core::rect<irr::s32>(pad, cy, w - pad, cy + statusH),
        false, true, sizeEditWindow);
    cy += statusH + pad;
    if (vessel < 0) {
        sizeEditStatus->setText(L"Navire propre : coque, vues, radar et commandes suivent en direct. "
            L"Sa man\u0153uvrabilit\u00E9 (masse, inertie) suit au prochain lancement du sc\u00E9nario.");
    }
    else {
        sizeEditStatus->setText(L"Non enregistr\u00E9.");
    }

    guienv->addStaticText(
        L"Fl\u00E8ches haut / bas : longueur \u00B11 m (Maj : 10 cm, Ctrl : 10 m)\n"
        L"Pg.Pr\u00E9c / Pg.Suiv : monter / descendre de 5 cm (Maj : 1 cm, Ctrl : 50 cm)\n"
        L"Ou taper une valeur, puis Entr\u00E9e\n"
        L"Tourner autour : glisser dans la vue, ou A / D et W / S\n"
        L"Zoom : molette, ou Q / E",
        irr::core::rect<irr::s32>(pad, cy, w - pad, cy + helpH), false, true, sizeEditWindow);

    sizeEditShownRevision = -1; //fill the boxes on the first refresh
    refreshSizeEditor();
}

int GUIMain::getSizeVessel() const
{
    if (!sizeVesselBox) { return -1; }
    return sizeVesselBox->getSelected() - 1; //item 0 is the own ship
}

void GUIMain::setInstructorTools(bool colregTab, bool sizeTool, bool instrumentTool)
{
    showColregTab = colregTab;
    showSizeTool = sizeTool;
    showInstrumentTool = instrumentTool;
}

//=================================================================================================
//The command bar and the day / dusk / night colours
//=================================================================================================

//Every key of the bar in one row, grouped: view keys on the left, zoom, lighting and the palette
//switch in the middle, the tools on the right and Quitter apart at the far end. Sized from the
//labels; if they do not all fit, the icons go first.
void GUIMain::layoutCommandBar()
{
    if (!commandBar || !bridgeSkin || !guienv->getSkin()) { return; }
    irr::gui::IGUIFont* font = guienv->getSkin()->getFont();
    if (!font) { return; }
    const irr::core::rect<irr::s32> bar = commandBarRect;
    const irr::s32 keyH = bar.getHeight() - 10;
    const irr::s32 y0 = bar.UpperLeftCorner.Y + 5, y1 = y0 + keyH;
    const irr::s32 iconSize = bridge::BridgeSkin::iconSizeFor(irr::core::rect<irr::s32>(0, 0, 10, keyH));
    const irr::s32 spaceW = irr::core::max_(1, (irr::s32)font->getDimension(L" ").Width);

    //Roomy with icons; tighter with icons; tight without them
    for (int attempt = 0; attempt < 3; attempt++) {
        const bool icons = (attempt < 2);
        const irr::s32 keyPad = (attempt == 0) ? 22 : 12;
        iconSpace = L"";
        for (irr::s32 w = 0; icons && w < iconSize + 6; w += spaceW) { iconSpace += L" "; }
        commandBar->clearMarks();

        auto textW = [&](const irr::core::stringw& t) { return (irr::s32)font->getDimension(t.c_str()).Width; };
        //A key: its label (with room for the icon) and its width, the widest of its labels
        auto key = [&](irr::gui::IGUIButton* b, irr::s32& x, bool fromRight, const wchar_t* label, const wchar_t* longest,
            bridge::Icon icon, bridge::BridgeSkin::KeyStyle style) {
            if (!b) { return; }
            const irr::core::stringw prefix = (icon != bridge::ICON_NONE) ? iconSpace : irr::core::stringw(L"");
            const irr::s32 w = textW(prefix + irr::core::stringw(longest ? longest : label)) + keyPad;
            b->setText((prefix + irr::core::stringw(label)).c_str());
            if (fromRight) { x -= w; b->setRelativePosition(irr::core::rect<irr::s32>(x, y0, x + w, y1)); x -= 2; }
            else { b->setRelativePosition(irr::core::rect<irr::s32>(x, y0, x + w, y1)); x += w + 2; }
            bridgeSkin->setKeyStyle(b->getID(), style, icons ? icon : bridge::ICON_NONE);
        };
        auto separator = [&](irr::s32& x, bool fromRight) {
            if (fromRight) { x -= 6; commandBar->addSeparator(x); x -= 7; }
            else { x += 6; commandBar->addSeparator(x); x += 7; }
        };

        //--- left: view ---
        irr::s32 x = bar.UpperLeftCorner.X + 6;
        key(togglePrimaryControlsButton, x, false, showPrimaryControls ? L"MASQUER" : L"AFFICHER", L"AFFICHER",
            bridge::ICON_HIDE, bridge::BridgeSkin::KEY_BAR);
        separator(x, false);
        const irr::s32 ifaceX = x;
        key(hideInterfaceButton, x, false, L"PLEIN \u00C9CRAN", L"PLEIN \u00C9CRAN", bridge::ICON_INTERFACE, bridge::BridgeSkin::KEY_BAR);
        irr::s32 xi = ifaceX;
        key(showInterfaceButton, xi, false, L"INTERFACE", L"PLEIN \u00C9CRAN", bridge::ICON_INTERFACE, bridge::BridgeSkin::KEY_BAR);
        key(binosButton, x, false, L"JUMELLES", 0, bridge::ICON_BINOCULARS, bridge::BridgeSkin::KEY_BAR);
        key(bearingButton, x, false, L"REL\u00C8VEMENT", 0, bridge::ICON_BEARING, bridge::BridgeSkin::KEY_BAR);
        key(changeViewButton, x, false, L"VUE", 0, bridge::ICON_VIEW, bridge::BridgeSkin::KEY_BAR);
        separator(x, false);

        //--- middle: zoom, lighting time, palette ---
        const irr::s32 capZoomW = textW(L"ZOOM") + 8;
        commandBar->addCaption(irr::core::rect<irr::s32>(x, y0, x + capZoomW, y1), L"ZOOM", true);
        x += capZoomW + 4;
        const irr::s32 sliderW = (attempt == 0) ? 100 : 80;
        if (magnificationScrollbar) {
            const irr::s32 sh2 = irr::core::min_(keyH, 18);
            magnificationScrollbar->setRelativePosition(irr::core::rect<irr::s32>(x, (y0 + y1 - sh2) / 2, x + sliderW, (y0 + y1 + sh2) / 2));
        }
        x += sliderW + 4;
        const irr::s32 valW = textW(L"\u00D720.0") + 8;
        commandBar->addCaption(irr::core::rect<irr::s32>(x, y0, x + valW, y1), L"\u00D71.0", false);
        x += valW;
        separator(x, false);
        const irr::s32 capLightW = textW(L"\u00C9CLAIRAGE") + 8;
        commandBar->addCaption(irr::core::rect<irr::s32>(x, y0, x + capLightW, y1), L"\u00C9CLAIRAGE", true);
        x += capLightW + 2;
        if (lightingTimeBox) {
            const irr::s32 boxW = textW(L"00:00") + 18;
            lightingTimeBox->setRelativePosition(irr::core::rect<irr::s32>(x, y0 + 2, x + boxW, y1 - 2));
            x += boxW + 8;
        }
        for (int i = 0; i < PALETTE_KEYS; i++) {
            key(paletteButton[i], x, false, paletteButton[i] ? paletteButton[i]->getText() : L"", 0, bridge::ICON_NONE, bridge::BridgeSkin::KEY_BAR);
        }
        separator(x, false);
        const irr::s32 capGlowW = textW(L"LUEUR") + 8;
        commandBar->addCaption(irr::core::rect<irr::s32>(x, y0, x + capGlowW, y1), L"LUEUR", true);
        x += capGlowW + 4;
        if (glowScrollbar) {
            //The arrow buttons take a slider's height at each end: wide and slim leaves the knob room to travel
            const irr::s32 glowW = (attempt == 0) ? 140 : 110;
            const irr::s32 sh2 = irr::core::min_(keyH, 14);
            glowScrollbar->setRelativePosition(irr::core::rect<irr::s32>(x, (y0 + y1 - sh2) / 2, x + glowW, (y0 + y1 + sh2) / 2));
            x += glowW + 4;
        }
        const irr::s32 leftEnd = x;

        //--- right: tools, then Quitter on its own ---
        irr::s32 xr = bar.LowerRightCorner.X - 6;
        key(exitButton, xr, true, L"QUITTER", 0, bridge::ICON_QUIT, bridge::BridgeSkin::KEY_DANGER);
        separator(xr, true);
        key(detachConsoleButton, xr, true, consoleDetached ? L"RATTACHER" : L"D\u00C9TACHER", L"RATTACHER",
            bridge::ICON_DETACH, bridge::BridgeSkin::KEY_BAR);
        key(pcLogButton, xr, true, L"JOURNAL", 0, bridge::ICON_LOG, bridge::BridgeSkin::KEY_BAR);
        key(showLinesControlsButton, xr, true, L"AMARRES", 0, bridge::ICON_LINES, bridge::BridgeSkin::KEY_BAR);
        key(showExtraControlsButton, xr, true, L"CONTR\u00D4LES", 0, bridge::ICON_CONTROLS, bridge::BridgeSkin::KEY_BAR);
        key(weatherButton, xr, true, L"M\u00C9T\u00C9O", 0, bridge::ICON_WEATHER, bridge::BridgeSkin::KEY_BAR);

        if (leftEnd + 8 <= xr || attempt == 2) { break; }
    }
    if (pcLogButton) { pcLogButton->setToolTipText(L"Journal des messages du simulateur"); }
    if (hideInterfaceButton) { hideInterfaceButton->setToolTipText(L"Vue 3D en plein \u00E9cran (cacher la console)"); }
    if (showInterfaceButton) { showInterfaceButton->setToolTipText(L"Revenir \u00E0 la console"); }
    if (togglePrimaryControlsButton) { togglePrimaryControlsButton->setToolTipText(L"Cacher / afficher les commandes et la barre"); }
}

void GUIMain::setPaletteChoice(int choice)
{
    if (choice < -1 || choice >= bridge::MODE_COUNT) { choice = -1; }
    paletteChoice = choice;
    const int pressed = (choice < 0) ? PALETTE_KEYS - 1 : choice;
    for (int i = 0; i < PALETTE_KEYS; i++) {
        if (paletteButton[i]) { paletteButton[i]->setPressed(i == pressed); }
    }
    applyPaletteMode(choice >= 0 ? choice : paletteAutoMode);
}

void GUIMain::placeWeatherPanel()
{
    if (!weatherPanel) { return; }
    //Over the bridge view, on the middle screen (Surround), as large as the view allows
    const irr::s32 viewH = (irr::s32)(sh * viewProportion3D());
    const irr::s32 w = irr::core::min_(1260, (irr::s32)(consoleArea.getWidth() * 0.92f));
    const irr::s32 h = irr::core::max_(irr::core::min_(780, (irr::s32)(viewH * 0.9f)), irr::core::min_(480, (irr::s32)sh - 40));
    const irr::s32 x = consoleArea.getCenter().X - w / 2;
    const irr::s32 y = irr::core::max_(10, (viewH - h) / 2);
    weatherPanel->setRelativePosition(irr::core::rect<irr::s32>(x, y, x + w, y + h));
}

void GUIMain::toggleWeatherPanel()
{
    if (!weatherPanel) { return; }
    if (weatherPanel->isVisible()) {
        weatherPanel->setVisible(false);
        return;
    }
    placeWeatherPanel();
    weatherPanel->setVisible(true);
    guienv->getRootGUIElement()->bringToFront(weatherPanel);
}

void GUIMain::setGlowLevel(int level)
{
    level = irr::core::clamp(level, 0, 100);
    bridge::glowLevel() = level;
    if (glowScrollbar) {
        if (glowScrollbar->getPos() != level) { glowScrollbar->setPos(level); }
        wchar_t tip[96];
        swprintf(tip, 96, L"Lueur des instruments \u00E9clair\u00E9s : %d %% (cr\u00E9puscule, nuit, digital)", level);
        glowScrollbar->setToolTipText(tip);
    }
}

void GUIMain::applyPaletteMode(int mode)
{
    static bool applied = false;
    if (applied && mode == bridge::currentMode()) { return; }
    applied = true;
    bridge::currentMode() = mode;
    if (bridgeSkin) { bridgeSkin->applyPalette(); }
    const bridge::Palette& p = bridge::palette();
    if (portText && portText->isOverrideColorEnabled()) { portText->setOverrideColor(p.portText); }
    if (stbdText) { stbdText->setOverrideColor(p.stbdText); }
    refreshLightsTab();
}

void GUIMain::closeSizeEditor()
{
    if (sizeEditWindow) { sizeEditWindow->remove(); }
    sizeEditWindow = 0;
    sizeEditReadout = 0;
    sizeEditLengthBox = 0;
    sizeEditDraughtBox = 0;
    sizeEditStatus = 0;
}

void GUIMain::refreshSizeEditor()
{
    if (!sizeEditWindow || !model || !model->isSizeEditing()) { return; }
    SimulationModel::VesselSize size;
    if (!model->getVesselSize(model->getSizeEditVessel(), size)) { return; }

    if (sizeEditReadout) {
        std::wstring t = L"Longueur hors tout : " + metresFr(size.length, 2) + L" m\n";
        t += L"Largeur : " + metresFr(size.breadth, 2) + L" m\n";
        t += L"Tirant d'eau : " + metresFr(size.draught, 2) + L" m (jusqu'au point le plus bas : quille, safran, h\u00E9lice)\n";
        t += L"Tirant d'air : " + metresFr(size.airDraught, 2) + L" m\n";
        wchar_t buf[96];
        std::swprintf(buf, 96, L"ScaleFactor = %g    YCorrection = %g\n", size.scale, size.yCorrection);
        t += buf;
        if (size.sharing > 1) {
            t += L"M\u00EAme mod\u00E8le pour " + std::to_wstring(size.sharing) +
                L" navires du sc\u00E9nario : ils changent ensemble.\n";
        }
        t += std::wstring(size.iniFile.begin(), size.iniFile.end());
        sizeEditReadout->setText(t.c_str());
    }
    //The boxes follow the ship, except one being typed in
    if (sizeEditShownRevision != model->getSizeEditRevision()) {
        sizeEditShownRevision = model->getSizeEditRevision();
        irr::gui::IGUIElement* focus = guienv->getFocus();
        if (sizeEditLengthBox && focus != sizeEditLengthBox) {
            sizeEditLengthBox->setText(metresFr(size.length, 2).c_str());
        }
        if (sizeEditDraughtBox && focus != sizeEditDraughtBox) {
            sizeEditDraughtBox->setText(metresFr(size.draught, 2).c_str());
        }
    }
}

void GUIMain::setSizeEditorStatus(const std::wstring& text, bool isError)
{
    if (!sizeEditStatus) { return; }
    sizeEditStatus->setText(text.c_str());
    sizeEditStatus->setOverrideColor(isError ? bridge::palette().error : bridge::palette().ok);
}

bool GUIMain::getSizeEditorValue(int boxId, irr::f32& out) const
{
    const irr::gui::IGUIEditBox* box = (boxId == GUI_ID_SEDIT_LENGTH_BOX) ? sizeEditLengthBox
        : (boxId == GUI_ID_SEDIT_DRAUGHT_BOX) ? sizeEditDraughtBox : 0;
    if (!box || !box->getText()) { return false; }
    //"12,5", "12.5" and "12,5 m" all read as 12.5
    std::string t;
    for (const wchar_t* c = box->getText(); *c; c++) {
        if (*c == L',' || *c == L'.') { t += '.'; }
        else if ((*c >= L'0' && *c <= L'9') || *c == L'-') { t += (char)*c; }
        else if (*c == L' ' || *c == L'm' || *c == L'M') { continue; }
        else { return false; }
    }
    if (t.empty()) { return false; }
    const char* end = 0;
    const irr::f32 v = irr::core::fast_atof(t.c_str(), &end);
    if (!end || *end != 0) { return false; }
    out = v;
    return true;
}

//=================================================================================================
//The "Écrans éclairés" window
//=================================================================================================
void GUIMain::openInstrumentEditor()
{
    closeInstrumentEditor();
    closeLightEditor();
    closeSizeEditor();
    if (!model) { return; }

    irr::s32 fh = 16;
    if (guienv->getSkin() && guienv->getSkin()->getFont()) {
        fh = (irr::s32)guienv->getSkin()->getFont()->getDimension(L"Ag").Height;
    }
    const irr::s32 pad = fh / 2;
    const irr::s32 rowH = fh + 10;
    const irr::s32 w = fh * 26;
    const irr::s32 helpH = fh * 5;
    const irr::s32 listH = fh * 16;
    const irr::s32 statusH = fh * 3;
    const irr::s32 h = 2 * fh + helpH + pad + rowH + pad + listH + pad + 2 * (rowH + pad) + statusH + pad;
    const irr::s32 sw = (irr::s32)device->getVideoDriver()->getScreenSize().Width;
    const irr::s32 sh = (irr::s32)device->getVideoDriver()->getScreenSize().Height;
    const irr::s32 x = sw - w - fh;
    //Low on the right, clear of the instrument console along the top
    irr::s32 y = sh - h - fh * 4;
    if (y < fh * 3) { y = fh * 3; }

    instrEditWindow = guienv->addWindow(irr::core::rect<irr::s32>(x, y, x + w, y + h), false,
        L"\u00C9crans \u00E9clair\u00E9s - navire propre");
    if (instrEditWindow->getCloseButton()) { instrEditWindow->getCloseButton()->setVisible(false); }

    irr::s32 cy = 2 * fh;
    guienv->addStaticText(
        L"Cliquez sur un \u00E9cran ou un cadran dans la vue : il s'allume ou s'\u00E9teint, et clignote un "
        L"instant pour montrer ce qui a \u00E9t\u00E9 choisi. Un second clic l'annule.\n"
        L"Glisser dans la vue : regarder autour. \u00C0 essayer de nuit, \u00E9clairage \u00AB Pleins feux \u00BB.",
        irr::core::rect<irr::s32>(pad, cy, w - pad, cy + helpH), false, true, instrEditWindow);
    cy += helpH + pad;

    instrEditCount = guienv->addStaticText(L"", irr::core::rect<irr::s32>(pad, cy, w - pad, cy + rowH),
        false, false, instrEditWindow);
    instrEditCount->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_CENTER);
    cy += rowH + pad;

    //Every material of the model: [x] = lit at night
    instrEditList = guienv->addListBox(irr::core::rect<irr::s32>(pad, cy, w - pad, cy + listH),
        instrEditWindow, GUI_ID_IEDIT_LIST, true);
    cy += listH + pad;

    guienv->addButton(irr::core::rect<irr::s32>(pad, cy, w - pad, cy + rowH), instrEditWindow,
        GUI_ID_IEDIT_TOGGLE, L"Allumer / \u00E9teindre la ligne choisie",
        L"Pour ce qui est difficile \u00E0 cliquer dans la vue");
    cy += rowH + pad;

    const irr::s32 third = (w - 4 * pad) / 3;
    guienv->addButton(irr::core::rect<irr::s32>(pad, cy, pad + third, cy + rowH), instrEditWindow,
        GUI_ID_IEDIT_SAVE, L"Enregistrer", L"\u00C9crire la liste (InstrumentMaterials) dans le boat.ini du navire (copie .bak / .prev)");
    guienv->addButton(irr::core::rect<irr::s32>(2 * pad + third, cy, 2 * pad + 2 * third, cy + rowH), instrEditWindow,
        GUI_ID_IEDIT_REVERT, L"Annuler", L"Revenir aux \u00E9crans \u00E9clair\u00E9s de l'ouverture");
    guienv->addButton(irr::core::rect<irr::s32>(3 * pad + 2 * third, cy, w - pad, cy + rowH), instrEditWindow,
        GUI_ID_IEDIT_CLOSE, L"Terminer", L"Fermer (ce qui n'est pas enregistr\u00E9 reste jusqu'\u00E0 la fin du sc\u00E9nario)");
    cy += rowH + pad;

    instrEditStatus = guienv->addStaticText(L"Non enregistr\u00E9.", irr::core::rect<irr::s32>(pad, cy, w - pad, cy + statusH),
        false, true, instrEditWindow);

    instrEditShownRevision = -1;
    refreshInstrumentEditor();
}

void GUIMain::closeInstrumentEditor()
{
    if (instrEditWindow) { instrEditWindow->remove(); }
    instrEditWindow = 0;
    instrEditList = 0;
    instrEditStatus = 0;
    instrEditCount = 0;
}

void GUIMain::refreshInstrumentEditor()
{
    if (!instrEditWindow || !instrEditList || !model || !model->isInstrumentEditing()) { return; }
    if (instrEditShownRevision == model->getInstrumentEditRevision()) { return; }
    instrEditShownRevision = model->getInstrumentEditRevision();

    const irr::u32 count = model->getOwnShipMaterialCount();
    int lit = 0;
    const irr::s32 scroll = instrEditList->getVerticalScrollBar() ? instrEditList->getVerticalScrollBar()->getPos() : 0;
    instrEditList->clear();
    for (irr::u32 i = 0; i < count; i++) {
        const bool on = model->isOwnShipInstrumentMaterial(i);
        if (on) { lit++; }
        std::string tex = model->getOwnShipMaterialTexture(i);
        std::wstring label = on ? L"[x]  " : L"[  ]  ";
        label += std::to_wstring(i) + L"   ";
        label += tex.empty() ? std::wstring(L"(couleur unie, sans image)") : std::wstring(tex.begin(), tex.end());
        instrEditList->addItem(label.c_str());
    }
    if (instrEditList->getVerticalScrollBar()) { instrEditList->getVerticalScrollBar()->setPos(scroll); }
    const int selected = model->getInstrumentEditSelected();
    if (selected >= 0 && selected < (int)count) { instrEditList->setSelected(selected); }
    if (instrEditCount) {
        std::wstring t = std::to_wstring(lit) + L" \u00E9clair\u00E9(s) sur " + std::to_wstring(count) + L" mat\u00E9riaux du mod\u00E8le";
        instrEditCount->setText(t.c_str());
    }
}

void GUIMain::setInstrumentEditorStatus(const std::wstring& text, bool isError)
{
    if (!instrEditStatus) { return; }
    instrEditStatus->setText(text.c_str());
    instrEditStatus->setOverrideColor(isError ? bridge::palette().error : bridge::palette().ok);
}

void GUIMain::instrumentEditorListPicked()
{
    if (!instrEditList || !model) { return; }
    model->instrumentEditSelect(instrEditList->getSelected());
}
