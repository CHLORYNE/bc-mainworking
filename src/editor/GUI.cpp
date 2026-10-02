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

#include "GUI.hpp"
#include "../Constants.hpp"
#include "../Utilities.hpp"
#include "../chartView/ChartView.hpp"
#include "../chartView/ChartDraw.hpp"

#include <iostream>
#include <limits>
#include <string>
#include <algorithm>

//using namespace irr;

GUIMain::GUIMain(irr::IrrlichtDevice* device, Lang* language, std::vector<std::string> ownShipTypes, std::vector<std::string> otherShipTypes, bool multiplayer)
{
    this->device = device;
    guienv = device->getGUIEnvironment();
    returnToMenuFlag = false;

//CHANGES COLORS AND BUTTONS -KYARA
   
    irr::video::IVideoDriver* driver = device->getVideoDriver();
    irr::u32 su = driver->getScreenSize().Width;
    irr::u32 sh = driver->getScreenSize().Height;
    
    // --- MATCH MODERN MENU STYLING ---
    irr::gui::IGUISkin* skin = guienv->getSkin();

    irr::video::SColor bgDark(255, 30, 34, 43);       // Main window bg
    irr::video::SColor panelColor(255, 45, 52, 60);   // Buttons, lists, tabs
    irr::video::SColor borderDark(255, 20, 24, 30);   // Shadows/Borders
    irr::video::SColor borderLight(255, 65, 75, 85);  // Highlights
    irr::video::SColor textMain(255, 240, 245, 250);  // Off-white text
    irr::video::SColor highlightBlue(255, 52, 152, 219); // Azure selection highlight
    irr::video::SColor editBg(255, 20, 24, 30);       // Darker inset for text inputs

    // Apply Base & Windows
    skin->setColor(irr::gui::EGDC_WINDOW, bgDark);
    skin->setColor(irr::gui::EGDC_3D_FACE, panelColor);

    // Flatten 3D effects
    skin->setColor(irr::gui::EGDC_3D_SHADOW, borderDark);
    skin->setColor(irr::gui::EGDC_3D_DARK_SHADOW, borderDark);
    skin->setColor(irr::gui::EGDC_3D_HIGH_LIGHT, borderLight);

    // Text Styling
    skin->setColor(irr::gui::EGDC_BUTTON_TEXT, textMain);
    skin->setColor(irr::gui::EGDC_GRAY_TEXT, borderLight);
    skin->setColor(irr::gui::EGDC_TOOLTIP, textMain);

    // Selections / Highlighting
    skin->setColor(irr::gui::EGDC_HIGH_LIGHT, highlightBlue);
    skin->setColor(irr::gui::EGDC_HIGH_LIGHT_TEXT, irr::video::SColor(255, 0, 0, 0));

    // Edit Boxes & Inputs
    skin->setColor(irr::gui::EGDC_EDITABLE, editBg);
    skin->setColor(irr::gui::EGDC_FOCUSED_EDITABLE, borderLight);
    skin->setColor(irr::gui::EGDC_GRAY_EDITABLE, bgDark);
  
    // create the window with a Title Bar (making it draggable) and an ID
    generalDataWindow = guienv->addWindow(
        irr::core::rect<irr::s32>(0.01f * su, 0.01f * sh, 0.49f * su, 0.49f * sh),
        false, L"Paramètres du Scénario", nullptr, GUI_ID_GENERAL_WINDOW);
    // REMOVED the setVisible(false) line so the top-right 'X' button appears!

    tabControl = guienv->addTabControl(
        irr::core::rect<irr::s32>(0.010f * su, 0.03f * sh, 0.470f * su, 0.470f * sh),
        generalDataWindow);
    tabControl->setTabHeight((irr::f32)(0.03f * sh));

    irr::gui::IGUITab* generalTab = tabControl->addTab(
        language->translate("general").c_str());
    irr::gui::IGUITab* shipTab = tabControl->addTab(
        language->translate("ships").c_str());
    irr::gui::IGUITab* weatherTab = tabControl->addTab(language->translate("weather").c_str());
    
    
    //kyara back button (Fixed overlap with zoom button)
    backButton = guienv->addButton(
        irr::core::rect<irr::s32>(0.85f * su, 0.01f * sh, 0.95f * su, 0.05f * sh),
        nullptr, GUI_ID_BACK_BUTTON, L"Back to Menu");
    backButton->setOverrideColor(irr::video::SColor(255, 255, 100, 100)); // red

    // NEW: Hide/Show UI Button
    toggleUIButton = guienv->addButton(
        irr::core::rect<irr::s32>(0.85f * su, 0.06f * sh, 0.95f * su, 0.10f * sh),
        nullptr, GUI_ID_TOGGLE_UI_BUTTON, L"Cacher Menu");

    zoomIn = guienv->addButton(
        irr::core::rect<irr::s32>(0.96f * su, 0.01f * sh, 0.99f * su, 0.05f * sh),
        nullptr, GUI_ID_ZOOMIN_BUTTON, L"+");
    zoomOut = guienv->addButton(
        irr::core::rect<irr::s32>(0.96f * su, 0.06f * sh, 0.99f * su, 0.10f * sh),
        nullptr, GUI_ID_ZOOMOUT_BUTTON, L"-");

    // Chart background (nautical chart day / night, original map, HD image); label set in updateGuiData
    chartStyleButton = guienv->addButton(
        irr::core::rect<irr::s32>(0.64f * su, 0.01f * sh, 0.84f * su, 0.05f * sh),
        nullptr, GUI_ID_CHARTSTYLE_BUTTON, L"Fond");
    //--------------------------------------------------

    shipSelector = guienv->addComboBox(
        irr::core::rect<irr::s32>(0.01f * su, 0.09f * sh, 0.13f * su, 0.12f * sh),
        shipTab, GUI_ID_SHIP_COMBOBOX);
    ownShipTypeSelector = guienv->addComboBox(
        irr::core::rect<irr::s32>(0.01f * su, 0.16f * sh, 0.13f * su, 0.19f * sh),
        shipTab, GUI_ID_OWNSHIPSELECT_COMBOBOX);
    otherShipTypeSelector = guienv->addComboBox(
        irr::core::rect<irr::s32>(0.01f * su, 0.16f * sh, 0.13f * su, 0.19f * sh),
        shipTab, GUI_ID_OTHERSHIPSELECT_COMBOBOX);

    legSelector = guienv->addListBox(
        irr::core::rect<irr::s32>(0.32f * su, 0.09f * sh, 0.45f * su, 0.19f * sh),
        shipTab, GUI_ID_LEG_LISTBOX);

    setMMSI = guienv->addButton(
        irr::core::rect<irr::s32>(0.18f * su, 0.13f * sh, 0.31f * su, 0.16f * sh),
        shipTab, GUI_ID_SETMMSI_BUTTON,
        language->translate("setMMSI").c_str());
    changeLeg = guienv->addButton(
        irr::core::rect<irr::s32>(0.03f * su, 0.28f * sh, 0.23f * su, 0.31f * sh),
        shipTab, GUI_ID_CHANGE_BUTTON,
        language->translate("changeLeg").c_str());
    addShip = guienv->addButton(
        irr::core::rect<irr::s32>(0.25f * su, 0.28f * sh, 0.45f * su, 0.31f * sh),
        shipTab, GUI_ID_ADDSHIP_BUTTON,
        language->translate("addShip").c_str());
    addLeg = guienv->addButton(
        irr::core::rect<irr::s32>(0.03f * su, 0.31f * sh, 0.23f * su, 0.34f * sh),
        shipTab, GUI_ID_ADDLEG_BUTTON,
        language->translate("addLeg").c_str());
    deleteLeg = guienv->addButton(
        irr::core::rect<irr::s32>(0.25f * su, 0.31f * sh, 0.45f * su, 0.34f * sh),
        shipTab, GUI_ID_DELETELEG_BUTTON,
        language->translate("deleteLeg").c_str());
    moveShip = guienv->addButton(
        irr::core::rect<irr::s32>(0.14f * su, 0.34f * sh, 0.34f * su, 0.37f * sh),
        shipTab, GUI_ID_MOVESHIP_BUTTON,
        language->translate("move").c_str());
    deleteShip = guienv->addButton(
        irr::core::rect<irr::s32>(0.14f * su, 0.09f * sh, 0.17f * su, 0.12f * sh),
        shipTab, GUI_ID_DELETESHIP_BUTTON,
        language->translate("deleteShip").c_str());

   
    //  edit boxes
    legCourseEdit = guienv->addEditBox(
        L"C", irr::core::rect<irr::s32>(0.01f * su, 0.24f * sh, 0.13f * su, 0.27f * sh),
        false, shipTab, GUI_ID_COURSE_EDITBOX);
    legSpeedEdit = guienv->addEditBox(
        L"S", irr::core::rect<irr::s32>(0.18f * su, 0.24f * sh, 0.30f * su, 0.27f * sh),
        false, shipTab, GUI_ID_SPEED_EDITBOX);
    legDistanceEdit = guienv->addEditBox(
        L"D", irr::core::rect<irr::s32>(0.35f * su, 0.24f * sh, 0.45f * su, 0.27f * sh),
        false, shipTab, GUI_ID_DISTANCE_EDITBOX);
    mmsiEdit = guienv->addEditBox(
        L"", irr::core::rect<irr::s32>(0.18f * su, 0.09f * sh, 0.31f * su, 0.12f * sh),
        false, shipTab, GUI_ID_MMSI_EDITBOX);

    this->language = language;
    this->multiplayer = multiplayer;

    

    //add data display:
    dataDisplay = guienv->addStaticText(L"", irr::core::rect<irr::s32>(0.01*su,0.01*sh,0.45*su,0.04*sh), true, false, shipTab, -1, true); //Actual text set later

    //Add ship selector drop down
    shipSelector = guienv->addComboBox(irr::core::rect<irr::s32>(0.01*su,0.09*sh,0.13*su,0.12*sh),shipTab,GUI_ID_SHIP_COMBOBOX);
    guienv->addStaticText(language->translate("selectShip").c_str(),irr::core::rect<irr::s32>(0.01*su,0.05*sh,0.13*su,0.08*sh),false,false,shipTab);

    //Add selectors to allow changing own and other ships (only one visible at a time)
    guienv->addStaticText(language->translate("shipType").c_str(),irr::core::rect<irr::s32>(0.01*su,0.13*sh,0.13*su,0.16*sh),false,false,shipTab);
    ownShipTypeSelector = guienv->addComboBox(irr::core::rect<irr::s32>(0.01*su,0.16*sh,0.13*su,0.19*sh),shipTab,GUI_ID_OWNSHIPSELECT_COMBOBOX);
    for (int i = 0; i<ownShipTypes.size(); i++) {
        ownShipTypeSelector->addItem( irr::core::stringw(ownShipTypes.at(i).c_str()).c_str() );
    }
    otherShipTypeSelector = guienv->addComboBox(irr::core::rect<irr::s32>(0.01*su,0.16*sh,0.13*su,0.19*sh),shipTab,GUI_ID_OTHERSHIPSELECT_COMBOBOX);
    for (int i = 0; i<otherShipTypes.size(); i++) {
        otherShipTypeSelector->addItem( irr::core::stringw(otherShipTypes.at(i).c_str()).c_str() );
    }
    otherShipTypeSelector->setVisible(false); //Initially show own ship selector.

    //Add leg selector drop down
    legSelector  = guienv->addListBox(irr::core::rect<irr::s32>(0.32*su,0.09*sh,0.45*su,0.19*sh),shipTab,GUI_ID_LEG_LISTBOX);
    guienv->addStaticText(language->translate("selectLeg").c_str(),irr::core::rect<irr::s32>(0.32*su,0.05*sh,0.45*su,0.08*sh),false,false,shipTab);

    //Add edit boxes for this leg element
    legCourseEdit   = guienv->addEditBox(L"C",irr::core::rect<irr::s32>(0.01*su,0.24*sh,0.13*su,0.27*sh),false,shipTab,GUI_ID_COURSE_EDITBOX);
    legSpeedEdit    = guienv->addEditBox(L"S",irr::core::rect<irr::s32>(0.18*su,0.24*sh,0.30*su,0.27*sh),false,shipTab,GUI_ID_SPEED_EDITBOX);
    legDistanceEdit = guienv->addEditBox(L"D",irr::core::rect<irr::s32>(0.35*su,0.24*sh,0.45*su,0.27*sh),false,shipTab,GUI_ID_DISTANCE_EDITBOX);

    guienv->addStaticText(language->translate("setCourse").c_str(),irr::core::rect<irr::s32>(0.01*su,0.20*sh,0.13*su,0.23*sh),false,false,shipTab);
    guienv->addStaticText(language->translate("setSpeed").c_str(),irr::core::rect<irr::s32>(0.18*su,0.20*sh,0.30*su,0.23*sh),false,false,shipTab);
    guienv->addStaticText(language->translate("setDistance").c_str(),irr::core::rect<irr::s32>(0.35*su,0.20*sh,0.45*su,0.23*sh),false,false,shipTab);

    //Add MMSI editing
    mmsiEdit = guienv->addEditBox(L"",irr::core::rect<irr::s32>(0.18*su,0.09*sh,0.31*su,0.12*sh),false,shipTab,GUI_ID_MMSI_EDITBOX);
    setMMSI = guienv->addButton(irr::core::rect<irr::s32>(0.18*su,0.13*sh,0.31*su,0.16*sh),shipTab,GUI_ID_SETMMSI_BUTTON,language->translate("setMMSI").c_str());

    // Set if the ship can drift with wind and current
    isDrifting = guienv->addCheckBox(false, irr::core::rect<irr::s32>(0.18*su,0.16*sh,0.20*su,0.19*sh), shipTab, GUI_ID_DRIFTING_CHECKBOX);
    guienv->addStaticText(language->translate("allowDrifting").c_str(), irr::core::rect<irr::s32>(0.20 * su, 0.16 * sh, 0.31 * su, 0.19 * sh),false, false, shipTab);

    //Add buttons
    changeLeg       = guienv->addButton(irr::core::rect<irr::s32>(0.03*su, 0.28*sh, 0.23*su, 0.31*sh),shipTab,GUI_ID_CHANGE_BUTTON,language->translate("changeLeg").c_str());
    addShip         = guienv->addButton(irr::core::rect<irr::s32>(0.25*su, 0.28*sh, 0.45*su, 0.31*sh),shipTab, GUI_ID_ADDSHIP_BUTTON,language->translate("addShip").c_str());
    addLeg          = guienv->addButton(irr::core::rect<irr::s32>(0.03*su, 0.31*sh, 0.23*su, 0.34*sh),shipTab,GUI_ID_ADDLEG_BUTTON,language->translate("addLeg").c_str());
    deleteLeg       = guienv->addButton(irr::core::rect<irr::s32>(0.25*su, 0.31*sh, 0.45*su, 0.34*sh),shipTab, GUI_ID_DELETELEG_BUTTON,language->translate("deleteLeg").c_str());
    moveShip        = guienv->addButton(irr::core::rect<irr::s32>(0.14*su, 0.34*sh, 0.34*su, 0.37*sh),shipTab, GUI_ID_MOVESHIP_BUTTON,language->translate("move").c_str());
	deleteShip		= guienv->addButton(irr::core::rect<irr::s32>(0.14*su, 0.09*sh, 0.17*su, 0.12*sh), shipTab, GUI_ID_DELETESHIP_BUTTON, language->translate("deleteShip").c_str());
    //kyara
    // --- SHIP PREVIEW IMAGE BLOCK ---
    // Coordinates match the bottom right corner (Width: 70% to 98%, Height: 65% to 95%)
    irr::core::rect<irr::s32> imageRect(0.70 * su, 0.65 * sh, 0.98 * su, 0.95 * sh);

    shipImageDisplay = guienv->addImage(imageRect);
    shipImageDisplay->setScaleImage(true); // Forces image to fit the box
    shipImageDisplay->setVisible(false);

    shipImageNotFoundText = guienv->addStaticText(L"Picture not found", imageRect, true, true, 0, -1, true);
    shipImageNotFoundText->setTextAlignment(irr::gui::EGUIA_CENTER, irr::gui::EGUIA_CENTER);
    shipImageNotFoundText->setBackgroundColor(irr::video::SColor(150, 0, 0, 0)); // Semi-transparent dark background
    shipImageNotFoundText->setOverrideColor(irr::video::SColor(255, 255, 255, 255)); // White text
    shipImageNotFoundText->setVisible(false);

    hasValidImage = false;
    //This is used to track when the edit boxes need updating, when ship or legs have changed. Set to true for initial load
    editBoxesNeedUpdating = true;

    // General scenario information
    guienv->addStaticText(language->translate("startTime").c_str(),irr::core::rect<irr::s32>(0.010*su,0.01*sh,0.115*su,0.04*sh),false,false,generalTab);
    startHours = guienv->addEditBox(L"",irr::core::rect<irr::s32>(0.010*su,0.04*sh,0.035*su,0.07*sh),false,generalTab,GUI_ID_STARTHOURS_EDITBOX );
    startMins = guienv->addEditBox(L"",irr::core::rect<irr::s32>(0.045*su,0.04*sh,0.070*su,0.07*sh),false,generalTab,GUI_ID_STARTMINS_EDITBOX );

    guienv->addStaticText(language->translate("startDate").c_str(),irr::core::rect<irr::s32>(0.130*su,0.01*sh,0.280*su,0.04*sh),false,false,generalTab);
    startYear = guienv->addEditBox(L"",irr::core::rect<irr::s32>(0.130*su,0.04*sh,0.180*su,0.07*sh),false,generalTab,GUI_ID_STARTYEAR_EDITBOX );
    startMonth = guienv->addEditBox(L"",irr::core::rect<irr::s32>(0.190*su,0.04*sh,0.215*su,0.07*sh),false,generalTab,GUI_ID_STARTMONTH_EDITBOX );
    startDay = guienv->addEditBox(L"",irr::core::rect<irr::s32>(0.225*su,0.04*sh,0.250*su,0.07*sh),false,generalTab,GUI_ID_STARTDAY_EDITBOX );

    guienv->addStaticText(language->translate("sunRise").c_str(),irr::core::rect<irr::s32>(0.010*su,0.08*sh,0.115*su,0.11*sh),false,false,generalTab);
    guienv->addStaticText(language->translate("sunSet").c_str(),irr::core::rect<irr::s32>(0.130*su,0.08*sh,0.280*su,0.11*sh),false,false,generalTab);
    sunRise = guienv->addEditBox(L"",irr::core::rect<irr::s32>(0.010*su,0.11*sh,0.085*su,0.14*sh),false,generalTab,GUI_ID_SUNRISE_EDITBOX );
    sunSet = guienv->addEditBox(L"",irr::core::rect<irr::s32>(0.130*su,0.11*sh,0.205*su,0.14*sh),false,generalTab,GUI_ID_SUNSET_EDITBOX );

    guienv->addStaticText(language->translate("weather").c_str(),irr::core::rect<irr::s32>(0.010*su,0.15*sh,0.130*su,0.18*sh),false,false,weatherTab);
    guienv->addStaticText(language->translate("rain").c_str(),irr::core::rect<irr::s32>(0.130*su,0.15*sh,0.250*su,0.18*sh),false,false,weatherTab);
    guienv->addStaticText(language->translate("visibility").c_str(),irr::core::rect<irr::s32>(0.250*su,0.15*sh,0.370*su,0.18*sh),false,false,weatherTab);
    weather    = guienv->addComboBox(irr::core::rect<irr::s32>(0.010*su,0.18*sh,0.085*su,0.21*sh),weatherTab,GUI_ID_WEATHER_COMBOBOX);
    rain       = guienv->addComboBox(irr::core::rect<irr::s32>(0.130*su,0.18*sh,0.205*su,0.21*sh),weatherTab,GUI_ID_RAIN_COMBOBOX);
    visibility = guienv->addComboBox(irr::core::rect<irr::s32>(0.250*su,0.18*sh,0.325*su,0.21*sh),weatherTab,GUI_ID_VISIBILITY_COMBOBOX);

    guienv->addStaticText(language->translate("scenario").c_str(),irr::core::rect<irr::s32>(0.010*su,0.22*sh,0.280*su,0.25*sh),false,false,generalTab);
    scenarioName = guienv->addEditBox(L"",irr::core::rect<irr::s32>(0.010*su,0.25*sh,0.205*su,0.28*sh),false,generalTab,GUI_ID_SCENARIONAME_EDITBOX );
    overwriteWarning = guienv->addStaticText(language->translate("overwrite").c_str(),irr::core::rect<irr::s32>(0.215*su,0.25*sh,0.450*su,0.28*sh),false,false,generalTab);

    descriptionEdit = guienv->addEditBox(L"",irr::core::rect<irr::s32>(0.010*su,0.29*sh,0.450*su,0.37*sh),false,generalTab,GUI_ID_DESCRIPTION_EDITBOX );
    descriptionEdit->setMultiLine(true);
    descriptionEdit->setWordWrap(true);
    descriptionEdit->setAutoScroll(true);
    descriptionEdit->setTextAlignment(irr::gui::EGUIA_UPPERLEFT, irr::gui::EGUIA_UPPERLEFT);

    multiplayerNameWarning = guienv->addStaticText(language->translate("multiplayerNeedsMP").c_str(),irr::core::rect<irr::s32>(0.215*su,0.25*sh,0.450*su,0.31*sh),false,true,generalTab);
    notMultiplayerNameWarning = guienv->addStaticText(language->translate("nonMultiplayerNoMP").c_str(),irr::core::rect<irr::s32>(0.215*su,0.25*sh,0.450*su,0.31*sh),false,true,generalTab);

    apply = guienv->addButton(irr::core::rect<irr::s32>(0.300*su,0.01*sh,0.450*su,0.07*sh),generalTab,GUI_ID_APPLY_BUTTON,language->translate("apply").c_str());
    save = guienv->addButton(irr::core::rect<irr::s32>(0.300*su,0.08*sh,0.450*su,0.14*sh),generalTab,GUI_ID_SAVE_BUTTON,language->translate("save").c_str());

    weather->addItem(L"0"); weather->addItem(L"0.5"); weather->addItem(L"1"); weather->addItem(L"1.5");
    weather->addItem(L"2"); weather->addItem(L"2.5"); weather->addItem(L"3"); weather->addItem(L"3.5");
    weather->addItem(L"4"); weather->addItem(L"4.5"); weather->addItem(L"5"); weather->addItem(L"5.5");
    weather->addItem(L"6"); weather->addItem(L"6.5"); weather->addItem(L"7"); weather->addItem(L"7.5");
    weather->addItem(L"8"); weather->addItem(L"8.5"); weather->addItem(L"9"); weather->addItem(L"9.5");
    weather->addItem(L"10"); weather->addItem(L"10.5"); weather->addItem(L"11"); weather->addItem(L"11.5");
    weather->addItem(L"12");

    rain->addItem(L"0"); rain->addItem(L"0.5"); rain->addItem(L"1"); rain->addItem(L"1.5");
    rain->addItem(L"2"); rain->addItem(L"2.5"); rain->addItem(L"3"); rain->addItem(L"3.5");
    rain->addItem(L"4"); rain->addItem(L"4.5"); rain->addItem(L"5"); rain->addItem(L"5.5");
    rain->addItem(L"6"); rain->addItem(L"6.5"); rain->addItem(L"7"); rain->addItem(L"7.5");
    rain->addItem(L"8"); rain->addItem(L"8.5"); rain->addItem(L"9"); rain->addItem(L"9.5");
    rain->addItem(L"10");

    visibility->addItem(L"10.0");visibility->addItem(L"9.5");visibility->addItem(L"9.0");visibility->addItem(L"8.5");
    visibility->addItem(L"8.0");visibility->addItem(L"7.5");visibility->addItem(L"7.0");visibility->addItem(L"6.5");
    visibility->addItem(L"6.0");visibility->addItem(L"5.5");visibility->addItem(L"5.0");visibility->addItem(L"4.5");
    visibility->addItem(L"4.0");visibility->addItem(L"3.5");visibility->addItem(L"3.0");visibility->addItem(L"2.5");
    visibility->addItem(L"2.0");visibility->addItem(L"1.5");visibility->addItem(L"1.0");
    visibility->addItem(L"0.9");visibility->addItem(L"0.8");visibility->addItem(L"0.7");
    visibility->addItem(L"0.6");visibility->addItem(L"0.5");visibility->addItem(L"0.4");
    visibility->addItem(L"0.3"); visibility->addItem(L"0.2"); visibility->addItem(L"0.1"); visibility->addItem(L"0");

    // Wind
    guienv->addStaticText(language->translate("windDirection").c_str(),irr::core::rect<irr::s32>(0.010*su,0.01*sh,0.115*su,0.04*sh),false,false,weatherTab);
    windDirection = guienv->addEditBox(L"",irr::core::rect<irr::s32>(0.010*su,0.04*sh,0.070*su,0.07*sh),false,weatherTab,GUI_ID_WINDDIRECTION_EDITBOX );

    guienv->addStaticText(language->translate("windSpeed").c_str(),irr::core::rect<irr::s32>(0.130*su,0.01*sh,0.280*su,0.04*sh),false,false,weatherTab);
    windSpeed = guienv->addEditBox(L"",irr::core::rect<irr::s32>(0.130*su,0.04*sh,0.180*su,0.07*sh),false,weatherTab,GUI_ID_WINDSPEED_EDITBOX );
    

    //Fill in initial info into dialog boxes:
    irr::f32 timeFloat = oldScenarioInfo.startTime/SECONDS_IN_HOUR;
    irr::u32 timeHrs = floor(timeFloat);
    irr::u32 timeMins = (timeFloat - timeHrs)*60;
    irr::core::stringw hoursString(timeHrs);
    irr::core::stringw minsString(timeMins);
    if (hoursString.size() == 1) hoursString = irr::core::stringw(L"0") + hoursString;
    if (minsString.size() == 1) minsString = irr::core::stringw(L"0") + minsString;
    startHours->setText(hoursString.c_str());
    startMins->setText(minsString.c_str());

    startYear->setText((irr::core::stringw(oldScenarioInfo.startYear)).c_str());

    descriptionEdit->setText(irr::core::stringw(oldScenarioInfo.description.c_str()).c_str());

    irr::core::stringw monthString(oldScenarioInfo.startMonth);
    if (monthString.size() == 1) monthString = irr::core::stringw(L"0") + monthString;
    startMonth->setText(monthString.c_str());

    irr::core::stringw dayString(oldScenarioInfo.startDay);
    if (dayString.size() == 1) dayString = irr::core::stringw(L"0") + dayString;
    startDay->setText(dayString.c_str());

    //SunRise, SunSet, Weather, Rain
    sunRise->setText((irr::core::stringw(oldScenarioInfo.sunRise)).c_str());
    sunSet->setText((irr::core::stringw(oldScenarioInfo.sunSet)).c_str());
    weather->setSelected(floor(oldScenarioInfo.weather*2));
    rain->setSelected(floor(oldScenarioInfo.rainIntensity*2));
    
    windDirection->setText((irr::core::stringw(oldScenarioInfo.windDirection)).c_str());
    windSpeed->setText((irr::core::stringw(oldScenarioInfo.windSpeed)).c_str());

    irr::s32 selectedVis;
    if (oldScenarioInfo.visibilityRange<=1) {
        selectedVis = Utilities::round(-10.0*oldScenarioInfo.visibilityRange + 28); //Equation of relation between visibility and items in visibility list where in the 0.1 to 1.0 range, with a spacing of 0.1)
    } else {
        selectedVis = Utilities::round(-2.0*oldScenarioInfo.visibilityRange + 20); //Equation of relation between visibility and items in visibility list where in the 1.0 to 10.0 range, with a spacing of 0.5)
    }
    if(selectedVis >= 0 && selectedVis < visibility->getItemCount()) {
        visibility->setSelected(selectedVis);
    } else if (selectedVis < 0) {
        visibility->setSelected(0);
    } else {
        visibility->setSelected(visibility->getItemCount()-1);
    }

    scenarioName->setText(irr::core::stringw(oldScenarioInfo.scenarioName.c_str()).c_str());

    //These get updated in updateGuiData
    mapCentreX = 0;
    mapCentreZ = 0;

    //Add an info box if in multiplayer mode
    if (multiplayer) {
        irr::gui::IGUIWindow* multiplayerInstructions = guienv->addMessageBox(L"",language->translate("multiplayerinfo").c_str());
    }

}

void GUIMain::updateEditBoxes()
{
    //Trigger update the edit boxes for course, speed & distance when the selection is changed.
    editBoxesNeedUpdating = true;
}

void GUIMain::updateGuiData(ScenarioData scenarioData, ChartView& chart, const std::vector<PositionData>& buoys, irr::s32 selectedShip, irr::s32 selectedLeg, irr::s32 hoverShip, bool draggingShip, irr::core::position2di mouse)
{
    irr::video::IVideoDriver* driver = device->getVideoDriver();
    irr::gui::IGUIFont* font = guienv->getSkin()->getFont();
    irr::s32 statusBarHeight = (font ? (irr::s32)font->getDimension(L"Ag").Height : 14) + 6;

    //Show the chart, with a lat/long grid
    chart.draw(driver);
    chart.drawGraticule(driver, font, 0, statusBarHeight);

    std::wstring styleText = L"Fond : " + chart.styleName();
    if (styleText != chartStyleShown) {
        chartStyleButton->setText(styleText.c_str());
        chartStyleShown = styleText;
    }

    //Map centre as displayed (where 'move' and 'add ship' place a ship)
    double centreX, centreZ;
    chart.centreXZ(centreX, centreZ);
    mapCentreX = centreX;
    mapCentreZ = centreZ;

    irr::f32 mapCentreLong = chart.xToLon(centreX);
    irr::f32 mapCentreLat = chart.zToLat(centreZ);

    //Convert lat/long into a readable format
    wchar_t eastWest;
    wchar_t northSouth;
    if (mapCentreLat >= 0) {
        northSouth='N';
    } else {
        northSouth='S';
    }
    if (mapCentreLong >= 0) {
        eastWest='E';
    } else {
        eastWest='W';
    }
    irr::f32 displayLat = fabs(mapCentreLat);
    irr::f32 displayLong = fabs(mapCentreLong);

    irr::f32 latMinutes = (displayLat - (int)displayLat)*60;
    irr::f32 lonMinutes = (displayLong - (int)displayLong)*60;
    irr::u8 latDegrees = (int) displayLat;
    irr::u8 lonDegrees = (int) displayLong;

    //update heading display element
    irr::core::stringw displayText = language->translate("pos");
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
    displayText.append(L" (");
    displayText.append(f32To4dp(mapCentreLat).c_str());
    displayText.append(L",");
    displayText.append(f32To4dp(mapCentreLong).c_str());
    displayText.append(L")\n");

    //Display
    dataDisplay->setText(displayText.c_str());

    //Note that this section is duplicated in constructor to populate with initial values
    //Show start time & data
    if (oldScenarioInfo.startTime != scenarioData.startTime) {
        irr::f32 timeFloat = scenarioData.startTime/SECONDS_IN_HOUR;
        irr::u32 timeHrs = floor(timeFloat);
        irr::u32 timeMins = (timeFloat - timeHrs)*60;
        irr::core::stringw hoursString(timeHrs);
        irr::core::stringw minsString(timeMins);
        if (hoursString.size() == 1) hoursString = irr::core::stringw(L"0") + hoursString;
        if (minsString.size() == 1) minsString = irr::core::stringw(L"0") + minsString;
        startHours->setText(hoursString.c_str());
        startMins->setText(minsString.c_str());
    }

    if (oldScenarioInfo.startYear != scenarioData.startYear) {
        startYear->setText((irr::core::stringw(scenarioData.startYear)).c_str());
    }

    if (oldScenarioInfo.startMonth != scenarioData.startMonth) {
        irr::core::stringw monthString(scenarioData.startMonth);
        if (monthString.size() == 1) monthString = irr::core::stringw(L"0") + monthString;
        startMonth->setText(monthString.c_str());
    }

    if (oldScenarioInfo.startDay != scenarioData.startDay) {
        irr::core::stringw dayString(scenarioData.startDay);
        if (dayString.size() == 1) dayString = irr::core::stringw(L"0") + dayString;
        startDay->setText(dayString.c_str());
    }

    if (oldScenarioInfo.sunRise != scenarioData.sunRise) {
        sunRise->setText(f32To2dp(scenarioData.sunRise).c_str());
    }
    if (oldScenarioInfo.sunSet != scenarioData.sunSet) {
        sunSet->setText(f32To2dp(scenarioData.sunSet).c_str());
    }
    if (oldScenarioInfo.weather != scenarioData.weather) {
        weather->setSelected(floor(scenarioData.weather*2));
    }
    if (oldScenarioInfo.rainIntensity != scenarioData.rainIntensity) {
        rain->setSelected(floor(scenarioData.rainIntensity*2));
    }
    if (oldScenarioInfo.visibilityRange != scenarioData.visibilityRange) {
        irr::s32 selectedVis;
        if (scenarioData.visibilityRange<=1) {
            selectedVis = Utilities::round(-10.0*scenarioData.visibilityRange + 28.0); //Equation of relation between visibility and items in visibility list where in the 0 to 1.0 range, with a spacing of 0.1)
        } else {
            selectedVis = Utilities::round(-2.0*scenarioData.visibilityRange + 20); //Equation of relation between visibility and items in visibility list where in the 1.0 to 10.0 range, with a spacing of 0.5)
        }
        if(selectedVis >= 0 && selectedVis < visibility->getItemCount()) {
            visibility->setSelected(selectedVis);
        } else if (selectedVis < 0) {
            visibility->setSelected(0);
        } else {
            visibility->setSelected(visibility->getItemCount()-1);
        }
    }
    if (oldScenarioInfo.windDirection != scenarioData.windDirection) {
        windDirection->setText((irr::core::stringw(scenarioData.windDirection)).c_str());
    }
    if (oldScenarioInfo.windSpeed != scenarioData.windSpeed) {
        windSpeed->setText((irr::core::stringw(scenarioData.windSpeed)).c_str());
    }
    if (oldScenarioInfo.scenarioName != scenarioData.scenarioName) {
        scenarioName->setText(irr::core::stringw(scenarioData.scenarioName.c_str()).c_str());
    }

    if (oldScenarioInfo.description != scenarioData.description) {
        descriptionEdit->setText(irr::core::stringw(scenarioData.description.c_str()).c_str());
    }

    //Initially set name colour as default, unless a warning is shown
    scenarioName->enableOverrideColor(false);

    //Check and warn about name validitiy for multiplayer
    if (multiplayer && ! scenarioData.multiplayerName) {
        //Name needs to have _mp at end
        scenarioName->setOverrideColor(irr::video::SColor(255, 255, 165, 0)); //Highlight in orange
        //Show relevant warning
        multiplayerNameWarning->setVisible(true);
        notMultiplayerNameWarning->setVisible(false);
    } else if (!multiplayer && scenarioData.multiplayerName) {
        //Name needs not to have _mp at end
        scenarioName->setOverrideColor(irr::video::SColor(255, 255, 165, 0)); //Highlight in orange
        //Show relevant warning
        notMultiplayerNameWarning->setVisible(true);
        multiplayerNameWarning->setVisible(false);
    } else {
        //Name ok for multiplayer status - hide warnings
        multiplayerNameWarning->setVisible(false);
        notMultiplayerNameWarning->setVisible(false);
    }

    //Check and warn about scenario overwriting
    if (scenarioData.willOverwrite) {
        scenarioName->setOverrideColor(irr::video::SColor(255, 255, 0, 0)); //Highlight in red
        overwriteWarning->setVisible(true); //Show warning
    } else {
        overwriteWarning->setVisible(false); //Hide warning
    }

    //End of duplicated section
    //Store what's been shown
    oldScenarioInfo = scenarioData;

    //Draw centre mark, buoys, ships and their routes, then the status line
    drawInformationOnMap(chart, scenarioData, buoys, selectedShip, selectedLeg, hoverShip);
    drawStatusBar(chart, mouse, draggingShip);

  //KYARA UPDATE -----------------------------------------------------
    if (editBoxesNeedUpdating) {
        if (selectedShip >= 0 && selectedShip < scenarioData.otherShipsData.size()) {

            // --- NEW STRICTLY UNIQUE MMSI LOGIC ---
            irr::u32 currentMmsi = scenarioData.otherShipsData.at(selectedShip).mmsi;
            if (currentMmsi == 0) {
                irr::u32 maxMmsi = 242000100; // Base MMSI
                for (size_t j = 0; j < scenarioData.otherShipsData.size(); j++) {
                    if (scenarioData.otherShipsData[j].mmsi > maxMmsi) {
                        maxMmsi = scenarioData.otherShipsData[j].mmsi;
                    }
                }
                currentMmsi = maxMmsi + 1; // Always grab the next available slot globally
                mmsiEdit->setText(irr::core::stringw(currentMmsi).c_str());
                manuallyTriggerGUIEvent(setMMSI, irr::gui::EGET_BUTTON_CLICKED);
            }
            else {
                mmsiEdit->setText(irr::core::stringw(currentMmsi).c_str());
            }

            isDrifting->setEnabled(true);
            isDrifting->setChecked(scenarioData.otherShipsData.at(selectedShip).drifting);
        }
        else if (selectedShip == -1) {

            // --- AUTO-POPULATE OWNSHIP MMSI ---
            irr::core::stringw currentText = mmsiEdit->getText();
            if (currentText.empty() || currentText == L"-" || currentText == L"0") {
                mmsiEdit->setText(L"242000100"); // Base strictly reserved for Own Ship
                manuallyTriggerGUIEvent(setMMSI, irr::gui::EGET_BUTTON_CLICKED);
            }

            isDrifting->setEnabled(false);
            isDrifting->setChecked(false);
        }

        if (selectedShip >= 0 && selectedShip < scenarioData.otherShipsData.size() && selectedLeg >= 0 && selectedLeg < scenarioData.otherShipsData.at(selectedShip).legs.size()) {
            // FORMATTED TO 1 DECIMAL (E.G. 6.0)
            legCourseEdit->setText(f32To1dp(scenarioData.otherShipsData.at(selectedShip).legs.at(selectedLeg).bearing).c_str());
            legSpeedEdit->setText(f32To1dp(scenarioData.otherShipsData.at(selectedShip).legs.at(selectedLeg).speed).c_str());

            //Distance
            if ((selectedLeg + 1) < scenarioData.otherShipsData.at(selectedShip).legs.size()) {
                irr::f32 legDurationS = scenarioData.otherShipsData.at(selectedShip).legs.at(selectedLeg + 1).startTime - scenarioData.otherShipsData.at(selectedShip).legs.at(selectedLeg).startTime;
                irr::f32 legDurationH = legDurationS / SECONDS_IN_HOUR;
                irr::f32 legDistanceNm = legDurationH * scenarioData.otherShipsData.at(selectedShip).legs.at(selectedLeg).speed;
                legDistanceEdit->setText(f32To2dp(legDistanceNm).c_str()); // FORMATTED
            }
            else {
                legDistanceEdit->setText(L"");
            }
        }
        else if (selectedShip == -1) {
            //Own ship
            legCourseEdit->setText(f32To1dp(scenarioData.ownShipData.initialBearing).c_str()); // FORMATTED
            legSpeedEdit->setText(f32To1dp(scenarioData.ownShipData.initialSpeed).c_str()); // FORMATTED
            legDistanceEdit->setText(L"---");
        }
        else {
            //Set blank (invalid other ship or leg)
            legCourseEdit->setText(L"");
            legSpeedEdit->setText(L"");
            legDistanceEdit->setText(L"");
        }

        //----------------------------------------------------------------------------------------
        //For visibility of ship selector boxes:
     
        if (selectedShip == -1) {
            otherShipTypeSelector->setVisible(false);
            ownShipTypeSelector->setVisible(true);
            //Find the ship name in the list that matches (if it exists)
            irr::core::stringw ownShipName = irr::core::stringw(scenarioData.ownShipData.ownShipName.c_str());
            for(int i = 0; i < ownShipTypeSelector->getItemCount(); i++) {
                irr::core::stringw thisName(ownShipTypeSelector->getItem(i));
                if (thisName.equals_ignore_case(ownShipName)) {ownShipTypeSelector->setSelected(i);
                //kyara 
                updateShipImageDisplay(getOwnShipTypeSelected());
                
                }
            }
        } else {
            otherShipTypeSelector->setVisible(true);
            ownShipTypeSelector->setVisible(false);
            //Find the ship name in the list that matches (if it exists)
            if (selectedShip >= 0 && selectedShip < scenarioData.otherShipsData.size()) {
                //Find the ship name in the list that matches (if it exists)
                irr::core::stringw otherShipName = irr::core::stringw(scenarioData.otherShipsData.at(selectedShip).shipName.c_str());
                for(int i = 0; i < otherShipTypeSelector->getItemCount(); i++) {
                    irr::core::stringw thisName(otherShipTypeSelector->getItem(i));
                    if (thisName.equals_ignore_case(otherShipName)) {otherShipTypeSelector->setSelected(i);
                    //kyara
                    updateShipImageDisplay(getOtherShipTypeSelected());
                    
                    }
                }
            }
        }

        editBoxesNeedUpdating = false;
    }

    //Update comboboxes for other ships and legs
    updateDropDowns(scenarioData.otherShipsData,selectedShip,scenarioData.startTime);

    //kyara 
    // Control visibility of the picture block so it only shows on the "Les navires" (Ships) tab
    if (tabControl->getActiveTab() == 1 && generalDataWindow->isVisible()) { // Tab 1 is the Ships tab (and the window is shown)
        if (hasValidImage) {
            shipImageDisplay->setVisible(true);
            shipImageNotFoundText->setVisible(false);
        }
        else {
            shipImageDisplay->setVisible(false);
            shipImageNotFoundText->setVisible(true);
        }
    }
    else {
        shipImageDisplay->setVisible(false);
        shipImageNotFoundText->setVisible(false);
    }

    guienv->drawAll();

}

namespace {
const irr::video::SColor kOwnShipFill(255, 70, 140, 255);
const irr::video::SColor kOtherShipFill(255, 0, 190, 165);
const irr::video::SColor kOtherShipText(255, 150, 255, 230);
const irr::video::SColor kRouteColour(255, 215, 70, 215);
const irr::video::SColor kSelectedLegColour(255, 255, 160, 0);
const irr::video::SColor kSelectColour(255, 0, 255, 255);
const irr::video::SColor kHoverColour(200, 255, 255, 255);
const irr::video::SColor kShadow(150, 0, 0, 0);
const irr::video::SColor kWhiteColour(255, 255, 255, 255);
const irr::video::SColor kBlackColour(255, 0, 0, 0);
}

void GUIMain::drawLabel(ChartView& chart, const std::wstring& text, irr::core::position2di at, irr::video::SColor colour)
{
    irr::gui::IGUIFont* font = guienv->getSkin()->getFont();
    if (!font) {
        return;
    }
    irr::core::recti clip = chart.getViewport();
    irr::core::dimension2du d = font->getDimension(text.c_str());
    irr::core::recti box(at.X - 2, at.Y, at.X + (irr::s32)d.Width + 2, at.Y + (irr::s32)d.Height);
    box.clipAgainst(clip);
    if (box.isValid()) {
        device->getVideoDriver()->draw2DRectangle(irr::video::SColor(150, 10, 16, 26), box);
    }
    ChartDraw::text(font, text, at, colour, &clip, false);
}

void GUIMain::drawInformationOnMap(ChartView& chart, const ScenarioData& scenarioInfo, const std::vector<PositionData>& buoys, irr::s32 selectedShip, irr::s32 selectedLeg, irr::s32 hoverShip)
{
    irr::video::IVideoDriver* driver = device->getVideoDriver();
    const std::vector<OtherShipData>& otherShips = scenarioInfo.otherShipsData;
    irr::f32 time = scenarioInfo.startTime;

    //Centre mark: where 'move' and 'add ship' place a ship
    irr::core::position2di c = chart.getViewport().getCenter();
    irr::video::SColor markColour = chart.lightBackground() ? irr::video::SColor(220, 40, 50, 60) : irr::video::SColor(220, 230, 230, 230);
    driver->draw2DLine(irr::core::position2di(c.X - 10, c.Y), irr::core::position2di(c.X - 3, c.Y), markColour);
    driver->draw2DLine(irr::core::position2di(c.X + 3, c.Y), irr::core::position2di(c.X + 10, c.Y), markColour);
    driver->draw2DLine(irr::core::position2di(c.X, c.Y - 10), irr::core::position2di(c.X, c.Y - 3), markColour);
    driver->draw2DLine(irr::core::position2di(c.X, c.Y + 3), irr::core::position2di(c.X, c.Y + 10), markColour);

    //Buoys
    for (std::vector<PositionData>::const_iterator it = buoys.begin(); it != buoys.end(); ++it) {
        ChartDraw::marker(driver, chart.toScreenXZ(it->X, it->Z), ChartDraw::Diamond, irr::video::SColor(255, 255, 210, 0), irr::video::SColor(255, 40, 40, 40), 4);
    }

    //Other ships: route, then the ship symbol and its label
    for (irr::u32 i = 0; i < otherShips.size(); i++) {
        const OtherShipData& ship = otherShips.at(i);
        bool selected = (selectedShip == (irr::s32)i);
        irr::core::position2di shipPos = chart.toScreenXZ(ship.initialX, ship.initialZ);

        //Route from the start position along each leg, except the final 'stop' leg. Leg times are from
        //the start of the day of the scenario start; the current leg is the one running at the start.
        if (ship.legs.size() > 1) {
            irr::u32 currentLeg = ship.legs.size() - 1;
            for (irr::u32 l = 0; l + 1 < ship.legs.size(); l++) {
                if (time >= ship.legs.at(l).startTime && time < ship.legs.at(l + 1).startTime) {
                    currentLeg = l;
                }
            }
            std::vector<irr::core::position2di> route;
            std::vector<irr::u32> routeLeg; //leg drawn by the segment ending at route[k + 1]
            route.push_back(shipPos);
            irr::f32 x = ship.initialX;
            irr::f32 z = ship.initialZ;
            for (irr::u32 l = currentLeg; l + 1 < ship.legs.size(); l++) {
                irr::f32 legStart = (l == currentLeg) ? time : ship.legs.at(l).startTime;
                irr::f32 duration = ship.legs.at(l + 1).startTime - legStart;
                if (!(duration < 1e9f)) { break; } //Stopped (zero speed) leg: never ends
                if (!(duration > 0)) { continue; }
                irr::f32 distance = duration * ship.legs.at(l).speed * KTS_TO_MPS;
                x += distance * sin(ship.legs.at(l).bearing * RAD_IN_DEG);
                z += distance * cos(ship.legs.at(l).bearing * RAD_IN_DEG);
                route.push_back(chart.toScreenXZ(x, z));
                routeLeg.push_back(l);
            }
            if (route.size() > 1) {
                irr::video::SColor routeColour = selected ? kRouteColour : irr::video::SColor(170, kRouteColour.getRed(), kRouteColour.getGreen(), kRouteColour.getBlue());
                ChartDraw::polyline(driver, route, routeColour, !selected, selected ? 2 : 1);
                for (irr::u32 k = 0; k < routeLeg.size(); k++) {
                    if (selected && (irr::s32)routeLeg.at(k) == selectedLeg) {
                        std::vector<irr::core::position2di> segment;
                        segment.push_back(route.at(k));
                        segment.push_back(route.at(k + 1));
                        ChartDraw::polyline(driver, segment, kSelectedLegColour, false, 3);
                    }
                    ChartDraw::marker(driver, route.at(k + 1), ChartDraw::Square, routeColour, kBlackColour, selected ? 4 : 3);
                    if (selected) {
                        //Leg number, as in the leg list, half way along the leg
                        irr::core::position2di mid((route.at(k).X + route.at(k + 1).X) / 2, (route.at(k).Y + route.at(k + 1).Y) / 2);
                        drawLabel(chart, std::wstring(irr::core::stringw(routeLeg.at(k) + 1).c_str()), mid + irr::core::position2di(6, -6),
                            (irr::s32)routeLeg.at(k) == selectedLeg ? kSelectedLegColour : kRouteColour);
                    }
                }
            }
        }

        irr::f32 heading = ship.legs.size() > 0 ? ship.legs.at(0).bearing : 0;
        ChartDraw::ship(driver, shipPos, heading, kOtherShipFill, kBlackColour, 12.0f);

        irr::core::stringw label(i + 1);
        label.append(L" ");
        label.append(ship.shipName.c_str());
        drawLabel(chart, std::wstring(label.c_str()), shipPos + irr::core::position2di(21, -8), kOtherShipText);
    }

    //Own ship on top, with a six minute run along its initial course when under way
    irr::core::position2di ownPos = chart.toScreenXZ(scenarioInfo.ownShipData.initialX, scenarioInfo.ownShipData.initialZ);
    if (fabs(scenarioInfo.ownShipData.initialSpeed) > 0.01) {
        irr::f32 run = scenarioInfo.ownShipData.initialSpeed * KTS_TO_MPS * 360.0f;
        irr::f32 bearing = scenarioInfo.ownShipData.initialBearing * RAD_IN_DEG;
        std::vector<irr::core::position2di> vector;
        vector.push_back(ownPos);
        vector.push_back(chart.toScreenXZ(scenarioInfo.ownShipData.initialX + run * sin(bearing), scenarioInfo.ownShipData.initialZ + run * cos(bearing)));
        ChartDraw::polyline(driver, vector, kOwnShipFill, false, 2);
    }
    ChartDraw::ship(driver, ownPos, scenarioInfo.ownShipData.initialBearing, kOwnShipFill, kWhiteColour, 15.0f);
    drawLabel(chart, std::wstring(language->translate("own").c_str()), ownPos + irr::core::position2di(21, -8), irr::video::SColor(255, 150, 200, 255));

    //Hover and selection rings (selectedShip: -1 own ship, 0.. other ships; hoverShip: 0 own ship, 1.. other ships)
    irr::s32 selectedIndex = selectedShip + 1;
    for (int pass = 0; pass < 2; pass++) {
        irr::s32 ship = (pass == 0) ? hoverShip : selectedIndex;
        if (ship < 0 || ship > (irr::s32)otherShips.size() || (pass == 0 && ship == selectedIndex)) {
            continue;
        }
        irr::core::position2di at = (ship == 0) ? ownPos : chart.toScreenXZ(otherShips.at(ship - 1).initialX, otherShips.at(ship - 1).initialZ);
        ChartDraw::ring(driver, at, 19.0f, kShadow);
        ChartDraw::ring(driver, at, 18.0f, pass == 0 ? kHoverColour : kSelectColour);
    }
}

void GUIMain::drawStatusBar(ChartView& chart, irr::core::position2di mouse, bool draggingShip)
{
    irr::video::IVideoDriver* driver = device->getVideoDriver();
    irr::gui::IGUIFont* font = guienv->getSkin()->getFont();
    if (!font) {
        return;
    }
    const irr::core::recti& vp = chart.getViewport();
    irr::s32 barHeight = (irr::s32)font->getDimension(L"Ag").Height + 6;
    irr::core::recti bar(vp.UpperLeftCorner.X, vp.LowerRightCorner.Y - barHeight, vp.LowerRightCorner.X, vp.LowerRightCorner.Y);
    driver->draw2DRectangle(irr::video::SColor(170, 0, 0, 0), bar);

    //Cursor position, then the mouse and key controls
    std::wstring text;
    if (vp.isPointInside(mouse)) {
        IncidentPoint p = chart.toLatLong(mouse);
        wchar_t buf[64];
        double lat = fabs(p.lat);
        double lon = fabs(p.lon);
        swprintf(buf, 64, L"%02d\u00B0%06.3f'%lc  %03d\u00B0%06.3f'%lc", (int)lat, (lat - (int)lat) * 60.0, p.lat >= 0 ? L'N' : L'S',
            (int)lon, (lon - (int)lon) * 60.0, p.lon >= 0 ? L'E' : L'W');
        text = buf;
        text += L"    ";
    }
    if (draggingShip) {
        text += L"Rel\u00E2chez pour poser le navire";
    } else {
        text += L"clic sur un navire : le choisir, glisser : le d\u00E9placer    glisser la carte : la d\u00E9placer    molette : zoom    Origine : recentrer    fl\u00E8ches gauche/droite : cap du navire s\u00E9lectionn\u00E9";
    }
    ChartDraw::text(font, text, irr::core::position2di(bar.UpperLeftCorner.X + 10, bar.UpperLeftCorner.Y + 3), irr::video::SColor(255, 220, 220, 220), &bar, false);

    ChartDraw::scaleBar(driver, font, chart, barHeight);
}

void GUIMain::selectShip(irr::s32 shipIndex)
{
    if (shipIndex < 0 || shipIndex >= (irr::s32)shipSelector->getItemCount()) {
        return;
    }
    tabControl->setActiveTab(1); //Ships tab, to show the chosen ship's details
    if (shipSelector->getSelected() != shipIndex) {
        shipSelector->setSelected(shipIndex);
        manuallyTriggerGUIEvent((irr::gui::IGUIElement*)shipSelector, irr::gui::EGET_COMBO_BOX_CHANGED);
    }
}

void GUIMain::updateDropDowns(const std::vector<OtherShipData>& otherShips, irr::s32 selectedShip, irr::f32 time) {

//Update drop down menus for ships and legs

    //Update text in ship selector list. If a new item, make sure it's selected
    irr::s32 shipSelectorSelection = shipSelector->getSelected();
    bool changedShipSelectorLength = (shipSelector->getItemCount() != otherShips.size() + 1);
    bool initialiseList = (shipSelector->getItemCount() == 0); //If there were no items in list, then we're populating it for the first time (we'll use this to select the first item)
    shipSelector->clear();
    shipSelector->addItem(language->translate("own").c_str()); //add own ship (at index 0)
    for(irr::u32 i = 0; i<otherShips.size(); i++) { //Add other ships (at index 1,2,...)
        irr::core::stringw otherShipLabel(irr::core::stringw(i+1));
        otherShipLabel.append(L" ");
        otherShipLabel.append(otherShips.at(i).shipName.c_str());
        shipSelector->addItem(otherShipLabel.c_str());
    }
    //Set selection
    if (changedShipSelectorLength) {
        //Select the first item if new, or the last one if it's just been added to the existing list
        if (initialiseList) {
            shipSelector->setSelected(0);
        } else {
            shipSelector->setSelected(shipSelector->getItemCount()-1); //Select the newly added item (I think that the 'trigger gui event' should make sure that the model selection follows suit
        }
        manuallyTriggerGUIEvent((irr::gui::IGUIElement*)shipSelector, irr::gui::EGET_COMBO_BOX_CHANGED); //Trigger event here so any changes caused by the update are found
    } else {
        //Re-select previously selected item
        shipSelector->setSelected(shipSelectorSelection);
    }

    //Find number of legs for selected ship if known
    irr::u32 selectedShipNoLegs = 0;
    if (selectedShip>=0) {
        if (otherShips.size() > selectedShip) { //SelectedShip is valid
            selectedShipNoLegs = otherShips.at(selectedShip).legs.size();
        }
    }
    //Update number of legs displayed, if required
    if(selectedShipNoLegs>0) {selectedShipNoLegs--;} //Note that we display legs-1, as the final 'stop' leg shouldn't be changed by the user
    if(legSelector->getItemCount() != selectedShipNoLegs) {
        legSelector->clear();
        for(irr::u32 i = 0; i<selectedShipNoLegs; i++) {
            legSelector->addItem(irr::core::stringw(i+1).c_str());
        }
        manuallyTriggerGUIEvent((irr::gui::IGUIElement*)legSelector, irr::gui::EGET_LISTBOX_CHANGED ); //Trigger event here so any changes caused by the update are found

    } else {
        //don't clear and update, but show which legs are past, current and future
        if (legSelector->getItemCount() > 0) {

            //Get legs for selected ship
            if (selectedShip>=0 && otherShips.size() > selectedShip) { //SelectedShip is valid
                std::vector<LegData> selectedShipLegs = otherShips.at(selectedShip).legs;

                //Find current leg (FIXME: Duplicated code)
                //Find current leg: This is the last leg, or the leg where the start time is in the past, and then next start time is in the future. Leg times are from the start of the day of the scenario start.
                irr::u32 currentLeg = 0;
                bool currentLegFound = false;
                for (irr::u32 i=0; i < (selectedShipLegs.size()-1); i++) {
                    if (time >= selectedShipLegs.at(i).startTime &&  time < selectedShipLegs.at(i+1).startTime) {
                        currentLeg = i;
                        currentLegFound = true;
                    }
                }
                if (!currentLegFound) {
                    currentLeg = selectedShipLegs.size()-1;
                }

                //Update text for past, current and future legs.
                for (irr::u32 i=0; i<legSelector->getItemCount(); i++) {
                    if (i < currentLeg) {
                        std::wstring label(irr::core::stringw(i+1).c_str());
                        //label.append(language->translate("past").c_str());
                        legSelector->setItem(i,label.c_str(),-1);
                    }
                    if (i == currentLeg) {
                        std::wstring label(irr::core::stringw(i+1).c_str());
                        //label.append(language->translate("current").c_str());
                        legSelector->setItem(i,label.c_str(),-1);
                    }
                    if (i > currentLeg) {
                        std::wstring label(irr::core::stringw(i+1).c_str());
                        //label.append(language->translate("future").c_str());
                        legSelector->setItem(i,label.c_str(),-1);
                    }
                }

            } //Selected ships valid
        } //At least one leg in selector
    } //Update descriptive text on legs, if they don't need updating entirely

}

bool GUIMain::manuallyTriggerGUIEvent(irr::gui::IGUIElement* caller, irr::gui::EGUI_EVENT_TYPE eType) {

    irr::SEvent triggerUpdateEvent;
    triggerUpdateEvent.EventType = irr::EET_GUI_EVENT;
    triggerUpdateEvent.GUIEvent.Caller = caller;
    triggerUpdateEvent.GUIEvent.Element = 0;
    triggerUpdateEvent.GUIEvent.EventType = eType;
    return device->postEventFromUser(triggerUpdateEvent);
}

irr::f32 GUIMain::getEditBoxCourse() const {
    //irr::f32 course = _wtof(legCourseEdit->getText()); //TODO: Check portability
    wchar_t* endPtr;
    irr::f32 course = wcstof(legCourseEdit->getText(), &endPtr);
    return course;
}

irr::f32 GUIMain::getEditBoxSpeed() const {
    //irr::f32 speed = _wtof(legSpeedEdit->getText()); //TODO: Check portability
    wchar_t* endPtr;
    irr::f32 speed = wcstof(legSpeedEdit->getText(), &endPtr); //TODO: Check portability
    return speed;
}

irr::f32 GUIMain::getEditBoxDistance() const {
    //irr::f32 distance = _wtof(legDistanceEdit->getText()); //TODO: Check portability
    wchar_t* endPtr;
    irr::f32 distance = wcstof(legDistanceEdit->getText(), &endPtr); //TODO: Check portability
    return distance;
}

irr::u32 GUIMain::getEditBoxMMSI() const {
    wchar_t* endPtr;
    irr::u32 mmsi = wcstol(mmsiEdit->getText(),&endPtr,10); //TODO: Check portability
    return mmsi;
}

int GUIMain::getSelectedShip() const {
    return shipSelector->getSelected();
}

int GUIMain::getSelectedLeg() const {
    //Note that this returns the leg, starting at 0 (Different from controller implementation, which starts at 1 (not 0))
    return legSelector->getSelected();
}


std::string GUIMain::getOwnShipTypeSelected() const {
    //Todo: Instead of this, should probably use the strings directly from 'std::vector<std::string> ownShipTypes'
    if (ownShipTypeSelector->getSelected()<0) {return "";} //If nothing selected
    std::wstring wideName(ownShipTypeSelector->getItem(ownShipTypeSelector->getSelected()));
    std::string nameString(wideName.begin(), wideName.end());
    return nameString;
}

std::string GUIMain::getOtherShipTypeSelected() const {
    //Todo: Instead of this, should probably use the strings directly from 'std::vector<std::string> otherShipTypes'

    if (otherShipTypeSelector->getSelected()<0) {return "";} //If nothing selected
    std::wstring wideName(otherShipTypeSelector->getItem(otherShipTypeSelector->getSelected()));
    std::string nameString(wideName.begin(),wideName.end());
    return nameString;
}


irr::core::vector2df GUIMain::getScreenCentrePosition() const {
    return irr::core::vector2df(mapCentreX, mapCentreZ);
}

/*
irr::gui::IGUIEditBox* startHours;
    irr::gui::IGUIEditBox* startMins;
    irr::gui::IGUIEditBox* startDay;
    irr::gui::IGUIEditBox* startMonth;
    irr::gui::IGUIEditBox* startYear;
    irr::gui::IGUIEditBox* sunRise;
    irr::gui::IGUIEditBox* sunSet;
    irr::gui::IGUIComboBox* weather;
    irr::gui::IGUIComboBox* rain;
*/

irr::f32 GUIMain::getStartTime() const {
    wchar_t* endPtr;
    irr::f32 hours = floor(wcstof(startHours->getText(),&endPtr));
    wchar_t* endPtr2; //Is this needed? Is endPtr changed in the call above
    irr::f32 mins = floor(wcstof(startMins->getText(),&endPtr2));

    return ((hours + mins/60.0) * SECONDS_IN_HOUR);
}

irr::u32 GUIMain::getStartDay() const {
    wchar_t* endPtr;
    return wcstof(startDay->getText(),&endPtr);
}

irr::u32 GUIMain::getStartMonth() const {
    wchar_t* endPtr;
    return wcstof(startMonth->getText(),&endPtr);
}

irr::u32 GUIMain::getStartYear() const {
    wchar_t* endPtr;
    return wcstof(startYear->getText(),&endPtr);
}

irr::f32 GUIMain::getSunRise() const {
    wchar_t* endPtr;
    return wcstof(sunRise->getText(),&endPtr);
}

irr::f32 GUIMain::getSunSet() const {
    wchar_t* endPtr;
    return wcstof(sunSet->getText(),&endPtr);
}

irr::f32 GUIMain::getWeather() const {

    return ((irr::f32)weather->getSelected())/2.0; //Entries for integer and half values, so Nth entry is for N/2
}

irr::f32 GUIMain::getRain() const {
    return ((irr::f32)rain->getSelected())/2.0;
}

irr::f32 GUIMain::getVisibility() const {
    //Get value from string in drop down.
    std::wstring wStringVal = std::wstring(visibility->getText());
    std::string sStringVal(wStringVal.begin(), wStringVal.end());
    irr::f32 value = Utilities::lexical_cast<irr::f32>(sStringVal);
    return value;
}

irr::f32 GUIMain::getWindDirection() const {
    wchar_t* endPtr;
    return wcstof(windDirection->getText(),&endPtr);
}
    
irr::f32 GUIMain::getWindSpeed() const {
    wchar_t* endPtr;
    return wcstof(windSpeed->getText(),&endPtr);
}

std::string GUIMain::getScenarioName() const {

    //Convert from wide to narrow string: Todo: Think about having this all wide.
    std::wstring wideName(scenarioName->getText());
    std::string scenarioNameString(wideName.begin(),wideName.end());

    //Strip any invalid characters: /\*:"|?<>
    replace(scenarioNameString.begin(), scenarioNameString.end(),'/',' ');
    replace(scenarioNameString.begin(), scenarioNameString.end(),'\\',' ');
    replace(scenarioNameString.begin(), scenarioNameString.end(),'*',' ');
    replace(scenarioNameString.begin(), scenarioNameString.end(),':',' ');
    replace(scenarioNameString.begin(), scenarioNameString.end(),'"',' ');
    replace(scenarioNameString.begin(), scenarioNameString.end(),'|',' ');
    replace(scenarioNameString.begin(), scenarioNameString.end(),'?',' ');
    replace(scenarioNameString.begin(), scenarioNameString.end(),'<',' ');
    replace(scenarioNameString.begin(), scenarioNameString.end(),'>',' ');

    scenarioNameString = Utilities::trim(scenarioNameString);

    return scenarioNameString;
}

std::string GUIMain::getDescription() const {
    //Convert from wide to narrow string: Todo: Think about having this all wide.
    std::wstring wideDescription(descriptionEdit->getText());
    std::string descriptionString(wideDescription.begin(),wideDescription.end());
    return descriptionString;
}

std::wstring GUIMain::f32To3dp(irr::f32 value) const
{
    //Convert a floating point value to a wstring, with 3dp
    char tempStr[100];
    snprintf(tempStr,100,"%.3f",value);
    return std::wstring(tempStr, tempStr+strlen(tempStr));
}

std::wstring GUIMain::f32To4dp(irr::f32 value) const
{
    //Convert a floating point value to a wstring, with 3dp
    char tempStr[100];
    snprintf(tempStr,100,"%.4f",value);
    return std::wstring(tempStr, tempStr+strlen(tempStr));

}

std::wstring GUIMain::f32To2dp(irr::f32 value) const
{
    char tempStr[100];
    snprintf(tempStr, 100, "%.2f", value);
    return std::wstring(tempStr, tempStr + strlen(tempStr));
}

std::wstring GUIMain::f32To1dp(irr::f32 value) const
{
    char tempStr[100];
    snprintf(tempStr, 100, "%.1f", value);
    return std::wstring(tempStr, tempStr + strlen(tempStr));
}


//kyara 
void GUIMain::updateShipImageDisplay(std::string shipName) {
    if (shipName.empty()) {
        hasValidImage = false;
        return;
    }

    // Path to the image folder you created
    std::string imagePath = "boat_pictures/" + shipName + ".png";

    // Attempt to load the texture
    irr::video::ITexture* texture = device->getVideoDriver()->getTexture(imagePath.c_str());

    if (texture) {
        shipImageDisplay->setImage(texture);
        hasValidImage = true;
    }
    else {
        hasValidImage = false;
    }

}
bool GUIMain::wantsToReturnToMenu() const {
    return returnToMenuFlag;
}

void GUIMain::setReturnToMenu() {
    returnToMenuFlag = true;
}


//hide/show controls 
void GUIMain::hideUI() {
    generalDataWindow->setVisible(false);
    toggleUIButton->setText(L"Afficher Menu");
}

void GUIMain::toggleUI() {
    if (generalDataWindow->isVisible()) {
        hideUI();
    }
    else {
        generalDataWindow->setVisible(true);
        toggleUIButton->setText(L"Cacher Menu");
    }
}