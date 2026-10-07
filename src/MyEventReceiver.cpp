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

#include "MyEventReceiver.hpp"

#include <string>
#include <cstdlib> //abs

#include "GUIMain.hpp"
#include "SimulationModel.hpp"
#include "ShipLights.hpp" //KYARA FEUX
#include <iostream>
#include "Lines.hpp"
#include "Utilities.hpp"
#include "AzimuthDial.h"
#include "VRInterface.hpp"
#include "Constants.hpp"

     // using namespace irr;

MyEventReceiver::MyEventReceiver(irr::IrrlichtDevice* dev, SimulationModel* model, GUIMain* gui, Network* network, VRInterface* vrInterface, JoystickSetup joystickSetup, std::vector<std::string>* logMessages) // Constructor
{
    this->model = model; // Link to the model
    this->gui = gui;     // Link to GUI
    this->vrInterface = vrInterface; // Link to VR interface
    scrollBarPosSpeed = 0;
    scrollBarPosHeading = 0;

    // store device
    device = dev;

    //network
    net = network;

    lastShownJoystickStatus = device->getTimer()->getRealTime() - 5000;      // Show joystick raw data every 5s in log
    lastTimeAzimuth1MasterChanged = device->getTimer()->getRealTime() - 500; // Allow azimuth master to change every 500ms (debounce)
    lastTimeAzimuth2MasterChanged = device->getTimer()->getRealTime() - 500; // Allow azimuth master to change every 500ms (debounce)

    // set up joystick if present, and inform user what's available
    dev->activateJoysticks(joystickInfo);

    // Tell user about joysticks via the log
    dev->getLogger()->log(""); // add a blank line
    std::string joystickInfoMessage = "Number of joysticks detected: ";
    joystickInfoMessage.append(std::string(irr::core::stringc(joystickInfo.size()).c_str()));
    dev->getLogger()->log(joystickInfoMessage.c_str());
    for (unsigned int i = 0; i < joystickInfo.size(); i++)
    {
        // Print out name and number of each joystick
        joystickInfoMessage = "Joystick number: ";
        joystickInfoMessage.append(irr::core::stringc(i).c_str());
        joystickInfoMessage.append(", Name: ");
        joystickInfoMessage.append(std::string(joystickInfo[i].Name.c_str()));
        dev->getLogger()->log(joystickInfoMessage.c_str());
    }
    dev->getLogger()->log(""); // add a blank line

    this->joystickSetup = joystickSetup;

    // Indicate that previous joystick information hasn't been initialised
    previousJoystickPort = INFINITY; // DEE 10JAN26 note ... port thrust lever in azimuth drive
    previousJoystickStbd = INFINITY; // DEE 10JAN26 note ... stbd thrust lever in azimuth drive
    previousJoystickRudder = INFINITY;
    previousJoystickBowThruster = INFINITY;
    previousJoystickSternThruster = INFINITY;
    // DEE 10JAN23 vvvv
    //        previousJoystickAzimuthAngPort = INFINITY;
    //        previousJoystickAzimuthAngStbd = INFINITY;
    previousJoystickSchottelPort = INFINITY;
    previousJoystickSchottelStbd = INFINITY;
    previousJoystickThrustLeverPort = INFINITY;
    previousJoystickThrustLeverStbd = INFINITY;

    // DEE 10JAN23 ^^^^

    previousJoystickPOVInitialised = false;

    this->logMessages = logMessages;

    // assume mouse buttons not pressed initially
    leftMouseDown = false;
    rightMouseDown = false;

    shutdownDialogActive = false;
    acceleratorBeforeShutdownDialog = 1.0f;
    rootVisibleBeforeShutdownDialog = true;
    escapeReleasedSinceShutdownDialog = false;

    linesMode = 0;
    fireAimMode = false; // FIRE FEATURE
}

bool MyEventReceiver::OnEvent(const irr::SEvent& event)
{

    // std::cout << "Any event in receiver" << std::endl;
    // From log
    if (event.EventType == irr::EET_LOG_TEXT_EVENT)
    {
        // Store these in a global log.
        std::string eventText(event.LogEvent.Text);
        logMessages->push_back(eventText);
        return true;
    }

    // Special event used to pass VR click event for mooring lines
    if (event.EventType == irr::EET_USER_EVENT)
    {
        if ((linesMode == 1) || (linesMode == 2))
        {
            irr::core::line3df rayForLines;
            if (vrInterface->getRayFromController(&rayForLines, 1000.0)) {
                // Ray found from VR interface
                handleMooringLines(rayForLines);
            }
        }
    }

    // From mouse - keep track of button press state
    if (event.EventType == irr::EET_MOUSE_INPUT_EVENT)
    {
        if (event.MouseInput.Event == irr::EMIE_LMOUSE_PRESSED_DOWN)
        {
            leftMouseDown = true;
            // Log position of mouse click, so we can track relative movement
            mouseClickX = event.MouseInput.X;
            mouseClickY = event.MouseInput.Y;
            pressX = event.MouseInput.X;
            pressY = event.MouseInput.Y;
        }
        if (event.MouseInput.Event == irr::EMIE_LMOUSE_LEFT_UP)
        {
            leftMouseDown = false;
            //Instrument editor: a click in the view (not a drag to look round) lights what is under it
            if (model->isInstrumentEditing() &&
                abs(event.MouseInput.X - pressX) <= 4 && abs(event.MouseInput.Y - pressY) <= 4)
            {
                irr::gui::IGUIElement* rootGUIElement = device->getGUIEnvironment()->getRootGUIElement();
                irr::gui::IGUIElement* clickElement = rootGUIElement->getElementFromPoint(irr::core::position2d<irr::s32>(pressX, pressY));
                if (clickElement == rootGUIElement)
                {
                    const irr::core::line3df ray = model->getMooringRay(pressX, pressY, gui->getCompact3dView());
                    const int material = model->instrumentEditPick(ray);
                    if (material >= 0) {
                        const std::string tex = model->getOwnShipMaterialTexture((irr::u32)material);
                        std::wstring msg = model->isOwnShipInstrumentMaterial((irr::u32)material) ? L"Allum\u00E9 : " : L"\u00C9teint : ";
                        msg += std::to_wstring(material) + L"  " + (tex.empty() ? std::wstring(L"(sans image)") : std::wstring(tex.begin(), tex.end()));
                        msg += L"  - non enregistr\u00E9.";
                        gui->setInstrumentEditorStatus(msg, false);
                    }
                }
            }
        }

        if (event.MouseInput.Event == irr::EMIE_RMOUSE_PRESSED_DOWN)
        {
            rightMouseDown = true;
            // Force focus on right click
            irr::gui::IGUIElement* overElement;
            overElement = device->getGUIEnvironment()->getRootGUIElement()->getElementFromPoint(irr::core::position2d<irr::s32>(event.MouseInput.X, event.MouseInput.Y));
            if (overElement)
            {
                device->getGUIEnvironment()->setFocus(overElement);
            }
        }
        if (event.MouseInput.Event == irr::EMIE_RMOUSE_LEFT_UP)
        {
            rightMouseDown = false;
        }
        model->setMouseDown(leftMouseDown || rightMouseDown); // Set if either mouse is down

        // Mooring lines controls
        if (event.MouseInput.Event == irr::EMIE_LMOUSE_PRESSED_DOWN)
        {
            // Add line (mooring/towing) start or end if in required mode
            if ((linesMode == 1) || (linesMode == 2))
            {
                // Ignore click if over a gui element (getElementFromPoint will return root element if not over anything else)
                irr::gui::IGUIElement* rootGUIElement = device->getGUIEnvironment()->getRootGUIElement();
                irr::gui::IGUIElement* clickElement = rootGUIElement->getElementFromPoint(irr::core::position2d<irr::s32>(event.MouseInput.X, event.MouseInput.Y));
                if (clickElement == rootGUIElement)
                {
                    // Scale if required because 3d view may be different
                    irr::core::line3df rayForLines = model->getMooringRay(mouseClickX, mouseClickY, gui->getCompact3dView());
                    handleMooringLines(rayForLines);
                }
            }
        }

        if (event.MouseInput.Event == irr::EMIE_MOUSE_MOVED && leftMouseDown)
        {
            irr::gui::IGUIElement* focussedElement = device->getGUIEnvironment()->getFocus();
            if (!focussedElement)
            {
                irr::s32 deltaX = event.MouseInput.X - mouseClickX;
                irr::s32 deltaY = event.MouseInput.Y - mouseClickY;
                if (model->isLightEditing() || model->isFreeView() || model->isSizeEditing()) {
                    //KYARA FEUX EDIT: dragging in the view turns the camera round the lamp (or the ship, in free view)
                    model->lightEditOrbit(0.4f * (irr::f32)deltaX, 0.4f * (irr::f32)deltaY, 1.0f);
                }
                else {
                    model->changeLookPx(deltaX, deltaY);
                }
            }
            mouseClickX = event.MouseInput.X;
            mouseClickY = event.MouseInput.Y;
        }

        if (event.MouseInput.Event == irr::EMIE_MOUSE_WHEEL)
        {
            //Over a control (radar knob, list, scroll bar) the wheel belongs to that control. Given to
            //it directly: Irrlicht would only pass the wheel to whichever control has the focus.
            irr::gui::IGUIElement* wheelRoot = device->getGUIEnvironment()->getRootGUIElement();
            irr::gui::IGUIElement* wheelOver = wheelRoot->getElementFromPoint(irr::core::position2d<irr::s32>(event.MouseInput.X, event.MouseInput.Y));
            if (wheelOver && wheelOver != wheelRoot) {
                wheelOver->OnEvent(event);
                return true;
            }
            //KYARA FEUX EDIT: while placing lamps the wheel moves the camera in and out
            if (model->isLightEditing() || model->isFreeView() || model->isSizeEditing()) {
                model->lightEditOrbit(0.0f, 0.0f, (event.MouseInput.Wheel > 0) ? 0.85f : 1.18f);
                return true;
            }
            //KYARA: the wheel now drives the ZOOM (magnification) bar. It used to drive the
            //rudder, which is far too easy to nudge by accident while looking around.
            irr::s32 step = (event.MouseInput.Wheel > 0) ? 5 : -5;
            irr::s32 rawZoomLevel = gui->adjustMagnification(step);
            irr::f32 zoomLevel = (irr::f32)rawZoomLevel / 10.0f;

            if (rawZoomLevel > 10) {
                gui->zoomOn();
                model->setZoom(true, zoomLevel);
            }
            else {
                gui->zoomOff();
                model->setZoom(false, zoomLevel);
            }
            return true;
        }
    }



    if (event.EventType == irr::EET_GUI_EVENT)
    {
        irr::s32 id = event.GUIEvent.Caller->getID();

        if (event.GUIEvent.EventType == irr::gui::EGET_LISTBOX_SELECTED_AGAIN)
        {
            if (id == GUIMain::GUI_ID_LINES_LIST)
            {
                // Allow de-selection by double click
                if (!vrInterface->isVRActive()) {
                    // Workaround for now for VR mode, as 'SELECTED_AGAIN' always seems to run
                    ((irr::gui::IGUIListBox*)event.GUIEvent.Caller)->setSelected(-1);
                }
                model->getLines()->setSelectedLine(((irr::gui::IGUIListBox*)event.GUIEvent.Caller)->getSelected());
            }

            if (id == GUIMain::GUI_ID_ARPA_LIST || id == GUIMain::GUI_ID_BIG_ARPA_LIST)
            {
                // Allow de-selection
                int arpaSelected = -1;
                if (vrInterface->isVRActive()) {
                    // Workaround for now for VR mode, as 'SELECTED_AGAIN' always seems to run
                    arpaSelected = ((irr::gui::IGUIListBox*)event.GUIEvent.Caller)->getSelected();
                }
                gui->setARPAList(arpaSelected);
                // Set selected ID via model.
                model->setArpaListSelection(arpaSelected);
            }
        }
        // NOTE: the buoy ARPA / trail toggles used to be handled here, outside any
        // event-type guard. That meant they fired on EVERY GUI event for these buttons
        // (including EGET_ELEMENT_HOVERED / EGET_ELEMENT_LEFT), so they toggled on hover.
        // They are now handled in the EGET_BUTTON_CLICKED block below, so they only
        // respond to an actual click.

        //A length or draught typed in, then Enter
        if (event.GUIEvent.EventType == irr::gui::EGET_EDITBOX_ENTER && model->isSizeEditing() &&
            (id == GUIMain::GUI_ID_SEDIT_LENGTH_BOX || id == GUIMain::GUI_ID_SEDIT_DRAUGHT_BOX))
        {
            applySizeEditorBox(id);
            return true;
        }

        if (event.GUIEvent.EventType == irr::gui::EGET_LISTBOX_CHANGED)
        {
            //Instrument editor: a line of the list flashes its material in the view
            if (id == GUIMain::GUI_ID_IEDIT_LIST && model->isInstrumentEditing())
            {
                gui->instrumentEditorListPicked();
                device->getGUIEnvironment()->setFocus(0);
            }

            //KYARA FEUX EDIT: lamp picked in the placement window
            if (id == GUIMain::GUI_ID_LEDIT_LIST && model->isLightEditing())
            {
                ShipLights* lights = model->getShipLights(model->getLightEditVessel());
                const irr::s32 sel = ((irr::gui::IGUIListBox*)event.GUIEvent.Caller)->getSelected();
                if (lights && sel >= 0) { lights->selectEditItem(sel); }
                device->getGUIEnvironment()->setFocus(0); //so the arrow keys move the lamp, not the list
            }

            if (id == GUIMain::GUI_ID_LINES_LIST)
            {
                model->getLines()->setSelectedLine(((irr::gui::IGUIListBox*)event.GUIEvent.Caller)->getSelected());
            }

            if (id == GUIMain::GUI_ID_ARPA_LIST || id == GUIMain::GUI_ID_BIG_ARPA_LIST)
            {
                // Set coupled list
                int arpaSelected = ((irr::gui::IGUIListBox*)event.GUIEvent.Caller)->getSelected();
                gui->setARPAList(arpaSelected);

                // Set selected ID via model.
                model->setArpaListSelection(arpaSelected);
            }
        }

        if (event.GUIEvent.EventType == irr::gui::EGET_CHECKBOX_CHANGED)
        {
            if (id == GUIMain::GUI_ID_LIGHTNING_CHECKBOX)
            {
                model->setLightningEnabled(((irr::gui::IGUICheckBox*)event.GUIEvent.Caller)->isChecked());
            }
            if (id == GUIMain::GUI_ID_THUNDER_CHECKBOX)
            {
                model->setThunderEnabled(((irr::gui::IGUICheckBox*)event.GUIEvent.Caller)->isChecked());
            }
            if (id == GUIMain::GUI_ID_RADAR_ARPA_BUOYS_CHECKBOX) {
                model->setArpaOnBuoys(((irr::gui::IGUICheckBox*)event.GUIEvent.Caller)->isChecked());
            }
            if (id == GUIMain::GUI_ID_RADAR_BUOY_TRAILS_CHECKBOX) {
                model->setRadarBuoyTrails(((irr::gui::IGUICheckBox*)event.GUIEvent.Caller)->isChecked());
            }
            if (id == GUIMain::GUI_ID_RADAR_SHIP_TRAILS_CHECKBOX) {
                model->setRadarShipTrails(((irr::gui::IGUICheckBox*)event.GUIEvent.Caller)->isChecked());
            }
            if (id == GUIMain::GUI_ID_RADAR_OWN_SHIP_TRAILS_CHECKBOX) {
                model->setRadarOwnShipTrails(((irr::gui::IGUICheckBox*)event.GUIEvent.Caller)->isChecked());
            }
            if (id == GUIMain::GUI_ID_RADAR_MMSI_CHECKBOX) {
                model->setRadarMMSI(((irr::gui::IGUICheckBox*)event.GUIEvent.Caller)->isChecked());
            }
            if (id == GUIMain::GUI_ID_AZIMUTH_1_MASTER_CHECKBOX)
            {
                model->setAzimuth1Master(((irr::gui::IGUICheckBox*)event.GUIEvent.Caller)->isChecked());
            }

            if (id == GUIMain::GUI_ID_AZIMUTH_2_MASTER_CHECKBOX)
            {
                model->setAzimuth2Master(((irr::gui::IGUICheckBox*)event.GUIEvent.Caller)->isChecked());
            }

            //KYARA FEUX EDIT
            if (id == GUIMain::GUI_ID_LEDIT_MIRROR && model->isLightEditing())
            {
                ShipLights* lights = model->getShipLights(model->getLightEditVessel());
                if (lights) { lights->setMirror(((irr::gui::IGUICheckBox*)event.GUIEvent.Caller)->isChecked()); }
                device->getGUIEnvironment()->setFocus(0);
            }

            //KYARA FEUX TAB: "hide this light" boxes and the working lights, on the vessel
            //picked in the Feux tab.
            if (id >= GUIMain::GUI_ID_LIGHTS_OVR_0 && id < GUIMain::GUI_ID_LIGHTS_OVR_END)
            {
                ShipLights* lights = model->getShipLights(gui->getLightsVessel());
                if (lights) {
                    const bool hide = ((irr::gui::IGUICheckBox*)event.GUIEvent.Caller)->isChecked();
                    lights->setRoleOverride(ShipLights::overrideRole(id - GUIMain::GUI_ID_LIGHTS_OVR_0), hide);
                }
                gui->refreshLightsTab();
            }
            if (id == GUIMain::GUI_ID_LIGHTS_DECK_CHECKBOX)
            {
                ShipLights* lights = model->getShipLights(gui->getLightsVessel());
                if (lights) { lights->setDeckLights(((irr::gui::IGUICheckBox*)event.GUIEvent.Caller)->isChecked()); }
                gui->refreshLightsTab();
            }
            if (id == GUIMain::GUI_ID_OWN_DECK_LIGHTS_CHECKBOX)
            {
                model->setOwnShipDeckLights(((irr::gui::IGUICheckBox*)event.GUIEvent.Caller)->isChecked());
                gui->refreshLightsTab();
            }

            if (id == GUIMain::GUI_ID_KEEP_SLACK_LINE_CHECKBOX)
            {
                model->getLines()->setKeepSlack(
                    model->getLines()->getSelectedLine(),
                    ((irr::gui::IGUICheckBox*)event.GUIEvent.Caller)->isChecked());
            }

            if (id == GUIMain::GUI_ID_HAUL_IN_LINE_CHECKBOX)
            {
                model->getLines()->setHeaveIn(
                    model->getLines()->getSelectedLine(),
                    ((irr::gui::IGUICheckBox*)event.GUIEvent.Caller)->isChecked());
            }

            if (id == GUIMain::GUI_ID_STREAMOVERRIDE_BOX)
            {
                model->setStreamOverride(((irr::gui::IGUICheckBox*)event.GUIEvent.Caller)->isChecked());
            }

        }

        if (event.GUIEvent.EventType == irr::gui::EGET_SCROLL_BAR_CHANGED)
        {

            if (id == GUIMain::GUI_ID_HEADING_SCROLL_BAR)
            {
                scrollBarPosHeading = ((irr::gui::IGUIScrollBar*)event.GUIEvent.Caller)->getPos();
                model->setHeading(scrollBarPosHeading);
            }

            if (id == GUIMain::GUI_ID_SPEED_SCROLL_BAR)
            {
                scrollBarPosSpeed = ((irr::gui::IGUIScrollBar*)event.GUIEvent.Caller)->getPos();
                model->setSpeed(scrollBarPosSpeed);
            }

            if (id == GUIMain::GUI_ID_STBD_SCROLL_BAR)
            {
                irr::f32 value = ((irr::gui::IGUIScrollBar*)event.GUIEvent.Caller)->getPos() / -100.0; // Convert to from +-100 to +-1, and invert up/down
                model->setStbdEngine(value);
                // If right mouse button, set the other engine as well
                if (rightMouseDown)
                {
                    model->setPortEngine(value);
                }
            }
            if (id == GUIMain::GUI_ID_PORT_SCROLL_BAR)
            {
                irr::f32 value = ((irr::gui::IGUIScrollBar*)event.GUIEvent.Caller)->getPos() / -100.0; // Convert to from +-100 to +-1, and invert up/down
                model->setPortEngine(value);
                // If right mouse button, set the other engine as well
                if (rightMouseDown)
                {
                    model->setStbdEngine(value);
                }
            }

            // DEE debug this must be disabled becuase only the wheel is directly controlled
            // disbaling mouse controlling the rudder directly change it to change wheel

            if (id == GUIMain::GUI_ID_RUDDER_SCROLL_BAR)
            {
                //                        model->setRudder(((irr::gui::IGUIScrollBar*)event.GUIEvent.Caller)->getPos());
            }

            // DEE capture the wheel
            if (id == GUIMain::GUI_ID_WHEEL_SCROLL_BAR)
            {
                // Check if either NFU button is down, in which case force the change (even if the follow up rudder isn't working)
                bool nfuActive = gui->isNFUActive();
                model->setWheel(((irr::gui::IGUIScrollBar*)event.GUIEvent.Caller)->getPos(), nfuActive);
            }
            // DEE capture the wheel

            // DEE_NOV22 this deals with capturing input from the Azimuth gui
            // JP: Removed GUI inputs from GUI_ID_AZIMUTH_1 and GUI_ID_AZIMUTH_2, as I think these are intended to be display only, not for input?
            // DEE_NOV22 ^^^^ end of the inputs from the azimth GUI

            // DEE_NOV22 vvvv controls for mouse inputs to the engine rpm indicators and the schottels

            if (id == GUIMain::GUI_ID_SCHOTTEL_PORT)
            {
                // DEE_NOV22 only really want this to respond to left mouse click , no master mode
                // as in practice if you want to steer with only one schottel, you just
                // leave the other dead ahead, for small steering corrections then that
                // is adequate
                irr::f32 angle = (((irr::gui::AzimuthDial*)event.GUIEvent.Caller)->getPos()); // Range 0-360
                // not intersted in magnitude
                model->setPortSchottel(angle);
            } // end if schottel port

            if (id == GUIMain::GUI_ID_SCHOTTEL_STBD)
            {
                irr::f32 angle = (((irr::gui::AzimuthDial*)event.GUIEvent.Caller)->getPos()); // Range 0-360
                // not intersted in magnitude
                model->setStbdSchottel(angle);
            } // end if schottel stbd

            if (id == GUIMain::GUI_ID_AZIMUTH_ENGINE_PORT)
            {
                irr::f32 angle = (((irr::gui::AzimuthDial*)event.GUIEvent.Caller)->getPos()); // Range 0-360
                // we arent interested in the getMag
                // DEE_Boxing_Day_2022 vvvv

                model->setPortAzimuthThrustLever(model->inputToAzimuthEngineMapping(angle));
                //  DEE_Boxing_Day_2022 ^^^^
            } // end if engine port

            if (id == GUIMain::GUI_ID_AZIMUTH_ENGINE_STBD)
            {
                // DEE_Boxing_Day_2022 vvvv
                irr::f32 angle = (((irr::gui::AzimuthDial*)event.GUIEvent.Caller)->getPos()); // Range 0-360
                // we arent interested in the getMag
                // DEE_Boxing_Day_2022 vvvv
                model->setStbdAzimuthThrustLever(model->inputToAzimuthEngineMapping(angle));
                // DEE_Boxing_Day_2022 ^^^^
            } // end if engine port

            // DEAL WITH THRUSTER SCROLL BARS HERE - ALSO WITH JOYSTICK

            if (id == GUIMain::GUI_ID_BOWTHRUSTER_SCROLL_BAR)
            {
                irr::f32 value = ((irr::gui::IGUIScrollBar*)event.GUIEvent.Caller)->getPos() / 100.0; // Convert to from +-100 to +-1
                model->setBowThruster(value);
            }
            if (id == GUIMain::GUI_ID_STERNTHRUSTER_SCROLL_BAR)
            {
                irr::f32 value = ((irr::gui::IGUIScrollBar*)event.GUIEvent.Caller)->getPos() / 100.0; // Convert to from +-100 to +-1
                model->setSternThruster(value);
            }

            if (id == GUIMain::GUI_ID_RADAR_GAIN_SCROLL_BAR)
            {
                model->setRadarGain(((irr::gui::IGUIScrollBar*)event.GUIEvent.Caller)->getPos());
            }
            if (id == GUIMain::GUI_ID_RADAR_CLUTTER_SCROLL_BAR)
            {
                model->setRadarClutter(((irr::gui::IGUIScrollBar*)event.GUIEvent.Caller)->getPos());
            }
            if (id == GUIMain::GUI_ID_RADAR_RAIN_SCROLL_BAR)
            {
                model->setRadarRain(((irr::gui::IGUIScrollBar*)event.GUIEvent.Caller)->getPos());
            }
            if (id == GUIMain::GUI_ID_WEATHER_SCROLL_BAR)
            {
                model->setWeather(((irr::gui::IGUIScrollBar*)event.GUIEvent.Caller)->getPos() / 10.0); // Scroll bar 0-120, weather 0-12
            }
            if (id == GUIMain::GUI_ID_MOTION_SCALE_SCROLL_BAR) // KYARA HOULE
            {
                model->setMotionScale(((irr::gui::IGUIScrollBar*)event.GUIEvent.Caller)->getPos() / 100.0f); // 0-150 -> 0-1.5
            }
            if (id == GUIMain::GUI_ID_RAIN_SCROLL_BAR)
            {
                model->setRain(((irr::gui::IGUIScrollBar*)event.GUIEvent.Caller)->getPos() / 10.0); // Scroll bar 0-100, rain 0-10
            }
            if (id == GUIMain::GUI_ID_VISIBILITY_SCROLL_BAR)
            {
                model->setVisibility(((irr::gui::IGUIScrollBar*)event.GUIEvent.Caller)->getPos() / 10.0); // Scroll bar 0-100, vis near 0 to 10 nm
            }
            if (id == GUIMain::GUI_ID_WINDDIRECTION_SCROLL_BAR)
            {
                model->setWindDirection(((irr::gui::IGUIScrollBar*)event.GUIEvent.Caller)->getPos());
            }
            if (id == GUIMain::GUI_ID_WINDSPEED_SCROLL_BAR)
            {
                model->setWindSpeed(((irr::gui::IGUIScrollBar*)event.GUIEvent.Caller)->getPos());
            }
            if (id == GUIMain::GUI_ID_STREAMDIRECTION_SCROLL_BAR)
            {
                model->setStreamOverrideDirection(((irr::gui::IGUIScrollBar*)event.GUIEvent.Caller)->getPos());
            }
            if (id == GUIMain::GUI_ID_STREAMSPEED_SCROLL_BAR)
            {
                model->setStreamOverrideSpeed(((irr::gui::IGUIScrollBar*)event.GUIEvent.Caller)->getPos());
            }
            if (id == GUIMain::GUI_ID_GLOW_SCROLL_BAR)
            {
                gui->setGlowLevel(((irr::gui::IGUIScrollBar*)event.GUIEvent.Caller)->getPos());
            }
            if (id == GUIMain::GUI_ID_MAGNIFICATION_SCROLL_BAR)
            {
                irr::s32 rawZoomLevel = ((irr::gui::IGUIScrollBar*)event.GUIEvent.Caller)->getPos();
                irr::f32 zoomLevel = (irr::f32)rawZoomLevel / 10.0;
                if (rawZoomLevel > 10) {
                    gui->zoomOn();
                    model->setZoom(true, zoomLevel);
                }
                else {
                    gui->zoomOff();
                    model->setZoom(false, zoomLevel);
                }
            }

        }

        if (event.GUIEvent.EventType == irr::gui::EGET_MESSAGEBOX_OK)
        {
            if (id == GUIMain::GUI_ID_CLOSE_BOX)
            {

                net->shutdownAllSecondaries();
                device->closeDevice(); // Confirm shutdown.
            }
        }

        if (event.GUIEvent.EventType == irr::gui::EGET_MESSAGEBOX_CANCEL)
        {
            if (id == GUIMain::GUI_ID_CLOSE_BOX)
            {
                //KYARA: "Non" - the message box removes itself; we only restore the clock
                cancelShutdown();
            }
        }

        if (event.GUIEvent.EventType == irr::gui::EGET_BUTTON_CLICKED)
        {
            // ===== kyara: touches à bascule grand radar =====
            if (id == GUIMain::GUI_ID_BIG_ARPA_MODE_BUTTON) {
                irr::gui::IGUIElement* e = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(GUIMain::GUI_ID_BIG_ARPA_ON_BOX, true);
                if (e) {
                    irr::gui::IGUIComboBox* cb = (irr::gui::IGUIComboBox*)e;
                    irr::s32 next = (cb->getSelected() + 1) % 3; // Man->MARPA->ARPA
                    cb->setSelected(next);
                    model->setArpaMode(next);
                    gui->setARPAComboboxes(next);
                    irr::gui::IGUIElement* btn = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(GUIMain::GUI_ID_BIG_ARPA_MODE_BUTTON, true);
                    if (btn) btn->setText(next == 0 ? L"ARPA: Man" : next == 1 ? L"ARPA: MARPA" : L"ARPA: ARPA");
                }
            }
            if (id == GUIMain::GUI_ID_RADAR_OFFCENTRE_BUTTON) { model->toggleRadarOffCentre(); }
            if (id == GUIMain::GUI_ID_RADAR_ARPA_TAB_BUTTON) { gui->setAisDataMode(false); }
            if (id == GUIMain::GUI_ID_RADAR_AIS_TAB_BUTTON) { gui->setAisDataMode(true); }
            if (id == GUIMain::GUI_ID_BIG_ARPA_VECTOR_BUTTON) {
                irr::gui::IGUIElement* e = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(GUIMain::GUI_ID_BIG_ARPA_TRUE_REL_BOX, true);
                if (e) {
                    irr::gui::IGUIComboBox* cb = (irr::gui::IGUIComboBox*)e;
                    irr::s32 next = (cb->getSelected() == 0) ? 1 : 0; // 0=Vrai,1=Rel
                    cb->setSelected(next);
                    if (next == 0) model->setRadarARPATrue(); else model->setRadarARPARel();
                    irr::gui::IGUIElement* btn = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(GUIMain::GUI_ID_BIG_ARPA_VECTOR_BUTTON, true);
                    if (btn) btn->setText(next == 0 ? L"Vect: Vrai" : L"Vect: Rel");
                }
            }
            if (id == GUIMain::GUI_ID_RADAR_HEADING_MODE_BUTTON) {
                bool useReal = !model->getRadarHeadingMode();
                model->setRadarHeadingMode(useReal);
                irr::gui::IGUIElement* btn = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(GUIMain::GUI_ID_RADAR_HEADING_MODE_BUTTON, true);
                if (btn) btn->setText(useReal ? L"Cap: Vrai" : L"Cap: ARPA");
            }
            if (id == GUIMain::GUI_ID_RADAR_ECHO_STRETCH_BUTTON) {
                model->cycleRadarEchoStretch();
            }
            if (id == GUIMain::GUI_ID_EXIT_BUTTON)
            {
                startShutdown();
            }
            if (id == GUIMain::GUI_ID_STORM_PRESET_BUTTON)
            {
                model->setBadWeatherPreset();
            }

            //Command bar: day / dusk / night / digital / auto colours
            if (id >= GUIMain::GUI_ID_PALETTE_DAY && id <= GUIMain::GUI_ID_PALETTE_AUTO)
            {
                gui->setPaletteChoice(id == GUIMain::GUI_ID_PALETTE_AUTO ? -1 : (int)(id - GUIMain::GUI_ID_PALETTE_DAY));
                device->getGUIEnvironment()->setFocus(0);
                return true;
            }

            //Which screens and gauges of the own ship glow at night
            if (id == GUIMain::GUI_ID_INSTR_EDIT_BUTTON)
            {
                if (model->beginInstrumentEdit()) {
                    gui->openInstrumentEditor();
                }
                device->getGUIEnvironment()->setFocus(0);
                return true;
            }
            if (model->isInstrumentEditing() && id >= GUIMain::GUI_ID_IEDIT_TOGGLE && id <= GUIMain::GUI_ID_IEDIT_CLOSE)
            {
                if (id == GUIMain::GUI_ID_IEDIT_TOGGLE) {
                    const int material = model->getInstrumentEditSelected();
                    if (material >= 0) {
                        model->instrumentEditToggle(material);
                        model->instrumentEditSelect(material);
                    }
                }
                if (id == GUIMain::GUI_ID_IEDIT_SAVE) {
                    std::wstring msg;
                    const bool ok = model->instrumentEditSave(msg);
                    gui->setInstrumentEditorStatus(msg, !ok);
                }
                if (id == GUIMain::GUI_ID_IEDIT_REVERT) {
                    model->instrumentEditRevert();
                    gui->setInstrumentEditorStatus(L"\u00C9crans \u00E9clair\u00E9s de l'ouverture r\u00E9tablis (rien n'est enregistr\u00E9).", false);
                }
                if (id == GUIMain::GUI_ID_IEDIT_CLOSE) {
                    model->endInstrumentEdit();
                    gui->closeInstrumentEditor();
                }
                device->getGUIEnvironment()->setFocus(0);
                return true;
            }

            //Size and waterline of the vessel picked in the Taille tab
            if (id == GUIMain::GUI_ID_SIZE_EDIT_BUTTON)
            {
                const int vessel = gui->getSizeVessel();
                if (model->beginSizeEdit(vessel)) {
                    gui->openSizeEditor(vessel);
                }
                device->getGUIEnvironment()->setFocus(0);
                return true;
            }
            if (model->isSizeEditing() && id >= GUIMain::GUI_ID_SEDIT_LENGTH_BOX && id <= GUIMain::GUI_ID_SEDIT_CLOSE)
            {
                switch (id) {
                case GUIMain::GUI_ID_SEDIT_LENGTH_APPLY: applySizeEditorBox(GUIMain::GUI_ID_SEDIT_LENGTH_BOX); break;
                case GUIMain::GUI_ID_SEDIT_DRAUGHT_APPLY: applySizeEditorBox(GUIMain::GUI_ID_SEDIT_DRAUGHT_BOX); break;
                case GUIMain::GUI_ID_SEDIT_LENGTH_MINUS: model->sizeEditStepLength(-1.0f); break;
                case GUIMain::GUI_ID_SEDIT_LENGTH_PLUS: model->sizeEditStepLength(1.0f); break;
                case GUIMain::GUI_ID_SEDIT_RAISE: model->sizeEditStepDraught(-0.1f); break;
                case GUIMain::GUI_ID_SEDIT_LOWER: model->sizeEditStepDraught(0.1f); break;
                case GUIMain::GUI_ID_SEDIT_SAVE: {
                    std::wstring msg;
                    const bool ok = model->sizeEditSave(msg);
                    if (ok && model->getSizeEditVessel() < 0) {
                        msg += L"\nSa man\u0153uvrabilit\u00E9 suit au prochain lancement du sc\u00E9nario.";
                    }
                    gui->setSizeEditorStatus(msg, !ok);
                    break;
                }
                case GUIMain::GUI_ID_SEDIT_REVERT:
                    model->sizeEditRevert();
                    gui->setSizeEditorStatus(L"Taille et flottaison de l'ouverture r\u00E9tablies (rien n'est enregistr\u00E9).", false);
                    break;
                case GUIMain::GUI_ID_SEDIT_CLOSE:
                    model->endSizeEdit();
                    gui->closeSizeEditor();
                    break;
                default: break;
                }
                device->getGUIEnvironment()->setFocus(0); //keys go back to sizing the ship
                return true;
            }

            //KYARA FEUX EDIT: the placement editor
            if (id == GUIMain::GUI_ID_LIGHTS_EDIT_BUTTON)
            {
                const int vessel = gui->getLightsVessel();
                if (model->beginLightEdit(vessel)) {
                    gui->openLightEditor(vessel);
                }
                device->getGUIEnvironment()->setFocus(0);
                return true;
            }
            if (model->isLightEditing() &&
                (id == GUIMain::GUI_ID_LEDIT_SAVE || id == GUIMain::GUI_ID_LEDIT_REVERT ||
                 id == GUIMain::GUI_ID_LEDIT_SPACING_DOWN || id == GUIMain::GUI_ID_LEDIT_SPACING_UP ||
                 id == GUIMain::GUI_ID_LEDIT_CLOSE || id == GUIMain::GUI_ID_LEDIT_ROLE ||
                 id == GUIMain::GUI_ID_LEDIT_DELETE))
            {
                ShipLights* lights = model->getShipLights(model->getLightEditVessel());
                if (lights) {
                    if (id == GUIMain::GUI_ID_LEDIT_SAVE) {
                        std::wstring msg;
                        const bool ok = lights->saveToIni(msg);
                        gui->setLightEditorStatus(msg, !ok);
                        device->getLogger()->log(ok ? "Light editor: saved" : "Light editor: SAVE FAILED");
                    }
                    if (id == GUIMain::GUI_ID_LEDIT_REVERT) {
                        lights->revertEdit();
                        gui->setLightEditorStatus(L"Positions remises comme \u00E0 l'ouverture (rien n'est enregistr\u00E9).", false);
                    }
                    if (id == GUIMain::GUI_ID_LEDIT_ROLE) { lights->cycleSelectedRole(1); }
                    if (id == GUIMain::GUI_ID_LEDIT_DELETE && lights->deleteSelected()) {
                        gui->setLightEditorStatus(L"Feu supprim\u00E9 - d\u00E9finitif \u00E0 l'enregistrement.", false);
                    }
                    if (id == GUIMain::GUI_ID_LEDIT_SPACING_DOWN) { lights->changeSignalSpacing(-0.5f); }
                    if (id == GUIMain::GUI_ID_LEDIT_SPACING_UP) { lights->changeSignalSpacing(0.5f); }
                }
                if (id == GUIMain::GUI_ID_LEDIT_CLOSE) {
                    model->endLightEdit();
                    gui->closeLightEditor();
                }
                device->getGUIEnvironment()->setFocus(0); //keys go back to moving the lamp
                return true;
            }

            //KYARA FEUX TAB: COLREG situation of the selected vessel
            if (id >= GUIMain::GUI_ID_LIGHTS_SIT_0 && id < GUIMain::GUI_ID_LIGHTS_SIT_END)
            {
                const int vessel = gui->getLightsVessel();
                ShipLights* lights = model->getShipLights(vessel);
                if (lights) {
                    const ShipLights::Situation sit = (ShipLights::Situation)(id - GUIMain::GUI_ID_LIGHTS_SIT_0);
                    lights->setSituation(sit);
                    //Logged, so the debrief can say when the instructor changed what
                    std::string msg = (vessel < 0) ? std::string("Own ship")
                        : ("Other ship " + std::to_string(vessel + 1) + " (" + model->getOtherShipName(vessel) + ")");
                    msg.append(" lights: ").append(ShipLights::getSituationName(sit));
                    device->getLogger()->log(msg.c_str());
                }
                gui->refreshLightsTab();
            }
            if (id == GUIMain::GUI_ID_LIGHTS_OVR_RESET)
            {
                ShipLights* lights = model->getShipLights(gui->getLightsVessel());
                if (lights) { lights->clearOverrides(); }
                gui->refreshLightsTab();
            }
            //KYARA FEUX TAB: bridge instrument lighting, 0 off / 1 dim / 2 bright
            if (id >= GUIMain::GUI_ID_INSTR_LIGHTS_0 && id < GUIMain::GUI_ID_INSTR_LIGHTS_END)
            {
                model->setOwnShipInstrumentLights(id - GUIMain::GUI_ID_INSTR_LIGHTS_0);
                gui->refreshLightsTab();
            }
            // KYARA NEW HIDE CONTROLS BUTTON 
            if (id == GUIMain::GUI_ID_TOGGLE_PRIMARY_CONTROLS_BUTTON)
            {
                gui->togglePrimaryControls();
            }



            if (id == GUIMain::GUI_ID_START_BUTTON)
            {
                model->setAccelerator(1.0);
            }

            if (id == GUIMain::GUI_ID_RADAR_ONOFF_BUTTON)
            {
                model->toggleRadarOn();
            }

            // Buoy ARPA on/off - now click only (was previously firing on hover)


            if (id == GUIMain::GUI_ID_RADAR_INCREASE_BUTTON)
            {
                model->increaseRadarRange();
            }

            if (id == GUIMain::GUI_ID_RADAR_DECREASE_BUTTON)
            {
                model->decreaseRadarRange();
            }

            if (id == GUIMain::GUI_ID_BIG_RADAR_BUTTON)
            {
                gui->setLargeRadar(true);
                model->setRadarDisplayRadius(gui->getRadarPixelRadius());
                gui->hide2dInterface();
            }

            if (id == GUIMain::GUI_ID_SMALL_RADAR_BUTTON)
            {
                gui->setLargeRadar(false);
                model->setRadarDisplayRadius(gui->getRadarPixelRadius());
                gui->show2dInterface();
            }

            if (id == GUIMain::GUI_ID_SHOW_INTERFACE_BUTTON)
            {
                gui->show2dInterface();
            }

            if (id == GUIMain::GUI_ID_HIDE_INTERFACE_BUTTON)
            {
                gui->hide2dInterface();
            }

            if (id == GUIMain::GUI_ID_BINOS_INTERFACE_BUTTON)
            {
                model->setZoom(((irr::gui::IGUIButton*)event.GUIEvent.Caller)->isPressed());
            }

            if (id == GUIMain::GUI_ID_RADAR_EBL_LEFT_BUTTON)
            {
                model->decreaseRadarEBLBrg();
            }

            if (id == GUIMain::GUI_ID_RADAR_EBL_RIGHT_BUTTON)
            {
                model->increaseRadarEBLBrg();
            }

            if (id == GUIMain::GUI_ID_RADAR_EBL_UP_BUTTON)
            {
                model->increaseRadarEBLRange();
            }

            if (id == GUIMain::GUI_ID_RADAR_EBL_DOWN_BUTTON)
            {
                model->decreaseRadarEBLRange();
            }
            if (id == GUIMain::GUI_ID_RADAR_EBL_SELECT_BUTTON)
            {
                model->selectRadarEBL();
            }

            if (id == GUIMain::GUI_ID_RADAR_VRM_SELECT_BUTTON)
            {
                model->selectRadarVRM();
            }

            if (id == GUIMain::GUI_ID_RADAR_EBL_COLOUR_BUTTON)
            {
                gui->cycleEBLColour();
            }

            if (id == GUIMain::GUI_ID_RADAR_VRM_COLOUR_BUTTON)
            {
                gui->cycleVRMColour();
            }

            if (id == GUIMain::GUI_ID_RADAR_GUARD_ALARM_BUTTON)
            {
                model->cycleRadarGuardAlarm();
            }
            if (id == GUIMain::GUI_ID_RADAR_RANGE_RINGS_BUTTON)
            {
                model->cycleRadarRangeRings();
            }
            if (id == GUIMain::GUI_ID_RADAR_INCREASE_X_BUTTON)
            {
                model->increaseRadarXCursor();
            }

            if (id == GUIMain::GUI_ID_RADAR_DECREASE_X_BUTTON)
            {
                model->decreaseRadarXCursor();
            }

            if (id == GUIMain::GUI_ID_RADAR_INCREASE_Y_BUTTON)
            {
                model->increaseRadarYCursor();
            }

            if (id == GUIMain::GUI_ID_RADAR_DECREASE_Y_BUTTON)
            {
                model->decreaseRadarYCursor();
            }

            if (id == GUIMain::GUI_ID_RADAR_COLOUR_BUTTON)
            {
                model->changeRadarColourChoice();
            }

            // Radar mode buttons
            if (id == GUIMain::GUI_ID_RADAR_NORTH_BUTTON)
            {
                model->setRadarNorthUp();
            }
            if (id == GUIMain::GUI_ID_RADAR_COURSE_BUTTON)
            {
                model->setRadarCourseUp();
            }
            if (id == GUIMain::GUI_ID_RADAR_HEAD_BUTTON)
            {
                model->setRadarHeadUp();
            }

            // Manual/MARPA acquire/update
            if (id == GUIMain::GUI_ID_MANUAL_SCAN_BUTTON)
            {
                if (model->getArpaMode() == 0)
                {
                    model->addManualPoint(false);
                }
                // Don't do anything in full ARPA mode (as updated automatically)
            }

            if (id == GUIMain::GUI_ID_MANUAL_NEW_BUTTON)
            {
                if (model->getArpaMode() == 0)
                {
                    model->addManualPoint(true);
                }
                else if (model->getArpaMode() == 1)
                {
                    model->trackTargetFromCursor();
                }
                // TODO: Should we allow user to trigger manual tracking in full ARPA?
            }

            if (id == GUIMain::GUI_ID_MANUAL_CLEAR_BUTTON)
            {
                if (model->getArpaMode() == 0)
                {
                    model->clearManualPoints();
                }
                else if (model->getArpaMode() == 1)
                {
                    model->clearTargetFromCursor();
                }
                // TODO: Should we allow user to manually stop tracking in full ARPA?
                // And should this allow clearing of manual target if acquired in manual mode, but switched to MARPA/ARPA
            }

            if (id == GUIMain::GUI_ID_SHOW_LOG_BUTTON)
            {
                gui->showLogWindow();
            }

            if (id == GUIMain::GUI_ID_DETACH_CONSOLE_BUTTON)
            {
                gui->toggleConsoleDetached();
            }

            if (id == GUIMain::GUI_ID_HIDE_EXTRA_CONTROLS_BUTTON)
            {
                gui->setExtraControlsWindowVisible(false);
            }

            if (id == GUIMain::GUI_ID_SHOW_EXTRA_CONTROLS_BUTTON)
            {
                gui->setExtraControlsWindowVisible(true);
            }

            if (id == GUIMain::GUI_ID_HIDE_LINES_CONTROLS_BUTTON)
            {
                gui->setLinesControlsWindowVisible(false);
            }

            if (id == GUIMain::GUI_ID_SHOW_LINES_CONTROLS_BUTTON)
            {
                gui->setLinesControlsWindowVisible(true);
            }

            if (id == GUIMain::GUI_ID_RUDDERPUMP_1_WORKING_BUTTON)
            {
                model->setRudderPumpState(1, true);
                if (model->getRudderPumpState(2))
                {
                    model->setAlarm(false); // Only turn off alarm if other pump is working
                }
            }

            if (id == GUIMain::GUI_ID_RUDDERPUMP_1_FAILED_BUTTON)
            {
                model->setRudderPumpState(1, false);
                model->setAlarm(true);
            }

            if (id == GUIMain::GUI_ID_RUDDERPUMP_2_WORKING_BUTTON)
            {
                model->setRudderPumpState(2, true);
                if (model->getRudderPumpState(1))
                {
                    model->setAlarm(false); // Only turn off alarm if other pump is working
                }
            }

            if (id == GUIMain::GUI_ID_RUDDERPUMP_2_FAILED_BUTTON)
            {
                model->setRudderPumpState(2, false);
                model->setAlarm(true);
            }

            if (id == GUIMain::GUI_ID_FOLLOWUP_WORKING_BUTTON)
            {
                model->setFollowUpRudderWorking(true);
            }

            if (id == GUIMain::GUI_ID_FOLLOWUP_FAILED_BUTTON)
            {
                model->setFollowUpRudderWorking(false);
            }

            if (id == GUIMain::GUI_ID_ACK_ALARMS_BUTTON)
            {
                model->setAlarm(false);
            }

            if (id == GUIMain::GUI_ID_ADD_LINE_BUTTON)
            {
                linesMode = 1;
                model->addLine();
                gui->setLinesControlsText("Click in 3d view to set start position for line (on own ship)"); // TODO: Add translation
            }

            if (id == GUIMain::GUI_ID_REMOVE_LINE_BUTTON)
            {
                model->getLines()->removeLine(model->getLines()->getSelectedLine());
            }

            if (id == GUIMain::GUI_ID_CHANGE_VIEW_BUTTON)
            {
                model->changeView();
            }

        } // Button clicked

        if (event.GUIEvent.EventType == irr::gui::EGET_COMBO_BOX_CHANGED)
        {

            //KYARA FEUX TAB: another vessel picked - show her state, change nothing
            if (id == GUIMain::GUI_ID_LIGHTS_VESSEL_COMBO)
            {
                gui->refreshLightsTab();
            }

            if ((id == GUIMain::GUI_ID_ARPA_ON_BOX || id == GUIMain::GUI_ID_BIG_ARPA_ON_BOX))
            {
                // ARPA on/off options
                irr::s32 boxState = ((irr::gui::IGUIComboBox*)event.GUIEvent.Caller)->getSelected();
                model->setArpaMode(boxState);

                // Set the linked checkbox (big/small radar window)
                gui->setARPAComboboxes(boxState);
            }

            if (id == GUIMain::GUI_ID_ARPA_TRUE_REL_BOX || id == GUIMain::GUI_ID_BIG_ARPA_TRUE_REL_BOX)
            {
                irr::s32 selected = ((irr::gui::IGUIComboBox*)event.GUIEvent.Caller)->getSelected();
                if (selected == 0)
                {
                    model->setRadarARPATrue();
                }
                else if (selected == 1)
                {
                    model->setRadarARPARel();
                }

                // Set both linked inputs - brute force
                irr::gui::IGUIElement* other = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(GUIMain::GUI_ID_ARPA_TRUE_REL_BOX, true);
                if (other != 0)
                {
                    ((irr::gui::IGUIComboBox*)other)->setSelected(((irr::gui::IGUIComboBox*)event.GUIEvent.Caller)->getSelected());
                }
                other = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(GUIMain::GUI_ID_BIG_ARPA_TRUE_REL_BOX, true);
                if (other != 0)
                {
                    ((irr::gui::IGUIComboBox*)other)->setSelected(((irr::gui::IGUIComboBox*)event.GUIEvent.Caller)->getSelected());
                }
            }

        } // Combo box

        if (event.GUIEvent.EventType == irr::gui::EGET_COMBO_BOX_CHANGED)
        {
            // Marker colour selectors for buoys / ships. The selected index maps
            // directly to a fixed colour palette inside RadarCalculation.
            if (id == GUIMain::GUI_ID_RADAR_BUOY_COLOUR_BOX)
            {
                irr::s32 selected = ((irr::gui::IGUIComboBox*)event.GUIEvent.Caller)->getSelected();
                model->setRadarBuoyMarkerColour(selected);
            }
            if (id == GUIMain::GUI_ID_RADAR_SHIP_COLOUR_BOX)
            {
                irr::s32 selected = ((irr::gui::IGUIComboBox*)event.GUIEvent.Caller)->getSelected();
                model->setRadarShipMarkerColour(selected);
            }
        } // Marker colour combo boxes

        //KYARA LIGHTING TIME: must be its own top-level block. Do NOT nest it inside the ARPA
        //vector-time-box block - that block only runs when id is an ARPA box, so a lighting
        //box id could never reach it.
        //FOCUS_LOST is included as well as ENTER, so clicking away from the box also applies
        //the time rather than silently discarding it.
        if (id == GUIMain::GUI_ID_LIGHTING_TIME_BOX &&
            (event.GUIEvent.EventType == irr::gui::EGET_EDITBOX_ENTER ||
                event.GUIEvent.EventType == irr::gui::EGET_ELEMENT_FOCUS_LOST))
        {
            irr::core::stringc text(((irr::gui::IGUIEditBox*)event.GUIEvent.Caller)->getText());

            irr::f32 hours = 12.0f;
            int hh = 0, mm = 0;

            if (sscanf_s(text.c_str(), "%d:%d", &hh, &mm) >= 1) {
                hours = (irr::f32)hh + (irr::f32)mm / 60.0f;
            }
            model->setLightingTimeOfDay(hours);
        }
        if ((id == GUIMain::GUI_ID_ARPA_VECTOR_TIME_BOX || id == GUIMain::GUI_ID_BIG_ARPA_VECTOR_TIME_BOX) && (event.GUIEvent.EventType == irr::gui::EGET_EDITBOX_ENTER || event.GUIEvent.EventType == irr::gui::EGET_ELEMENT_FOCUS_LOST))
        {

            std::wstring boxWString = std::wstring(((irr::gui::IGUIEditBox*)event.GUIEvent.Caller)->getText());
            std::string boxString(boxWString.begin(), boxWString.end());
            irr::f32 value = Utilities::lexical_cast<irr::f32>(boxString);

            if (value > 0 && value <= 60)
            {
                model->setRadarARPAVectors(value);
            }
            else
            {
                event.GUIEvent.Caller->setText(L"Invalid");
            }

            // Set both linked inputs - brute force
            irr::gui::IGUIElement* other = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(GUIMain::GUI_ID_ARPA_VECTOR_TIME_BOX, true);
            if (other != 0)
            {
                other->setText(event.GUIEvent.Caller->getText());
            }
            other = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(GUIMain::GUI_ID_BIG_ARPA_VECTOR_TIME_BOX, true);
            if (other != 0)
            {
                other->setText(event.GUIEvent.Caller->getText());
            }
        }

        // Radar PI controls
        // PI selected
        if (event.GUIEvent.EventType == irr::gui::EGET_COMBO_BOX_CHANGED)
        {
            if (id == GUIMain::GUI_ID_PI_SELECT_BOX || id == GUIMain::GUI_ID_BIG_PI_SELECT_BOX)
            {

                // Set to match
                if (id == GUIMain::GUI_ID_PI_SELECT_BOX)
                { // Selected on small screen
                    irr::gui::IGUIElement* other = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(GUIMain::GUI_ID_BIG_PI_SELECT_BOX, true);
                    if (other != 0)
                    {
                        ((irr::gui::IGUIComboBox*)other)->setSelected(((irr::gui::IGUIComboBox*)event.GUIEvent.Caller)->getSelected());
                    }
                }
                else
                { // Selected on big screen
                    irr::gui::IGUIElement* other = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(GUIMain::GUI_ID_PI_SELECT_BOX, true);
                    if (other != 0)
                    {
                        ((irr::gui::IGUIComboBox*)other)->setSelected(((irr::gui::IGUIComboBox*)event.GUIEvent.Caller)->getSelected());
                    }
                }

                // Get PI data for the newly selected PI
                irr::s32 selectedPI = ((irr::gui::IGUIComboBox*)event.GUIEvent.Caller)->getSelected(); //(-1 or 0-9)
                // TODO: Use this to get data from model, and set fields
                irr::gui::IGUIElement* piBrg = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(GUIMain::GUI_ID_PI_BEARING_BOX, true);
                irr::gui::IGUIElement* piRng = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(GUIMain::GUI_ID_PI_RANGE_BOX, true);
                irr::gui::IGUIElement* piBrgBig = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(GUIMain::GUI_ID_BIG_PI_BEARING_BOX, true);
                irr::gui::IGUIElement* piRngBig = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(GUIMain::GUI_ID_BIG_PI_RANGE_BOX, true);
                if (piBrg && piRng && piBrgBig && piRngBig)
                {
                    piBrg->setText(f32To3dp(model->getPIbearing(selectedPI), true).c_str());
                    piBrgBig->setText(f32To3dp(model->getPIbearing(selectedPI), true).c_str());
                    piRng->setText(f32To3dp(model->getPIrange(selectedPI), true).c_str());
                    piRngBig->setText(f32To3dp(model->getPIrange(selectedPI), true).c_str());
                }
            }
        }

        if (event.GUIEvent.EventType == irr::gui::EGET_EDITBOX_CHANGED || event.GUIEvent.EventType == irr::gui::EGET_EDITBOX_ENTER || event.GUIEvent.EventType == irr::gui::EGET_ELEMENT_FOCUS_LOST)
        {

            // Bearing/range boxes:
            if (id == GUIMain::GUI_ID_PI_BEARING_BOX || id == GUIMain::GUI_ID_BIG_PI_BEARING_BOX || id == GUIMain::GUI_ID_PI_RANGE_BOX || id == GUIMain::GUI_ID_BIG_PI_RANGE_BOX)
            {
                // Make the controls match
                if (id == GUIMain::GUI_ID_PI_BEARING_BOX)
                {
                    irr::gui::IGUIElement* other = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(GUIMain::GUI_ID_BIG_PI_BEARING_BOX, true);
                    if (other != 0)
                    {
                        other->setText(event.GUIEvent.Caller->getText());
                    }
                }
                if (id == GUIMain::GUI_ID_BIG_PI_BEARING_BOX)
                {
                    irr::gui::IGUIElement* other = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(GUIMain::GUI_ID_PI_BEARING_BOX, true);
                    if (other != 0)
                    {
                        other->setText(event.GUIEvent.Caller->getText());
                    }
                }
                if (id == GUIMain::GUI_ID_PI_RANGE_BOX)
                {
                    irr::gui::IGUIElement* other = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(GUIMain::GUI_ID_BIG_PI_RANGE_BOX, true);
                    if (other != 0)
                    {
                        other->setText(event.GUIEvent.Caller->getText());
                    }
                }
                if (id == GUIMain::GUI_ID_BIG_PI_RANGE_BOX)
                {
                    irr::gui::IGUIElement* other = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(GUIMain::GUI_ID_PI_RANGE_BOX, true);
                    if (other != 0)
                    {
                        other->setText(event.GUIEvent.Caller->getText());
                    }
                }

                if (event.GUIEvent.EventType == irr::gui::EGET_EDITBOX_ENTER || event.GUIEvent.EventType == irr::gui::EGET_ELEMENT_FOCUS_LOST)
                {
                    // Use the result
                    irr::gui::IGUIElement* piCombo = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(GUIMain::GUI_ID_PI_SELECT_BOX, true);
                    irr::gui::IGUIElement* piBrg = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(GUIMain::GUI_ID_PI_BEARING_BOX, true);
                    irr::gui::IGUIElement* piRng = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(GUIMain::GUI_ID_PI_RANGE_BOX, true);
                    if (piCombo && piBrg && piRng)
                    {
                        irr::s32 selectedPI = ((irr::gui::IGUIComboBox*)piCombo)->getSelected(); //(0-9)

                        std::wstring brgWString = std::wstring(piBrg->getText());
                        std::string brgString(brgWString.begin(), brgWString.end());
                        irr::f32 bearingChosen = Utilities::lexical_cast<irr::f32>(brgString);

                        std::wstring rngWString = std::wstring(piRng->getText());
                        std::string rngString(rngWString.begin(), rngWString.end());
                        irr::f32 rangeChosen = Utilities::lexical_cast<irr::f32>(rngString);

                        // Apply to model
                        model->setPIData(selectedPI, bearingChosen, rangeChosen);
                    }
                }
            }
        }

    } // GUI Event

    // From keyboard
    //KYARA: while the quit dialog is open the simulation is frozen and its keys are ignored
    //(otherwise e.g. '1' would restart the clock behind the dialog).
    //  Esc again  -> same as "Non": close the dialog, resume
    //  Entree     -> passed through to the dialog ("Oui")
    if (shutdownDialogActive && event.EventType == irr::EET_KEY_INPUT_EVENT)
    {
        if (event.KeyInput.Key == irr::KEY_RETURN) {
            return false;
        }
        if (!event.KeyInput.PressedDown && event.KeyInput.Key == irr::KEY_ESCAPE) {
            escapeReleasedSinceShutdownDialog = true;
        }
        //Only a NEW press of Esc closes it - holding the key that opened it must not
        if (event.KeyInput.PressedDown && event.KeyInput.Key == irr::KEY_ESCAPE && escapeReleasedSinceShutdownDialog) {
            irr::gui::IGUIElement* box = device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(GUIMain::GUI_ID_CLOSE_BOX, true);
            if (box) {
                box->remove(); //its modal backdrop removes itself with it
            }
            cancelShutdown();
        }
        return true;
    }

    //While sizing a vessel, Up / Down change her length and Page Up / Down lift or sink
    //her; A/D, W/S, Q/E turn and zoom the camera. Taken here so none of them also steers the ship.
    irr::gui::IGUIElement* sizeFocus = device->getGUIEnvironment()->getFocus();
    if (event.EventType == irr::EET_KEY_INPUT_EVENT && model->isSizeEditing() &&
        !(sizeFocus && sizeFocus->getType() == irr::gui::EGUIET_EDIT_BOX)) //typing in a box wins
    {
        const irr::EKEY_CODE k = event.KeyInput.Key;
        const bool ours = (k == irr::KEY_UP || k == irr::KEY_DOWN || k == irr::KEY_PRIOR || k == irr::KEY_NEXT ||
            k == irr::KEY_KEY_A || k == irr::KEY_KEY_D || k == irr::KEY_KEY_W || k == irr::KEY_KEY_S ||
            k == irr::KEY_KEY_Q || k == irr::KEY_KEY_E);
        if (ours) {
            if (event.KeyInput.PressedDown) {
                const irr::f32 lenStep = event.KeyInput.Shift ? 0.1f : (event.KeyInput.Control ? 10.0f : 1.0f);
                const irr::f32 wlStep = event.KeyInput.Shift ? 0.01f : (event.KeyInput.Control ? 0.5f : 0.05f);
                switch (k) {
                case irr::KEY_UP:    model->sizeEditStepLength(lenStep); break;
                case irr::KEY_DOWN:  model->sizeEditStepLength(-lenStep); break;
                case irr::KEY_PRIOR: model->sizeEditStepDraught(-wlStep); break; //up out of the water
                case irr::KEY_NEXT:  model->sizeEditStepDraught(wlStep); break;
                case irr::KEY_KEY_A: model->lightEditOrbit(-5.0f, 0.0f, 1.0f); break;
                case irr::KEY_KEY_D: model->lightEditOrbit(5.0f, 0.0f, 1.0f); break;
                case irr::KEY_KEY_W: model->lightEditOrbit(0.0f, 5.0f, 1.0f); break;
                case irr::KEY_KEY_S: model->lightEditOrbit(0.0f, -5.0f, 1.0f); break;
                case irr::KEY_KEY_Q: model->lightEditOrbit(0.0f, 0.0f, 0.9f); break;
                case irr::KEY_KEY_E: model->lightEditOrbit(0.0f, 0.0f, 1.1f); break;
                default: break;
                }
            }
            return true; //the key-up too, so nothing downstream sees half a key press
        }
    }

    //KYARA FEUX EDIT: while placing lamps, the arrows / Page keys move the selected lamp in the
    //ship's frame, Tab picks the next one, and A/D, W/S, Q/E turn and zoom the camera. Taken here,
    //before the normal key handling, so none of them also steers the ship or turns the bridge view.
    irr::gui::IGUIElement* editFocus = device->getGUIEnvironment()->getFocus();
    if (event.EventType == irr::EET_KEY_INPUT_EVENT && model->isLightEditing() &&
        !(editFocus && editFocus->getType() == irr::gui::EGUIET_EDIT_BOX)) //typing in a box wins
    {
        ShipLights* lights = model->getShipLights(model->getLightEditVessel());
        const irr::EKEY_CODE k = event.KeyInput.Key;
        const bool ours = (k == irr::KEY_UP || k == irr::KEY_DOWN || k == irr::KEY_LEFT || k == irr::KEY_RIGHT ||
            k == irr::KEY_PRIOR || k == irr::KEY_NEXT || k == irr::KEY_TAB ||
            k == irr::KEY_KEY_A || k == irr::KEY_KEY_D || k == irr::KEY_KEY_W || k == irr::KEY_KEY_S ||
            k == irr::KEY_KEY_Q || k == irr::KEY_KEY_E || k == irr::KEY_KEY_R || k == irr::KEY_DELETE);
        if (lights && ours) {
            if (event.KeyInput.PressedDown) {
                const irr::f32 step = event.KeyInput.Shift ? 0.05f : (event.KeyInput.Control ? 1.0f : 0.25f);
                switch (k) {
                case irr::KEY_UP:    lights->moveSelected(step, 0.0f, 0.0f); break;
                case irr::KEY_DOWN:  lights->moveSelected(-step, 0.0f, 0.0f); break;
                case irr::KEY_LEFT:  lights->moveSelected(0.0f, -step, 0.0f); break;
                case irr::KEY_RIGHT: lights->moveSelected(0.0f, step, 0.0f); break;
                case irr::KEY_PRIOR: lights->moveSelected(0.0f, 0.0f, step); break;
                case irr::KEY_NEXT:  lights->moveSelected(0.0f, 0.0f, -step); break;
                case irr::KEY_TAB:
                    lights->selectEditItem(lights->getSelectedEditItem() + (event.KeyInput.Shift ? -1 : 1));
                    break;
                case irr::KEY_KEY_A: model->lightEditOrbit(-5.0f, 0.0f, 1.0f); break;
                case irr::KEY_KEY_D: model->lightEditOrbit(5.0f, 0.0f, 1.0f); break;
                case irr::KEY_KEY_W: model->lightEditOrbit(0.0f, 5.0f, 1.0f); break;
                case irr::KEY_KEY_S: model->lightEditOrbit(0.0f, -5.0f, 1.0f); break;
                case irr::KEY_KEY_Q: model->lightEditOrbit(0.0f, 0.0f, 0.9f); break;
                case irr::KEY_KEY_E: model->lightEditOrbit(0.0f, 0.0f, 1.1f); break;
                case irr::KEY_KEY_R: lights->cycleSelectedRole(event.KeyInput.Shift ? -1 : 1); break;
                case irr::KEY_DELETE: lights->deleteSelected(); break;
                default: break;
                }
            }
            return true; //the key-up too, so nothing downstream sees half a key press
        }
    }

    if (event.EventType == irr::EET_KEY_INPUT_EVENT && event.KeyInput.PressedDown)
    {
        // Check here that there isn't focus on a GUI edit box. If we are, don't process key inputs here.
        irr::gui::IGUIElement* focussedElement = device->getGUIEnvironment()->getFocus();
        if (!(focussedElement && focussedElement->getType() == irr::gui::EGUIET_EDIT_BOX))
        {

            //KYARA FEUX: shortcuts for the own ship, kept alongside the Feux / Eclairage tabs (which
            //they keep in sync). CTRL+SHIFT, because a plain L is already the starboard azipod schottel.
            //  Ctrl+Shift+L : cycle the own ship's COLREG situation
            //  Ctrl+Shift+K : deck and accommodation working lights on/off
            //  Ctrl+Shift+J : bridge instrument lighting off / dim / bright
            if (event.KeyInput.Shift && event.KeyInput.Control && event.KeyInput.Key == irr::KEY_KEY_J)
            {
                //Bridge instrument lighting: off / dim / bright
                const int next = (model->getOwnShipInstrumentLights() + 1) % 3;
                model->setOwnShipInstrumentLights(next);
                const char* names[3] = { "Instrument lights OFF", "Instrument lights DIM", "Instrument lights BRIGHT" };
                device->getLogger()->log(names[next]);
                std::cout << names[next] << std::endl;
                gui->refreshLightsTab();
                return true;
            }

            if (event.KeyInput.Shift && event.KeyInput.Control &&
                ((KYARA_COLREG_ENABLED && event.KeyInput.Key == irr::KEY_KEY_L) ||
                    event.KeyInput.Key == irr::KEY_KEY_K))
            {
                if (event.KeyInput.Key == irr::KEY_KEY_K) {
                    model->setOwnShipDeckLights(!model->getOwnShipDeckLights());
                    device->getLogger()->log(model->getOwnShipDeckLights() ? "Deck lights ON" : "Deck lights OFF");
                }
                else {
                    const int next = (model->getOwnShipLightSituation() + 1) % (int)ShipLights::SIT_COUNT;
                    model->setOwnShipLightSituation(next);
                    //Feedback, so it is obvious the key arrived even when the change is subtle
                    std::string msg = "Own ship lights: ";
                    msg.append(ShipLights::getSituationName((ShipLights::Situation)next));
                    device->getLogger()->log(msg.c_str());
                    std::cout << msg << std::endl;
                }
                gui->refreshLightsTab();
                return true;
            }

            if (event.KeyInput.Shift && event.KeyInput.Control)
            {

                switch (event.KeyInput.Key)
                {
                    // Move camera
                case irr::KEY_UP:
                    device->getGUIEnvironment()->setFocus(0); // Remove focus if space key is pressed, otherwise we get weird effects when the user changes view (as space bar toggles focussed GUI element)
                    model->moveCameraForwards();
                    break;
                case irr::KEY_DOWN:
                    device->getGUIEnvironment()->setFocus(0); // Remove focus if space key is pressed, otherwise we get weird effects when the user changes view (as space bar toggles focussed GUI element)
                    model->moveCameraBackwards();
                    break;
                case irr::KEY_SPACE:
                    device->getGUIEnvironment()->setFocus(0); // Remove focus if space key is pressed, otherwise we get weird effects when the user changes view (as space bar toggles focussed GUI element)
                    model->toggleFrozenCamera();
                    break;
                default:
                    // don't do anything
                    break;
                }
            }
            else if (event.KeyInput.Shift)
            {
                // Shift down

                switch (event.KeyInput.Key)
                {
                    // Camera look
                case irr::KEY_LEFT:
                    device->getGUIEnvironment()->setFocus(0); // Remove focus if space key is pressed, otherwise we get weird effects when the user changes view (as space bar toggles focussed GUI element)
                    model->lookStepLeft();
                    break;
                case irr::KEY_RIGHT:
                    device->getGUIEnvironment()->setFocus(0); // Remove focus if space key is pressed, otherwise we get weird effects when the user changes view (as space bar toggles focussed GUI element)
                    model->lookStepRight();
                    break;
                case irr::KEY_SPACE:
                    device->getGUIEnvironment()->setFocus(0); // Remove focus if space key is pressed, otherwise we get weird effects when the user changes view (as space bar toggles focussed GUI element)
                    model->changeView();
                    model->setMoveViewWithPrimary(false); // Don't allow the view to change automatically after this
                    break;
                default:
                    // don't do anything
                    break;
                }
            }
            else if (event.KeyInput.Control)
            {
                // Ctrl down

                switch (event.KeyInput.Key)
                {
                    // Camera look
                case irr::KEY_UP:
                    device->getGUIEnvironment()->setFocus(0); // Remove focus if space key is pressed, otherwise we get weird effects when the user changes view (as space bar toggles focussed GUI element)
                    model->lookAhead();
                    break;
                case irr::KEY_DOWN:
                    device->getGUIEnvironment()->setFocus(0); // Remove focus if space key is pressed, otherwise we get weird effects when the user changes view (as space bar toggles focussed GUI element)
                    model->lookAstern();
                    break;
                case irr::KEY_LEFT:
                    device->getGUIEnvironment()->setFocus(0); // Remove focus if space key is pressed, otherwise we get weird effects when the user changes view (as space bar toggles focussed GUI element)
                    model->lookPort();                        // DEENOV22 TODO make all screens lookPort
                    break;
                case irr::KEY_RIGHT:
                    device->getGUIEnvironment()->setFocus(0); // Remove focus if space key is pressed, otherwise we get weird effects when the user changes view (as space bar toggles focussed GUI element)
                    model->lookStbd();
                    break;
                case irr::KEY_KEY_M:
                    model->retrieveManOverboard();
                    break;
                case irr::KEY_KEY_F:
                    // FIRE FEATURE: ignite the nearest other ship (instructor trigger)
                    model->igniteNearestOtherShipFire();
                    break;
                case irr::KEY_KEY_E:
                    // FIRE FEATURE: toggle the water monitor (auto-aims at the fire)
                    model->toggleMonitorFiring();
                    break;
                case irr::KEY_KEY_A:
                    // Inc 3 (comms): log the next standard distress-comms action
                    model->advanceComms();
                    break;
                }
            }
            else
            {
                // Shift and Ctrl not down

                switch (event.KeyInput.Key)
                {
                    // Accelerator
                case irr::KEY_KEY_0:
                    model->setAccelerator(0.0);
                    break;
                case irr::KEY_RETURN:
                    model->setAccelerator(1.0);
                    break;
                case irr::KEY_KEY_1:
                    model->setAccelerator(1.0);
                    break;
                case irr::KEY_KEY_2:
                    model->setAccelerator(2.0);
                    break;
                case irr::KEY_KEY_3:
                    model->setAccelerator(5.0);
                    break;
                case irr::KEY_KEY_4:
                    model->setAccelerator(15.0);
                    break;
                case irr::KEY_KEY_5:
                    model->setAccelerator(30.0);
                    break;
                case irr::KEY_KEY_6:
                    model->setAccelerator(60.0);
                    break;
                case irr::KEY_KEY_7:
                    model->setAccelerator(3600.0);
                    break;
                case irr::KEY_KEY_H:
                    model->startHorn();
                    break;
                case irr::KEY_KEY_R:            //kyara: anneaux de portée clair/faible/off
                    model->cycleRadarRangeRings();
                    break;

                    // DEE_NOV22 vvvvv

                    // only assign these key bindings if this is an Azimuth Drive

                    // Purpose of this code is to allow keyboard control of the azipods
                    // they should be ineffective if the ship does not have azipods however
                    // I shall put this in the model object

                    // ultimately there should be a choice of Non followup mode and Follow up mode
                    // so they keys control the movement of the schottels in follow up mode
                    // the pod's azimuth then chases the commanded azimuth
                    // and the engine chases each thrust lever and clutches in and out automatically when thresholds are reached
                    // there can be no direct engine in reverse

                    // todo also need to model the shetland trader type case of when going forward they act as a steering wheel
                    // rather than a tiller, which could be the normal case

                    // key map for this currently is as follows
                    // Port Azipod : A pod anticlockwise , D pod clockwise, W thrust lever forward, S thrust lever backwards
                    // Starboard Azipod : J pod anticlockwise, L pod clockwise, I thrust lever forward, K thrust lever backwards

                    // the if ASDs are left in the below controls so that the keys can be assigned to something else on a
                    // non azipod ship in the future

                case irr::KEY_KEY_W:
                    if (model->isAzimuthDrive())
                    {
                        // Port Azipod Thrust lever increase
                        model->btnIncrementPortThrustLever();
                    }
                    break;

                    // KEY_KEY_S ... decrement port thrust is further down the code as it has a duplicate use

                case irr::KEY_KEY_J:
                    if (model->isAzimuthDrive())
                    {
                        // Starboard Schottel anticlockwise decrement
                        model->btnDecrementStbdSchottel();
                    }
                    break;

                case irr::KEY_KEY_L:
                    if (model->isAzimuthDrive())
                    {
                        // Starboard Azipod Schottel clockwise increment
                        model->btnIncrementStbdSchottel();
                    }
                    break;

                case irr::KEY_KEY_I:
                    if (model->isAzimuthDrive())
                    {
                        // Starboard Azipod Thurst lever increase
                        model->btnIncrementStbdThrustLever();
                    }
                    break;

                case irr::KEY_KEY_K:
                    if (model->isAzimuthDrive())
                    {
                        // Starboard Azipod Thrust lever decrease
                        model->btnDecrementStbdThrustLever();
                    }
                    break;

                    // DEE_NOV22 ^^^^^

                // Camera look
                case irr::KEY_UP:
                    device->getGUIEnvironment()->setFocus(0); // Remove focus if space key is pressed, otherwise we get weird effects when the user changes view (as space bar toggles focussed GUI element)
                    model->lookUp();
                    break;
                case irr::KEY_DOWN:
                    device->getGUIEnvironment()->setFocus(0); // Remove focus if space key is pressed, otherwise we get weird effects when the user changes view (as space bar toggles focussed GUI element)
                    model->lookDown();
                    break;
                case irr::KEY_LEFT:
                    device->getGUIEnvironment()->setFocus(0); // Remove focus if space key is pressed, otherwise we get weird effects when the user changes view (as space bar toggles focussed GUI element)
                    model->lookLeft();
                    break;
                case irr::KEY_RIGHT:
                    device->getGUIEnvironment()->setFocus(0); // Remove focus if space key is pressed, otherwise we get weird effects when the user changes view (as space bar toggles focussed GUI element)
                    model->lookRight();
                    break;
                case irr::KEY_SPACE:
                    device->getGUIEnvironment()->setFocus(0); // Remove focus if space key is pressed, otherwise we get weird effects when the user changes view (as space bar toggles focussed GUI element)
                    model->changeView();
                    model->setMoveViewWithPrimary(true); // Allow the view to change automatically after this
                    break;

                    // toggle full screen 3d
                case irr::KEY_KEY_F:
                    gui->toggleShow2dInterface();
                    break;

                    // Quit with esc or F4 (for alt-F4)
                case irr::KEY_ESCAPE:
                case irr::KEY_F4:
                    startShutdown();
                    //KYARA: Esc is still held down - wait for its release before a new Esc may close the dialog
                    escapeReleasedSinceShutdownDialog = (event.KeyInput.Key != irr::KEY_ESCAPE);
                    return true; // Return true here, so second 'esc' button pushes don't close the message box
                    break;

                case irr::KEY_KEY_M:
                    model->releaseManOverboard();
                    break;

                case irr::KEY_KEY_G:
                    // KYARA SEAGULL: one-shot ambience sound, no relation to ship controls.
                    model->triggerSeagull();
                    break;

                case irr::KEY_KEY_P:
                    // Toggle the proxy alarm mute state
                    model->toggleProxyAlarmMute();
                    break;

                    // Keyboard control of engines
                case irr::KEY_KEY_A:
                    // DEE_NOV22 vvvv
                    if (model->isAzimuthDrive())
                    {
                        // if vessel is Azimuth drive then turn Port Schottel anticlockwise decrement
                        model->btnDecrementPortSchottel();
                    }
                    else
                    {
                        // DEE_NOV_22 ^^^^
                        // (if not Azimuth Drive) Increase port engine revs:
                        model->setPortEngine(model->getPortEngine() + 0.1); // setPortEngine clamps the setting to the allowable range
                        // DEE_NOV22 vvvv
                    }
                    // DEE_NOV22 ^^^^
                    break;

                case irr::KEY_KEY_Z:
                    // Decrease port engine revs:
                    // DEE_NOV22 vvvv disable this function if azimuth drive
                    if (!(model->isAzimuthDrive()))
                    {
                        model->setPortEngine(model->getPortEngine() - 0.1); // setPortEngine clamps the setting to the allowable range
                    }
                    // DEE_NOV22 ^^^^
                    break;

                case irr::KEY_KEY_S:
                    // DEE_NOV22 vvvv
                    if (model->isAzimuthDrive())
                    {
                        // as Azimuth drive then Port thrust lever decrease
                        model->btnDecrementPortThrustLever();
                    }
                    else
                    {
                        // this is an non azimuth drive vessel to interpret S as Increase stpd engine revs
                        // DEE_NOV22 ^^^^
                        // Increase stbd engine revs:
                        model->setStbdEngine(model->getStbdEngine() + 0.1); // setPortEngine clamps the setting to the allowable range
                    }                                                       // DEE_NOV22 end if isAzimuthDrive and indentations
                    break;

                case irr::KEY_KEY_X:

                    // DEE_NOV22 vvvv only enable this response if it is not an azimuth drive
                    if (!(model->isAzimuthDrive()))
                    {
                        // Decrease stbd engine revs:
                        model->setStbdEngine(model->getStbdEngine() - 0.1); // setPortEngine clamps the setting to the allowable range
                    }
                    // DEE_NOV22 ^^^^
                    break;

                case irr::KEY_KEY_D:
                    // DEE_NOV22 vvvv in the case of the ship being Azimuth Drive
                    if (model->isAzimuthDrive())
                    {
                        // as Azimuth drive then Port Azipod Schottel clockwise
                        model->btnIncrementPortSchottel();
                    }
                    else
                    {
                        // DEE_NOV22 ^^^^
                        // else the ship is normally propelled indents made to code below DEE_NOV22 ^^^^
                        // Increase stbd and port engine revs:
                        model->setStbdEngine(model->getStbdEngine() + 0.1); // setPortEngine clamps the setting to the allowable range
                        model->setPortEngine(model->getPortEngine() + 0.1); // setPortEngine clamps the setting to the allowable range
                    }                                                       // end if DEE_NOV22
                    break;
                case irr::KEY_KEY_C:
                    // DEE_NOV22 vvvv only if not azimuth drive
                    if (!(model->isAzimuthDrive()))
                    {
                        // DEE_NOV22 ^^^^ indentations added below
                        // Decrease stbd engine revs:
                        model->setStbdEngine(model->getStbdEngine() - 0.1); // setPortEngine clamps the setting to the allowable range
                        model->setPortEngine(model->getPortEngine() - 0.1); // setPortEngine clamps the setting to the allowable range
                        // DEE_NOV22 vvvv
                    }
                    // DEE_NOV22 ^^^^
                    break;

                    // DEE vvvv key rudder to port changed to rudder wheel to port
                case irr::KEY_KEY_V:

                    // DEE_NOV22 vvvv the 'wheel' should be enabled only when not an azimuth drive
                    if (!(model->isAzimuthDrive()))
                    {
                        //                               model->setRudder(model->getRudder()-5);
                        model->setWheel(model->getWheel() - 1);
                    }
                    // DEE_NOV22 ^^^^ indentations added to original code
                    break;
                    // DEE ^^^^

                    // DEE vvvv key rudder to starboard changed to key wheel to starboard
                case irr::KEY_KEY_B:
                    // DEE_NOV22 vvvv
                    if (!(model->isAzimuthDrive()))
                    {
                        // DEE_NOV22 ^^^^

                        //                                model->setRudder(model->getRudder()+5);
                        model->setWheel(model->getWheel() + 1);
                        // DEE_NOV22 vvvv
                    }
                    // DEE_NOV22 ^^^^
                    break;
                    // DEE ^^^^

                default:
                    // don't do anything
                    break;
                }
            }
        }
    }

    if (event.EventType == irr::EET_KEY_INPUT_EVENT && !event.KeyInput.PressedDown)
    {
        if (event.KeyInput.Key == irr::KEY_KEY_H)
        {
            model->endHorn();
        }
    }

    // DEE 10JAN23 comment  joystickCode starts here

    // From joystick (actually polled, once per run():
    if (event.EventType == irr::EET_JOYSTICK_INPUT_EVENT)
    {

        irr::u8 thisJoystick = event.JoystickEvent.Joystick;

        // Initialise the joystick POV
        if (!previousJoystickPOVInitialised)
        {
            if (thisJoystick == joystickSetup.joystickNoPOV)
            {
                previousJoystickPOVInitialised = true;
                previousJoystickPOV = event.JoystickEvent.POV;
            }
        }

        // Show joystick raw status in log window
        if (device->getTimer()->getRealTime() - lastShownJoystickStatus > 5000)
        {

            std::string joystickInfoMessage = "Joystick status (";
            joystickInfoMessage.append(irr::core::stringc(event.JoystickEvent.Joystick).c_str());
            joystickInfoMessage.append(")\n");
            device->getLogger()->log(joystickInfoMessage.c_str());

            std::string thisJoystickStatus = "";
            for (irr::u8 thisAxis = 0; thisAxis < event.JoystickEvent.NUMBER_OF_AXES; thisAxis++)
            {
                irr::s16 axisSetting = event.JoystickEvent.Axis[thisAxis];
                thisJoystickStatus.append(irr::core::stringc(axisSetting).c_str());
                thisJoystickStatus.append(" ");
            }
            thisJoystickStatus.append("POV: ");
            thisJoystickStatus.append(irr::core::stringc(event.JoystickEvent.POV).c_str());
            device->getLogger()->log(thisJoystickStatus.c_str());
            device->getLogger()->log("");

            // If we've shown for all joysticks, don't show again.
            if (event.JoystickEvent.Joystick + 1 == joystickInfo.size())
            {
                lastShownJoystickStatus = device->getTimer()->getRealTime();
            }
        }

        // Keep joystick values the same unless they are being changed by user input
        irr::f32 newJoystickPort = previousJoystickPort;
        irr::f32 newJoystickStbd = previousJoystickStbd;
        irr::f32 newJoystickRudder = previousJoystickRudder;

        // DEE 10JAN23 vvvv Azimuth drive physical controls
        // for disambuigity then define a separate variable for thrust levers, as they control the engine but are not the engine
        irr::f32 newJoystickThrustLeverPort = previousJoystickThrustLeverPort;
        irr::f32 newJoystickThrustLeverStbd = previousJoystickThrustLeverStbd;
        // DEE 10JAN23 ^^^^

        // DEE 10JAN23 vvvv
        //            irr::f32 newJoystickAzimuthAngPort = previousJoystickAzimuthAngPort;
        //            irr::f32 newJoystickAzimuthAngStbd = previousJoystickAzimuthAngStbd;
        irr::f32 newJoystickSchottelPort = previousJoystickSchottelPort;
        irr::f32 newJoystickSchottelStbd = previousJoystickSchottelStbd;
        // DEE 10JAN23 ^^^^
        irr::f32 newJoystickBowThruster = previousJoystickBowThruster;
        irr::f32 newJoystickSternThruster = previousJoystickSternThruster;

        for (irr::u8 thisAxis = 0; thisAxis < event.JoystickEvent.NUMBER_OF_AXES; thisAxis++)
        {

            // Check which type we correspond to
            if (thisJoystick == joystickSetup.portJoystickNo && thisAxis == joystickSetup.portJoystickAxis)
            {
                newJoystickPort = event.JoystickEvent.Axis[joystickSetup.portJoystickAxis] / 32768.0;
                // If previous value is Inf, store current value in previous and current, otherwise only in current
                if (previousJoystickPort == INFINITY)
                {
                    previousJoystickPort = newJoystickPort;
                }
            }
            if (thisJoystick == joystickSetup.stbdJoystickNo && thisAxis == joystickSetup.stbdJoystickAxis)
            {
                newJoystickStbd = event.JoystickEvent.Axis[joystickSetup.stbdJoystickAxis] / 32768.0;
                // If previous value is Inf, store current value in previous and current, otherwise only in current
                if (previousJoystickStbd == INFINITY)
                {
                    previousJoystickStbd = newJoystickStbd;
                }
            }
            if (thisJoystick == joystickSetup.rudderJoystickNo && thisAxis == joystickSetup.rudderJoystickAxis)
            {
                newJoystickRudder = 30 * event.JoystickEvent.Axis[joystickSetup.rudderJoystickAxis] / 32768.0;
                // If previous value is Inf, store current value in previous and current, otherwise only in current
                if (previousJoystickRudder == INFINITY)
                {
                    previousJoystickRudder = newJoystickRudder;
                }
            }

            // DEE 10 Jan 23 vvvv TODO change this to control port schottel not the azimuth, let ownship.cpp take care of the follow up

            //		if (thisJoystick == joystickSetup.azimuth1JoystickNo && thisAxis == joystickSetup.azimuth1JoystickAxis) {
            //                    newJoystickAzimuthAngPort = 180*event.JoystickEvent.Axis[joystickSetup.azimuth1JoystickAxis]/32768.0;
            // If previous value is Inf, store current value in previous and current, otherwise only in current
            //                    if (previousJoystickAzimuthAngPort==INFINITY) {
            //                        previousJoystickAzimuthAngPort = newJoystickAzimuthAngPort;
            //                    }
            //                }
            //                if (thisJoystick == joystickSetup.azimuth2JoystickNo && thisAxis == joystickSetup.azimuth2JoystickAxis) {
            //                    newJoystickAzimuthAngStbd = 180*event.JoystickEvent.Axis[joystickSetup.azimuth2JoystickAxis]/32768.0;
            // If previous value is Inf, store current value in previous and current, otherwise only in current
            //                    if (previousJoystickAzimuthAngStbd==INFINITY) {
            //                        previousJoystickAzimuthAngStbd = newJoystickAzimuthAngStbd;
            //                    }
            //                }

            // TODO 10JAN23 apply scaling and offset to these

            // DEE 10JAN23 Port Thrust Lever for Azimuth Drive
            if (thisJoystick == joystickSetup.portThrustLever_joystickNo && thisAxis == joystickSetup.portThrustLever_channel)
            {
                newJoystickThrustLeverPort = joystickSetup.thrustLeverPortDirection * (joystickSetup.thrustLeverPortOffset + (joystickSetup.thrustLeverPortScaling * event.JoystickEvent.Axis[joystickSetup.portThrustLever_channel] / 32768.0));
                // If previous value is Inf, store current value in previous and current, otherwise only in current
                if (previousJoystickThrustLeverPort == INFINITY)
                {
                    previousJoystickThrustLeverPort = newJoystickThrustLeverPort;
                }
            }

            // DEE 10JAN23 Stbd Thrust Lever for Azimuth Drive
            if (thisJoystick == joystickSetup.stbdThrustLever_joystickNo && thisAxis == joystickSetup.stbdThrustLever_channel)
            {
                newJoystickThrustLeverStbd = joystickSetup.thrustLeverStbdDirection * (joystickSetup.thrustLeverStbdOffset + (joystickSetup.thrustLeverStbdScaling * event.JoystickEvent.Axis[joystickSetup.stbdThrustLever_channel] / 32768.0));
                // If previous value is Inf, store current value in previous and current, otherwise only in current
                if (previousJoystickThrustLeverStbd == INFINITY)
                {
                    previousJoystickThrustLeverStbd = newJoystickThrustLeverStbd;
                }
            }

            // DEE 10JAN23 Port Schottel for Azimuth Drive NB changed 180 to 360
            if (thisJoystick == joystickSetup.portSchottel_joystickNo && thisAxis == joystickSetup.portSchottel_channel)
            {
                newJoystickSchottelPort = joystickSetup.schottelPortDirection * (joystickSetup.schottelPortOffset + 180.0 * joystickSetup.schottelPortScaling * (event.JoystickEvent.Axis[joystickSetup.portSchottel_channel] / 32768.0));
                // If previous value is Inf, store current value in previous and current, otherwise only in current
                if (previousJoystickSchottelPort == INFINITY)
                {
                    previousJoystickSchottelPort = newJoystickSchottelPort;
                }
            }

            // DEE 10JAN23 Stbd Schottel for Azimuth Drive
            if (thisJoystick == joystickSetup.stbdSchottel_joystickNo && thisAxis == joystickSetup.stbdSchottel_channel)
            {
                newJoystickSchottelStbd = joystickSetup.schottelStbdDirection * (joystickSetup.schottelStbdOffset + 180.0 * joystickSetup.schottelStbdScaling * (event.JoystickEvent.Axis[joystickSetup.stbdSchottel_channel] / 32768.0));
                // If previous value is Inf, store current value in previous and current, otherwise only in current
                if (previousJoystickSchottelStbd == INFINITY)
                {
                    previousJoystickSchottelStbd = newJoystickSchottelStbd;
                }
            }
            // TODO 10JAN23 To JAMES check apply scaling and offsets to these as I wrote it on holiday in tenerife and I dont have my rudimentary physical controls with me

            // DEE 10Jan23 ^^^^

            if (thisJoystick == joystickSetup.bowThrusterJoystickNo && thisAxis == joystickSetup.bowThrusterJoystickAxis)
            {
                newJoystickBowThruster = event.JoystickEvent.Axis[joystickSetup.bowThrusterJoystickAxis] / 32768.0;
                // If previous value is Inf, store current value in previous and current, otherwise only in current
                if (previousJoystickBowThruster == INFINITY)
                {
                    previousJoystickBowThruster = newJoystickBowThruster;
                }
            }
            if (thisJoystick == joystickSetup.sternThrusterJoystickNo && thisAxis == joystickSetup.sternThrusterJoystickAxis)
            {
                newJoystickSternThruster = event.JoystickEvent.Axis[joystickSetup.sternThrusterJoystickAxis] / 32768.0;
                // If previous value is Inf, store current value in previous and current, otherwise only in current
                if (previousJoystickSternThruster == INFINITY)
                {
                    previousJoystickSternThruster = newJoystickSternThruster;
                }
            }
        }

        // Do joystick stuff here

        // check if any have changed
        bool joystickChanged = false;
        bool portChanged = fabs(newJoystickPort - previousJoystickPort) > 0.01;
        bool stbdChanged = fabs(newJoystickStbd - previousJoystickStbd) > 0.01;
        bool wheelChanged = fabs(newJoystickRudder - previousJoystickRudder) > 0.01;
        // DEE 10JAN23 vvvv
        //	    irr::f32 azimuth1AngChange = fabs(newJoystickAzimuthAngPort - previousJoystickAzimuthAngPort);
        //          irr::f32 azimuth2AngChange = fabs(newJoystickAzimuthAngStbd - previousJoystickAzimuthAngStbd);

        bool thrustLeverPortChanged = fabs(newJoystickThrustLeverPort - previousJoystickThrustLeverPort) > 0.01;
        bool thrustLeverStbdChanged = fabs(newJoystickThrustLeverStbd - previousJoystickThrustLeverStbd) > 0.01;
        bool schottelPortChanged = fabs(newJoystickSchottelPort - previousJoystickSchottelPort) > 0.01;
        bool schottelStbdChanged = fabs(newJoystickSchottelStbd - previousJoystickSchottelStbd) > 0.01;
        // DEE 10JAN23 ^^^^

        // DEE
        //            irr::f32 rudderChange = fabs(newJoystickRudder - previousJoystickRudder);
        bool bowThrusterChanged = fabs(newJoystickBowThruster - previousJoystickBowThruster) > 0.01;
        bool sternThrusterChanged = fabs(newJoystickSternThruster - previousJoystickSternThruster) > 0.01;
        // DEE
        //            if (portChange > 0.01 || stbdChange > 0.01 || rudderChange > 0.01 || bowThrusterChange > 0.01 || sternThrusterChange > 0.01 )
        if (portChanged || stbdChanged || wheelChanged ||
            bowThrusterChanged || sternThrusterChanged ||
            schottelPortChanged || schottelStbdChanged ||
            thrustLeverPortChanged || thrustLeverStbdChanged)
        {
            joystickChanged = true;
        }

        // If any have changed, use all (iff non-infinite)
        if (joystickChanged)
        {

            if (newJoystickPort < INFINITY && (joystickSetup.updateAllAxes || portChanged))
            { // refers to the port engine control
                irr::f32 mappedValue = lookup1D(newJoystickPort, joystickSetup.inputPoints, joystickSetup.outputPoints);
                // DEE 10JAN23 vvvv
                // if this an azidrive then change thrust lever.  In fact in the future I suggest that all engines are controlled via thrust lever
                // as the bigger the engine, then the longer the spool up time is.

                if (!(model->isAzimuthDrive()))
                {
                    model->setPortEngine(mappedValue);
                } // fi
                previousJoystickPort = newJoystickPort;
            }

            if (newJoystickStbd < INFINITY && (joystickSetup.updateAllAxes || stbdChanged))
            { // refers to the starboard engine control
                irr::f32 mappedValue = lookup1D(newJoystickStbd, joystickSetup.inputPoints, joystickSetup.outputPoints);
                if (!(model->isAzimuthDrive()))
                {
                    model->setStbdEngine(mappedValue);
                }
                previousJoystickStbd = newJoystickStbd;
            }

            // Azimuth drive specific
            // prefer to separate off the azimuth drive code from the conventional code for clarity and ease of future modification
            //
            if (model->isAzimuthDrive())
            {

                // Port Thrust Lever
                if (newJoystickThrustLeverPort < INFINITY && (joystickSetup.updateAllAxes || thrustLeverPortChanged))
                {

                    irr::f32 mappedValue = lookup1D(newJoystickThrustLeverPort, joystickSetup.inputPoints, joystickSetup.outputPoints);
                    if (model->isAzimuthAsternAllowed()) {
                        model->setPortAzimuthThrustLever(newJoystickThrustLeverPort);
                    }
                    else {
                        // the above does range -1 to 1 as output, however we want a range 0..1, dont want to change the mappings so
                        // we can ammend this to
                        // mappedValue = (mappedValue*0.5)+0.5;
                        model->setPortAzimuthThrustLever(0.5 + newJoystickThrustLeverPort * 0.5);
                    }
                    previousJoystickThrustLeverPort = newJoystickThrustLeverPort;
                }

                // Stbd Thrust Lever
                if (newJoystickThrustLeverStbd < INFINITY && (joystickSetup.updateAllAxes || thrustLeverStbdChanged))
                {
                    irr::f32 mappedValue = lookup1D(newJoystickThrustLeverStbd, joystickSetup.inputPoints, joystickSetup.outputPoints);
                    if (model->isAzimuthAsternAllowed()) {
                        model->setStbdAzimuthThrustLever(newJoystickThrustLeverStbd);
                    }
                    else {
                        // the above does range -1 to 1 as output, however we want a range 0..1, dont want to change the mappings so
                        // we can ammend this to
                        // mappedValue = (mappedValue*0.5)+0.5;
                        model->setStbdAzimuthThrustLever(0.5 + newJoystickThrustLeverStbd * 0.5);
                    }
                    previousJoystickThrustLeverStbd = newJoystickThrustLeverStbd;
                }

                // Port Schottel
                if (newJoystickSchottelPort < INFINITY && (joystickSetup.updateAllAxes || schottelPortChanged))
                {
                    model->setPortSchottel(newJoystickSchottelPort);
                    previousJoystickSchottelPort = newJoystickSchottelPort;
                }

                // Stbd Schottel
                if (newJoystickSchottelStbd < INFINITY && (joystickSetup.updateAllAxes || schottelStbdChanged))
                {
                    model->setStbdSchottel(newJoystickSchottelStbd);
                    previousJoystickSchottelStbd = newJoystickSchottelStbd;
                }

                // DEE note perhaps the "master" should be implemented in here

            } // end if is azimuth drive

            // DEE 10JAN23 ^^^^ end of joystick engine controls

            if (newJoystickRudder < INFINITY && (joystickSetup.updateAllAxes || wheelChanged))
            {
                // DEE if the joystick rudder control is used then make it change the wheel not the rudder
                model->setWheel(newJoystickRudder * joystickSetup.rudderDirection);
                //                    model->setRudder(newJoystickRudder);
                previousJoystickRudder = newJoystickRudder;
            }

            if (newJoystickBowThruster < INFINITY && (joystickSetup.updateAllAxes || bowThrusterChanged))
            {
                model->setBowThruster(newJoystickBowThruster);
                previousJoystickBowThruster = newJoystickBowThruster;
            }

            if (newJoystickSternThruster < INFINITY && (joystickSetup.updateAllAxes || sternThrusterChanged))
            {
                model->setSternThruster(newJoystickSternThruster);
                previousJoystickSternThruster = newJoystickSternThruster;
            }

        } // DEE 10JAN23 end if JoysickChanged

        // Check joystick buttons here
        // Make sure the joystickPreviousButtonStates has an entry for this joystick
        while (joystickPreviousButtonStates.size() <= thisJoystick)
        {
            joystickPreviousButtonStates.push_back(0); // All zeros equivalent to no buttons pressed
        }

        irr::u32 thisButtonState = event.JoystickEvent.ButtonStates;
        irr::u32 previousButtonState = joystickPreviousButtonStates.at(thisJoystick);

        // Horn
        if (thisJoystick == joystickSetup.joystickNoHorn)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonHorn, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonHorn, previousButtonState))
            {
                model->startHorn();
            }
            if (!IsButtonPressed(joystickSetup.joystickButtonHorn, thisButtonState) && IsButtonPressed(joystickSetup.joystickButtonHorn, previousButtonState))
            {
                model->endHorn();
            }
        }
        // Change view
        if (thisJoystick == joystickSetup.joystickNoChangeView)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonChangeView, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonChangeView, previousButtonState))
            {
                model->changeView();
                model->setMoveViewWithPrimary(true); // Allow the view to change automatically after this
            }
        }
        if (thisJoystick == joystickSetup.joystickNoChangeAndLockView)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonChangeAndLockView, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonChangeAndLockView, previousButtonState))
            {
                model->changeView();
                model->setMoveViewWithPrimary(false); // Don't allow the view to change automatically after this
            }
        }
        // Look step left
        if (thisJoystick == joystickSetup.joystickNoLookStepLeft)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonLookStepLeft, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonLookStepLeft, previousButtonState))
            {
                model->lookStepLeft();
            }
        }
        // Look step right
        if (thisJoystick == joystickSetup.joystickNoLookStepRight)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonLookStepRight, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonLookStepRight, previousButtonState))
            {
                model->lookStepRight();
            }
        }
        // Decrease bow thrust
        if (thisJoystick == joystickSetup.joystickNoDecreaseBowThrust)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonDecreaseBowThrust, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonDecreaseBowThrust, previousButtonState))
            {
                model->setBowThrusterRate(-0.5);
            }
            if (!IsButtonPressed(joystickSetup.joystickButtonDecreaseBowThrust, thisButtonState) && IsButtonPressed(joystickSetup.joystickButtonDecreaseBowThrust, previousButtonState))
            {
                model->setBowThrusterRate(0);
            }
        }
        // Increase bow thrust
        if (thisJoystick == joystickSetup.joystickNoIncreaseBowThrust)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonIncreaseBowThrust, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonIncreaseBowThrust, previousButtonState))
            {
                model->setBowThrusterRate(0.5);
            }
            if (!IsButtonPressed(joystickSetup.joystickButtonIncreaseBowThrust, thisButtonState) && IsButtonPressed(joystickSetup.joystickButtonIncreaseBowThrust, previousButtonState))
            {
                model->setBowThrusterRate(0);
            }
        }
        // Decrease stern thrust
        if (thisJoystick == joystickSetup.joystickNoDecreaseSternThrust)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonDecreaseSternThrust, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonDecreaseSternThrust, previousButtonState))
            {
                model->setSternThrusterRate(-0.5);
            }
            if (!IsButtonPressed(joystickSetup.joystickButtonDecreaseSternThrust, thisButtonState) && IsButtonPressed(joystickSetup.joystickButtonDecreaseSternThrust, previousButtonState))
            {
                model->setSternThrusterRate(0);
            }
        }
        // Increase stern thrust
        if (thisJoystick == joystickSetup.joystickNoIncreaseSternThrust)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonIncreaseSternThrust, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonIncreaseSternThrust, previousButtonState))
            {
                model->setSternThrusterRate(0.5);
            }
            if (!IsButtonPressed(joystickSetup.joystickButtonIncreaseSternThrust, thisButtonState) && IsButtonPressed(joystickSetup.joystickButtonIncreaseSternThrust, previousButtonState))
            {
                model->setSternThrusterRate(0);
            }
        }
        // Bearings on
        if (thisJoystick == joystickSetup.joystickNoBearingOn)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonBearingOn, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonBearingOn, previousButtonState))
            {
                gui->showBearings();
            }
        }
        // Bearings off
        if (thisJoystick == joystickSetup.joystickNoBearingOff)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonBearingOff, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonBearingOff, previousButtonState))
            {
                gui->hideBearings();
            }
        }
        // Zoom on
        if (thisJoystick == joystickSetup.joystickNoZoomOn)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonZoomOn, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonZoomOn, previousButtonState))
            {
                gui->zoomOn();
                model->setZoom(true);
            }
        }
        // Zoom off
        if (thisJoystick == joystickSetup.joystickNoZoomOff)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonZoomOff, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonZoomOff, previousButtonState))
            {
                gui->zoomOff();
                model->setZoom(false);
            }
        }
        // Look around
        // Left
        if (thisJoystick == joystickSetup.joystickNoLookLeft)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonLookLeft, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonLookLeft, previousButtonState))
            {
                model->setPanSpeed(-5);
            }
            if (!IsButtonPressed(joystickSetup.joystickButtonLookLeft, thisButtonState) && IsButtonPressed(joystickSetup.joystickButtonLookLeft, previousButtonState))
            {
                model->setPanSpeed(0);
            }
        }

        // Right
        if (thisJoystick == joystickSetup.joystickNoLookRight)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonLookRight, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonLookRight, previousButtonState))
            {
                model->setPanSpeed(5);
            }
            if (!IsButtonPressed(joystickSetup.joystickButtonLookRight, thisButtonState) && IsButtonPressed(joystickSetup.joystickButtonLookRight, previousButtonState))
            {
                model->setPanSpeed(0);
            }
        }

        // Up
        if (thisJoystick == joystickSetup.joystickNoLookUp)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonLookUp, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonLookUp, previousButtonState))
            {
                model->setVerticalPanSpeed(5);
            }
            if (!IsButtonPressed(joystickSetup.joystickButtonLookUp, thisButtonState) && IsButtonPressed(joystickSetup.joystickButtonLookUp, previousButtonState))
            {
                model->setVerticalPanSpeed(0);
            }
        }

        // Down
        if (thisJoystick == joystickSetup.joystickNoLookDown)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonLookDown, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonLookDown, previousButtonState))
            {
                model->setVerticalPanSpeed(-5);
            }
            if (!IsButtonPressed(joystickSetup.joystickButtonLookDown, thisButtonState) && IsButtonPressed(joystickSetup.joystickButtonLookDown, previousButtonState))
            {
                model->setVerticalPanSpeed(0);
            }
        }

        // Rudder pump 1 on
        if (thisJoystick == joystickSetup.joystickNoPump1On)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonPump1On, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonPump1On, previousButtonState))
            {
                model->setRudderPumpState(1, true);
                if (model->getRudderPumpState(2))
                {
                    model->setAlarm(false); // Only turn off alarm if other pump is working
                }
            }
        }

        // Rudder pump 1 off
        if (thisJoystick == joystickSetup.joystickNoPump1Off)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonPump1Off, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonPump1Off, previousButtonState))
            {
                model->setRudderPumpState(1, false);
                model->setAlarm(true);
            }
        }

        // Rudder pump 2 on
        if (thisJoystick == joystickSetup.joystickNoPump2On)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonPump2On, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonPump2On, previousButtonState))
            {
                model->setRudderPumpState(2, true);
                if (model->getRudderPumpState(1))
                {
                    model->setAlarm(false); // Only turn off alarm if other pump is working
                }
            }
        }

        // Rudder pump 2 off
        if (thisJoystick == joystickSetup.joystickNoPump2Off)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonPump2Off, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonPump2Off, previousButtonState))
            {
                model->setRudderPumpState(2, false);
                model->setAlarm(true);
            }
        }

        // Follow up rudder on
        if (thisJoystick == joystickSetup.joystickNoFollowUpOn)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonFollowUpOn, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonFollowUpOn, previousButtonState))
            {
                model->setFollowUpRudderWorking(true);
            }
        }

        // Follow up rudder off
        if (thisJoystick == joystickSetup.joystickNoFollowUpOff)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonFollowUpOff, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonFollowUpOff, previousButtonState))
            {
                model->setFollowUpRudderWorking(false);
            }
        }

        if (thisJoystick == joystickSetup.joystickNoNFUPort)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonNFUPort, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonNFUPort, previousButtonState))
            {
                model->setWheel(-30, true);
            }
            if (!IsButtonPressed(joystickSetup.joystickButtonNFUPort, thisButtonState) && IsButtonPressed(joystickSetup.joystickButtonNFUPort, previousButtonState))
            {
                model->setWheel(model->getRudder(), true);
            }
        }

        if (thisJoystick == joystickSetup.joystickNoNFUStbd)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonNFUStbd, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonNFUStbd, previousButtonState))
            {
                model->setWheel(30, true);
            }
            if (!IsButtonPressed(joystickSetup.joystickButtonNFUStbd, thisButtonState) && IsButtonPressed(joystickSetup.joystickButtonNFUStbd, previousButtonState))
            {
                model->setWheel(model->getRudder(), true);
            }
        }

        if (thisJoystick == joystickSetup.joystickNoAckAlarm)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonAckAlarm, thisButtonState))
            {
                model->setAlarm(false);
            }
        }

        // Radar control buttons
        if (thisJoystick == joystickSetup.joystickNoIncreaseClutterSetting)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonIncreaseClutterSetting, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonIncreaseClutterSetting, previousButtonState))
            {
                model->increaseRadarClutter(5);
            }
        }

        if (thisJoystick == joystickSetup.joystickNoDecreaseClutterSetting)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonDecreaseClutterSetting, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonDecreaseClutterSetting, previousButtonState))
            {
                model->decreaseRadarClutter(5);
            }
        }

        if (thisJoystick == joystickSetup.joystickNoIncreaseGainSetting)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonIncreaseGainSetting, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonIncreaseGainSetting, previousButtonState))
            {
                model->increaseRadarGain(5);
            }
        }

        if (thisJoystick == joystickSetup.joystickNoDecreaseGainSetting)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonDecreaseGainSetting, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonDecreaseGainSetting, previousButtonState))
            {
                model->decreaseRadarGain(5);
            }
        }

        if (thisJoystick == joystickSetup.joystickNoIncreaseRainSetting)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonIncreaseRainSetting, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonIncreaseRainSetting, previousButtonState))
            {
                model->increaseRadarRain(5);
            }
        }

        if (thisJoystick == joystickSetup.joystickNoDecreaseRainSetting)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonDecreaseRainSetting, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonDecreaseRainSetting, previousButtonState))
            {
                model->decreaseRadarRain(5);
            }
        }

        if (thisJoystick == joystickSetup.joystickNoIncreaseRange)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonIncreaseRange, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonIncreaseRange, previousButtonState))
            {
                model->increaseRadarRange();
            }
        }

        if (thisJoystick == joystickSetup.joystickNoDecreaseRange)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonDecreaseRange, thisButtonState) && !IsButtonPressed(joystickSetup.joystickButtonDecreaseRange, previousButtonState))
            {
                model->decreaseRadarRange();
            }
        }
        // End of radar control buttons

        // DEE 10JAN23 .... Ive never seen the master concept implemented on azimuth drives in real life as in practice you can steer
        // 			perfectly well with just one drive on passage.  Whilst Maneouvering or steaming in confined waters then
        // 			both drives are needed to operate independently.
        // 			When under autopilot, then the autopilot can be set to control port stbd or both azidrives.
        // 		    Is it worth the effort of implementing master for drives ?
        if (thisJoystick == joystickSetup.joystickNoAzimuth1Master)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonAzimuth1Master, thisButtonState))
            {
                // debounce:
                if (device->getTimer()->getRealTime() - lastTimeAzimuth1MasterChanged > 500)
                {
                    // Allow azimuth master to change every 500ms (debounce)
                    model->setAzimuth1Master(!model->getAzimuth1Master());
                    lastTimeAzimuth1MasterChanged = device->getTimer()->getRealTime();
                }
            }
        }

        if (thisJoystick == joystickSetup.joystickNoAzimuth2Master)
        {
            if (IsButtonPressed(joystickSetup.joystickButtonAzimuth2Master, thisButtonState))
            {
                // debounce:
                if (device->getTimer()->getRealTime() - lastTimeAzimuth2MasterChanged > 500)
                {
                    // Allow azimuth master to change every 500ms (debounce)
                    model->setAzimuth2Master(!model->getAzimuth2Master());
                    lastTimeAzimuth2MasterChanged = device->getTimer()->getRealTime();
                }
            }
        }

        // Store previous settings
        joystickPreviousButtonStates.at(thisJoystick) = event.JoystickEvent.ButtonStates;

        // POV hat
        if (previousJoystickPOVInitialised && thisJoystick == joystickSetup.joystickNoPOV)
        {
            if (event.JoystickEvent.POV == joystickSetup.joystickPOVLookLeft && previousJoystickPOV != joystickSetup.joystickPOVLookLeft)
            {
                model->setPanSpeed(-5);
            }
            if (event.JoystickEvent.POV != joystickSetup.joystickPOVLookLeft && previousJoystickPOV == joystickSetup.joystickPOVLookLeft)
            {
                model->setPanSpeed(0);
            }

            if (event.JoystickEvent.POV == joystickSetup.joystickPOVLookRight && previousJoystickPOV != joystickSetup.joystickPOVLookRight)
            {
                model->setPanSpeed(5);
            }
            if (event.JoystickEvent.POV != joystickSetup.joystickPOVLookRight && previousJoystickPOV == joystickSetup.joystickPOVLookRight)
            {
                model->setPanSpeed(0);
            }

            if (event.JoystickEvent.POV == joystickSetup.joystickPOVLookUp && previousJoystickPOV != joystickSetup.joystickPOVLookUp)
            {
                model->setVerticalPanSpeed(5);
            }
            if (event.JoystickEvent.POV != joystickSetup.joystickPOVLookUp && previousJoystickPOV == joystickSetup.joystickPOVLookUp)
            {
                model->setVerticalPanSpeed(0);
            }

            if (event.JoystickEvent.POV == joystickSetup.joystickPOVLookDown && previousJoystickPOV != joystickSetup.joystickPOVLookDown)
            {
                model->setVerticalPanSpeed(-5);
            }
            if (event.JoystickEvent.POV != joystickSetup.joystickPOVLookDown && previousJoystickPOV == joystickSetup.joystickPOVLookDown)
            {
                model->setVerticalPanSpeed(0);
            }
            previousJoystickPOV = event.JoystickEvent.POV; // Store for next time
        }
    }

    return false;
}

irr::f32 MyEventReceiver::lookup1D(irr::f32 lookupValue, std::vector<irr::f32> inputPoints, std::vector<irr::f32> outputPoints)
{
    // Check that the input and output points list are the same length
    if (inputPoints.size() != outputPoints.size() || inputPoints.size() < 2)
    {
        std::cerr << "Error: lookup1D needs inputPoints and outputPoints list size to be the same, and needs at least two points." << std::endl;
        return 0;
    }

    std::vector<irr::f32>::size_type numberOfPoints = inputPoints.size();

    // Check that inputPoints does not have decreasing values (must be increasing or equal)
    for (unsigned int i = 0; i + 1 < numberOfPoints; i++)
    {
        if (inputPoints.at(i + 1) < inputPoints.at(i))
        {
            std::cerr << "Error: inputPoints to lookup1D must not be in a decreasing order." << std::endl;
            return 0;
        }
    }

    // Return first output if at or below lowest input
    if (lookupValue <= inputPoints.at(0))
    {
        return outputPoints.at(0);
    }

    // Return last output if at or above highest input
    if (lookupValue >= inputPoints.at(numberOfPoints - 1))
    {
        return outputPoints.at(numberOfPoints - 1);
    }

    // Main interpolation
    // Find the first point above the one we're interested in
    unsigned int nextPoint = 1;
    while (nextPoint < numberOfPoints && inputPoints.at(nextPoint) <= lookupValue)
    {
        nextPoint++;
    }

    // check for div by zero - shouldn't happen, but protect against
    if (inputPoints.at(nextPoint) - inputPoints.at(nextPoint - 1) == 0)
        return 0.0;

    // do interpolation
    return outputPoints.at(nextPoint - 1) + (outputPoints.at(nextPoint) - outputPoints.at(nextPoint - 1)) * (lookupValue - inputPoints.at(nextPoint - 1)) / (inputPoints.at(nextPoint) - inputPoints.at(nextPoint - 1));
}

std::wstring MyEventReceiver::f32To3dp(irr::f32 value, bool stripZeros)
{
    // Convert a floating point value to a wstring, with 3dp
    char tempStr[100];
    snprintf(tempStr, 100, "%.3f", value);
    std::wstring outputWstring = std::wstring(tempStr, tempStr + strlen(tempStr));
    // Strip trailing zeros and decimal point
    if (stripZeros)
    {
        while (outputWstring.back() == '0')
        {
            outputWstring.pop_back();
        }
        if (outputWstring.back() == '.')
        {
            outputWstring.pop_back();
        }
    }
    return outputWstring;
}

bool MyEventReceiver::IsButtonPressed(irr::u32 button, irr::u32 buttonBitmap) const
{
    if (button >= 32)
        return false;

    return (buttonBitmap & (1 << button)) ? true : false;
}

void MyEventReceiver::applySizeEditorBox(int boxId)
{
    irr::f32 metres = 0.0f;
    if (!gui->getSizeEditorValue(boxId, metres)) {
        gui->setSizeEditorStatus(L"Valeur illisible : tapez un nombre en m\u00E8tres, par exemple 42,5", true);
    }
    else if (boxId == GUIMain::GUI_ID_SEDIT_LENGTH_BOX) {
        model->sizeEditSetLength(metres);
    }
    else {
        model->sizeEditSetDraught(metres);
    }
    device->getGUIEnvironment()->setFocus(0); //so the box takes the new value, and the keys work again
}

void MyEventReceiver::startShutdown()
{
    //KYARA: Esc / Quitter now PAUSES and asks. "Oui" quits, "Non" (or Esc again) resumes the
    //exercise where it was. Previously "Non" only cleared the flag and left the clock at 0,
    //so the exercise stayed frozen. The 500 ms sleep is gone too (it just froze the window).
    if (shutdownDialogActive) {
        return;
    }

    //Remember the clock rate (x1, x2, x5... or already paused) so "Non" restores exactly that
    acceleratorBeforeShutdownDialog = model->getAccelerator();
    model->setAccelerator(0.0);

    irr::gui::IGUIEnvironment* env = device->getGUIEnvironment();
    rootVisibleBeforeShutdownDialog = env->getRootGUIElement()->isVisible();
    escapeReleasedSinceShutdownDialog = true; //the Esc key handler sets this back to false when Esc opened it
    env->getRootGUIElement()->setVisible(true);

    //French button labels for Irrlicht's message box (OK/Cancel by default)
    env->getSkin()->setDefaultText(irr::gui::EGDT_MSG_BOX_OK, L"Oui");
    env->getSkin()->setDefaultText(irr::gui::EGDT_MSG_BOX_CANCEL, L"Non");
    env->addMessageBox(L"Quitter", L"Quitter la simulation ?", true, irr::gui::EMBF_OK | irr::gui::EMBF_CANCEL, 0, GUIMain::GUI_ID_CLOSE_BOX);
    shutdownDialogActive = true;
}

void MyEventReceiver::cancelShutdown()
{
    if (!shutdownDialogActive) {
        return;
    }
    shutdownDialogActive = false;
    device->getGUIEnvironment()->getRootGUIElement()->setVisible(rootVisibleBeforeShutdownDialog);
    model->setAccelerator(acceleratorBeforeShutdownDialog);
}
// FIRE FEATURE
void MyEventReceiver::aimMonitor(irr::s32 mx, irr::s32 my)
{
    // Reuse the SAME column-aware pick ray as mooring, so aiming is correct on the
    // triple-screen (Eyefinity) build - no separate ray maths to drift out of sync.
    irr::core::line3df ray = model->getMooringRay(mx, my, gui->getCompact3dView());
    model->setMonitorAimFromRay(ray);
}
void MyEventReceiver::handleMooringLines(irr::core::line3df rayForLines)
{
    if ((linesMode == 1) || (linesMode == 2)) {
        irr::scene::ISceneNode* contactNode = model->getContactFromRay(rayForLines, linesMode);

        if (contactNode)
        {
            // If returns non-null, then successful, so move onto next point or finish

            // Find the type of node (0: Unknown, 1: Own ship, 2: Other ship, 3: Buoy, 4: Land object, 5: Terrain)
            int nodeType = 0;
            int nodeID = 0;

            // Find node type and ID from name
            std::string nodeName = std::string(contactNode->getName());

            if (nodeName.find("OwnShip") == 0)
            {
                nodeType = 1;
                nodeID = 0;
            }
            else if (nodeName.find("OtherShip") == 0)
            {
                nodeType = 2;
                // Find other ship ID from name (should be OtherShip_#)
                std::vector<std::string> splitName = Utilities::split(nodeName, '_');
                if (splitName.size() == 2)
                {
                    nodeID = Utilities::lexical_cast<irr::s32>(splitName.at(1));
                }
            }
            else if (nodeName.find("Buoy") == 0)
            {
                nodeType = 3;
                // Find other buoy ID from name (should be Buoy_#)
                std::vector<std::string> splitName = Utilities::split(nodeName, '_');
                if (splitName.size() == 2)
                {
                    nodeID = Utilities::lexical_cast<irr::s32>(splitName.at(1));
                }
            }
            else if (nodeName.find("LandObject") == 0)
            {
                nodeType = 4;
                // Find other land object ID from name (should be LandObject_#)
                std::vector<std::string> splitName = Utilities::split(nodeName, '_');
                if (splitName.size() == 2)
                {
                    nodeID = Utilities::lexical_cast<irr::s32>(splitName.at(1));
                }
            }
            else if (nodeName.find("Terrain") == 0)
            {
                nodeType = 5;
                // Find terrain ID from name (should be Terrain_#)
                std::vector<std::string> splitName = Utilities::split(nodeName, '_');
                if (splitName.size() == 2)
                {
                    nodeID = Utilities::lexical_cast<irr::s32>(splitName.at(1));
                }
            }
            // std::cout << "Node name: " << nodeName << " nodeType: " << nodeType << " nodeID: " << nodeID << std::endl;

            if (linesMode == 2)
            {
                irr::f32 nominalMass = model->getOwnShipMassEstimate();
                if (nodeType == 2) {
                    // If connecting to another ship, find the minimum mass to use as the nominal mass for estimating default line properties
                    nominalMass = fmin(model->getOtherShipMassEstimate(nodeID), nominalMass);
                }
                model->getLines()->setLineEnd(contactNode, nominalMass, nodeType, nodeID, 1.0, false, -1);
                // Finished
                linesMode = 0;
                gui->setLinesControlsText("");
            }
            if (linesMode == 1)
            {
                model->getLines()->setLineStart(contactNode, nodeType, nodeID, false, -1); // Start should always be on 'own ship' so nodeType = 1, and ID does not matter (leave as 0)
                // Move on to end point
                linesMode = 2;
                gui->setLinesControlsText("Click in 3d view to set end position for line"); // TODO: Add translation

                // special case for 'anchoring', set end node at sea bed under the starting node
                if (gui->getAnchorLine()) {

                    irr::f32 nominalMass = model->getOwnShipMassEstimate();

                    // Create a 'contact node' at the terrain height below the anchor point
                    irr::core::vector3df intersection = contactNode->getAbsolutePosition();
                    intersection.Y = model->getTerrainHeight(intersection.X, intersection.Z);
                    irr::scene::ISceneNode* terrainSceneNode = model->getTerrainSceneNode(0);

                    // Add a 'sphere' scene node, with selectedSceneNode as parent.
                    // Find local coordinates from the global one
                    irr::core::vector3df localPosition(intersection);
                    irr::core::matrix4 worldToLocal = terrainSceneNode->getAbsoluteTransformation();
                    worldToLocal.makeInverse();
                    worldToLocal.transformVect(localPosition);

                    irr::core::vector3df sphereScale = irr::core::vector3df(1.0, 1.0, 1.0);
                    irr::scene::ISceneNode* contactPointNode = device->getSceneManager()->addSphereSceneNode(0.25f, 16, terrainSceneNode, -1,
                        localPosition,
                        irr::core::vector3df(0, 0, 0),
                        sphereScale);

                    // Set name to match parent for convenience
                    contactPointNode->setName(terrainSceneNode->getName());

                    // Node ID is 0 as we always assume parent is terrain 0
                    model->getLines()->setLineEnd(contactPointNode, nominalMass, 5, 0, 1.5, false, -1);

                    // Tidy up
                    linesMode = 0;
                    gui->setLinesControlsText("");
                }

            }
        }
    }
}

/*
    irr::s32 MyEventReceiver::GetScrollBarPosSpeed() const
    {
        return scrollBarPosSpeed;
    }

    irr::s32 MyEventReceiver::GetScrollBarPosHeading() const
    {
        return scrollBarPosHeading;
    }
*/