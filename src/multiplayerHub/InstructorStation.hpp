/*   NAUTITECH - Simulateur de Navigation
     The multiplayer hub's live screen: the instructor's master control. The instructor is not on a
     ship: from here they run the clock, watch every student's ship on a live chart with its state
     (alarms not acknowledged, failures, collisions, groundings, depth), impose the weather on all
     the students, give failures to one student or to all (now or later), and send messages.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#ifndef __INSTRUCTORSTATION_HPP_INCLUDED__
#define __INSTRUCTORSTATION_HPP_INCLUDED__

#include "irrlicht.h"
#include "../GUIPanelDraw.hpp"
#include <string>
#include <vector>
#include <deque>

class ChartView;

class InstructorStation : public irr::gui::IGUIElement
{
public:
    //One student's station, as last reported
    struct Station {
        std::wstring host, ship;
        irr::f32 x = 0, z = 0;              //world metres
        irr::f32 speedKts = 0, heading = 0;
        bool reported = false;              //has sent its position
        bool hasStatus = false;             //sends its state (a simulator that knows the instructor station)
        int failure[5] = { 0, 0, 0, 0, 0 }; //port engine, starboard engine, gyro, GPS, radar: 0 ok, 1 degraded, 2 failed
        bool pump1 = true, pump2 = true, followUp = true;
        int alerts = 0, unacked = 0;
        irr::f32 oldestUnacked = 0;         //seconds
        int collisions = 0, groundings = 0, contacts = 0;
        irr::f32 depth = 0;
        bool depthAlarm = false;
        irr::f32 gyroError = 0, gpsError = 0;
        int scheduled = 0;
        bool weatherLocked = false;
    };
    //A command for the students' simulators: station -1 = every station
    struct Command { int station; std::string text; };
    //The scenario's own weather, for the "Scenario" preset
    struct ScenarioWeather { irr::f32 sea = 0, rain = 0, visibilityNm = 10, windDir = 0, windKn = 0; };

    InstructorStation(irr::IrrlichtDevice* device, bool french, const std::wstring& exercise, irr::u32 stationCount,
        const std::string& worldName, const ScenarioWeather& scenarioWeather,
        irr::gui::IGUIFont* bigFont, irr::gui::IGUIFont* titleFont, irr::gui::IGUIFont* textFont, irr::gui::IGUIFont* smallFont);
    ~InstructorStation();

    void setUnreached(const std::vector<std::wstring>& names) { unreached = names; }
    irr::core::rect<irr::s32> runRect() const;
    irr::core::rect<irr::s32> pauseRect() const;

    void update(const std::wstring& clockText, const std::wstring& dateText, irr::f32 accelerator, irr::f32 scenarioTime,
        const std::vector<Station>& stations, irr::u32 lines);
    //What the instructor asked for since the last call
    std::vector<Command> takeCommands();

    virtual void draw();
    virtual bool OnEvent(const irr::SEvent& event);

private:
    //Clickable zones of the last frame
    enum ZoneKind { Z_PRESET, Z_WINDDIR, Z_TRANSITION, Z_HOUR, Z_SEND_WEATHER, Z_RELEASE_WEATHER, Z_STATION, Z_DELAY, Z_FAILURE,
        Z_REPAIR, Z_MOB, Z_SEND_MESSAGE, Z_CHART_FIT, Z_CHART_STYLE };
    struct Zone { irr::core::rect<irr::f32> r; ZoneKind kind; int value; };
    struct Label { irr::gui::IGUIFont* font; std::wstring text; irr::core::rect<irr::f32> r; irr::video::SColor col; int align; bool clip; };
    struct Preset { std::wstring name; irr::f32 cloud, windKn, windVariation, gustKn, visibilityNm, rain, snow, dust, sea; int significant; };

    void layout();
    void chip(irr::gui::PanelBatch& b, const irr::core::rect<irr::f32>& r, const std::wstring& text, bool selected, ZoneKind kind, int value,
        int tone = 0);
    void label(irr::gui::IGUIFont* font, const std::wstring& text, const irr::core::rect<irr::f32>& r, irr::video::SColor col, int align = 0, bool clip = false);
    void drawChart(irr::gui::PanelBatch& b);
    void drawWeather(irr::gui::PanelBatch& b);
    void drawStations(irr::gui::PanelBatch& b);
    void drawFailures(irr::gui::PanelBatch& b);
    void fitChartToShips();
    void click(const Zone& z);
    void sendToTargets(const std::string& text);
    void sendMessage();
    int stationAt(irr::core::position2di p) const;
    std::wstring targetName() const;
    irr::video::SColor stationColour(size_t i) const;

    irr::IrrlichtDevice* device;
    bool french;
    std::wstring exercise;
    irr::u32 stationCount;
    irr::gui::IGUIFont* bigFont;
    irr::gui::IGUIFont* titleFont;
    irr::gui::IGUIFont* textFont;
    irr::gui::IGUIFont* smallFont;
    ScenarioWeather scenarioWeather;
    std::vector<Preset> presets;

    //Layout
    irr::core::rect<irr::f32> clockCard, weatherCard, stationCard, failureCard, chartCard, chartArea, run, pause;
    irr::gui::IGUIEditBox* messageBox;

    //Live state
    std::wstring clock, date;
    bool running;
    irr::f32 accelerator;
    irr::f32 scenarioTime;
    std::vector<Station> stations;
    std::vector<std::deque<irr::core::vector2df> > tracks;  //world x, z every few seconds
    std::vector<irr::f32> lastTrackTime;
    std::vector<std::wstring> unreached;
    irr::u32 lines;

    //Chart
    ChartView* chart;
    bool chartLoaded;
    int chartFittedShips;    //how many ships the chart was framed on
    bool chartTouched;       //the instructor moved or zoomed it: no more automatic framing
    bool dragging;
    bool dragMoved;
    irr::core::position2di dragStart, dragLast;

    //Instructor choices
    int selected;            //-1: every station
    int weatherPreset;       //-1: the scenario's
    int windDirIndex;        //-1: the scenario's; 0..7: N, NE...
    int transitionIndex;
    int hourIndex;           //0: unchanged
    int delayIndex;
    std::wstring weatherSent; //what was last sent, for the status line

    std::vector<Zone> zones;
    std::vector<Label> labels;
    irr::core::position2di mouse;
    std::vector<Command> commands;
};

#endif
