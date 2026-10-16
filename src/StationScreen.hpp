/*   NAUTITECH - Simulateur de Navigation
     The instructor screen: CONTROLES and METEO together in a window of their own, for a second
     monitor (portrait or landscape).

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#ifndef __STATIONSCREEN_HPP_INCLUDED__
#define __STATIONSCREEN_HPP_INCLUDED__

#include "irrlicht.h"
#include "StationUi.hpp"

//A parentless element drawn into the instructor screen's window. Its children are the controls
//window and the weather window, laid out one over the other on a portrait monitor and side by
//side on a landscape one. Either of them can be borrowed by the bridge screen (its key in the
//command bar): its place here then says so, with a key to bring it back.
class StationScreen : public irr::gui::IGUIElement
{
public:
    enum Layout { LAYOUT_AUTO = 0, LAYOUT_PORTRAIT, LAYOUT_LANDSCAPE };
    enum { SLOT_CONTROLS = 0, SLOT_WEATHER = 1, SLOT_COUNT = 2 };

    StationScreen(irr::gui::IGUIEnvironment* env, irr::IrrlichtDevice* device, const irr::core::rect<irr::s32>& area);

    //The window that belongs in each place (0 if there is none, e.g. no weather tool)
    void setPanel(int slot, irr::gui::IGUIElement* panel);
    irr::gui::IGUIElement* panel(int slot) const { return panels[slot]; }
    //True when the window is here (not borrowed by the bridge screen)
    bool isHome(int slot) const;
    //Portrait: one over the other; landscape: side by side; auto: from the window's shape
    void setLayout(int layout);
    //Lays the places out for a window of this size and puts the windows that are here in them
    void layoutFor(const irr::core::dimension2du& size);
    //A place's key "bring it back" under the point, or -1
    int recallAt(const irr::core::position2di& p) const;
    //The window under the point, if it is here
    irr::gui::IGUIElement* panelAt(const irr::core::position2di& p) const;
    void setHoveredRecall(int slot) { hoveredRecall = slot; }

    virtual void draw();
    //GUI events of the windows (their hidden widgets) go to the application, as on the bridge screen
    virtual bool OnEvent(const irr::SEvent& event);

private:
    irr::core::rect<irr::f32> recallRect(int slot) const;

    irr::IrrlichtDevice* device;
    irr::gui::IGUIElement* panels[SLOT_COUNT];
    irr::core::rect<irr::s32> slots[SLOT_COUNT];
    int layoutMode;
    int hoveredRecall;
    irr::core::dimension2du laidOut;
    station::Fonts fonts;
    irr::f32 k;
};

#endif
