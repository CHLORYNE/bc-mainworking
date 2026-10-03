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

#ifndef __GUIMAIN_HPP_INCLUDED__
#define __GUIMAIN_HPP_INCLUDED__

#include "irrlicht.h"
#include "Lang.hpp"
#include "OperatingModeEnum.hpp"
#include "HeadingIndicator.h"
     //#include "RateOfTurnIndicator.h" // DEE addition
#include "OutlineScrollBar.h"
#include "AzimuthDial.h"
#include "GUIRectangle.hpp"
#include "GUIInstrumentPanel.hpp" //kyara: instrument console
#include "GUIEngineLever.hpp"     //kyara: styled engine levers
#include "RadarCalculation.hpp"
#include "ShipLights.hpp" //KYARA FEUX TAB: SIT_COUNT / OVERRIDE_SLOTS size the id ranges below
#include <vector>
#include <string>

// Forward declarations
class SimulationModel;
class ConsoleWindow;

struct GUIData {
    irr::f32 radarOffsetX, radarOffsetY;
    std::wstring distressTimer;
    irr::f32 lat;
    irr::f32 longitude;
    irr::f32 cursorLat;   //kyara: latitude du curseur radar
    irr::f32 cursorLong;  //kyara: longitude du curseur radar
    irr::f32 hdg;
    irr::f32 viewAngle;
    irr::f32 viewElevationAngle;
    irr::f32 spd; // Show speed through water
    irr::f32 cog = 0; //kyara: course over ground, deg (for the instrument console)
    irr::f32 sog = 0; //kyara: speed over ground, m/s (for the instrument console)
    irr::f32 portRPM = 0; //kyara: shaft tachometer
    irr::f32 stbdRPM = 0;
    irr::f32 maxRPM = 0;  //full-ahead revs, from the ship's MaxRevs
    irr::f32 portEng;
    irr::f32 stbdEng;
    irr::f32 rudder;
    irr::f32 bowThruster;
    irr::f32 sternThruster;
    irr::f32 wheel;
    irr::f32 portAzimuthAngle;
    irr::f32 stbdAzimuthAngle;
    bool azimuth1Master;
    bool azimuth2Master;
    irr::f32 RateOfTurn;
    irr::f32 depth;
    irr::f32 weather;
    irr::f32 lightningFlash;   // KYARA 0..1
    irr::f32 rain;
    irr::f32 visibility;
    irr::f32 windDirection;
    irr::f32 windSpeed;
    irr::f32 streamDirection;
    irr::f32 streamSpeed;
    irr::gui::IGUIScrollBar* magSlider;
    bool streamOverride;
    bool radarOn;
    irr::f32 radarRangeNm;
    int radarRangeRingBrightness;   //kyara: 0=clair, 1=faible, 2=off - so GUI labels match the rings
    irr::f32 radarGain;
    irr::f32 radarClutter;
    irr::f32 radarRain;



    irr::f32 guiRadarEBLBrg[2];
    irr::f32 guiRadarVRMNm[2];
    irr::u32 guiRadarActiveEBL;
    irr::u32 guiRadarActiveVRM;
    irr::s32 guiRadarGuardAlarmMode;



    irr::f32 guiRadarCursorBrg;
    irr::f32 guiRadarCursorRangeNm;
    irr::s32 arpaListSelection;
    std::vector<ARPAEstimatedState> arpaContactStates;
    std::string currentTime;
    bool distressActive = false;                  // Inc 3 (comms)
    std::vector<std::wstring> maydayLines;        // Inc 3 (comms)
    std::vector<std::wstring> commsLog;           // Inc 3 (comms)
    bool paused;
    bool collided;
    bool proxyAlarmMuted;   //KYARA: proximity alarm silenced with 'P'
    bool headUp;
    bool radarStabilised;
    irr::s32 guiRadarRingLevel;   //kyara: 0=clair, 1=faible, 2=off (pour teinter le bouton)
    irr::s32 guiRadarEchoStretch; //kyara: 0/1/2 pour teinter le bouton ES
    bool pump1On;
    bool pump2On;
    // DEE_NOV22 Azimuth Drive related gui items
    irr::f32 schottelPort; // angle of the schottels +ve clockwise 0 dead ahead
    irr::f32 schottelStbd;
    irr::f32 thrustLeverPort; // thrust levers (0..1) leave this in here for future graphical lever
    irr::f32 thrustLeverStbd;
    irr::f32 azimuthEnginePort;
    irr::f32 azimuthEngineStbd;
    irr::f32 emergencySteering;
    bool azimuthClutchPort;  // Clutches true for engaged false for disengaged
    bool azimuthClutchStbd;

    // the angle of each azimuth drive and each engine level is defined elsewhere
    // DEE_NOV22 some indication and or switch from normal steering to non follow up emergency steering

    irr::f32 tideHeight; // DEE FEB 23

    // KYARA HOULE
    irr::f32 pitch = 0;        // deg, + bow up (as shown on screen)
    irr::f32 roll = 0;         // deg, + heeled to starboard
    irr::f32 motionScale = 1;  // instructor setting, 1 = realistic
    irr::f32 swellHs = 0;      // m, long-wave (swell) part
    irr::f32 seaHs = 0;        // m, whole sea state
    irr::f32 swellTp = 0;      // s
    irr::f32 swellDirFrom = 0; // deg true, waves come from
};

class GUIMain //Create, build and update GUI
{
public:
    GUIMain();
    ~GUIMain();
    void load(irr::IrrlichtDevice* device, Lang* language, std::vector<std::string>* logMessages, SimulationModel* model, bool singleEngine, bool azimuthDrive, bool controlsHidden, bool hasDepthSounder, irr::f32 maxSounderDepth, bool hasGPS, bool showTideHeight, bool hasBowThruster, bool hasSternThruster, bool hasRateOfTurnIndicator, bool showCollided, bool vr3dMode);

    enum GUI_ELEMENTS// Define some values that we'll use to identify individual GUI controls.
    {
        GUI_ID_HEADING_SCROLL_BAR = 101,
        GUI_ID_SPEED_SCROLL_BAR,
        GUI_ID_PORT_SCROLL_BAR,
        GUI_ID_STBD_SCROLL_BAR,
        GUI_ID_AZIMUTH_1,
        GUI_ID_AZIMUTH_2,
        GUI_ID_LIGHTING_TIME_BOX,
        GUI_ID_AZIMUTH_1_MASTER_CHECKBOX,
        GUI_ID_AZIMUTH_2_MASTER_CHECKBOX,
        //kyara
        GUI_ID_TOGGLE_PRIMARY_CONTROLS_BUTTON,
        // DEE_NOV22 vvvv
        GUI_ID_SCHOTTEL_PORT,
        GUI_ID_SCHOTTEL_STBD,
        GUI_ID_AZIMUTH_ENGINE_PORT,
        GUI_ID_AZIMUTH_ENGINE_STBD,
        GUI_ID_AZIMUTH_CLUTCH_PORT,
        GUI_ID_AZIMUTH_CLUTCH_STBD,
        GUI_ID_EMERGENCY_STEERING,
        // DEE_NOV22 ^^^^
        GUI_ID_RUDDER_SCROLL_BAR,
        // DEE vvvv
        GUI_ID_WHEEL_SCROLL_BAR,
        GUI_ID_RATE_OF_TURN_SCROLL_BAR,
        GUI_ID_RATE_OF_TURN_INDICATOR,
        // DEE ^^^^
        GUI_ID_BOWTHRUSTER_SCROLL_BAR,
        GUI_ID_STERNTHRUSTER_SCROLL_BAR,
        GUI_ID_START_BUTTON,
        GUI_ID_RADAR_ONOFF_BUTTON,
        GUI_ID_BIG_RADAR_BUTTON,
        GUI_ID_SMALL_RADAR_BUTTON,
        GUI_ID_RADAR_INCREASE_BUTTON,
        GUI_ID_RADAR_DECREASE_BUTTON,
        GUI_ID_RADAR_GAIN_SCROLL_BAR,
        GUI_ID_RADAR_CLUTTER_SCROLL_BAR,
        GUI_ID_RADAR_RAIN_SCROLL_BAR,
        GUI_ID_RADAR_EBL_LEFT_BUTTON,
        GUI_ID_RADAR_EBL_RIGHT_BUTTON,
        GUI_ID_RADAR_EBL_UP_BUTTON,
        GUI_ID_RADAR_EBL_DOWN_BUTTON,
        //radar


        GUI_ID_RADAR_EBL_SELECT_BUTTON,
        GUI_ID_RADAR_VRM_SELECT_BUTTON,
        //colors EBL VRM 
        GUI_ID_RADAR_EBL_COLOUR_BUTTON,
        GUI_ID_RADAR_VRM_COLOUR_BUTTON,
        GUI_ID_RADAR_GUARD_ALARM_BUTTON,
        //------END
        GUI_ID_RADAR_ARPA_TAB_BUTTON,
        GUI_ID_RADAR_AIS_TAB_BUTTON,
        GUI_ID_RADAR_INCREASE_X_BUTTON,
        GUI_ID_RADAR_DECREASE_X_BUTTON,
        GUI_ID_RADAR_INCREASE_Y_BUTTON,
        GUI_ID_RADAR_DECREASE_Y_BUTTON,
        GUI_ID_RADAR_NORTH_BUTTON,
        GUI_ID_RADAR_COURSE_BUTTON,
        GUI_ID_RADAR_HEAD_BUTTON,
        GUI_ID_RADAR_COLOUR_BUTTON,
        GUI_ID_NFU_PORT_BUTTON,
        GUI_ID_NFU_STBD_BUTTON,
        GUI_ID_PI_SELECT_BOX,
        GUI_ID_PI_RANGE_BOX,
        GUI_ID_PI_BEARING_BOX,
        GUI_ID_BIG_PI_SELECT_BOX,
        GUI_ID_BIG_PI_RANGE_BOX,
        GUI_ID_BIG_PI_BEARING_BOX,
        GUI_ID_ARPA_ON_BOX,
        GUI_ID_BIG_ARPA_ON_BOX,
        GUI_ID_ARPA_TRUE_REL_BOX,
        GUI_ID_ARPA_VECTOR_TIME_BOX,
        GUI_ID_BIG_ARPA_TRUE_REL_BOX,
        GUI_ID_BIG_ARPA_MODE_BUTTON,
        GUI_ID_BIG_ARPA_VECTOR_BUTTON,
        GUI_ID_RADAR_HEADING_MODE_BUTTON,
        GUI_ID_RADAR_ECHO_STRETCH_BUTTON,
        GUI_ID_BIG_ARPA_VECTOR_TIME_BOX,
        GUI_ID_ARPA_LIST,
        GUI_ID_MANUAL_SCAN_BUTTON,
        GUI_ID_MANUAL_NEW_BUTTON,
        GUI_ID_MANUAL_CLEAR_BUTTON,
        GUI_ID_RADAR_ARPA_BUOYS_CHECKBOX,
        GUI_ID_RADAR_BUOY_TRAILS_CHECKBOX,
        GUI_ID_RADAR_SHIP_TRAILS_CHECKBOX,
        GUI_ID_RADAR_OWN_SHIP_TRAILS_CHECKBOX,
        GUI_ID_RADAR_MMSI_CHECKBOX,
        GUI_ID_RADAR_BUOY_COLOUR_BOX,
        GUI_ID_RADAR_SHIP_COLOUR_BOX,
        GUI_ID_BIG_ARPA_LIST,
        GUI_ID_WEATHER_SCROLL_BAR,
        GUI_ID_RAIN_SCROLL_BAR,
        GUI_ID_VISIBILITY_SCROLL_BAR,
        GUI_ID_STORM_PRESET_BUTTON,
        GUI_ID_THUNDER_CHECKBOX,
        GUI_ID_LIGHTNING_CHECKBOX,
        GUI_ID_MOTION_SCALE_SCROLL_BAR, // KYARA HOULE
        GUI_ID_WINDDIRECTION_SCROLL_BAR,
        GUI_ID_WINDSPEED_SCROLL_BAR,
        GUI_ID_STREAMDIRECTION_SCROLL_BAR,
        GUI_ID_STREAMSPEED_SCROLL_BAR,
        GUI_ID_STREAMOVERRIDE_BOX,
        GUI_ID_MAGNIFICATION_SCROLL_BAR,
        GUI_ID_SHOW_INTERFACE_BUTTON,
        GUI_ID_HIDE_INTERFACE_BUTTON,
        GUI_ID_BINOS_INTERFACE_BUTTON,
        GUI_ID_BEARING_INTERFACE_BUTTON,
        GUI_ID_SHOW_LOG_BUTTON,
        GUI_ID_SHOW_EXTRA_CONTROLS_BUTTON,
        GUI_ID_HIDE_EXTRA_CONTROLS_BUTTON,
        GUI_ID_SHOW_LINES_CONTROLS_BUTTON,
        GUI_ID_HIDE_LINES_CONTROLS_BUTTON,
        GUI_ID_RUDDERPUMP_1_WORKING_BUTTON,
        GUI_ID_RUDDERPUMP_1_FAILED_BUTTON,
        GUI_ID_RUDDERPUMP_2_WORKING_BUTTON,
        GUI_ID_RUDDERPUMP_2_FAILED_BUTTON,
        GUI_ID_FOLLOWUP_WORKING_BUTTON,
        GUI_ID_FOLLOWUP_FAILED_BUTTON,
        GUI_ID_ACK_ALARMS_BUTTON,
        GUI_ID_ADD_LINE_BUTTON,
        GUI_ID_REMOVE_LINE_BUTTON,
        GUI_ID_KEEP_SLACK_LINE_CHECKBOX,
        GUI_ID_HAUL_IN_LINE_CHECKBOX,
        GUI_ID_ANCHOR_LINE_CHECKBOX,
        GUI_ID_LINES_LIST,
        GUI_ID_CHANGE_VIEW_BUTTON,
        GUI_ID_EXIT_BUTTON,
        GUI_ID_CLOSE_BOX,
        GUI_ID_RADAR_RANGE_RINGS_BUTTON,
        GUI_ID_RADAR_OFFCENTRE_BUTTON,
        GUI_ID_COMMS_MINIMISE_BUTTON,                  // Inc 3 (comms): minimise/restore toggle
        //KYARA FEUX TAB
        GUI_ID_LIGHTS_VESSEL_COMBO,
        GUI_ID_LIGHTS_SIT_0,                                                  // + Situation
        GUI_ID_LIGHTS_SIT_END = GUI_ID_LIGHTS_SIT_0 + ShipLights::SIT_COUNT,
        GUI_ID_LIGHTS_OVR_0 = GUI_ID_LIGHTS_SIT_END,                          // + override slot
        GUI_ID_LIGHTS_OVR_END = GUI_ID_LIGHTS_OVR_0 + ShipLights::OVERRIDE_SLOTS,
        GUI_ID_LIGHTS_OVR_RESET = GUI_ID_LIGHTS_OVR_END,
        GUI_ID_LIGHTS_DECK_CHECKBOX,
        GUI_ID_INSTR_LIGHTS_0,                                                // 0 off, 1 dim, 2 bright
        GUI_ID_INSTR_LIGHTS_END = GUI_ID_INSTR_LIGHTS_0 + 3,
        GUI_ID_OWN_DECK_LIGHTS_CHECKBOX = GUI_ID_INSTR_LIGHTS_END,
        GUI_ID_DETACH_CONSOLE_BUTTON, //instrument console in its own window (second screen)

    };

    bool getShowInterface() const;
    //True when the 3D view only fills the top of the screen (2D interface shown with the console
    //below it). False when the 3D view fills the screen - interface hidden, or console detached.
    bool getCompact3dView() const;

    //Instrument console in a separate window, so it can go on another screen and leave the whole
    //main screen to the bridge view. The window's place is remembered for the next session.
    void toggleConsoleDetached();
    bool isConsoleDetached() const { return consoleDetached; }
    //Call before load(). Instance: which copy of the simulator this is on the PC (1 for the first), so
    //that each copy keeps its own console window placement. Screen: desktop area of a screen given
    //to the console (launcher -console N, bc5.ini console_monitor) - the console opens there, without
    //a frame, filling it, and that placement is not saved.
    void setInstanceNumber(irr::u32 instance) { consoleInstance = instance; }
    void setConsoleScreen(const irr::core::rect<irr::s32>& area) { consoleScreen = area; consoleOnScreen = true; }
    //Call once per frame, after the main window's endScene(): handles the console window's input and
    //draws it. Restores the driver's screen size for the main window before returning.
    void renderDetachedConsole();
    //Pass to the main window's beginScene() (switches the OpenGL context back from the console window).
    const irr::video::SExposedVideoData& getMainVideoData() const;
    //Saves the console window placement and puts the console back. Call before the device is dropped.
    void shutdownConsoleWindow();
    bool getSmallRadarEnabled() const; //kyara: false when the instrument console replaces the small radar
    //kyara
    void togglePrimaryControls();
    void setAisDataMode(bool on); //kyara
    //KYARA FEUX TAB
    void refreshLightsTab();       // pull the selected vessel's light state into both tabs
    int getLightsVessel() const;   // -1 = own ship, 0.. = other ship
    irr::s32 adjustMagnification(irr::s32 delta); //Returns the new raw scrollbar position
    void toggleShow2dInterface();
    void show2dInterface();
    void hide2dInterface();
    bool getShow3d() const;
    void zoomOn();
    void zoomOff();
    void toggleBearings();
    void showBearings();
    void hideBearings();
    void setLargeRadar(bool radarState);
    bool getLargeRadar() const;
    void setARPAComboboxes(irr::s32 arpaState);
    void setARPAList(int arpaSelected);
    irr::u32 getRadarPixelRadius() const;
    irr::core::vector2di getCursorPositionRadar() const;
    irr::core::rect<irr::s32> getSmallRadarRect() const;
    irr::core::rect<irr::s32> getLargeRadarRect() const;
    bool isNFUActive() const;
    void setSingleEngine(); //Used for single engine operation
    void hideEngineAndRudder(); //Used for secondary mode

    //    void setInstruments(bool hasDepthSounder, irr::f32 maxSounderDepth, bool hasGPS);
    void updateGuiData(GUIData* guiData);
    void cycleEBLColour() { eblColourIndex = (eblColourIndex + 1) % 6; }
    void cycleVRMColour() { vrmColourIndex = (vrmColourIndex + 1) % 6; }

    void showLogWindow();
    void drawGUI();
    void setExtraControlsWindowVisible(bool windowVisible);
    void setLinesControlsWindowVisible(bool windowVisible);
    void setLinesControlsText(std::string textToShow);
    bool getAnchorLine() const;

private:
    irr::gui::IGUIButton* offCentreButton2;
    irr::IrrlichtDevice* device;
    irr::gui::IGUIEnvironment* guienv;
    //collision -KYARA
    irr::video::ITexture* collisionWarningTexture;
    irr::video::ITexture* alarmMutedTexture;   //KYARA: optional media/alarm_muted.png
    //adding hide button -kyara
    bool showPrimaryControls;
    irr::gui::IGUIButton* togglePrimaryControlsButton;
    irr::gui::IGUIEditBox* lightingTimeBox;
    //declaration for engine colors -Kyara

    irr::video::SColor engineTopColor;
    irr::video::SColor engineBottomColor;
    //end of declaration of engine colors
    irr::gui::IGUIScrollBar* hdgScrollbar;
    irr::gui::IGUIScrollBar* spdScrollbar;
    irr::gui::IGUIScrollBar* portScrollbar;
    irr::gui::IGUIScrollBar* stbdScrollbar;
    irr::gui::OutlineScrollBar* wheelScrollbar;
    irr::gui::IGUIScrollBar* bowThrusterScrollbar;
    irr::gui::IGUIScrollBar* sternThrusterScrollbar;
    irr::gui::IGUIStaticText* portText;
    irr::gui::IGUIStaticText* stbdText;
    irr::gui::IGUIStaticText* dataDisplay;
    irr::gui::IGUIStaticText* radarText;
    irr::gui::IGUIStaticText* radarPosText2;        //kyara: grand radar - position navire (lat/long)

    irr::gui::IGUIStaticText* radarCursorPosText2; //kyara: position (lat/long) du curseur
    irr::gui::IGUIButton* arpaModeButton2;    //kyara: soft-key ARPA (Man/MARPA/ARPA)
    irr::gui::IGUIButton* arpaVectorButton2;  //kyara: soft-key vecteurs (Vrai/Rel)
    irr::gui::IGUIButton* headingModeButton2; //kyara: soft-key cap (ARPA/Réel)
    irr::gui::IGUIButton* echoStretchButton2; //kyara: échostretch
    irr::gui::IGUIScrollBar* rateofturnScrollbar;

    irr::gui::AzimuthDial* azimuth1Control;
    irr::gui::AzimuthDial* azimuth2Control;

    irr::gui::IGUICheckBox* azimuth1Master;
    irr::gui::IGUICheckBox* azimuth2Master;

    // DEE_NOV22 vvvv
    irr::gui::AzimuthDial* azimuthEnginePort;
    irr::gui::AzimuthDial* azimuthEngineStbd;
    irr::gui::AzimuthDial* schottelPort;
    irr::gui::AzimuthDial* schottelStbd;
    irr::gui::IGUICheckBox* azimuthClutchPort;
    irr::gui::IGUICheckBox* azimuthClutchStbd;
    irr::gui::IGUICheckBox* emergencySteering;


    // DEE_NOV22 ^^^^

    irr::gui::IGUIListBox* arpaList;
    irr::gui::IGUIListBox* arpaText;
    irr::gui::IGUIButton* pausedButton;
    irr::gui::IGUIButton* bigRadarButton;
    irr::gui::IGUIButton* smallRadarButton;
    irr::gui::IGUIButton* radarOnOffButton;
    irr::gui::IGUIButton* eblLeftButton;
    irr::gui::IGUIButton* eblRightButton;
    irr::gui::IGUIButton* eblUpButton;
    irr::gui::IGUIButton* eblDownButton;

    irr::gui::IGUIButton* eblSelectButton;
    irr::gui::IGUIButton* vrmSelectButton;
    irr::gui::IGUIButton* eblColourButton;
    irr::gui::IGUIButton* vrmColourButton;
    irr::gui::IGUIButton* eblColourButton2;
    irr::gui::IGUIButton* vrmColourButton2;

    irr::gui::IGUIButton* guardAlarmButton2;
    irr::gui::IGUIButton* rangeRingsButton2 = 0;   //kyara
    //kyara: boutons d'orientation mémorisés pour pouvoir surligner l'actif
    irr::gui::IGUIButton* northUpButton = 0;
    irr::gui::IGUIButton* courseUpButton = 0;
    irr::gui::IGUIButton* headUpButton = 0;
    irr::gui::IGUIButton* northUpButton2 = 0;
    irr::gui::IGUIButton* courseUpButton2 = 0;
    irr::gui::IGUIButton* headUpButton2 = 0;
    bool guiRadarStabilised = false;   //kyara: copie locale pour le surlignage orientation
    irr::u32 eblColourIndex = 0;   //kyara: palette index for EBL1/EBL2
    irr::u32 vrmColourIndex = 0;   //kyara: palette index for VRM1/VRM2
    irr::gui::IGUIButton* radarCursorLeftButton;
    irr::gui::IGUIButton* radarCursorRightButton;
    irr::gui::IGUIButton* radarCursorUpButton;
    irr::gui::IGUIButton* radarCursorDownButton;
    irr::gui::IGUIButton* radarCursorLeftButton2;
    irr::gui::IGUIButton* radarCursorRightButton2;
    irr::gui::IGUIButton* radarCursorUpButton2;
    irr::gui::IGUIButton* radarCursorDownButton2;
    irr::gui::IGUIButton* radarColourButton;
    irr::gui::IGUIButton* radarColourButton2;
    irr::gui::IGUIButton* nonFollowUpPortButton;
    irr::gui::IGUIButton* nonFollowUpStbdButton;
    irr::gui::IGUIButton* changeViewButton;

    irr::gui::IGUITabControl* radarTabControl;
    irr::gui::IGUIScrollBar* radarGainScrollbar;
    irr::gui::IGUIScrollBar* radarClutterScrollbar;
    irr::gui::IGUIScrollBar* radarRainScrollbar;

    irr::gui::IGUIRectangle* largeRadarControls; //Parent rectangle for large radar controls
    irr::gui::IGUIRectangle* largeRadarPIControls; //Parent for PI controls on large radar
    irr::gui::IGUIScrollBar* radarGainScrollbar2; //For large radar
    irr::gui::IGUIScrollBar* radarClutterScrollbar2; //For large radar
    irr::gui::IGUIScrollBar* radarRainScrollbar2; //For large radar
    irr::gui::IGUIButton* eblLeftButton2;
    irr::gui::IGUIButton* eblRightButton2;
    irr::gui::IGUIButton* eblUpButton2;
    irr::gui::IGUIButton* eblDownButton2;

    irr::gui::IGUIButton* eblSelectButton2;
    irr::gui::IGUIButton* vrmSelectButton2;


    irr::gui::IGUIStaticText* radarText2;
    irr::gui::IGUIStaticText* radarCursorRangeText2; //kyara: grand radar - curseur distance (ligne+couleur)
    irr::gui::IGUIStaticText* radarCursorBrgText2;   //kyara: grand radar - curseur relèvement (ligne+couleur)
    irr::gui::IGUIListBox* arpaList2;
    irr::gui::IGUIListBox* arpaText2;
    irr::gui::IGUIListBox* aisText2;      //kyara: onglet AIS
    irr::gui::IGUIButton* arpaTabButton2; //kyara
    irr::gui::IGUIButton* aisTabButton2;  //kyara
    bool aisDataMode;                     //kyara: false=ARPA, true=AIS
    // RADAR PANEL
    irr::gui::IGUIStaticText* radarLeftInfoTop;
    irr::gui::IGUIStaticText* radarLeftInfoBottom;
    //------------------END
    irr::gui::IGUIScrollBar* visibilityScrollbar;
    irr::gui::IGUIScrollBar* weatherScrollbar;
    irr::gui::IGUIScrollBar* rainScrollbar;
    irr::gui::IGUIScrollBar* windDirectionScrollbar;
    irr::gui::IGUIScrollBar* windSpeedScrollbar;
    irr::gui::IGUIScrollBar* streamDirectionScrollbar;
    irr::gui::IGUIScrollBar* streamSpeedScrollbar;
    irr::gui::IGUICheckBox* streamOverride;

    //KYARA: live numeric readouts beside each weather/wind slider
    irr::gui::IGUIStaticText* weatherValue;
    irr::gui::IGUIScrollBar* motionScaleScrollbar = 0; // KYARA HOULE
    irr::gui::IGUIStaticText* motionScaleValue = 0;    // KYARA HOULE
    irr::gui::IGUIStaticText* swellInfoText = 0;       // KYARA HOULE: "Houle 1.2 m - 7 s - du 270"
    irr::gui::IGUIStaticText* rainValue;
    irr::gui::IGUIStaticText* visibilityValue;
    irr::gui::IGUIStaticText* windDirectionValue;
    irr::gui::IGUIStaticText* windSpeedValue;
    irr::gui::IGUIStaticText* streamDirectionValue;
    irr::gui::IGUIStaticText* streamSpeedValue;
    irr::gui::HeadingIndicator* headingIndicator;

    //KYARA: instrument console (replaces the text data box, heading tape and small radar in the normal view)
    irr::gui::GUIInstrumentPanel* instrumentPanel = 0;
    bool instrumentsEnabled = true;       //bc5.ini classic_panel=1 turns it off

    //Detached console: the panel and its status column (RADAR, pumps, ack) are moved under
    //consoleHost - a parentless element, so they leave the main GUI tree - and drawn in consoleWindow.
    ConsoleWindow* consoleWindow = 0;
    irr::gui::IGUIElement* consoleHost = 0;
    bool consoleDetached = false;
    bool consoleWindowUsed = false;                  //the OpenGL drawable has been switched at least once
    irr::core::rect<irr::s32> consolePanelAttachedRect;
    irr::core::rect<irr::s32> consoleStatusAttachedRect[4]; //RADAR, pump 1, pump 2, ack
    irr::core::dimension2du consoleLaidOutSize;
    irr::gui::IGUIElement* consoleInputTarget = 0;   //element under a button press in the console window
    irr::gui::IGUIButton* detachConsoleButton = 0;
    irr::u32 consoleLastRenderMs = 0;
    irr::u32 consoleFrameMs = 33;                    //console window refresh, ~30 Hz
    irr::u32 consoleRenderCount = 0, consoleRateStartMs = 0, consoleRenderRate = 0;
    irr::video::SExposedVideoData noVideoData;
    std::string consoleFontName;                     //bc5.ini font, for bigger lettering in a big console window
    irr::s32 consoleBaseFontSize = 12;
    irr::s32 consoleBaseStatusW = 0;                 //status column width with the normal font
    irr::f32 consoleAttachedGaugeD = 0;              //dial diameter on the main screen
    void layoutDetachedConsole(const irr::core::dimension2du& size);
    irr::s32 consoleStatusWidthFor(irr::gui::IGUIFont* font) const;
    irr::s32 consolePlaceX = 80, consolePlaceY = 80;  //console window placement (consoleWindow.ini)
    irr::u32 consolePlaceW = 0, consolePlaceH = 0;
    irr::u32 consoleInstance = 1;                    //copy of the simulator: consoleWindow-N.ini from the second
    bool consoleOnScreen = false;                    //console given a screen of its own
    irr::core::rect<irr::s32> consoleScreen;
    void setConsoleDetached(bool detached);
    void layoutConsoleStatusColumn();
    void dispatchConsoleWindowInput();
    void applyDetachedConsoleVisibility();
    void saveConsolePlacement(bool detached);
    irr::u32 mainWindowFPS() const;                  //driver FPS minus the console window's own frames
    bool instrumentExtraRPM = false;      //bc5.ini instrument_extra
    bool instrumentExtraWind = false;
    irr::f32 guiCOG = 0;                  //deg
    irr::f32 guiSOGKts = 0;               //knots
    irr::f32 guiRudder = 0;               //deg, -ve port
    irr::f32 guiWheel = 0;                //helm order, deg
    irr::f32 guiRateOfTurnDegMin = 0;     //deg/min, +ve starboard
    irr::f32 guiPortRPM = 0;              //rev/min, -ve astern
    irr::f32 guiStbdRPM = 0;              //rev/min
    irr::f32 guiMaxRPM = 0;               //rev/min at full ahead, from the ship's MaxRevs
    irr::f32 guiWindDirection = 0;        //deg true, direction the wind blows from
    irr::f32 guiWindSpeed = 0;            //knots

    irr::gui::IGUIScrollBar* magnificationScrollbar;
    irr::gui::IGUICheckBox* show3d;

    irr::gui::IGUIButton* showInterfaceButton;
    irr::gui::IGUIButton* hideInterfaceButton;
    irr::gui::IGUIButton* binosButton;
    irr::gui::IGUIButton* bearingButton;
    irr::gui::IGUIButton* exitButton;
    irr::gui::IGUIButton* pcLogButton;
    irr::gui::IGUIButton* showExtraControlsButton;
    irr::gui::IGUIButton* showLinesControlsButton;

    irr::gui::IGUIButton* addLine;
    irr::gui::IGUIButton* removeLine;
    irr::gui::IGUICheckBox* keepLineSlack;
    irr::gui::IGUICheckBox* heaveLineIn;
    irr::gui::IGUICheckBox* anchorLine;
    irr::gui::IGUIListBox* linesList;
    irr::gui::IGUIStaticText* linesText;

    irr::gui::IGUIStaticText* pump1On;
    irr::gui::IGUIStaticText* pump2On;
    irr::gui::IGUIButton* ackAlarms;

    irr::gui::IGUIStaticText* clickForRudderText;
    irr::gui::IGUIStaticText* clickForEngineText;

    irr::gui::IGUIWindow* extraControlsWindow;
    irr::gui::IGUIWindow* linesControlsWindow;

    irr::u32 su;
    irr::u32 sh;

    irr::s32 azimuthGUIOffsetL;
    irr::s32 azimuthGUIOffsetR;

    irr::f32 guiLat;
    irr::f32 guiRadarOffsetX, guiRadarOffsetY;
    irr::f32 guiSpd; //kyara: vitesse navire (SOG) pour le bloc données
    irr::f32 guiLong;
    irr::f32 guiCursorLat;   //kyara
    irr::f32 guiCursorLong;  //kyara
    irr::f32 guiHeading;
    irr::f32 viewHdg;
    irr::f32 viewElev;
    irr::f32 guiSpeed;
    irr::f32 guiDepth;
    irr::f32 guiTideHeight;
    irr::f32 guiPitch = 0; // KYARA HOULE
    irr::f32 guiRoll = 0;  // KYARA HOULE
    bool guiRadarOn;
    irr::f32 guiRadarRangeNm;
    int guiRadarRangeRingBrightness = 0;   //kyara: mirrors the radar's range-ring brightness cycle
    irr::f32 guiRadarGain;
    irr::f32 guiRadarClutter;
    irr::f32 guiRadarRain;
    irr::f32 guiRadarEBLBrg[2];
    irr::f32 guiRadarVRMNm[2];
    irr::u32 guiRadarActiveEBL;
    irr::u32 guiRadarActiveVRM;
    irr::s32 guiRadarGuardAlarmMode;
    irr::f32 guiRadarCursorBrg;
    irr::f32 guiRadarCursorRangeNm;
    bool radarHeadUp;
    bool radarLarge;
    irr::core::rect<irr::s32> radarLargeRect;
    irr::s32 largeRadarScreenCentreX;
    irr::s32 largeRadarScreenCentreY;
    irr::s32 largeRadarScreenRadius;
    irr::s32 smallRadarScreenCentreX;
    irr::s32 smallRadarScreenCentreY;
    irr::s32 smallRadarScreenRadius;
    std::vector<ARPAEstimatedState> arpaContactStates;
    std::string guiTime;
    bool singleEngine;
    bool azimuthDrive;
    bool hasBowThruster;
    bool hasSternThruster;
    bool hasRateOfTurnIndicator;
    bool guiPaused;
    bool guiCollided;
    bool guiProxyAlarmMuted;   //KYARA
    irr::f32 guiLightningFlash;   //KYARA
    void drawCommsOverlay();                       // Inc 3 (comms)
    bool guiDistressActive = false;                // Inc 3 (comms)
    std::vector<std::wstring> guiMaydayLines;      // Inc 3 (comms)
    std::vector<std::wstring> guiCommsLog;         // Inc 3 (comms)
    irr::gui::IGUIButton* commsMinButton = 0;      // Inc 3 (comms): minimise/restore toggle (push button, polled)
    void drawLightningFlash();    //KYARA
    irr::f32 guiPrevLightningFlash = 0.0f;                        // KYARA: rising-edge detect
    std::vector<std::vector<irr::core::vector2di>> boltStrokes;   // KYARA: current bolt polylines
    void generateLightningBolt();                                // KYARA
    void drawLightningBolt();                                    // KYARA
    bool showInterface;
    bool controlsHidden; //If controls should always be hidden (if a secondary screen etc)

    bool hasDepthSounder;
    irr::f32 maxSounderDepth;
    bool hasGPS;
    bool showTideHeight;
    bool showCollided;

    Lang* language;
    std::vector<std::string>* logMessages;
    SimulationModel* model;
    //KYARA FEUX TAB - in-class initialisers, so refreshLightsTab() is safe even before load()
    irr::gui::IGUIComboBox* lightsVesselBox = 0;
    irr::gui::IGUIButton* lightsSitButton[ShipLights::SIT_COUNT] = {};
    irr::gui::IGUICheckBox* lightsOverrideBox[ShipLights::OVERRIDE_SLOTS] = {};
    irr::gui::IGUICheckBox* lightsDeckBox = 0;
    irr::gui::IGUIStaticText* lightsStatusText = 0;
    irr::gui::IGUIButton* instrLightsButton[3] = {};
    irr::gui::IGUICheckBox* ownDeckLightsBox = 0;
    irr::gui::IGUIStaticText* interiorStatusText = 0;

    //Different locations for heading indicator depending on GUI visibility
    irr::core::rect<irr::s32> stdHdgIndicatorPos;
    irr::core::rect<irr::s32> radHdgIndicatorPos;
    irr::core::rect<irr::s32> maxHdgIndicatorPos;

    irr::core::rect<irr::s32> stdDataDisplayPos;
    irr::core::rect<irr::s32> radDataDisplayPos;
    irr::core::rect<irr::s32> altDataDisplayPos;
    irr::video::SColor stdDataDisplayBG;
    irr::video::SColor altDataDisplayBG;
    irr::video::SColor radDataDisplayBG;

    irr::core::rect<irr::s32> stdRateOfTurnIndicatorPos;

    bool nfuPortDown;
    bool nfuStbdDown;
    void setButtonHighlight(irr::gui::IGUIButton* button, irr::video::SColor colour); //kyara
    void setButtonHighlight(irr::gui::IGUIButton* button, irr::video::SColor colour, const wchar_t* labelText); //kyara
    void updateVisibility();
    void hideInSecondary();
    void draw2dRadar();
    void draw2dBearing();
    void drawCollisionWarning();
    void drawAlarmMutedIndicator();   //KYARA

    std::wstring guiDistressTimer;
    std::wstring f32To1dp(irr::f32 value);
    std::wstring f32To2dp(irr::f32 value);
    std::wstring f32To3dp(irr::f32 value);
    bool manuallyTriggerClick(irr::gui::IGUIButton* button);
    bool manuallyTriggerScroll(irr::gui::IGUIScrollBar* bar);


};

#endif