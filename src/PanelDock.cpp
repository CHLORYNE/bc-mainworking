/*   NAUTITECH - Simulateur de Navigation
     Placement of the big instructor windows. See PanelDock.hpp.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#include "PanelDock.hpp"
#include "BridgeSkin.hpp"
#include "launcher/HudText.hpp"

#include <cmath>
#include <string>

namespace bridge
{
    namespace
    {
        const wchar_t* captionOf(int place)
        {
            switch (nextPanelPlace(place)) {
            case PLACE_RIGHT: return L"\u00C0 DROITE";
            case PLACE_LEFT:  return L"\u00C0 GAUCHE";
            default:          return L"AU CENTRE";
            }
        }
        irr::core::rect<irr::s32> toI(const irr::core::rect<irr::f32>& r)
        {
            return irr::core::rect<irr::s32>((irr::s32)floorf(r.UpperLeftCorner.X + 0.5f), (irr::s32)floorf(r.UpperLeftCorner.Y + 0.5f),
                (irr::s32)floorf(r.LowerRightCorner.X + 0.5f), (irr::s32)floorf(r.LowerRightCorner.Y + 0.5f));
        }
    }

    const wchar_t* dockCaption(int place) { return captionOf(place); }

    int nextPanelPlace(int place)
    {
        switch (place) {
        case PLACE_CENTRE: return PLACE_RIGHT;
        case PLACE_RIGHT:  return PLACE_LEFT;
        default:           return PLACE_CENTRE;
        }
    }

    irr::core::rect<irr::f32> PanelDock::keyRect(const irr::core::rect<irr::f32>& closeBox, HudFont* font, irr::f32 k) const
    {
        const irr::f32 textW = font ? font->width(captionOf(place), 0.5f * k) : 80.0f * k;
        const irr::f32 iconW = 26 * k;
        const irr::f32 x1 = closeBox.UpperLeftCorner.X - 12 * k;
        return irr::core::rect<irr::f32>(x1 - (textW + iconW + 26 * k), closeBox.UpperLeftCorner.Y, x1, closeBox.LowerRightCorner.Y);
    }

    void PanelDock::draw(irr::video::IVideoDriver* driver, const irr::core::rect<irr::f32>& key, HudFont* font, irr::f32 k,
        const irr::core::rect<irr::s32>* clip) const
    {
        const Palette& pal = palette();
        const irr::video::SColor face = hover ? pal.keyHover : pal.raised;
        fillRound(driver, toI(key), (irr::s32)(5 * k), face, face, clip);
        //The picture: a screen, and where the window will go on it
        const irr::f32 iw = 22 * k, ih = 15 * k;
        const irr::f32 ix = key.UpperLeftCorner.X + 9 * k, iy = key.getCenter().Y - ih * 0.5f;
        const irr::core::rect<irr::f32> scr(ix, iy, ix + iw, iy + ih);
        const irr::video::SColor ink = pal.textDim;
        driver->draw2DRectangleOutline(toI(scr), ink);
        irr::core::rect<irr::f32> win;
        const int next = nextPanelPlace(place);
        if (next == PLACE_RIGHT) { win = irr::core::rect<irr::f32>(ix + iw * 0.58f, iy + 2 * k, ix + iw - 2 * k, iy + ih - 2 * k); }
        else if (next == PLACE_LEFT) { win = irr::core::rect<irr::f32>(ix + 2 * k, iy + 2 * k, ix + iw * 0.42f, iy + ih - 2 * k); }
        else { win = irr::core::rect<irr::f32>(ix + iw * 0.22f, iy + 3 * k, ix + iw * 0.78f, iy + ih - 3 * k); }
        driver->draw2DRectangle(pal.accent, toI(win), clip);
        if (font) {
            font->drawIn(captionOf(place), irr::core::rect<irr::f32>(scr.LowerRightCorner.X + 8 * k, key.UpperLeftCorner.Y,
                key.LowerRightCorner.X - 6 * k, key.LowerRightCorner.Y), pal.text, HudFont::Left, 0.5f * k, clip);
        }
    }

    bool PanelDock::onMouse(irr::gui::IGUIElement* window, irr::gui::IGUIEnvironment* env, const irr::SEvent::SMouseInput& m,
        const irr::core::rect<irr::f32>& titleBar, const irr::core::rect<irr::f32>& key, const irr::core::rect<irr::f32>& closeBox)
    {
        const irr::core::position2di p(m.X, m.Y);
        const irr::core::vector2df pf((irr::f32)m.X, (irr::f32)m.Y);
        switch (m.Event) {
        case irr::EMIE_MOUSE_MOVED:
            if (dragging) {
                if (!m.isLeftPressed()) { dragging = false; return false; }
                irr::core::rect<irr::s32> r = startRect + (p - grab);
                //Never dragged off the screen: the title bar stays where it can be caught
                const irr::gui::IGUIElement* parent = window->getParent();
                if (parent) {
                    const irr::core::rect<irr::s32> area = parent->getAbsolutePosition();
                    const irr::s32 w = r.getWidth();
                    const irr::s32 minX = -w + 120, maxX = area.getWidth() - 120;
                    const irr::s32 minY = 0, maxY = area.getHeight() - (irr::s32)titleBar.getHeight();
                    irr::core::position2di shift(0, 0);
                    if (r.UpperLeftCorner.X < minX) { shift.X = minX - r.UpperLeftCorner.X; }
                    if (r.UpperLeftCorner.X > maxX) { shift.X = maxX - r.UpperLeftCorner.X; }
                    if (r.UpperLeftCorner.Y < minY) { shift.Y = minY - r.UpperLeftCorner.Y; }
                    if (r.UpperLeftCorner.Y > maxY) { shift.Y = maxY - r.UpperLeftCorner.Y; }
                    r += shift;
                }
                window->setRelativePosition(r);
                return true;
            }
            hover = key.isPointInside(pf);
            return false;
        case irr::EMIE_LMOUSE_PRESSED_DOWN:
            if (key.isPointInside(pf)) {
                if (onPlace) { onPlace(nextPanelPlace(place)); }
                return true;
            }
            if (titleBar.isPointInside(pf) && !closeBox.isPointInside(pf)) {
                dragging = true;
                grab = p;
                startRect = window->getRelativePosition();
                env->setFocus(window); //the moves come here even off the window
                return true;
            }
            return false;
        case irr::EMIE_LMOUSE_LEFT_UP:
            if (dragging) {
                dragging = false;
                env->removeFocus(window); //keys go back to the ship
                if (onMoved) { onMoved(window->getRelativePosition()); }
                return true;
            }
            return false;
        default:
            return false;
        }
    }
}
