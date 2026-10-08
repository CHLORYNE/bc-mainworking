/*   NAUTITECH - Simulateur de Navigation
     The instructor's controls window (CONTROLES), drawn like the weather window.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#ifndef __CONTROLSPANEL_HPP_INCLUDED__
#define __CONTROLSPANEL_HPP_INCLUDED__

#include "irrlicht.h"
#include <functional>
#include <string>
#include <vector>

class HudFont;

//Tabs of rows (sliders, switches, keys, choices, read-only text) with an explanation card on the
//right, in the look of the weather window. Each row stands for one of the ordinary Irrlicht
//widgets the window used to show: those stay alive but hidden, they keep their ids, GUIMain keeps
//writing the simulation's values into them, and a click here sets them and sends the very event
//they would have sent. So MyEventReceiver and the rest of GUIMain work exactly as before.
class ControlsPanel : public irr::gui::IGUIElement
{
public:
    struct Readout
    {
        std::wstring label, text;
        int tone;                //0 normal, 1 good, 2 bad
    };
    typedef std::function<std::vector<Readout>()> StatusFn;

    ControlsPanel(irr::gui::IGUIEnvironment* environment, irr::gui::IGUIElement* parent, const irr::core::rect<irr::s32>& area);
    ~ControlsPanel();

    //Title and icon (bridge::Icon) of the window
    void setTitle(const std::wstring& text, int icon) { title = text; titleIcon = icon; }

    //Building: a tab, then its rows in order. Help may be left empty: the widget's tooltip is used.
    int addTab(const std::wstring& name, const std::wstring& help, StatusFn status = StatusFn());
    void addSection(int tab, const std::wstring& label);
    void addSlider(int tab, const std::wstring& label, const std::wstring& help, irr::gui::IGUIScrollBar* bar,
        irr::gui::IGUIStaticText* value);
    void addToggle(int tab, const std::wstring& label, const std::wstring& help, irr::gui::IGUICheckBox* box);
    //A row of keys. With 'selected', the key it returns is shown pressed (a choice of states);
    //without it they are plain actions. An empty label gives the keys the whole width.
    void addKeys(int tab, const std::wstring& label, const std::wstring& help, const std::vector<irr::gui::IGUIButton*>& keys,
        const std::vector<std::wstring>& names, std::function<int()> selected = std::function<int()>());
    void addChoice(int tab, const std::wstring& label, const std::wstring& help, irr::gui::IGUIComboBox* combo);
    void addText(int tab, irr::gui::IGUIStaticText* text, int lines, std::function<bool()> alert = std::function<bool()>());

    virtual void draw();
    virtual bool OnEvent(const irr::SEvent& event);
    virtual void updateAbsolutePosition();

private:
    enum RowKind { ROW_SECTION, ROW_SLIDER, ROW_TOGGLE, ROW_KEYS, ROW_CHOICE, ROW_TEXT };
    struct Row
    {
        RowKind kind;
        std::wstring label, help;
        irr::gui::IGUIScrollBar* bar;
        irr::gui::IGUIStaticText* text;      //slider value, or the text of a text row
        irr::gui::IGUICheckBox* box;
        irr::gui::IGUIComboBox* combo;
        std::vector<irr::gui::IGUIButton*> keys;
        std::vector<std::wstring> names;
        std::function<int()> selected;
        std::function<bool()> alert;
        int lines;
        Row() : kind(ROW_SECTION), bar(0), text(0), box(0), combo(0), lines(1) {}
    };
    struct Tab
    {
        std::wstring name, help;
        StatusFn status;
        std::vector<Row> rows;
        irr::f32 scroll;
        Tab() : scroll(0) {}
    };

    void layout();
    void loadFonts();
    void freeFonts();
    irr::core::rect<irr::f32> tabRect(int t) const;
    irr::core::rect<irr::f32> closeRect() const;
    irr::f32 rowHeight(const Row& row) const;
    irr::core::rect<irr::f32> rowRect(int index) const;
    irr::f32 split(const irr::core::rect<irr::f32>& row) const;
    irr::core::rect<irr::f32> valueCell(const Row& row, const irr::core::rect<irr::f32>& r) const;
    irr::core::rect<irr::f32> sliderTrack(const irr::core::rect<irr::f32>& cell) const;
    irr::core::rect<irr::f32> keyRect(const Row& row, const irr::core::rect<irr::f32>& cell, int index) const;
    irr::f32 contentHeight() const;
    void clampScroll();
    int rowAt(const irr::core::vector2df& p) const;
    int keyAt(int row, const irr::core::vector2df& p) const;
    void dragSlider(int row, irr::s32 mouseX);

    //Sends the event the hidden widget would have sent
    void send(irr::gui::IGUIElement* caller, irr::gui::EGUI_EVENT_TYPE type);

    void drawRows(irr::video::IVideoDriver* driver, const irr::core::rect<irr::s32>& clip);
    void drawInfo(irr::video::IVideoDriver* driver);

    std::wstring title;
    int titleIcon;
    std::vector<Tab> tabs;
    int tab;
    int hoveredRow, selectedRow, draggedRow, hoveredKey, pressedRow, pressedKey;
    int hoveredTab;
    bool hoveredClose;
    irr::f32 k;
    irr::core::rect<irr::f32> header, tabsBar, list, info;
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
