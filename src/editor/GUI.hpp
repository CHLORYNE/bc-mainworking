/*   Bridge Command 5.0 Ship Simulator
     Copyright (C) 2015 James Packer

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

#ifndef __GUIMAIN_HPP_INCLUDED__
#define __GUIMAIN_HPP_INCLUDED__

#include <string>
#include <vector>

#include "irrlicht.h"

#include "../Lang.hpp"
#include "PositionDataStruct.hpp"
#include "../ScenarioDataStructure.hpp"

class ChartView;
namespace Ui { class Button; }

class GUIMain //Create, build and update GUI
{
public:
    GUIMain(irr::IrrlichtDevice* device, Lang* language, std::vector<std::string> ownShipTypes, std::vector<std::string> otherShipTypes, bool multiplayer);

    enum GUI_ELEMENTS// Define some values that we'll use to identify individual GUI controls.
    {
        GUI_ID_SHIP_COMBOBOX = 101,
        GUI_ID_LEG_LISTBOX,
        GUI_ID_MMSI_EDITBOX,
        GUI_ID_COURSE_EDITBOX,
        GUI_ID_SPEED_EDITBOX,
        GUI_ID_DISTANCE_EDITBOX,
        GUI_ID_ZOOMIN_BUTTON,
        GUI_ID_ZOOMOUT_BUTTON,
        GUI_ID_CHANGE_BUTTON,
        GUI_ID_SETMMSI_BUTTON,
       //back button
        GUI_ID_BACK_BUTTON,
//        GUI_ID_CHANGE_COURSESPEED_BUTTON,
        GUI_ID_ADDSHIP_BUTTON,
		GUI_ID_DELETESHIP_BUTTON,
        GUI_ID_ADDLEG_BUTTON,
        GUI_ID_DELETELEG_BUTTON,
        GUI_ID_MOVESHIP_BUTTON,
        GUI_ID_STARTHOURS_EDITBOX,
        GUI_ID_STARTMINS_EDITBOX,
        GUI_ID_STARTDAY_EDITBOX,
        GUI_ID_STARTMONTH_EDITBOX,
        GUI_ID_STARTYEAR_EDITBOX,
        GUI_ID_SUNRISE_EDITBOX,
        GUI_ID_SUNSET_EDITBOX,
        GUI_ID_WEATHER_COMBOBOX,
        GUI_ID_VISIBILITY_COMBOBOX,
        GUI_ID_RAIN_COMBOBOX,
        GUI_ID_SCENARIONAME_EDITBOX,
        GUI_ID_DESCRIPTION_EDITBOX,
        GUI_ID_APPLY_BUTTON,
        GUI_ID_SAVE_BUTTON,
        GUI_ID_OWNSHIPSELECT_COMBOBOX,
        GUI_ID_OTHERSHIPSELECT_COMBOBOX,
        GUI_ID_WINDDIRECTION_EDITBOX,
        GUI_ID_WINDSPEED_EDITBOX,
        GUI_ID_DRIFTING_CHECKBOX,
        GUI_ID_GENERAL_WINDOW,    // NEW: ID for the UI Window
        GUI_ID_TOGGLE_UI_BUTTON,   // NEW: ID for the Show/Hide button
        GUI_ID_CHARTSTYLE_BUTTON,  // Chart background: nautical chart day / night, original map, HD image
        GUI_ID_TAB_EXERCISE,       // Side panel tabs: exercise, ships, route, weather
        GUI_ID_TAB_SHIPS,
        GUI_ID_TAB_ROUTE,
        GUI_ID_TAB_WEATHER,
    };
    ~GUIMain();
    //kyara
    void updateShipImageDisplay(std::string shipName);

    // Draw the chart, the ships and the GUI. hoverShip: ship under the cursor (0 own ship, 1.. other ships, -1 none).
    void updateGuiData(ScenarioData scenarioInfo, ChartView& chart, const std::vector<PositionData>& buoys, irr::s32 selectedShip, irr::s32 selectedLeg, irr::s32 hoverShip, bool draggingShip, irr::core::position2di mouse);
    void selectShip(irr::s32 shipIndex); // As if chosen in the ship list: 0 own ship, 1.. other ships
    void updateEditBoxes(); //Trigger an update of the edit boxes (carried out in next updateGuiData)
    irr::f32 getEditBoxCourse() const;
    irr::f32 getEditBoxSpeed() const;
    irr::f32 getEditBoxDistance() const;
    int getSelectedShip() const;
    int getSelectedLeg() const;
    std::string getOwnShipTypeSelected() const;
    std::string getOtherShipTypeSelected() const;
    irr::f32 getStartTime() const;
    irr::u32 getStartDay() const;
    irr::u32 getStartMonth() const;
    irr::u32 getStartYear() const;
    irr::f32 getSunRise() const;
    irr::f32 getSunSet() const;
    irr::f32 getWeather() const;
    irr::f32 getRain() const;
    irr::f32 getVisibility() const;
    irr::f32 getWindDirection() const;
    irr::f32 getWindSpeed() const;
    irr::u32 getEditBoxMMSI() const;
    std::string getScenarioName() const;
    std::string getDescription() const;
    irr::core::vector2df getScreenCentrePosition() const;
    //back buttons
    bool wantsToReturnToMenu() const;
    void setReturnToMenu();
    // NEW: UI Toggles
    void hideUI();
    void toggleUI();
    void setActiveTab(int tab); //0 exercise, 1 ships, 2 route, 3 weather
    void setWorldName(const std::string& world); //shown under the panel title
    irr::core::recti getMapViewport() const; //the chart's part of the window (beside the side panel)

private:

    Lang* language;

    irr::IrrlichtDevice* device;
    irr::gui::IGUIEnvironment* guienv;

    //Side panel: header, tabs, pages, and Apply / Save at the bottom. The ship selection card is shared
    //by the Ships and Route tabs.
    irr::gui::IGUIElement* sidebar;
    irr::gui::IGUIElement* shipStrip;
    irr::gui::IGUIElement* pages[4];
    Ui::Button* tabButtons[4];
    int activeTab;
    bool sidebarShown;
    irr::s32 sidebarWidth;
    bool french;
    irr::gui::IGUIFont* titleFont;
    irr::gui::IGUIFont* textFont;
    irr::gui::IGUIFont* smallFont;
    irr::gui::IGUIFont* mapFont;          //map labels and status bar keep the smaller font
    irr::gui::IGUIFont* originalSkinFont; //given back to the start screen
    std::string worldShown;

    irr::gui::IGUIElement* zoomIn;
    irr::gui::IGUIElement* zoomOut;
    irr::gui::IGUIElement* chartStyleButton;
    std::wstring chartStyleShown;

    irr::gui::IGUIStaticText* dataDisplay;
    irr::gui::IGUIStaticText* routeHint;
    irr::gui::IGUIEditBox* descriptionEdit;
    irr::gui::IGUIComboBox* shipSelector;
    irr::gui::IGUIListBox* legSelector;
    irr::gui::IGUIEditBox* legCourseEdit;
    irr::gui::IGUIEditBox* legSpeedEdit;
    irr::gui::IGUIEditBox* legDistanceEdit;
    irr::gui::IGUIEditBox* mmsiEdit;
    irr::gui::IGUIElement* changeLeg;
    irr::gui::IGUIImage* shipImageDisplay;
    irr::gui::IGUIStaticText* shipImageNotFoundText;
    bool hasValidImage;
    irr::gui::IGUIElement* backButton;
    irr::gui::IGUIElement* toggleUIButton; // Collapses / shows the side panel
    bool returnToMenuFlag;

    irr::gui::IGUIElement* addShip;
	irr::gui::IGUIElement* deleteShip;
    irr::gui::IGUIElement* addLeg;
    irr::gui::IGUIElement* deleteLeg;
    irr::gui::IGUIElement* moveShip;
    irr::gui::IGUIElement* setMMSI;

    irr::gui::IGUICheckBox* isDrifting;

    irr::gui::IGUIComboBox* ownShipTypeSelector;
    irr::gui::IGUIComboBox* otherShipTypeSelector;

    irr::gui::IGUIEditBox* startHours;
    irr::gui::IGUIEditBox* startMins;
    irr::gui::IGUIEditBox* startDay;
    irr::gui::IGUIEditBox* startMonth;
    irr::gui::IGUIEditBox* startYear;
    irr::gui::IGUIEditBox* sunRise;
    irr::gui::IGUIEditBox* sunSet;
    irr::gui::IGUIComboBox* weather;
    irr::gui::IGUIComboBox* visibility;
    irr::gui::IGUIComboBox* rain;
    irr::gui::IGUIEditBox* windDirection;
    irr::gui::IGUIEditBox* windSpeed;
    irr::gui::IGUIEditBox* scenarioName;
    irr::gui::IGUIStaticText* overwriteWarning;
    irr::gui::IGUIStaticText* notMultiplayerNameWarning;
    irr::gui::IGUIStaticText* multiplayerNameWarning;
    irr::gui::IGUIElement* apply;
    irr::gui::IGUIElement* save;


    irr::f32 mapCentreX;
    irr::f32 mapCentreZ;

    bool editBoxesNeedUpdating;
    bool multiplayer;

    ScenarioData oldScenarioInfo; //Keep a copy of the data we have already displayed, so the dialog boxes only get updated when needed

    void drawInformationOnMap(ChartView& chart, const ScenarioData& scenarioInfo, const std::vector<PositionData>& buoys, irr::s32 selectedShip, irr::s32 selectedLeg, irr::s32 hoverShip);
    void drawStatusBar(ChartView& chart, irr::core::position2di mouse, bool draggingShip);
    void drawLabel(ChartView& chart, const std::wstring& text, irr::core::position2di at, irr::video::SColor colour);
    void updateDropDowns(const std::vector<OtherShipData>& otherShips, irr::s32 selectedShip, irr::f32 time);
    std::wstring legLabel(const OtherShipData& ship, irr::u32 leg) const; //"1  090.0\u00B0  8.0 kn  2.00 NM"
    void placeToolbar(); //map buttons, beside the side panel or at the window edge
    bool manuallyTriggerGUIEvent(irr::gui::IGUIElement* caller, irr::gui::EGUI_EVENT_TYPE eType);
    std::wstring f32To3dp(irr::f32 value) const;
    std::wstring f32To4dp(irr::f32 value) const;
    std::wstring f32To2dp(irr::f32 value) const; // NEW: Forces 2 decimals
    std::wstring f32To1dp(irr::f32 value) const; // NEW: Forces 1 decimal

};

#endif


