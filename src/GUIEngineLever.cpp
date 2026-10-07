/*   NAUTITECH - Simulateur de Navigation
     Engine control lever for the main bridge view (Bridge Command fork).

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#include "GUIEngineLever.hpp"
#include "BridgeSkin.hpp"

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
    const wchar_t* TXT_AHEAD = L"AV";     //avant
    const wchar_t* TXT_ASTERN = L"AR";    //arri\u00E8re
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
    const bridge::Palette& bp = bridge::palette();
    const int mode = bridge::currentMode();
    const bool day = (mode == bridge::MODE_DAY), night = (mode == bridge::MODE_NIGHT);

    const core::rect<s32>& r = AbsoluteRect;
    const f32 x0 = (f32)r.UpperLeftCorner.X, x1 = (f32)r.LowerRightCorner.X;
    const f32 y0 = (f32)r.UpperLeftCorner.Y, y1 = (f32)r.LowerRightCorner.Y;
    const f32 w = x1 - x0, h = y1 - y0;
    if (w <= 4 || h <= 4) { return; }

    //Colours of the hardware, per palette: the plate follows the console, the handle stays a dark
    //moulded grip in every light, as on a real control stand.
    const video::SColor slotDark = day ? video::SColor(255, 46, 52, 60) : video::SColor(255, 4, 6, 9);
    const video::SColor slotEdge = day ? video::SColor(255, 120, 130, 142) : bp.edge;
    const video::SColor engrave = day ? video::SColor(255, 40, 48, 58) : (night ? video::SColor(255, 150, 108, 58) : video::SColor(255, 200, 208, 218));
    const video::SColor engraveDim = day ? video::SColor(255, 110, 120, 132) : (night ? video::SColor(255, 92, 70, 44) : video::SColor(255, 112, 126, 144));
    const video::SColor gripTop = night ? video::SColor(255, 46, 44, 42) : video::SColor(255, 78, 82, 90);
    const video::SColor gripBottom = night ? video::SColor(255, 14, 13, 12) : video::SColor(255, 22, 24, 28);
    const video::SColor gripShine = night ? video::SColor(255, 80, 70, 58) : video::SColor(255, 170, 176, 186);
    const video::SColor indexCol = night ? video::SColor(255, 210, 150, 70) : video::SColor(255, 245, 247, 250);
    const video::SColor lcdBg = day ? video::SColor(255, 186, 199, 180) : video::SColor(255, 6, 8, 10);
    const video::SColor lcdOff = day ? video::SColor(255, 40, 48, 44) : (night ? video::SColor(255, 150, 108, 58) : video::SColor(255, 150, 158, 170));
    //The ahead / astern colours of the ini, toned down at night
    const f32 dimK = night ? 0.6f : 1.0f;
    const video::SColor ahead(255, (u32)(aheadCol.getRed() * dimK), (u32)(aheadCol.getGreen() * dimK), (u32)(aheadCol.getBlue() * dimK));
    const video::SColor astern(255, (u32)(asternCol.getRed() * dimK), (u32)(asternCol.getGreen() * dimK), (u32)(asternCol.getBlue() * dimK));

    const f32 cx = (x0 + x1) * 0.5f;
    const f32 slotW = core::max_(8.0f, w * 0.30f);    //the gate the lever runs in
    const f32 zoneW = slotW - 4.0f;                   //the lit column inside it
    const f32 yStop = yFromPos(0);
    const f32 yTopTravel = yFromPos(Min), yBotTravel = yFromPos(Max);
    const f32 knobH = core::max_(14.0f, h * 0.075f);
    const f32 knobW = w * 0.92f;
    const f32 yKnob = yFromPos(Pos);
    const f32 headerH = headerHeight();

    //Plate: the console's panel, rounded like it
    bridge::frameRound(driver, r, 7, bp.edge, bp.panelTop, bp.panelBottom, &AbsoluteClippingRect);

    batch.begin(driver);

    //Readout window at the top
    const core::rect<f32> lcd(x0 + w * 0.10f, y0 + headerH * 0.14f, x1 - w * 0.10f, y0 + headerH * 0.90f);
    batch.rect(core::rect<f32>(lcd.UpperLeftCorner.X - 1, lcd.UpperLeftCorner.Y - 1, lcd.LowerRightCorner.X + 1, lcd.LowerRightCorner.Y + 1), slotEdge);
    batch.rect(lcd, lcdBg);

    //The gate: a recessed slot with its brush seal, a notch at stop
    batch.rect(core::rect<f32>(cx - slotW * 0.5f - 1, yTopTravel - 3, cx + slotW * 0.5f + 1, yBotTravel + 3), slotEdge);
    batch.rect(core::rect<f32>(cx - slotW * 0.5f, yTopTravel - 2, cx + slotW * 0.5f, yBotTravel + 2), slotDark);
    batch.rect(core::rect<f32>(cx - slotW * 0.5f - 4, yStop - 2, cx + slotW * 0.5f + 4, yStop + 2), slotEdge);

    //Faint ahead / astern halves, then the lit column from stop to the lever: its length is the
    //power ordered, its colour the direction - what the eye picks up from across the bridge
    batch.rectV(core::rect<f32>(cx - zoneW * 0.5f, yTopTravel, cx + zoneW * 0.5f, yStop),
        panelWithAlpha(ahead, 60), panelWithAlpha(ahead, 14));
    batch.rectV(core::rect<f32>(cx - zoneW * 0.5f, yStop, cx + zoneW * 0.5f, yBotTravel),
        panelWithAlpha(astern, 14), panelWithAlpha(astern, 60));
    if (Pos != 0) {
        const video::SColor fill = (Pos < 0) ? ahead : astern;
        const f32 fa = core::min_(yStop, yKnob), fb = core::max_(yStop, yKnob);
        batch.rect(core::rect<f32>(cx - zoneW * 0.5f, fa, cx + zoneW * 0.5f, fb), panelWithAlpha(fill, 215));
        batch.rect(core::rect<f32>(cx - zoneW * 0.5f, fa, cx - zoneW * 0.5f + 1, fb), panelWithAlpha(video::SColor(255, 255, 255, 255), 60));
    }
    //Brush seal: fine dark lines down the gate
    for (int k = -1; k <= 1; k += 2) {
        batch.rect(core::rect<f32>(cx + k * slotW * 0.25f, yTopTravel, cx + k * slotW * 0.25f + 1, yBotTravel), panelWithAlpha(video::SColor(255, 0, 0, 0), 70));
    }

    //Engraved scale: every 10 %, longer every 50 %, on both sides
    const f32 tickIn = slotW * 0.5f + core::max_(2.0f, w * 0.05f);
    for (int v = -100; v <= 100; v += 10) {
        const bool major = (v % 50 == 0);
        const f32 len = major ? w * 0.16f : w * 0.09f;
        const f32 ty = yFromPos((s32)(-v));
        const f32 th = major ? 1.0f : 0.5f;
        const video::SColor col = major ? engrave : engraveDim;
        batch.rect(core::rect<f32>(cx - tickIn - len, ty - th, cx - tickIn, ty + th), col);
        batch.rect(core::rect<f32>(cx + tickIn, ty - th, cx + tickIn + len, ty + th), col);
    }

    //The handle: a dark moulded grip with a lit top edge, grip ribs and a white index line
    const core::rect<f32> knob(cx - knobW * 0.5f, yKnob - knobH * 0.5f, cx + knobW * 0.5f, yKnob + knobH * 0.5f);
    batch.rect(core::rect<f32>(knob.UpperLeftCorner.X + 2, knob.UpperLeftCorner.Y + knobH * 0.35f,
        knob.LowerRightCorner.X + 2, knob.LowerRightCorner.Y + knobH * 0.35f), video::SColor(day ? 70 : 120, 0, 0, 0)); //shadow
    batch.rect(core::rect<f32>(knob.UpperLeftCorner.X - 1, knob.UpperLeftCorner.Y - 1, knob.LowerRightCorner.X + 1, knob.LowerRightCorner.Y + 1),
        video::SColor(255, 8, 9, 10));
    batch.rectV(knob, gripTop, gripBottom);
    batch.rect(core::rect<f32>(knob.UpperLeftCorner.X + 1, knob.UpperLeftCorner.Y, knob.LowerRightCorner.X - 1, knob.UpperLeftCorner.Y + core::max_(1.0f, knobH * 0.10f)), gripShine);
    for (int g = 1; g <= 4; g++) {
        const f32 gx0 = knob.UpperLeftCorner.X + knobW * (0.08f + 0.05f * (f32)g);
        batch.rect(core::rect<f32>(gx0, knob.UpperLeftCorner.Y + knobH * 0.25f, gx0 + 1, knob.LowerRightCorner.Y - knobH * 0.2f), panelWithAlpha(gripShine, 90));
        const f32 rx = knob.LowerRightCorner.X - knobW * (0.08f + 0.05f * (f32)g);
        batch.rect(core::rect<f32>(rx - 1, knob.UpperLeftCorner.Y + knobH * 0.25f, rx, knob.LowerRightCorner.Y - knobH * 0.2f), panelWithAlpha(gripShine, 90));
    }
    batch.rect(core::rect<f32>(cx - knobW * 0.22f, yKnob - core::max_(0.8f, knobH * 0.07f), cx + knobW * 0.22f, yKnob + core::max_(0.8f, knobH * 0.07f)), indexCol);
    //Direction lamp on the grip: green ahead, red astern, dark at stop
    const video::SColor lamp = (Pos < 0) ? ahead : ((Pos > 0) ? astern : video::SColor(255, 40, 42, 46));
    batch.disc(core::vector2df(knob.UpperLeftCorner.X + knobW * 0.12f, yKnob), core::max_(2.0f, knobH * 0.16f), lamp, panelWithAlpha(lamp, 200));

    batch.flush();

    if (font) {
        //Engraved AV / STOP / AR beside the scale
        const core::dimension2du dAv = font->getDimension(TXT_AHEAD);
        const f32 lx = x0 + w * 0.04f;
        font->draw(TXT_AHEAD, core::rect<s32>((s32)lx, (s32)(yTopTravel - 2), (s32)(cx - tickIn), (s32)(yTopTravel + dAv.Height)), engraveDim, false, false, &AbsoluteClippingRect);
        font->draw(TXT_ASTERN, core::rect<s32>((s32)lx, (s32)(yBotTravel - dAv.Height), (s32)(cx - tickIn), (s32)(yBotTravel + 2)), engraveDim, false, false, &AbsoluteClippingRect);

        //Readout: direction and percent ordered
        const s32 percent = (s32)floorf(fabsf((f32)Pos) + 0.5f);
        wchar_t buf[24];
        if (percent == 0) { swprintf(buf, 24, L"STOP"); }
        else { swprintf(buf, 24, L"%ls %d", (Pos < 0) ? TXT_AHEAD : TXT_ASTERN, percent); }
        const core::dimension2du d = font->getDimension(buf);
        video::SColor col = (Pos == 0) ? lcdOff : ((Pos < 0) ? ahead : astern);
        if (day && Pos != 0) { col = (Pos < 0) ? video::SColor(255, 10, 110, 40) : video::SColor(255, 170, 20, 20); }
        const f32 ty = (lcd.UpperLeftCorner.Y + lcd.LowerRightCorner.Y - (f32)d.Height) * 0.5f;
        const core::rect<s32> tr((s32)(cx - d.Width * 0.5f), (s32)ty, (s32)(cx + d.Width * 0.5f) + 2, (s32)ty + (s32)d.Height + 2);
        font->draw(buf, tr, col, false, false, &AbsoluteClippingRect);
    }

    IGUIElement::draw();
}

} // end namespace gui
} // end namespace irr
