/*   NAUTITECH - Simulateur de Navigation
     Engine control lever for the main bridge view (Bridge Command fork).

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#include "GUIEngineLever.hpp"

#include "IGUISkin.h"
#include "IGUIEnvironment.h"
#include "IGUIFont.h"
#include "IVideoDriver.h"
#include <cmath>
#include <cwchar>

namespace irr
{
namespace gui
{

namespace
{
    //Same family of greys as the instrument console, so the whole bottom panel reads as one console.
    const video::SColor PLATE_TOP(255, 72, 77, 86);
    const video::SColor PLATE_BOTTOM(255, 43, 47, 54);
    const video::SColor PLATE_EDGE_HI(255, 108, 114, 126);
    const video::SColor PLATE_EDGE_LO(255, 20, 22, 26);
    const video::SColor SLOT_DARK(255, 12, 13, 16);
    const video::SColor SLOT_EDGE(255, 58, 62, 70);
    const video::SColor TICK(255, 214, 219, 226);
    const video::SColor TICK_DIM(255, 128, 136, 148);
    const video::SColor KNOB_TOP(255, 214, 219, 226);
    const video::SColor KNOB_BOTTOM(255, 96, 102, 112);
    const video::SColor KNOB_EDGE(255, 26, 28, 32);
    const video::SColor KNOB_GRIP(255, 70, 75, 83);
    const video::SColor STOP_LINE(255, 236, 240, 245);
    const video::SColor TEXT_DIM(255, 150, 158, 170);

    const wchar_t* TXT_AHEAD = L"AV";     //avant
    const wchar_t* TXT_ASTERN = L"AR";    //arri\u00E8re
    const wchar_t* TXT_STOP = L"0";
}

GUIEngineLever::GUIEngineLever(IGUIEnvironment* environment, IGUIElement* parent, s32 id, core::rect<s32> rectangle,
    video::SColor aheadColour, video::SColor asternColour)
    : IGUIScrollBar(environment, parent, id, rectangle),
    aheadCol(aheadColour), asternCol(asternColour),
    Pos(0), Min(-100), Max(100), SmallStep(5), LargeStep(25),
    Dragging(false), DrawBackground(true)
{
#ifdef _DEBUG
    setDebugName("GUIEngineLever");
#endif
    //The ini colours are semi-transparent (they used to be painted as a faint wash over the old
    //slider). Here they are the lever's own scale, so they are drawn solid.
    aheadCol.setAlpha(255);
    asternCol.setAlpha(255);

    setTabStop(true);
    setTabOrder(-1);
}

GUIEngineLever::~GUIEngineLever()
{
}

void GUIEngineLever::setMax(s32 max)
{
    Max = max;
    if (Min > Max) { Min = Max; }
    setPos(Pos);
}

void GUIEngineLever::setMin(s32 min)
{
    Min = min;
    if (Max < Min) { Max = Min; }
    setPos(Pos);
}

void GUIEngineLever::setPos(s32 pos)
{
    Pos = core::s32_clamp(pos, Min, Max);
}

void GUIEngineLever::updateAbsolutePosition()
{
    IGUIElement::updateAbsolutePosition();
}

void GUIEngineLever::OnPostRender(u32 /*timeMs*/)
{
}

//KYARA: the readout lives in a fixed strip at the top of the plate, so the knob can never cover
//it and it is never hidden by the "Hide Controls" button, which overlaps the bottom of the lever.
f32 GUIEngineLever::headerHeight() const
{
    IGUISkin* skin = Environment ? Environment->getSkin() : 0;
    IGUIFont* font = skin ? skin->getFont() : 0;
    const f32 fh = font ? (f32)font->getDimension(L"0Ag").Height : 14.0f;
    const f32 h = (f32)AbsoluteRect.getHeight();
    return core::min_(h * 0.16f, fh * 1.6f);
}

//Travel of the knob centre, inset so the knob never overhangs the plate.
f32 GUIEngineLever::yFromPos(s32 pos) const
{
    const f32 top = (f32)AbsoluteRect.UpperLeftCorner.Y + headerHeight();
    const f32 h = (f32)AbsoluteRect.getHeight() - headerHeight();
    const f32 inset = h * 0.05f;
    const f32 range = (f32)(Max - Min);
    const f32 t = (range > 0) ? ((f32)(pos - Min) / range) : 0.5f;
    return top + inset + t * (h - 2.0f * inset);
}

s32 GUIEngineLever::posFromY(s32 y) const
{
    const f32 top = (f32)AbsoluteRect.UpperLeftCorner.Y + headerHeight();
    const f32 h = (f32)AbsoluteRect.getHeight() - headerHeight();
    const f32 inset = h * 0.05f;
    const f32 travel = h - 2.0f * inset;
    if (travel <= 0) { return Pos; }
    f32 t = ((f32)y - top - inset) / travel;
    t = panelClamp(t, 0.0f, 1.0f);
    s32 p = Min + (s32)floorf(t * (f32)(Max - Min) + 0.5f);
    //Detent at stop: the middle few percent snap to zero, so "stop" is easy to hit with a mouse.
    if (p > Min && p < Max && abs(p) <= 3) { p = 0; }
    return p;
}

void GUIEngineLever::sendChanged()
{
    if (!Parent) { return; }
    SEvent e;
    e.EventType = EET_GUI_EVENT;
    e.GUIEvent.Caller = this;
    e.GUIEvent.Element = 0;
    e.GUIEvent.EventType = EGET_SCROLL_BAR_CHANGED;
    Parent->OnEvent(e);
}

bool GUIEngineLever::OnEvent(const SEvent& event)
{
    if (!isEnabled()) { return IGUIElement::OnEvent(event); }

    switch (event.EventType)
    {
    case EET_KEY_INPUT_EVENT:
        if (event.KeyInput.PressedDown) {
            const s32 oldPos = Pos;
            bool absorb = true;
            switch (event.KeyInput.Key) {
                //Up on the keyboard means "more ahead", which is a decreasing Pos here.
            case KEY_UP:    setPos(Pos - SmallStep); break;
            case KEY_DOWN:  setPos(Pos + SmallStep); break;
            case KEY_PRIOR: setPos(Pos - LargeStep); break;
            case KEY_NEXT:  setPos(Pos + LargeStep); break;
            case KEY_HOME:  setPos(Min); break;
            case KEY_END:   setPos(Max); break;
            default: absorb = false;
            }
            if (Pos != oldPos) { sendChanged(); }
            if (absorb) { return true; }
        }
        break;

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
            if (Environment->hasFocus(this)) {
                const s32 oldPos = Pos;
                setPos(Pos + (event.MouseInput.Wheel < 0 ? SmallStep : -SmallStep));
                if (Pos != oldPos) { sendChanged(); }
                return true;
            }
            break;

            //Right click is handled the same way as left: MyEventReceiver uses it to drive both
            //engines together, exactly as it did with the old slider.
        case EMIE_LMOUSE_PRESSED_DOWN:
        case EMIE_RMOUSE_PRESSED_DOWN:
            if (isInside) {
                Dragging = true;
                const s32 oldPos = Pos;
                setPos(posFromY(p.Y));
                if (Pos != oldPos) { sendChanged(); }
                Environment->setFocus(this);
                return true;
            }
            break;

        case EMIE_LMOUSE_LEFT_UP:
        case EMIE_RMOUSE_LEFT_UP:
            Dragging = false;
            return isInside;

        case EMIE_MOUSE_MOVED:
            if (!event.MouseInput.isLeftPressed() && !event.MouseInput.isRightPressed()) {
                Dragging = false;
                break;
            }
            if (Dragging) {
                const s32 oldPos = Pos;
                setPos(posFromY(p.Y));
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

void GUIEngineLever::draw()
{
    if (!IsVisible) { return; }

    video::IVideoDriver* driver = Environment->getVideoDriver();
    IGUISkin* skin = Environment->getSkin();
    IGUIFont* font = skin ? skin->getFont() : 0;

    const core::rect<s32>& r = AbsoluteRect;
    const f32 x0 = (f32)r.UpperLeftCorner.X, x1 = (f32)r.LowerRightCorner.X;
    const f32 y0 = (f32)r.UpperLeftCorner.Y, y1 = (f32)r.LowerRightCorner.Y;
    const f32 w = x1 - x0, h = y1 - y0;
    if (w <= 4 || h <= 4) { return; }

    const f32 cx = (x0 + x1) * 0.5f;
    const f32 slotW = core::max_(7.0f, w * 0.34f);   //the travel slot
    const f32 zoneW = slotW * 0.80f;                 //the coloured fill inside it
    const f32 yStop = yFromPos(0);
    const f32 yTopTravel = yFromPos(Min), yBotTravel = yFromPos(Max);
    const f32 knobH = core::max_(9.0f, h * 0.045f);
    const f32 knobW = w * 0.86f;
    const f32 yKnob = yFromPos(Pos);
    const f32 headerH = headerHeight();

    batch.begin(driver);

    //Plate with a lit top edge and a shadowed bottom edge
    batch.rectV(core::rect<f32>(x0, y0, x1, y1), PLATE_TOP, PLATE_BOTTOM);
    batch.rect(core::rect<f32>(x0, y0, x1, y0 + 1), PLATE_EDGE_HI);
    batch.rect(core::rect<f32>(x0, y1 - 1, x1, y1), PLATE_EDGE_LO);
    batch.rect(core::rect<f32>(x0, y0, x0 + 1, y1), PLATE_EDGE_LO);
    batch.rect(core::rect<f32>(x1 - 1, y0, x1, y1), PLATE_EDGE_HI);

    //Readout window at the top of the plate
    batch.rect(core::rect<f32>(x0 + w * 0.10f - 1, y0 + headerH * 0.10f - 1, x1 - w * 0.10f + 1, y0 + headerH * 0.94f + 1), SLOT_EDGE);
    batch.rect(core::rect<f32>(x0 + w * 0.10f, y0 + headerH * 0.12f, x1 - w * 0.10f, y0 + headerH * 0.92f), SLOT_DARK);

    //Recessed slot down the middle
    batch.rect(core::rect<f32>(cx - slotW * 0.5f - 1, yTopTravel - 1, cx + slotW * 0.5f + 1, yBotTravel + 1), SLOT_EDGE);
    batch.rect(core::rect<f32>(cx - slotW * 0.5f, yTopTravel, cx + slotW * 0.5f, yBotTravel), SLOT_DARK);

    //Faint full-travel bands, so which half is ahead and which is astern is clear even at stop
    batch.rectV(core::rect<f32>(cx - zoneW * 0.5f, yTopTravel, cx + zoneW * 0.5f, yStop),
        panelWithAlpha(aheadCol, 70), panelWithAlpha(aheadCol, 18));
    batch.rectV(core::rect<f32>(cx - zoneW * 0.5f, yStop, cx + zoneW * 0.5f, yBotTravel),
        panelWithAlpha(asternCol, 18), panelWithAlpha(asternCol, 70));

    //Solid column from stop to the lever position - length is the power demanded, colour the
    //direction. This is what the eye picks up from across the bridge.
    if (Pos != 0) {
        const video::SColor fill = (Pos < 0) ? aheadCol : asternCol;
        const f32 yEnd = yFromPos(Pos);
        const f32 fa = core::min_(yStop, yEnd), fb = core::max_(yStop, yEnd);
        batch.rect(core::rect<f32>(cx - zoneW * 0.5f, fa, cx + zoneW * 0.5f, fb), fill);
        batch.rect(core::rect<f32>(cx - zoneW * 0.5f, fa, cx - zoneW * 0.5f + 1, fb), panelWithAlpha(fill, 120));
    }

    //Graduations every 10%, longer every 25%, on both sides of the slot
    const f32 tickIn = slotW * 0.5f + core::max_(1.5f, w * 0.04f);
    for (int v = -100; v <= 100; v += 10) {
        const bool major = (v % 25 == 0) || (v == 0);
        const f32 len = major ? w * 0.20f : w * 0.11f;
        const f32 ty = yFromPos((s32)(-v));  //v is percent ahead, Pos is inverted
        const f32 th = core::max_(1.0f, major ? h * 0.004f : h * 0.003f);
        const video::SColor col = major ? TICK : TICK_DIM;
        batch.rect(core::rect<f32>(cx - tickIn - len, ty - th, cx - tickIn, ty + th), col);
        batch.rect(core::rect<f32>(cx + tickIn, ty - th, cx + tickIn + len, ty + th), col);
    }
    //Stop line across the full width - the reference the trainee looks for
    batch.rect(core::rect<f32>(x0 + w * 0.10f, yStop - core::max_(1.0f, h * 0.0035f),
        x1 - w * 0.10f, yStop + core::max_(1.0f, h * 0.0035f)), STOP_LINE);

    //Knob: machined cap with a bevel, grip lines and an index line at its centre
    const core::rect<f32> knob(cx - knobW * 0.5f, yKnob - knobH * 0.5f, cx + knobW * 0.5f, yKnob + knobH * 0.5f);
    batch.rect(core::rect<f32>(knob.UpperLeftCorner.X, knob.UpperLeftCorner.Y + knobH * 0.25f,
        knob.LowerRightCorner.X, knob.LowerRightCorner.Y + knobH * 0.30f), video::SColor(110, 0, 0, 0)); //shadow
    batch.rect(core::rect<f32>(knob.UpperLeftCorner.X - 1, knob.UpperLeftCorner.Y - 1,
        knob.LowerRightCorner.X + 1, knob.LowerRightCorner.Y + 1), KNOB_EDGE);
    batch.rectV(knob, KNOB_TOP, KNOB_BOTTOM);
    for (int g = -1; g <= 1; g++) {
        const f32 gy = yKnob + (f32)g * knobH * 0.26f;
        batch.rect(core::rect<f32>(knob.UpperLeftCorner.X + knobW * 0.18f, gy - core::max_(0.7f, knobH * 0.055f),
            knob.LowerRightCorner.X - knobW * 0.18f, gy + core::max_(0.7f, knobH * 0.055f)), KNOB_GRIP);
    }
    //Index line in the lever's own colour, showing which side of stop it is on
    const video::SColor idx = (Pos < 0) ? aheadCol : ((Pos > 0) ? asternCol : KNOB_EDGE);
    batch.rect(core::rect<f32>(knob.UpperLeftCorner.X, yKnob - core::max_(0.8f, knobH * 0.07f),
        knob.LowerRightCorner.X, yKnob + core::max_(0.8f, knobH * 0.07f)), idx);

    batch.flush();

    //Readout strip at the top: direction and percent demanded
    if (font) {
        const s32 percent = (s32)floorf(fabsf((f32)Pos) + 0.5f);
        wchar_t buf[24];
        if (percent == 0) { swprintf(buf, 24, L"%ls", TXT_STOP); }
        else { swprintf(buf, 24, L"%ls %d", (Pos < 0) ? TXT_AHEAD : TXT_ASTERN, percent); }
        const core::dimension2du d = font->getDimension(buf);
        const video::SColor col = (Pos == 0) ? TEXT_DIM : ((Pos < 0) ? aheadCol : asternCol);
        const f32 ty = y0 + (headerH - (f32)d.Height) * 0.5f;
        const core::rect<s32> tr((s32)(cx - d.Width * 0.5f), (s32)ty, (s32)(cx + d.Width * 0.5f) + 2, (s32)ty + (s32)d.Height + 2);
        font->draw(buf, tr, col, false, false, &AbsoluteClippingRect);
    }

    IGUIElement::draw();
}

} // end namespace gui
} // end namespace irr
