/*   NAUTITECH - Simulateur de Navigation
     The instructor's controls window (CONTROLES), drawn as an instructor station page.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#ifndef __CONTROLSPANEL_HPP_INCLUDED__
#define __CONTROLSPANEL_HPP_INCLUDED__

#include "irrlicht.h"
#include "PanelDock.hpp"
#include "StationUi.hpp"
#include <functional>
#include <string>
#include <vector>

//Numbered pages (tabs) of rows - segmented keys, faders, two-way switches, choices, read-only
//text - with the state of each row beside it, and on the right the state of the page and the
//explanation of the row under the mouse. Each row stands for one of the ordinary Irrlicht widgets
//the window used to show: those stay alive but hidden, they keep their ids, GUIMain keeps writing
//the simulation's values into them, and a click here sets them and sends the very event they
//would have sent. So MyEventReceiver and the rest of GUIMain work exactly as before.
class ControlsPanel : public irr::gui::IGUIElement
{
public:
    struct Readout
    {
        std::wstring label, text;
        int tone;                //0 normal, 1 good, 2 fault, 3 warning
    };
    typedef std::function<std::vector<Readout>()> StatusFn;

    ControlsPanel(irr::gui::IGUIEnvironment* environment, irr::gui::IGUIElement* parent, const irr::core::rect<irr::s32>& area);
    ~ControlsPanel();

    //Header: the station's code ("CTL 01") and the name of the window
    void setTitle(const std::wstring& code, const std::wstring& name);
    //States shown in the header strip (simulation running, exercise time...)
    void setHeaderStatus(StatusFn status) { headerStatus = status; }
    //The time put after the last action in the footer ("T+ 00:12:30")
    void setClock(std::function<std::wstring()> clock) { clockText = clock; }

    //Building: a tab, then its rows in order. Help may be left empty: the widget's tooltip is used.
    int addTab(const std::wstring& name, const std::wstring& help, StatusFn status = StatusFn());
    //The heading over a tab in the column of pages (the tabs after it share it)
    void setGroup(int tab, const std::wstring& group);
    void addSection(int tab, const std::wstring& label);
    void addSlider(int tab, const std::wstring& label, const std::wstring& help, irr::gui::IGUIScrollBar* bar,
        irr::gui::IGUIStaticText* value);
    void addToggle(int tab, const std::wstring& label, const std::wstring& help, irr::gui::IGUICheckBox* box);
    //A row of keys. With 'selected', the key it returns is shown engaged (a choice of states);
    //without it they are plain actions. An empty label gives the keys the whole width.
    void addKeys(int tab, const std::wstring& label, const std::wstring& help, const std::vector<irr::gui::IGUIButton*>& keys,
        const std::vector<std::wstring>& names, std::function<int()> selected = std::function<int()>());
    void addChoice(int tab, const std::wstring& label, const std::wstring& help, irr::gui::IGUIComboBox* combo);
    void addText(int tab, irr::gui::IGUIStaticText* text, int lines, std::function<bool()> alert = std::function<bool()>());

    virtual void draw();
    virtual bool OnEvent(const irr::SEvent& event);
    virtual void updateAbsolutePosition();

    //The dock key and the drag by the title bar (GUIMain says what a place means)
    bridge::PanelDock dock;

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
        std::wstring name, help, group;
        StatusFn status;
        std::vector<Row> rows;
        irr::f32 scroll;
        Tab() : scroll(0) {}
    };
    struct Cells
    {
        station::Rect label, state, control;
    };

    void layout();
    station::Rect content() const;
    irr::f32 rowHeight(const Row& row) const;
    station::Rect rowRect(int index) const;
    Cells cells(const Row& row, const station::Rect& r) const;
    //The keys of a row (keys, the two sides of a switch, the options of a short choice)
    std::vector<std::wstring> segmentTexts(const Row& row) const;
    bool usesSegments(const Row& row, const Cells& c) const;
    std::vector<station::Rect> segmentRects(const Row& row, const station::Rect& r) const;
    station::Rect stepperRect(const Cells& c) const;
    station::Rect sliderTrack(const Cells& c) const;
    irr::f32 contentHeight() const;
    void clampScroll();
    int rowAt(const irr::core::vector2df& p) const;
    int segmentAt(int row, const irr::core::vector2df& p) const;
    void dragSlider(int row, irr::s32 mouseX);
    void recordAction(const Row& row, const std::wstring& value);
    std::wstring stateText(const Row& row, int& tone) const;

    //Sends the event the hidden widget would have sent
    void send(irr::gui::IGUIElement* caller, irr::gui::EGUI_EVENT_TYPE type);

    void drawRows(irr::video::IVideoDriver* driver, const irr::core::rect<irr::s32>& clip);
    void drawSide(irr::video::IVideoDriver* driver, const std::vector<Readout>& status, const irr::core::rect<irr::s32>& clip);

    station::Frame frame;
    std::vector<Tab> tabs;
    int tab;
    int hoveredRow, selectedRow, draggedRow, hoveredSeg, pressedRow, pressedSeg;
    std::wstring lastAction;
    StatusFn headerStatus;
    std::function<std::wstring()> clockText;
    irr::core::rect<irr::s32> laidOutRect;
};

#endif
