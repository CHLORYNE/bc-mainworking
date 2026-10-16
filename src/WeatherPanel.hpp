/*   NAUTITECH - Simulateur de Navigation
     The weather window (METEO): presets, conditions, evolution and time of day.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#ifndef __WEATHERPANEL_HPP_INCLUDED__
#define __WEATHERPANEL_HPP_INCLUDED__

#include "irrlicht.h"
#include "PanelDock.hpp"
#include "StationUi.hpp"
#include <string>
#include <vector>

class SimulationModel;

//An instructor station page with six pages:
//  PRESETS            a table of whole weathers (clear, cloudy, rain, fog, snow, thunderstorm, sand
//                     storm...): pick one, then apply it at once or over a few minutes
//  WIND & SEA         cloud, wind, gusts, sea state
//  VISIBILITY         fog, rain, snow, sand, thunderstorms and squalls
//  CURRENT            tide of the area, or a current set by hand
//  EVOLUTION          how presets change the weather, and a weather front passing through
//  TIME               the hour of the lighting
//On the right, the weather as it is now (wind rose and readings) and the explanation of the row
//under the mouse. The page reads the simulation every frame, so it always shows the weather as it
//is, whoever changed it (scenario, a front under way, the instructor in multiplayer).
class WeatherPanel : public irr::gui::IGUIElement
{
public:
    WeatherPanel(irr::gui::IGUIEnvironment* environment, irr::gui::IGUIElement* parent, SimulationModel* model,
        const irr::core::rect<irr::s32>& area);
    ~WeatherPanel();

    //The hour box of the command bar, kept in step when the hour is changed here
    void setLightingTimeBox(irr::gui::IGUIEditBox* box) { timeBox = box; }

    virtual void draw();
    virtual bool OnEvent(const irr::SEvent& event);
    virtual void updateAbsolutePosition();

    //The dock key and the drag by the title bar (GUIMain says what a place means)
    bridge::PanelDock dock;
    //On the instructor screen: no dock or close key (the window stays there), pages on top
    void setEmbedded(bool on);
    bool isEmbedded() const { return embedded; }

private:
    enum Tab { TAB_PRESETS = 0, TAB_WIND, TAB_VISIBILITY, TAB_CURRENT, TAB_EVOLUTION, TAB_TIME, TAB_COUNT };
    enum Param {
        P_NONE = 0, P_CLOUD, P_WIND, P_WINDDIR, P_WINDVAR, P_GUST, P_SEA, P_FOG, P_RAIN, P_SNOW, P_DUST,
        P_SIGWX, P_TRANSITION, P_FRONT, P_HOUR, P_MOMENT, P_THUNDER, P_LIGHTNING, P_STREAM_MODE, P_STREAM_DIR, P_STREAM_SPEED
    };
    enum RowKind { ROW_SECTION, ROW_SLIDER, ROW_CHOICE };
    struct Row
    {
        RowKind kind;
        Param param;
        std::wstring label;      //in the list
        std::wstring title;      //at the top of the explanation
        std::wstring help;       //under it
        irr::f32 minValue, maxValue, step;
        std::vector<std::wstring> choices;
    };
    struct Preset
    {
        std::wstring name, help;
        irr::f32 cloud, windKn, windVariation, gustKn, visibilityNm, rain, snow, dust, sea;
        int significant;
    };
    struct Cells
    {
        station::Rect label, state, control;
    };
    //Keys under the preset table: the transition times, then APPLY
    enum { KEY_APPLY = 100 };

    //Layout (absolute pixels), redone when the size changes
    void layout();
    void buildRows();
    void buildPresets();
    station::Rect content() const;

    //Values, read from and written to the simulation
    irr::f32 value(Param p) const;
    void setValue(Param p, irr::f32 v);
    int choiceIndex(Param p) const;
    void setChoice(Param p, int index);
    std::wstring valueText(const Row& row) const;
    std::wstring stateText(const Row& row) const;
    void applyPreset(int index);
    irr::f32 transitionSeconds() const;
    void recordAction(const std::wstring& what, const std::wstring& value);

    //Rows of the open tab, and where each one is on screen (scrolled)
    const std::vector<Row>& rowsOfTab() const;
    irr::f32 rowHeight(const Row& row) const;
    station::Rect rowRect(int index) const;
    Cells cells(const station::Rect& r) const;
    bool usesSegments(const Row& row, const Cells& c) const;
    std::vector<station::Rect> segmentRects(const Row& row, const station::Rect& r) const;
    station::Rect stepperRect(const Cells& c) const;
    station::Rect sliderTrack(const Cells& c) const;
    int rowAt(const irr::core::vector2df& p) const;
    int segmentAt(int row, const irr::core::vector2df& p) const;
    void dragSlider(int rowIndex, irr::s32 mouseX);

    //The preset page: a table, the transition keys and the apply key
    bool narrowTable() const;
    station::Rect presetRowRect(int index) const;
    station::Rect tableHeaderRect() const;
    std::vector<station::Rect> transitionRects() const;
    station::Rect applyRect() const;
    std::wstring applyCaption() const;
    int presetAt(const irr::core::vector2df& p) const;
    int presetKeyAt(const irr::core::vector2df& p) const;

    irr::f32 contentHeight() const;
    void clampScroll();

    //Drawing
    void drawRows(irr::video::IVideoDriver* driver, const irr::core::rect<irr::s32>& clip);
    void drawPresets(irr::video::IVideoDriver* driver, const irr::core::rect<irr::s32>& clip);
    void drawSide(irr::video::IVideoDriver* driver, const irr::core::rect<irr::s32>& clip);
    irr::f32 conditionsHeight(irr::f32 width) const;
    void drawConditions(irr::video::IVideoDriver* driver, const station::Rect& box, const irr::core::rect<irr::s32>& clip);
    void drawRose(irr::video::IVideoDriver* driver, const station::Rect& r, irr::f32 fromDegrees, const irr::core::rect<irr::s32>& clip);

    SimulationModel* model;
    irr::gui::IGUIEditBox* timeBox;
    std::vector<Row> rows[TAB_COUNT];
    std::vector<Preset> presets;
    station::Frame frame;
    int tab;
    int hoveredRow, selectedRow, draggedRow, hoveredSeg;
    int hoveredPreset, selectedPreset, chosenPreset, hoveredKey;
    int transitionIndex;
    irr::f32 scroll[TAB_COUNT];
    std::wstring lastAction;
    irr::core::rect<irr::s32> laidOutRect;
    bool embedded = false;
};

#endif
