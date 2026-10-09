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

#ifndef __RADARCALCULATION_HPP_INCLUDED__
#define __RADARCALCULATION_HPP_INCLUDED__

#include "irrlicht.h"

#include <vector>
#include <string>
#include <stdint.h> //for uint64_t

#include <ctime> //To check time elapsed between changing EBL when button held down

class Terrain;
class OwnShip;
class Buoys;
class OtherShips;
struct RadarData;

enum ARPA_CONTACT_TYPE {
    CONTACT_NONE,
    CONTACT_NORMAL,
    CONTACT_MANUAL
};

struct ARPAScan {
    //ARPA scan information based on the assumption that the own ship position is well known
    irr::f32 x; //Absolute metres
    irr::f32 z; //Absolute metres
    uint64_t timeStamp; //Timestamp in seconds
    //irr::f32 estimatedRCS; //Estimated radar cross section
    irr::f32 rangeNm; //Reference only
    irr::f32 bearingDeg; //For reference only

    ARPAScan() {
        x = 0;
        z = 0;
        timeStamp = 0;
        rangeNm = 0;
        bearingDeg = 0;
    }
};

struct ARPAEstimatedState {
    irr::u32 displayID; //User displayed ID
    bool stationary; // E.g. if detected as static and a small RCS or a buoy.
    irr::f32 absVectorX; //Estimated X speed (m/s)
    irr::f32 absVectorZ; //Estimated Z speed (m/s)
    irr::f32 absHeading; //Estimated heading (deg)
    irr::f32 trueHeading; //kyara: cap réel du contact (vs absHeading estimé)
    irr::f32 relVectorX; //Estimated X speed (m/s)
    irr::f32 relVectorZ; //Estimated Z speed (m/s)
    irr::f32 relHeading; //Estimated heading (deg)
    irr::f32 range; //Estimated current range from own ship (Nm)
    irr::f32 bearing; //Estimated current bearing from own ship (deg)
    irr::f32 speed; //Estimated current speed (Kts)
    bool lost; //True if we last scanned more than a defined time ago.
    irr::f32 cpa; //Closest point of approach in Nm
    irr::f32 tcpa; //Time to closest point of approach (mins)
    ARPA_CONTACT_TYPE contactType; //Duplicate of what's in the parent, but useful to pass to the GUI
    bool isBuoy;
    irr::u32 mmsi = 0;
    bool danger = false;      //CPA and TCPA both inside the alarm limits
    irr::f32 trialCpa = 0;    //with the trial manoeuvre (Nm), when one is set
    irr::f32 trialTcpa = 0;   //(mins)
    bool trialDanger = false;

    ARPAEstimatedState() {
        trueHeading = 0; //kyara
        displayID = 0;
        stationary = false;
        absVectorX = 0;
        absVectorZ = 0;
        absHeading = 0;
        relVectorX = 0;
        relVectorZ = 0;
        relHeading = 0;
        range = 0;
        bearing = 0;
        speed = 0;
        lost = false;
        cpa = 0;
        tcpa = 0;
        contactType = CONTACT_NONE;
        isBuoy = false;
        mmsi = 0; // assign the member (was previously shadowed by a local 'irr::u32 mmsi')
    }
};

struct ARPAContact {
    std::vector<ARPAScan> scans;
    irr::f32 totalXMovementEst; //Estimates of total movement (sum of absolutes) in X and Z, to help detect stationary contacts
    irr::f32 totalZMovementEst;
    ARPA_CONTACT_TYPE contactType;
    irr::u32 mmsi;
    void* contact;
    bool isBuoy;
    //irr::u32 displayID;
    ARPAEstimatedState estimate;
    //kyara flicker fix
    bool usingRecentReference;   // NEW: hysteresis state for vector flicker fix
    bool wasInGuardZone;         //kyara: guard zone alarm transition tracking (previous frame in/out)
    bool guardAlarmLatched;      //kyara: this contact is currently sustaining the guard alarm (mode-dependent: IN=inside, OUT=left)
    bool dangerAcknowledged = false; //CPA alarm silenced for this contact (until it becomes dangerous again)
    uint64_t safeSince = 0;          //when it last stopped being dangerous (0: dangerous, or never was)


    ARPAContact() {
        totalXMovementEst = 0;
        totalZMovementEst = 0;
        contactType = CONTACT_NONE;
        contact = 0;
        isBuoy = false;
        mmsi = 0; // ensure MMSI starts at 0 (was uninitialised)
        //kyara
        usingRecentReference = false;   // NEW
        wasInGuardZone = false;
        guardAlarmLatched = false;      //kyara
    }
};

class RadarCalculation
{
public:
    RadarCalculation();
    virtual ~RadarCalculation();
    void load(std::string radarConfigFile, irr::IrrlichtDevice* dev);
    void increaseRange();
    void decreaseRange();
    irr::f32 getRangeNm() const;
    void setGain(irr::f32 value);
    void setClutter(irr::f32 value);
    void setRainClutter(irr::f32 value);
    void increaseClutter(irr::f32 value);
    void decreaseClutter(irr::f32 value);
    void increaseRainClutter(irr::f32 value);
    void decreaseRainClutter(irr::f32 value);
    void increaseGain(irr::f32 value);
    void decreaseGain(irr::f32 value);
    void toggleRadarOffCentre();
    irr::f32 getGain() const;
    irr::f32 getClutter() const;
    irr::f32 getRainClutter() const;
    irr::f32 getEBLRangeNm() const;
    irr::f32 getEBLBrg() const;
    //kyara: EBL1/2 + VRM1/2 support
    irr::f32 getEBLBrg(irr::u32 index) const;      //indexed access for GUI
    irr::f32 getVRMRangeNm(irr::u32 index) const;
    void selectNextEBL(); //toggle active EBL between 1 and 2
    void selectNextVRM();
    irr::u32 getActiveEBL() const;
    irr::u32 getActiveVRM() const;
    //kyara: guard zone alarm (zone = between VRM1/VRM2 ranges and EBL1->EBL2 bearings)
    void cycleGuardAlarmMode(); //0=off, 1=alarm on entry (IN), 2=alarm on exit (OUT)
    int getGuardAlarmMode() const;
    bool isGuardAlarmSounding() const;
    //kyara: absolute setters, used for network sync of radar state (secondary -> primary)
    void setEBLBrg(irr::u32 index, irr::f32 brg);
    void setVRMRange(irr::u32 index, irr::f32 rangeNm);
    void setActiveEBL(irr::u32 index);
    void setActiveVRM(irr::u32 index);
    void setGuardAlarmMode(int mode);
    irr::f32 getCursorRangeNm() const;
    irr::f32 getCursorBrg() const;
    void setPIData(irr::s32 PIid, irr::f32 PIbearing, irr::f32 PIrange);
    irr::f32 getPIbearing(irr::s32 PIid) const;
    irr::f32 getPIrange(irr::s32 PIid) const;
    void increaseCursorRangeXNm();
    void decreaseCursorRangeXNm();
    void increaseCursorRangeYNm();
    void decreaseCursorRangeYNm();
    void increaseEBLRange();
    void decreaseEBLRange();
    void increaseEBLBrg();
    void decreaseEBLBrg();
    void setNorthUp();
    void setCourseUp();
    void setHeadUp();
    bool getHeadUp() const; //Head or course up
    bool getStabilised() const;            //kyara: pour distinguer CAP / ROUTE côté GUI
    void cycleRangeRingBrightness();       //kyara: anneaux de portée -> clair / faible / off
    int  getRangeRingBrightness() const;   //kyara
    void toggleRadarOn();
    //buoy arpa 


    void setArpaOnBuoys(bool on);
    bool getArpaOnBuoys() const;
    bool isRadarOn() const;
    int getArpaMode() const;

    // NEW GLOBAL TOGGLES
    void setBuoyTrails(bool state);
    void setShipTrails(bool state);
    void setOwnShipTrails(bool state);
    void setMMSI(bool state);

    //Marker colours for ARPA contacts (buoys vs ships)
    void setBuoyContactColour(int paletteIndex);
    void setShipContactColour(int paletteIndex);
    irr::video::SColor getBuoyContactColour() const;
    irr::video::SColor getShipContactColour() const;
    static irr::video::SColor paletteIndexToColour(int paletteIndex);
    void setArpaListSelection(irr::s32 selection);
    void setArpaMode(int mode);
    void setRadarARPARel();
    void setRadarARPATrue();
    void setUseRealHeading(bool state); //kyara: triangle = cap réel vs cap ARPA estimé
    void cycleEchoStretch();          //kyara: échostretch OFF->ES1->ES2
    void toggleOffCentre();                 //kyara: décentrage -> curseur / retour centre
    irr::f32 getOffsetXFraction() const;    //kyara: fraction du rayon (-0.66..0.66)
    irr::f32 getOffsetYFraction() const;
    int  getEchoStretch() const;
    bool getUseRealHeading() const;

    irr::s32 getArpaListSelection() const;
    void setRadarARPAVectors(irr::f32 vectorMinutes);
    void setRadarDisplayRadius(irr::u32 radiusPx);
    void changeRadarColourChoice();
    irr::u32 getARPATracksSize() const;
    int getARPAContactIDFromTrackIndex(irr::u32 trackIndex) const;
    ARPAContact getARPAContactFromTrackIndex(irr::u32 trackIndex) const;
    void addManualPoint(bool newContact, irr::core::vector3d<int64_t> offsetPosition, const OwnShip& ownShip, uint64_t absoluteTime);
    void clearManualPoints();
    void trackTargetFromCursor();
    void clearTargetFromCursor();
    irr::video::SColor getRadarForegroundColour() const;
    irr::video::SColor getRadarBackgroundColour() const;
    irr::video::SColor getRadarSurroundColour() const;
    //Mouse on the scope: a press or a release of the left or right button, with the mouse position
    //relative to the scope centre in pixels (as for update). Handled at the next update:
    //left click = track the echo there (switches MARPA on if ARPA is off), right click = stop
    //tracking it; while a parallel index line is being drawn, left drag = draw it, right click = clear it.
    void scopeMouse(bool left, bool down, irr::core::vector2di mouseRelPosition);
    bool takeArpaModeChangedByClick(); //true once after a click switched MARPA on

    //Parallel index lines drawn with the mouse
    static const int PI_LINES = 4;
    void setPIEditLine(int line);      //-1: not drawing; 0..PI_LINES-1: the line drawn by the next drag
    int getPIEditLine() const;
    void clearPILines();
    int countPILines() const;

    //CPA / TCPA alarm: a tracked ship whose CPA and TCPA are both under the limits is dangerous
    void setCPALimit(irr::f32 cpaNm);
    void setTCPALimit(irr::f32 tcpaMinutes);
    irr::f32 getCPALimit() const;
    irr::f32 getTCPALimit() const;
    void setCPAAlarmOn(bool on);
    bool getCPAAlarmOn() const;
    bool isCPAAlarmSounding() const;   //a dangerous target not yet acknowledged (danger is shown even with the alarm off)
    void acknowledgeCPAAlarm();
    int countDangerousTargets() const;

    //Trial manoeuvre: own ship's course and speed tried out (after a delay), giving trial CPA/TCPA
    void setTrial(bool on, irr::f32 courseDeg, irr::f32 speedKts, irr::f32 delayMinutes);
    bool getTrialOn() const;
    irr::f32 getTrialCourse() const;
    irr::f32 getTrialSpeed() const;
    irr::f32 getTrialDelay() const;

    //Vector and trail lengths
    irr::f32 getVectorMinutes() const;
    void setTrailMinutes(irr::f32 minutes);
    irr::f32 getTrailMinutes() const;
    bool getShipTrails() const;

    //Man overboard mark (world metres, as offsetPosition + scene position)
    void setManOverboardMark(bool on, irr::f32 absX, irr::f32 absZ) { mobMark = on; mobX = absX; mobZ = absZ; }

    //Coastline (sea level contour of the terrain) drawn over the radar picture
    void setCoastline(bool on);
    bool getCoastline() const;

    void update(irr::video::IImage* radarImage, irr::video::IImage* radarImageOverlaid, irr::core::vector3d<int64_t> offsetPosition, const Terrain& terrain, const OwnShip& ownShip, const Buoys& buoys, const OtherShips& otherShips, irr::f32 weather, irr::f32 rain, irr::f32 tideHeight, irr::f32 deltaTime, uint64_t absoluteTime, irr::core::vector2di mouseRelPosition, bool isMouseDown);

private:
    irr::IrrlichtDevice* device;
    std::vector<std::vector<irr::f32> > scanArray;
    std::vector<std::vector<irr::f32> > scanArrayAmplified;
    std::vector<std::vector<irr::f32> > scanArrayToPlot;
    std::vector<std::vector<irr::f32> > scanArrayToPlotPrevious;
    std::vector<bool> toReplot;
    std::vector<ARPAContact> arpaContacts;
    std::vector<irr::u32> arpaTracks;
    std::vector<ARPAScan> ownShipScans; //History of own ship's absolute position, for drawing its own trail
    bool radarOn;
    //buoy arpa 
    bool showArpaOnBuoys; //Buoys shown as ARPA targets by default
    bool showBuoyTrails;
    bool showShipTrails;
    bool showOwnShipTrails; //Own ship's historical track (true motion only)
    bool showMMSI;
    irr::video::SColor buoyContactColour; //Colour for buoy markers/trails on the radar overlay
    irr::video::SColor shipContactColour; //Colour for ship markers on the radar overlay
    int arpaMode; // 0: Off/Manual, 1: MARPA, 2: ARPA
    irr::s32 arpaListSelection;
    irr::f32 radarGain;
    irr::f32 radarRainClutterReduction;
    irr::f32 radarSeaClutterReduction;
    irr::f32 currentScanAngle;
    irr::f32 scanAngleStep;
    irr::u32 currentScanLine; //Note that this MUST be an integer, as the scanline number is used to look up values in radar scan arrays
    irr::u32 rangeResolution;
    irr::u32 angularResolution;
    irr::f32 rangeSensitivity; //Used for ARPA contacts - in metres
    irr::u32 radarRangeIndex;
    irr::f32 radarScannerHeight;
    //parameters for noise behaviour
    irr::f32 radarNoiseLevel;
    irr::f32 radarSeaClutter;
    irr::f32 radarRainClutter;
    bool landTexture; //Patchy, graded land echoes (bc5.ini RADAR_LandTexture, 0 = old uniform land)
    //Parameters for parallel index
    std::vector<irr::f32> piBearings;
    std::vector<irr::f32> piRanges;
    //Parameters for EBL/VRM - two independent sets (kyara: EBL1/2 + VRM1/2)
    irr::f32 eblBrg[2];
    irr::f32 vrmRangeNm[2];
    irr::u32 activeEBL; //0 or 1: which EBL the +/- buttons control
    irr::u32 activeVRM; //0 or 1: which VRM the +/- buttons control
    int guardAlarmMode;          //0 off, 1 IN, 2 OUT
    irr::f32 guardAlarmTimer;    //seconds of alarm remaining
    bool guardZoneNeedsPrime;    //true after mode change: capture states without alarming
    void updateGuardZoneAlarm(irr::f32 deltaTime);
    clock_t radarCursorsLastUpdated;
    //Parameters for radar cursor
    irr::f32 cursorRangeXNm;
    irr::f32 cursorRangeYNm;
    irr::f32 CursorRangeNm;
    irr::f32 CursorBrg;
    //Radar config
    bool headUp;
    bool stabilised;
    int rangeRingBrightness;   //kyara: 0=clair, 1=faible, 2=off
    irr::u32 radarRadiusPx;
    bool radarScreenStale;
    bool trueVectors;
    irr::f32 vectorLengthMinutes;
    bool useRealHeading; //kyara
    int echoStretchLevel; //kyara: 0=off, 1=ES1, 2=ES2
    irr::f32 offsetXFraction, offsetYFraction; //kyara: décentrage
    //colours
    std::vector<irr::video::SColor> radarBackgroundColours;
    std::vector<irr::video::SColor> radarForegroundColours;
    std::vector<irr::video::SColor> radarSurroundColours;
    irr::u32 currentRadarColourChoice;

    std::vector<irr::f32> radarRangeNm;
    void scan(irr::core::vector3d<int64_t> offsetPosition, const Terrain& terrain, const OwnShip& ownShip, const Buoys& buoys, const OtherShips& otherShips, irr::f32 weather, irr::f32 rain, irr::f32 tideHeight, irr::f32 deltaTime, uint64_t absoluteTime);
    void updateARPA(irr::core::vector3d<int64_t> offsetPosition, const OwnShip& ownShip, uint64_t absoluteTime);
    void updateArpaEstimate(ARPAContact& thisArpaContact, int contactID, const OwnShip& ownShip, irr::core::vector3d<int64_t> absolutePosition, uint64_t absoluteTime);
    irr::f32 radarNoise(irr::f32 radarNoiseLevel, irr::f32 radarSeaClutter, irr::f32 radarRainClutter, irr::f32 weather, irr::f32 radarRange, irr::f32 radarBrgDeg, irr::f32 windDirectionDeg, irr::f32 radarInclinationAngle, irr::f32 rainIntensity);
    void render(irr::video::IImage* radarImage, irr::video::IImage* radarImageOverlaid, irr::f32 ownShipHeading, irr::f32 ownShipSpeed, irr::core::vector3d<int64_t> absolutePosition);
    irr::f32 rangeAtAngle(irr::f32 checkAngle, irr::f32 centreX, irr::f32 centreZ, irr::f32 heading);
    void drawSector(irr::video::IImage* radarImage, irr::f32 centreX, irr::f32 centreY, irr::f32 innerRadius, irr::f32 outerRadius, irr::f32 startAngle, irr::f32 endAngle, irr::u32 alpha, irr::u32 red, irr::u32 green, irr::u32 blue, irr::f32 ownShipHeading);
    void drawLine(irr::video::IImage* radarImage, irr::f32 startX, irr::f32 startY, irr::f32 endX, irr::f32 endY, irr::u32 alpha, irr::u32 red, irr::u32 green, irr::u32 blue);//Try with f32 as inputs so we can do interpolation based on the theoretical start and end
    void drawCircle(irr::video::IImage* radarImage, irr::f32 centreX, irr::f32 centreY, irr::f32 radius, irr::u32 alpha, irr::u32 red, irr::u32 green, irr::u32 blue);//Try with f32 as inputs so we can do interpolation based on the theoretical start and end
    void drawTriangle(irr::video::IImage* radarImage, irr::f32 centreX, irr::f32 centreY, irr::f32 radius, irr::u32 alpha, irr::u32 red, irr::u32 green, irr::u32 blue, irr::f32 headingDeg = 0.0f);//Triangle marker, used for ship contacts. headingDeg rotates it (screen bearing convention, 0=up); defaults to straight up.
    bool isPointInEllipse(irr::f32 pointX, irr::f32 pointZ, irr::f32 centreX, irr::f32 centreZ, irr::f32 width, irr::f32 length, irr::f32 angle);

    //Mouse on the scope
    struct ScopeMouseEvent { bool left; bool down; irr::core::vector2di rel; };
    std::vector<ScopeMouseEvent> scopeEvents;
    irr::f32 pressXNm = 0, pressYNm = 0;   //true east/north of the cursor where the left button went down
    bool pressPending = false;
    bool arpaModeChangedByClick = false;
    int pendingSelectContact = -1;         //contact clicked: selected in the list once it has a track number
    bool setCursorFromMouse(irr::core::vector2di mouseRelPosition, irr::f32 ownShipHeading); //false: outside the range ring
    void handleScopeEvents(irr::f32 ownShipHeading);
    int contactNearPoint(irr::f32 xNm, irr::f32 yNm, bool tracked) const; //nearest ship echo to a point (true east/north of own ship)
    int piEditLine = -1;

    //CPA alarm and trial manoeuvre
    irr::f32 cpaLimitNm = 0.5f;
    irr::f32 tcpaLimitMinutes = 12.0f;
    bool cpaAlarmOn = true;
    bool trialOn = false;
    irr::f32 trialCourseDeg = 0, trialSpeedKts = 0, trialDelayMinutes = 0;
    irr::f32 trailMinutes = 6.0f;

    //Coastline: segments (absolute metres x1,z1,x2,z2), worked out around own ship when needed
    bool showCoastline = false;
    bool mobMark = false;
    irr::f32 mobX = 0, mobZ = 0;
    std::vector<irr::f32> coastSegments;
    irr::f32 coastCentreX = 0, coastCentreZ = 0, coastRangeNm = 0, coastTide = -1000;
    uint64_t coastTime = 0;
    void updateCoastline(irr::core::vector3d<int64_t> offsetPosition, const Terrain& terrain, const OwnShip& ownShip, irr::f32 tideHeight, uint64_t absoluteTime);
    uint64_t lastAbsoluteTime = 0;
    irr::f32 ownCogDeg = 0, ownSogMps = 0;  //own ship's last course and speed over ground (trial manoeuvre)
    irr::f32 pendingAcquireXNm = 0, pendingAcquireYNm = 0;
    int pendingAcquireTries = 0;           //ARPA was off when clicked: tried again once MARPA has estimates

};

#endif