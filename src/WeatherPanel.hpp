/*   NAUTITECH - Simulateur de Navigation
     The weather window (METEO): presets, conditions, evolution and time of day.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#ifndef __WEATHERPANEL_HPP_INCLUDED__
#define __WEATHERPANEL_HPP_INCLUDED__

#include "irrlicht.h"
#include <string>
#include <vector>

class SimulationModel;
class HudFont;

//One window, four tabs:
//  PRESETS     tiles (clear, cloudy, rain, fog, snow, thunderstorm, sand storm...) - one click sets
//              all the weather, at once or over a few minutes (EVOLUTION tab)
//  CONDITIONS  every value on its own slider: cloud, wind, gusts, sea, fog, rain, snow, sand,
//              significant weather
//  EVOLUTION   how presets change the weather, and a weather front passing through
//  TIME        the hour of the lighting
//The panel reads the simulation every frame, so it always shows the weather as it is, whoever
//changed it (scenario, older sliders, a front under way). Colours follow the bridge palette.
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

private:
    enum Tab { TAB_PRESETS = 0, TAB_CONDITIONS, TAB_EVOLUTION, TAB_TIME, TAB_COUNT };
    enum Param {
        P_NONE = 0, P_CLOUD, P_WIND, P_WINDDIR, P_WINDVAR, P_GUST, P_SEA, P_FOG, P_RAIN, P_SNOW, P_DUST,
        P_SIGWX, P_TRANSITION, P_FRONT, P_HOUR, P_MOMENT
    };
    enum RowKind { ROW_SECTION, ROW_SLIDER, ROW_CHOICE };
    struct Row
    {
        RowKind kind;
        Param param;
        std::wstring label;      //in the list
        std::wstring title;      //at the top of the info card
        std::wstring help;       //under it
        irr::f32 minValue, maxValue, step;
        std::vector<std::wstring> choices;
        int icon;                //WxIcon shown in the info tile
    };
    struct Preset
    {
        std::wstring name, help;
        int icon;
        irr::f32 cloud, windKn, windVariation, gustKn, visibilityNm, rain, snow, dust, sea;
        int significant;
    };

    //Layout (absolute pixels), redone when the size changes
    void layout();
    void buildRows();
    void buildPresets();
    irr::core::rect<irr::f32> tabRect(int tab) const;
    irr::core::rect<irr::f32> closeRect() const;

    //Values, read from and written to the simulation
    irr::f32 value(Param p) const;
    void setValue(Param p, irr::f32 v);
    int choiceIndex(Param p) const;
    void setChoice(Param p, int index);
    std::wstring valueText(const Row& row) const;
    void applyPreset(int index);
    irr::f32 transitionSeconds() const;

    //Drawing
    void drawRows(irr::video::IVideoDriver* driver, const irr::core::rect<irr::s32>& clip);
    void drawPresets(irr::video::IVideoDriver* driver, const irr::core::rect<irr::s32>& clip);
    void drawInfo(irr::video::IVideoDriver* driver);
    void drawWeatherIcon(irr::video::IVideoDriver* driver, int icon, const irr::core::rect<irr::f32>& box,
        irr::video::SColor ink, irr::video::SColor paper);
    int currentWeatherIcon() const;

    //Rows of the open tab, and where each one is on screen (scrolled)
    const std::vector<Row>& rowsOfTab() const;
    irr::f32 contentHeight() const;
    void clampScroll();
    int rowAt(const irr::core::position2di& p) const;
    int presetAt(const irr::core::position2di& p) const;
    irr::core::rect<irr::f32> rowRect(int index) const;
    irr::core::rect<irr::f32> sliderTrack(const irr::core::rect<irr::f32>& row) const;
    irr::core::rect<irr::f32> presetRect(int index) const;
    void dragSlider(int rowIndex, irr::s32 mouseX);

    void loadFonts();
    void freeFonts();

    SimulationModel* model;
    irr::gui::IGUIEditBox* timeBox;
    std::vector<Row> rows[TAB_COUNT];
    std::vector<Preset> presets;
    int tab;
    int hoveredRow, selectedRow, draggedRow;
    int hoveredPreset, chosenPreset;
    int hoveredTab;
    bool hoveredClose;
    int transitionIndex;
    irr::f32 scroll[TAB_COUNT];
    irr::f32 k;                          //scale from the panel's size
    irr::core::rect<irr::f32> header, tabsBar, list, info, tile;
    irr::core::dimension2du laidOut;
    irr::core::rect<irr::s32> laidOutRect;

    struct Fonts
    {
        HudFont* title = 0;
        HudFont* tab = 0;
        HudFont* label = 0;
        HudFont* value = 0;
        HudFont* section = 0;
        HudFont* infoTitle = 0;
        HudFont* infoText = 0;
        HudFont* caption = 0;
    } fonts;
};

#endif
