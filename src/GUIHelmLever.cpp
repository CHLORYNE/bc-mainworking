/*   NAUTITECH - Simulateur de Navigation
     Helm control for the main bridge view, for a station without a hardware console.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#include "GUIHelmLever.hpp"
#include "BridgeSkin.hpp"

#include "IGUISkin.h"
#include "IGUIEnvironment.h"
#include "IGUIFont.h"
#include "IVideoDriver.h"
#include <cmath>
#include <cwchar>
#include <cstdlib>

namespace irr
{
namespace gui
{

namespace
{
    const wchar_t* TXT_PORT = L"B\u00C2B";   //babord
    const wchar_t* TXT_STBD = L"TRI";         //tribord
    const wchar_t* TXT_TITLE = L"BARRE";
}

GUIHelmLever::GUIHelmLever(IGUIEnvironment* environment, IGUIElement* parent, s32 id, core::rect<s32> rectangle)
    : IGUIScrollBar(environment, parent, id, rectangle),
    Pos(0), Min(-30), Max(30), SmallStep(1), LargeStep(5),
    Dragging(false), DrawBackground(true)
{
#ifdef _DEBUG
    setDebugName("GUIHelmLever");
#endif
    setTabStop(true);
    setTabOrder(-1);
}

GUIHelmLever::~GUIHelmLever()
{
}

void GUIHelmLever::setMax(s32 max)
{
    Max = max;
    if (Min > Max) { Min = Max; }
    setPos(Pos);
}

void GUIHelmLever::setMin(s32 min)
{
    Min = min;
    if (Max < Min) { Max = Min; }
    setPos(Pos);
}

void GUIHelmLever::setPos(s32 pos)
{
    Pos = core::s32_clamp(pos, Min, Max);
}

//The readout strip along the top, as on the engine levers
f32 GUIHelmLever::headerHeight() const
{
    IGUISkin* skin = Environment ? Environment->getSkin() : 0;
    IGUIFont* font = skin ? skin->getFont() : 0;
    const f32 fh = font ? (f32)font->getDimension(L"0Ag").Height : 14.0f;
    const f32 h = (f32)AbsoluteRect.getHeight();
    return core::min_(h * 0.45f, fh * 1.6f);
}

//Travel of the grip centre, inset so the grip never overhangs the plate
f32 GUIHelmLever::xFromPos(s32 pos) const
{
    const f32 left = (f32)AbsoluteRect.UpperLeftCorner.X;
    const f32 w = (f32)AbsoluteRect.getWidth();
    const f32 inset = core::max_(8.0f, w * 0.07f);
    const f32 range = (f32)(Max - Min);
    const f32 t = (range > 0) ? ((f32)(pos - Min) / range) : 0.5f;
    return left + inset + t * (w - 2.0f * inset);
}

s32 GUIHelmLever::posFromX(s32 x) const
{
    const f32 left = (f32)AbsoluteRect.UpperLeftCorner.X;
    const f32 w = (f32)AbsoluteRect.getWidth();
    const f32 inset = core::max_(8.0f, w * 0.07f);
    const f32 travel = w - 2.0f * inset;
    if (travel <= 0) { return Pos; }
    f32 t = ((f32)x - left - inset) / travel;
    t = panelClamp(t, 0.0f, 1.0f);
    s32 p = Min + (s32)floorf(t * (f32)(Max - Min) + 0.5f);
    //Detent at midships, so "helm to zero" is easy to hit with a mouse
    if (p > Min && p < Max && abs(p) <= 1) { p = 0; }
    return p;
}

void GUIHelmLever::sendChanged()
{
    if (!Parent) { return; }
    SEvent e;
    e.EventType = EET_GUI_EVENT;
    e.GUIEvent.Caller = this;
    e.GUIEvent.Element = 0;
    e.GUIEvent.EventType = EGET_SCROLL_BAR_CHANGED;
    Parent->OnEvent(e);
}

bool GUIHelmLever::OnEvent(const SEvent& event)
{
    if (!isEnabled()) { return IGUIElement::OnEvent(event); }

    switch (event.EventType)
    {
    case EET_GUI_EVENT:
        if (event.GUIEvent.EventType == EGET_ELEMENT_FOCUS_LOST && event.GUIEvent.Caller == this) {
            Dragging = false;
        }
        break;

    case EET_MOUSE_INPUT_EVENT:
    {
        const core::position2di p(event.MouseInput.X, event.MouseInput.Y);
        const bool isInside = isPointInside(p);
        switch (event.MouseInput.Event)
        {
        case EMIE_MOUSE_WHEEL:
            if (isInside) {
                const s32 oldPos = Pos;
                setPos(Pos + (event.MouseInput.Wheel < 0 ? SmallStep : -SmallStep));
                if (Pos != oldPos) { sendChanged(); }
                return true;
            }
            break;

        case EMIE_LMOUSE_PRESSED_DOWN:
            if (isInside) {
                Dragging = true;
                const s32 oldPos = Pos;
                setPos(posFromX(p.X));
                if (Pos != oldPos) { sendChanged(); }
                Environment->setFocus(this);
                return true;
            }
            break;

        //Right click: helm back to midships
        case EMIE_RMOUSE_PRESSED_DOWN:
            if (isInside) {
                if (Pos != 0) { setPos(0); sendChanged(); }
                return true;
            }
            break;

        case EMIE_LMOUSE_LEFT_UP:
            if (Dragging) {
                Dragging = false;
                Environment->removeFocus(this); //the keyboard goes back to the ship
                return true;
            }
            return isInside;

        case EMIE_MOUSE_MOVED:
            if (!event.MouseInput.isLeftPressed()) {
                Dragging = false;
                break;
            }
            if (Dragging) {
                const s32 oldPos = Pos;
                setPos(posFromX(p.X));
                if (Pos != oldPos) { sendChanged(); }
                return true;
            }
            break;

        default:
            break;
        }
        break;
    }

    default:
        break;
    }

    return IGUIElement::OnEvent(event);
}

void GUIHelmLever::draw()
{
    if (!IsVisible) { return; }

    video::IVideoDriver* driver = Environment->getVideoDriver();
    IGUISkin* skin = Environment->getSkin();
    IGUIFont* font = skin ? skin->getFont() : 0;
    const bridge::Palette& bp = bridge::palette();
    const int mode = bridge::currentMode();
    const bool day = (mode == bridge::MODE_DAY), night = (mode == bridge::MODE_NIGHT), digital = (mode == bridge::MODE_DIGITAL);

    const core::rect<s32>& r = AbsoluteRect;
    const f32 x0 = (f32)r.UpperLeftCorner.X, x1 = (f32)r.LowerRightCorner.X;
    const f32 y0 = (f32)r.UpperLeftCorner.Y, y1 = (f32)r.LowerRightCorner.Y;
    const f32 w = x1 - x0, h = y1 - y0;
    if (w <= 4 || h <= 4) { return; }

    //The engine levers' hardware colours, per palette
    const video::SColor slotDark = day ? video::SColor(255, 46, 52, 60) : video::SColor(255, 4, 6, 9);
    const video::SColor slotEdge = day ? video::SColor(255, 120, 130, 142) : bp.edge;
    const video::SColor engrave = day ? video::SColor(255, 40, 48, 58) : (night ? video::SColor(255, 150, 108, 58) :
        (digital ? video::SColor(255, 76, 220, 92) : video::SColor(255, 200, 208, 218)));
    const video::SColor engraveDim = day ? video::SColor(255, 110, 120, 132) : (night ? video::SColor(255, 92, 70, 44) :
        (digital ? video::SColor(255, 34, 110, 44) : video::SColor(255, 112, 126, 144)));
    const video::SColor gripTop = night ? video::SColor(255, 46, 44, 42) : video::SColor(255, 78, 82, 90);
    const video::SColor gripBottom = night ? video::SColor(255, 14, 13, 12) : video::SColor(255, 22, 24, 28);
    const video::SColor gripShine = night ? video::SColor(255, 80, 70, 58) : video::SColor(255, 170, 176, 186);
    const video::SColor indexCol = night ? video::SColor(255, 210, 150, 70) : (digital ? video::SColor(255, 252, 228, 72) : video::SColor(255, 245, 247, 250));
    const video::SColor lcdBg = day ? video::SColor(255, 186, 199, 180) : video::SColor(255, 6, 8, 10);
    const video::SColor lcdOff = day ? video::SColor(255, 40, 48, 44) : (night ? video::SColor(255, 150, 108, 58) :
        (digital ? video::SColor(255, 96, 255, 116) : video::SColor(255, 150, 158, 170)));
    const f32 dimK = night ? 0.6f : 1.0f;
    const video::SColor portCol(255, (u32)(230 * dimK), (u32)(40 * dimK), (u32)(36 * dimK));
    const video::SColor stbdCol(255, (u32)(30 * dimK), (u32)(200 * dimK), (u32)(70 * dimK));

    const f32 headerH = headerHeight();
    const f32 cy = y0 + headerH + (h - headerH) * 0.5f;     //the gate's centre line
    const f32 slotH = core::max_(6.0f, (h - headerH) * 0.30f);
    const f32 zoneH = slotH - 4.0f;
    const f32 xMid = xFromPos(0);
    const f32 xLeft = xFromPos(Min), xRight = xFromPos(Max);
    const f32 knobW = core::max_(10.0f, w * 0.055f);
    const f32 knobH = (h - headerH) * 0.86f;
    const f32 xKnob = xFromPos(Pos);

    bridge::frameRound(driver, r, 7, bp.edge, bp.panelTop, bp.panelBottom, &AbsoluteClippingRect);

    batch.begin(driver);

    //Readout window along the top
    const core::rect<f32> lcd(x0 + w * 0.22f, y0 + headerH * 0.14f, x1 - w * 0.22f, y0 + headerH * 0.90f);
    batch.rect(core::rect<f32>(lcd.UpperLeftCorner.X - 1, lcd.UpperLeftCorner.Y - 1, lcd.LowerRightCorner.X + 1, lcd.LowerRightCorner.Y + 1), slotEdge);
    batch.rect(lcd, lcdBg);

    //The gate, across, with its notch at midships
    batch.rect(core::rect<f32>(xLeft - 3, cy - slotH * 0.5f - 1, xRight + 3, cy + slotH * 0.5f + 1), slotEdge);
    batch.rect(core::rect<f32>(xLeft - 2, cy - slotH * 0.5f, xRight + 2, cy + slotH * 0.5f), slotDark);
    batch.rect(core::rect<f32>(xMid - 2, cy - slotH * 0.5f - 4, xMid + 2, cy + slotH * 0.5f + 4), slotEdge);

    //Faint port / starboard halves, then the lit band from midships to the grip
    batch.rect(core::rect<f32>(xLeft, cy - zoneH * 0.5f, xMid, cy + zoneH * 0.5f), panelWithAlpha(portCol, 40));
    batch.rect(core::rect<f32>(xMid, cy - zoneH * 0.5f, xRight, cy + zoneH * 0.5f), panelWithAlpha(stbdCol, 40));
    if (Pos != 0) {
        const video::SColor fill = (Pos < 0) ? portCol : stbdCol;
        const f32 fa = core::min_(xMid, xKnob), fb = core::max_(xMid, xKnob);
        batch.rect(core::rect<f32>(fa, cy - zoneH * 0.5f, fb, cy + zoneH * 0.5f), panelWithAlpha(fill, 215));
    }

    //Engraved scale every 5 degrees, longer every 10, above and below the gate
    const f32 tickOut = slotH * 0.5f + core::max_(2.0f, h * 0.04f);
    const s32 span = core::max_(abs(Min), abs(Max));
    for (s32 v = -span; v <= span; v += 5) {
        if (v < Min || v > Max) { continue; }
        const bool major = (v % 10 == 0);
        const f32 len = major ? (h - headerH) * 0.16f : (h - headerH) * 0.09f;
        const f32 tx = xFromPos(v);
        const f32 tw = major ? 1.0f : 0.5f;
        const video::SColor col = major ? engrave : engraveDim;
        batch.rect(core::rect<f32>(tx - tw, cy - tickOut - len, tx + tw, cy - tickOut), col);
        batch.rect(core::rect<f32>(tx - tw, cy + tickOut, tx + tw, cy + tickOut + len), col);
    }

    //The grip: dark moulded handle standing across the gate, white index line, side lamp
    const core::rect<f32> knob(xKnob - knobW * 0.5f, cy - knobH * 0.5f, xKnob + knobW * 0.5f, cy + knobH * 0.5f);
    batch.rect(core::rect<f32>(knob.UpperLeftCorner.X + knobW * 0.35f, knob.UpperLeftCorner.Y + 2,
        knob.LowerRightCorner.X + knobW * 0.35f, knob.LowerRightCorner.Y + 2), video::SColor(day ? 70 : 120, 0, 0, 0)); //shadow
    batch.rect(core::rect<f32>(knob.UpperLeftCorner.X - 1, knob.UpperLeftCorner.Y - 1, knob.LowerRightCorner.X + 1, knob.LowerRightCorner.Y + 1),
        video::SColor(255, 8, 9, 10));
    batch.rectV(knob, gripTop, gripBottom);
    batch.rect(core::rect<f32>(knob.UpperLeftCorner.X, knob.UpperLeftCorner.Y + 1, knob.UpperLeftCorner.X + core::max_(1.0f, knobW * 0.10f), knob.LowerRightCorner.Y - 1), gripShine);
    batch.rect(core::rect<f32>(xKnob - core::max_(0.8f, knobW * 0.07f), cy - knobH * 0.25f, xKnob + core::max_(0.8f, knobW * 0.07f), cy + knobH * 0.25f), indexCol);
    const video::SColor lamp = (Pos < 0) ? portCol : ((Pos > 0) ? stbdCol : video::SColor(255, 40, 42, 46));
    batch.disc(core::vector2df(xKnob, knob.UpperLeftCorner.Y + knobH * 0.12f), core::max_(2.0f, knobW * 0.16f), lamp, panelWithAlpha(lamp, 200));

    batch.flush();

    if (font) {
        //BAB / TRI engraved at the ends of the scale, BARRE over the readout's left
        const core::dimension2du dP = font->getDimension(TXT_PORT);
        const core::dimension2du dS = font->getDimension(TXT_STBD);
        font->draw(TXT_PORT, core::rect<s32>((s32)(x0 + 4), (s32)y0, (s32)(lcd.UpperLeftCorner.X - 2), (s32)(y0 + headerH)),
            engraveDim, false, true, &AbsoluteClippingRect);
        font->draw(TXT_STBD, core::rect<s32>((s32)(lcd.LowerRightCorner.X + 2), (s32)y0, (s32)(x1 - 4), (s32)(y0 + headerH)),
            engraveDim, false, true, &AbsoluteClippingRect);
        (void)dP; (void)dS;

        //Readout: the order, side and degrees
        wchar_t buf[24];
        if (Pos == 0) { swprintf(buf, 24, L"%ls 0", TXT_TITLE); }
        else { swprintf(buf, 24, L"%ls %d", (Pos < 0) ? TXT_PORT : TXT_STBD, abs(Pos)); }
        const core::dimension2du d = font->getDimension(buf);
        video::SColor col = (Pos == 0) ? lcdOff : ((Pos < 0) ? portCol : stbdCol);
        if (day && Pos != 0) { col = (Pos < 0) ? video::SColor(255, 170, 20, 20) : video::SColor(255, 10, 110, 40); }
        const f32 cx = (lcd.UpperLeftCorner.X + lcd.LowerRightCorner.X) * 0.5f;
        const f32 ty = (lcd.UpperLeftCorner.Y + lcd.LowerRightCorner.Y - (f32)d.Height) * 0.5f;
        const core::rect<s32> tr((s32)(cx - d.Width * 0.5f), (s32)ty, (s32)(cx + d.Width * 0.5f) + 2, (s32)ty + (s32)d.Height + 2);
        font->draw(buf, tr, col, false, false, &AbsoluteClippingRect);
    }

    IGUIElement::draw();
}

} // end namespace gui
} // end namespace irr
