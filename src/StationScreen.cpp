/*   NAUTITECH - Simulateur de Navigation
     The instructor screen. See StationScreen.hpp.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#include "StationScreen.hpp"
#include "launcher/HudText.hpp"

#include <algorithm>
#include <cmath>

using station::Rect;

StationScreen::StationScreen(irr::gui::IGUIEnvironment* env, irr::IrrlichtDevice* dev, const irr::core::rect<irr::s32>& area)
    : irr::gui::IGUIElement(irr::gui::EGUIET_ELEMENT, env, 0, -1, area), device(dev), layoutMode(LAYOUT_AUTO), hoveredRecall(-1), k(1.0f)
{
    panels[SLOT_CONTROLS] = panels[SLOT_WEATHER] = 0;
}

void StationScreen::setPanel(int slot, irr::gui::IGUIElement* p)
{
    if (slot >= 0 && slot < SLOT_COUNT) { panels[slot] = p; }
    laidOut = irr::core::dimension2du();
}

bool StationScreen::isHome(int slot) const
{
    return slot >= 0 && slot < SLOT_COUNT && panels[slot] && panels[slot]->getParent() == this;
}

void StationScreen::setLayout(int layout)
{
    layoutMode = irr::core::clamp(layout, (int)LAYOUT_AUTO, (int)LAYOUT_LANDSCAPE);
    laidOut = irr::core::dimension2du();
}

void StationScreen::layoutFor(const irr::core::dimension2du& size)
{
    const irr::s32 W = (irr::s32)size.Width, H = (irr::s32)size.Height;
    setRelativePosition(irr::core::rect<irr::s32>(0, 0, W, H));
    const irr::f32 newK = irr::core::clamp(std::min(W, H) / 1000.0f, 0.85f, 2.0f);
    if (fabsf(newK - k) > 0.01f || !fonts.title) {
        k = newK;
        fonts.load(Environment, k);
    }
    const bool portrait = (layoutMode == LAYOUT_PORTRAIT) || (layoutMode == LAYOUT_AUTO && H > W);
    const irr::s32 gap = (irr::s32)(6 * k);
    if (!panels[SLOT_WEATHER] || !panels[SLOT_CONTROLS]) {
        //Only one window to show: all the room is its own
        slots[SLOT_CONTROLS] = slots[SLOT_WEATHER] = irr::core::rect<irr::s32>(0, 0, W, H);
    }
    else if (portrait) {
        //The controls have more rows: a little more of the height
        const irr::s32 split = (irr::s32)(H * 0.53f);
        slots[SLOT_CONTROLS] = irr::core::rect<irr::s32>(0, 0, W, split - gap / 2);
        slots[SLOT_WEATHER] = irr::core::rect<irr::s32>(0, split + (gap - gap / 2), W, H);
    }
    else {
        const irr::s32 split = W / 2;
        slots[SLOT_CONTROLS] = irr::core::rect<irr::s32>(0, 0, split - gap / 2, H);
        slots[SLOT_WEATHER] = irr::core::rect<irr::s32>(split + (gap - gap / 2), 0, W, H);
    }
    for (int i = 0; i < SLOT_COUNT; i++) {
        if (isHome(i)) { panels[i]->setRelativePosition(slots[i]); }
    }
    laidOut = size;
}

Rect StationScreen::recallRect(int slot) const
{
    const irr::core::rect<irr::s32>& s = slots[slot];
    const irr::f32 w = std::min(320 * k, s.getWidth() * 0.8f), h = 34 * k;
    const irr::f32 cx = (irr::f32)s.getCenter().X, cy = (irr::f32)s.getCenter().Y + 46 * k;
    return Rect(floorf(cx - w * 0.5f), floorf(cy - h * 0.5f), floorf(cx + w * 0.5f), floorf(cy + h * 0.5f));
}

int StationScreen::recallAt(const irr::core::position2di& p) const
{
    const irr::core::vector2df pf((irr::f32)p.X, (irr::f32)p.Y);
    for (int i = 0; i < SLOT_COUNT; i++) {
        if (panels[i] && !isHome(i) && recallRect(i).isPointInside(pf)) { return i; }
    }
    return -1;
}

irr::gui::IGUIElement* StationScreen::panelAt(const irr::core::position2di& p) const
{
    for (int i = 0; i < SLOT_COUNT; i++) {
        if (isHome(i) && panels[i]->isVisible() && panels[i]->getAbsolutePosition().isPointInside(p)) { return panels[i]; }
    }
    return 0;
}

void StationScreen::draw()
{
    if (!IsVisible) { return; }
    irr::video::IVideoDriver* driver = Environment->getVideoDriver();
    const station::Colours& c = station::colours();
    const irr::core::rect<irr::s32> all = AbsoluteRect;
    station::fill(driver, Rect((irr::f32)all.UpperLeftCorner.X, (irr::f32)all.UpperLeftCorner.Y, (irr::f32)all.LowerRightCorner.X, (irr::f32)all.LowerRightCorner.Y),
        c.raised, &all);

    //A window lent to the bridge screen: its place says so
    for (int i = 0; i < SLOT_COUNT; i++) {
        if (!panels[i] || isHome(i) || !fonts.title) { continue; }
        const irr::core::rect<irr::s32>& s = slots[i];
        const Rect box((irr::f32)s.UpperLeftCorner.X, (irr::f32)s.UpperLeftCorner.Y, (irr::f32)s.LowerRightCorner.X, (irr::f32)s.LowerRightCorner.Y);
        station::fill(driver, box, c.bg, &all);
        station::frame(driver, box, c.edge, &all);
        const irr::f32 cy = box.getCenter().Y;
        fonts.title->drawIn(i == SLOT_CONTROLS ? L"CONTR\u00D4LES" : L"M\u00C9T\u00C9O & ENVIRONNEMENT",
            Rect(box.UpperLeftCorner.X, cy - 60 * k, box.LowerRightCorner.X, cy - 36 * k), c.text, HudFont::Centre, 2.0f * k, &all);
        fonts.text->drawIn(L"Affich\u00E9 sur l'\u00E9cran de la passerelle",
            Rect(box.UpperLeftCorner.X, cy - 26 * k, box.LowerRightCorner.X, cy - 4 * k), c.dim, HudFont::Centre, 0, &all);
        station::drawKey(driver, fonts, k, recallRect(i), L"Ramener sur ce pupitre", true, hoveredRecall == i, false, all);
    }

    IGUIElement::draw();   //the windows that are here
}

bool StationScreen::OnEvent(const irr::SEvent& event)
{
    if (event.EventType == irr::EET_GUI_EVENT && device->getEventReceiver()) {
        return device->getEventReceiver()->OnEvent(event);
    }
    return false;
}
