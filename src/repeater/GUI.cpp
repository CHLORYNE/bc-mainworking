/*   Bridge Command 5.0 Ship Simulator
     Copyright (C) 2015 James Packer

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation

     This program is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY Or FITNESS For A PARTICULAR PURPOSE.  See the
     GNU General Public License For more details.

     You should have received a copy of the GNU General Public License along
     with this program; if not, write to the Free Software Foundation, Inc.,
     51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. */

#include "GUI.hpp"
#include "../Constants.hpp"
#include "../Utilities.hpp"

#include <iostream>
#include <limits>
#include <string>
#include <cmath>     // KYARA: sinf/cosf/sqrtf/floorf for the compass repeater
#include <cstdio>    // KYARA: snprintf
#include <cstring>   // KYARA: strlen
#include <cwchar>    // KYARA: swprintf

     //using namespace irr;

GUIMain::GUIMain(irr::IrrlichtDevice* device, Lang* language, irr::core::stringw message)
{
    this->device = device;
    guienv = device->getGUIEnvironment();

    irr::video::IVideoDriver* driver = device->getVideoDriver();
    irr::u32 su = driver->getScreenSize().Width;
    irr::u32 sh = driver->getScreenSize().Height;

    this->language = language;

    //gui

    messageText = guienv->addStaticText(message.c_str(), irr::core::rect<irr::s32>(su * 0.0, sh * 0.0, su * 0.9, sh * 0.1));

    //choice buttons
    headingButton = guienv->addButton(irr::core::rect<irr::s32>(su * 0.1, sh * 0.1, su * 0.9, sh * 0.5), 0, GUI_ID_HEADING_CHOICE, language->translate("headingIndicator").c_str());
    repeaterButton = guienv->addButton(irr::core::rect<irr::s32>(su * 0.1, sh * 0.5, su * 0.9, sh * 0.9), 0, GUI_ID_REPEATER_CHOICE, language->translate("repeater").c_str());

    //Heading indicator
    heading = new irr::gui::HeadingIndicator(guienv, guienv->getRootGUIElement(), irr::core::rect<irr::s32>(10, 10, su - 10, 10 + (su - 20) / 4));
    heading->setVisible(false);

    //User hasn't selected what mode to use
    modeChosen = false;

    nightButton = 0;   //KYARA: created lazily the first time the compass is drawn

}

GUIMain::~GUIMain()
{
    heading->drop();
}

void GUIMain::updateGuiData(irr::f32 time, irr::f32 ownShipHeading, irr::f32 rudderAngle, irr::f32 wheelAngle, irr::s32 portEngineRPM, irr::s32 stbdEngineRPM)
{

    if (!modeChosen) {
        //Show GUI choice buttons only
        if (nightButton) { nightButton->setVisible(false); }

    }
    else if (showHeadingIndicator) {

        irr::f32 angleScale = 2.0; //Magnification factor for indicator

        heading->setVisible(true);

        //hide choice buttons
        headingButton->setVisible(false);
        repeaterButton->setVisible(false);
        messageText->setVisible(false);
        if (nightButton) { nightButton->setVisible(false); }   //KYARA: only shown on the compass

        //Set value
        heading->setHeading(ownShipHeading);


        //Draw rudder angle
        irr::video::IVideoDriver* driver = device->getVideoDriver();
        irr::u32 su = driver->getScreenSize().Width;
        irr::u32 sh = driver->getScreenSize().Height;
        irr::core::vector2d<irr::s32> rudderIndicatorCentre = irr::core::vector2d<irr::s32>(0.5 * su, 0.5 * sh);
        irr::core::vector2d<irr::s32> rudderAngleVectorHead = irr::core::vector2d<irr::s32>(0, 0.240 * sh);
        irr::core::vector2d<irr::s32> wheelAngleVectorHead = irr::core::vector2d<irr::s32>(0, 0.240 * sh);
        irr::core::vector2d<irr::s32> rudderAngleVectorBack1 = irr::core::vector2d<irr::s32>(-0.040 * sh, -0.040 * sh);
        irr::core::vector2d<irr::s32> rudderAngleVectorBack2 = irr::core::vector2d<irr::s32>(0.040 * sh, -0.040 * sh);
        rudderAngleVectorHead.rotateBy(-1 * rudderAngle * angleScale);
        wheelAngleVectorHead.rotateBy(-1 * wheelAngle * angleScale);
        rudderAngleVectorBack1.rotateBy(-1 * rudderAngle * angleScale);
        rudderAngleVectorBack2.rotateBy(-1 * rudderAngle * angleScale);
        driver->draw2DLine(rudderIndicatorCentre + rudderAngleVectorBack1, rudderIndicatorCentre + rudderAngleVectorHead, irr::video::SColor(255, 0, 0, 0));
        driver->draw2DLine(rudderIndicatorCentre + rudderAngleVectorHead, rudderIndicatorCentre + rudderAngleVectorBack2, irr::video::SColor(255, 0, 0, 0));
        driver->draw2DLine(rudderIndicatorCentre + rudderAngleVectorBack2, rudderIndicatorCentre + rudderAngleVectorBack1, irr::video::SColor(255, 0, 0, 0));
        driver->draw2DLine(rudderIndicatorCentre, rudderIndicatorCentre + wheelAngleVectorHead, irr::video::SColor(200, 0, 0, 0));
        driver->draw2DPolygon(rudderIndicatorCentre, 0.01 * sh, irr::video::SColor(255, 0, 0, 0));

        //Draw scale
        for (int i = -30; i <= 30; i += 5) {

            irr::s32 centreX = rudderIndicatorCentre.X;
            irr::s32 centreY = rudderIndicatorCentre.Y;
            irr::s32 radius = 0.25 * sh;

            //Draw compass bearings
            irr::f32 xVector = sin(i * irr::core::DEGTORAD * angleScale);
            irr::f32 yVector = cos(i * irr::core::DEGTORAD * angleScale);

            //set scale of line
            irr::f32 lineEnd;
            bool printAngle = false;
            if (i % 10 == 0) {
                lineEnd = 1.1;
                printAngle = true;
            }
            else if (i % 5 == 0) {
                lineEnd = 1.05;
            }

            irr::s32 startX = centreX + xVector * radius;
            irr::s32 endX = centreX + lineEnd * xVector * radius;
            irr::s32 startY = centreY + yVector * radius;
            irr::s32 endY = centreY + lineEnd * yVector * radius;

            //Set colour
            irr::video::SColor indicatorColour;
            if (i < 0) {
                indicatorColour = irr::video::SColor(255, 175, 0, 0);
            }
            else if (i > 0) {
                indicatorColour = irr::video::SColor(255, 0, 175, 0);
            }
            else {
                indicatorColour = irr::video::SColor(255, 0, 0, 0);
            }

            driver->draw2DLine(irr::core::vector2d<irr::s32>(startX, startY), irr::core::vector2d<irr::s32>(endX, endY), indicatorColour);
            if (printAngle) {

                irr::core::stringw text;
                text = irr::core::stringw(abs(i));

                irr::s32 textWidth = guienv->getSkin()->getFont()->getDimension(text.c_str()).Width;
                irr::s32 textHeight = guienv->getSkin()->getFont()->getDimension(text.c_str()).Height;
                irr::s32 textStartX = centreX + 1.2 * xVector * radius - 0.5 * textWidth;
                irr::s32 textEndX = textStartX + textWidth;
                irr::s32 textStartY = centreY + 1.2 * yVector * radius - 0.5 * textHeight;
                irr::s32 textEndY = textStartY + textHeight;
                guienv->getSkin()->getFont()->draw(text, irr::core::rect<irr::s32>(textStartX, textStartY, textEndX, textEndY), indicatorColour);
            }
        }
        //End of draw scale

        //Show engine RPM
        irr::core::stringw portRPM(portEngineRPM);
        irr::core::stringw stbdRPM(stbdEngineRPM);
        guienv->getSkin()->getFont()->draw(portRPM, irr::core::rect<irr::s32>(0.25 * su, 0.515 * sh, 0.5 * su, 0.55 * sh), irr::video::SColor(255, 0, 0, 0));
        guienv->getSkin()->getFont()->draw(stbdRPM, irr::core::rect<irr::s32>(0.75 * su, 0.515 * sh, 1.0 * su, 0.55 * sh), irr::video::SColor(255, 0, 0, 0));



    }
    else {

        //hide choice buttons
        headingButton->setVisible(false);
        repeaterButton->setVisible(false);
        messageText->setVisible(false);

        //KYARA: professional gyro-repeater compass card (replaces the old flat rose).
        drawCompassRepeater(ownShipHeading);
    }
    guienv->drawAll();

}

void GUIMain::setMode(bool headingMode)
{
    modeChosen = true;
    showHeadingIndicator = headingMode;
}

std::wstring GUIMain::f32To1dp(irr::f32 value)
{
    //Convert a floating point value to a wstring, with 1dp
    char tempStr[100];
    snprintf(tempStr, 100, "%.1f", value);
    return std::wstring(tempStr, tempStr + strlen(tempStr));
}

std::wstring GUIMain::f32To3dp(irr::f32 value)
{
    //Convert a floating point value to a wstring, with 3dp
    char tempStr[100];
    snprintf(tempStr, 100, "%.3f", value);
    return std::wstring(tempStr, tempStr + strlen(tempStr));
}

// =====================================================================================
// KYARA: Marine gyro-repeater compass card
// -------------------------------------------------------------------------------------
// A heading-up rotating card (the graduation under the fixed top lubber index reads the
// ship's head) on a dark instrument field, with a large seven-segment digital readout.
// The seven-segment digits are drawn from filled rectangles, so the HDG number can be as
// large as we like without depending on the loaded bitmap font's fixed pixel size.
// Everything is sized from screen height, so it scales to any resolution / repeater panel.
// =====================================================================================

// Draw a line of the given pixel width by stacking parallel 1px lines. Irrlicht's
// draw2DLine is single-pixel, so this is how we get readable, non-hairline graduations.
void GUIMain::drawThickLine(irr::video::IVideoDriver* driver,
    irr::core::vector2df a, irr::core::vector2df b,
    irr::f32 width, irr::video::SColor colour)
{
    irr::f32 dx = b.X - a.X, dy = b.Y - a.Y;
    irr::f32 len = sqrtf(dx * dx + dy * dy);
    if (len < 0.0001f) { return; }
    irr::f32 px = -dy / len, py = dx / len;      // unit perpendicular
    irr::s32 w = (irr::s32)(width + 0.5f); if (w < 1) { w = 1; }
    for (irr::s32 k = 0; k < w; ++k) {
        irr::f32 off = (irr::f32)k - (w - 1) * 0.5f;
        driver->draw2DLine(
            irr::core::vector2d<irr::s32>((irr::s32)(a.X + px * off + 0.5f), (irr::s32)(a.Y + py * off + 0.5f)),
            irr::core::vector2d<irr::s32>((irr::s32)(b.X + px * off + 0.5f), (irr::s32)(b.Y + py * off + 0.5f)),
            colour);
    }
}

// One seven-segment character (digits 0-9) inside a box, using filled rectangles.
//   A          segment layout          A = top      D = bottom   G = middle
//  F B         F/B = upper sides       C/E = lower sides
//   G
//  E C
//   D
void GUIMain::drawSevenSegChar(irr::video::IVideoDriver* driver, wchar_t c,
    irr::core::rect<irr::s32> box, irr::s32 t,
    irr::video::SColor colour)
{
    if (c < L'0' || c > L'9') { return; }
    const int d = c - L'0';
    // segment on/off table, order A,B,C,D,E,F,G
    static const bool seg[10][7] = {
        {1,1,1,1,1,1,0}, //0
        {0,1,1,0,0,0,0}, //1
        {1,1,0,1,1,0,1}, //2
        {1,1,1,1,0,0,1}, //3
        {0,1,1,0,0,1,1}, //4
        {1,0,1,1,0,1,1}, //5
        {1,0,1,1,1,1,1}, //6
        {1,1,1,0,0,0,0}, //7
        {1,1,1,1,1,1,1}, //8
        {1,1,1,1,0,1,1}  //9
    };
    const irr::s32 x0 = box.UpperLeftCorner.X, y0 = box.UpperLeftCorner.Y;
    const irr::s32 x1 = box.LowerRightCorner.X, y1 = box.LowerRightCorner.Y;
    const irr::s32 xl = x0 + t / 2, xr = x1 - t / 2;   // left / right column centres
    const irr::s32 yt = y0 + t / 2, yb = y1 - t / 2;   // top / bottom row centres
    const irr::s32 ym = (y0 + y1) / 2;                 // middle row centre
    const irr::s32 g = t / 3 + 1;                      // gap so segments don't merge at corners
    const irr::s32 th = t - t / 2;                     // half-thickness (upper side)

    // horizontal segment centred on cy, spanning the column centres
#define HSEG(cy) driver->draw2DRectangle(colour, irr::core::rect<irr::s32>(xl - t/2 + g, (cy) - t/2, xr + th - g, (cy) + th))
// vertical segment centred on cx, from ya to yb2
#define VSEG(cx, ya, yb2) driver->draw2DRectangle(colour, irr::core::rect<irr::s32>((cx) - t/2, (ya) + g, (cx) + th, (yb2) - g))

    if (seg[d][0]) { HSEG(yt); }              // A
    if (seg[d][1]) { VSEG(xr, yt, ym); }      // B
    if (seg[d][2]) { VSEG(xr, ym, yb); }      // C
    if (seg[d][3]) { HSEG(yb); }              // D
    if (seg[d][4]) { VSEG(xl, ym, yb); }      // E
    if (seg[d][5]) { VSEG(xl, yt, ym); }      // F
    if (seg[d][6]) { HSEG(ym); }              // G
#undef HSEG
#undef VSEG
}

// Width in pixels a seven-segment string will occupy at a given digit height.
irr::s32 GUIMain::sevenSegWidth(const std::wstring& s, irr::s32 digitH)
{
    const irr::s32 digitW = (irr::s32)(digitH * 0.60f);
    irr::s32 t = (irr::s32)(digitH * 0.15f); if (t < 2) { t = 2; }
    const irr::s32 spacing = (irr::s32)(digitH * 0.16f);
    irr::s32 w = 0;
    for (wchar_t c : s) {
        if (c == L'.') { w += t + spacing; }
        else if (c == L' ') { w += digitW / 2 + spacing; }
        else { w += digitW + spacing; }
    }
    if (w > 0) { w -= spacing; }
    return w;
}

// Draw a seven-segment string (digits, '.', ' ') with its top-left at topLeft.
void GUIMain::drawSevenSegString(irr::video::IVideoDriver* driver, const std::wstring& s,
    irr::core::vector2d<irr::s32> topLeft, irr::s32 digitH,
    irr::video::SColor colour)
{
    const irr::s32 digitW = (irr::s32)(digitH * 0.60f);
    irr::s32 t = (irr::s32)(digitH * 0.15f); if (t < 2) { t = 2; }
    const irr::s32 spacing = (irr::s32)(digitH * 0.16f);
    irr::s32 x = topLeft.X;
    const irr::s32 y = topLeft.Y;
    for (wchar_t c : s) {
        if (c == L'.') {
            driver->draw2DRectangle(colour,
                irr::core::rect<irr::s32>(x, y + digitH - t, x + t, y + digitH));
            x += t + spacing;
        }
        else if (c == L' ') {
            x += digitW / 2 + spacing;
        }
        else {
            drawSevenSegChar(driver, c,
                irr::core::rect<irr::s32>(x, y, x + digitW, y + digitH), t, colour);
            x += digitW + spacing;
        }
    }
}

// 16-point compass name for a heading in degrees.
std::wstring GUIMain::compassPointName(irr::f32 heading)
{
    static const wchar_t* pts[16] = {
        L"N", L"NNE", L"NE", L"ENE", L"E", L"ESE", L"SE", L"SSE",
        L"S", L"SSW", L"SW", L"WSW", L"W", L"WNW", L"NW", L"NNW"
    };
    int idx = (int)floorf(heading / 22.5f + 0.5f);
    idx = ((idx % 16) + 16) % 16;
    return std::wstring(pts[idx]);
}

// Filled disc via horizontal scanline rectangles (Irrlicht 2D only fills rectangles).
void GUIMain::fillCircle(irr::video::IVideoDriver* driver, irr::s32 cx, irr::s32 cy,
    irr::s32 r, irr::video::SColor colour)
{
    if (r <= 0) { return; }
    for (irr::s32 yy = -r; yy <= r; ++yy) {
        irr::s32 dx = (irr::s32)(sqrtf((irr::f32)(r * r - yy * yy)) + 0.5f);
        driver->draw2DRectangle(colour,
            irr::core::rect<irr::s32>(cx - dx, cy + yy, cx + dx + 1, cy + yy + 1));
    }
}

// Filled triangle via scanlines - each row is a rectangle between the two edge crossings.
void GUIMain::fillTriangle(irr::video::IVideoDriver* driver,
    irr::core::vector2df a, irr::core::vector2df b, irr::core::vector2df c,
    irr::video::SColor colour)
{
    irr::f32 ymin = a.Y; if (b.Y < ymin) ymin = b.Y; if (c.Y < ymin) ymin = c.Y;
    irr::f32 ymax = a.Y; if (b.Y > ymax) ymax = b.Y; if (c.Y > ymax) ymax = c.Y;
    const irr::core::vector2df P[3] = { a, b, c };
    for (irr::s32 y = (irr::s32)ymin; y <= (irr::s32)ymax; ++y) {
        irr::f32 xs[3]; int n = 0;
        const irr::f32 fy = (irr::f32)y + 0.5f;
        for (int e = 0; e < 3; ++e) {
            const irr::core::vector2df& A = P[e];
            const irr::core::vector2df& B = P[(e + 1) % 3];
            if ((A.Y <= fy && B.Y > fy) || (B.Y <= fy && A.Y > fy)) {
                irr::f32 t = (fy - A.Y) / (B.Y - A.Y);
                xs[n++] = A.X + t * (B.X - A.X);
            }
        }
        if (n >= 2) {
            irr::f32 xl = xs[0], xr = xs[0];
            for (int k = 1; k < n; ++k) { if (xs[k] < xl) xl = xs[k]; if (xs[k] > xr) xr = xs[k]; }
            driver->draw2DRectangle(colour,
                irr::core::rect<irr::s32>((irr::s32)xl, y, (irr::s32)xr + 1, y + 1));
        }
    }
}

// =====================================================================================
// KYARA: ONWA-style heading repeater, with a day (white face) and night (dark, red-lit)
// palette. The palette is switched by the on-screen DAY/NIGHT button (a push button
// polled with isPressed(), the same self-contained pattern used elsewhere - no
// EventReceiver wiring needed), echoing the up/down buttons on the real instrument.
// Heading-up: the graduation under the fixed top lubber index is the ship's head.
// =====================================================================================
void GUIMain::drawCompassRepeater(irr::f32 heading)
{
    irr::video::IVideoDriver* driver = device->getVideoDriver();
    if (!driver) { return; }
    irr::gui::IGUIFont* font = guienv->getSkin() ? guienv->getSkin()->getFont() : 0;

    const irr::s32 sw = (irr::s32)driver->getScreenSize().Width;
    const irr::s32 sh = (irr::s32)driver->getScreenSize().Height;
    const irr::s32 cx = sw / 2;
    const irr::s32 cy = sh / 2;
    const irr::f32 R = (irr::f32)((sw < sh ? sw : sh)) * 0.46f;   // card radius

    while (heading < 0.0f) { heading += 360.0f; }
    while (heading >= 360.0f) { heading -= 360.0f; }

    // --- DAY / NIGHT toggle button (bottom-left, like the real unit's buttons) -----------
    const irr::s32 bw = (irr::s32)(R * 0.34f);
    const irr::s32 bh = (irr::s32)(R * 0.16f);
    const irr::s32 bx = (irr::s32)(R * 0.10f);
    const irr::s32 by = sh - bh - (irr::s32)(R * 0.10f);
    const irr::core::rect<irr::s32> btnRect(bx, by, bx + bw, by + bh);
    if (!nightButton) {
        nightButton = guienv->addButton(btnRect, 0, GUI_ID_NIGHT_TOGGLE, L"DAY");
        nightButton->setIsPushButton(true);
    }
    else {
        nightButton->setRelativePosition(btnRect);
    }
    nightButton->setVisible(true);
    const bool night = nightButton->isPressed();
    nightButton->setText(night ? L"NIGHT" : L"DAY");

    // --- palette -------------------------------------------------------------------------
    struct Pal {
        irr::video::SColor bezel, card, field, band, tMinor, t5, t10, num,
            starMain, starSec, starNorth, hub, letter, lubber, led, ledBox, ledEdge;
    } p;
    if (!night) {   // DAY: white face, red band, red LED (ONWA look)
        p.bezel = irr::video::SColor(255, 44, 45, 49);
        p.card = irr::video::SColor(255, 176, 167, 148);
        p.field = irr::video::SColor(255, 163, 148, 120);
        p.band = irr::video::SColor(255, 125, 122, 104);
        p.tMinor = irr::video::SColor(255, 95, 95, 95);
        p.t5 = irr::video::SColor(255, 60, 60, 60);
        p.t10 = irr::video::SColor(255, 28, 28, 28);
        p.num = irr::video::SColor(255, 28, 28, 28);
        p.starMain = irr::video::SColor(255, 247, 245, 239);
        p.starSec = irr::video::SColor(255, 225, 223, 217);
        p.starNorth = irr::video::SColor(255, 20, 20, 20);

        //hub to change the circle in the center 
        p.hub = irr::video::SColor(255, 20, 20, 23);
        p.letter = irr::video::SColor(255, 24, 24, 24);
        p.lubber = irr::video::SColor(255, 20, 20, 20);
        p.led = irr::video::SColor(255, 255, 59, 48);
        p.ledBox = irr::video::SColor(255, 10, 10, 10);
        p.ledEdge = irr::video::SColor(255, 0, 0, 0);
    }
    else {        // NIGHT: dark face, dim red graduations, red LED (night vision)
        p.bezel = irr::video::SColor(255, 8, 8, 10);
        p.card = irr::video::SColor(255, 16, 15, 15);
        p.field = irr::video::SColor(255, 30, 26, 26);
        p.band = irr::video::SColor(255, 158, 32, 0);
        p.tMinor = irr::video::SColor(255, 95, 45, 42);
        p.t5 = irr::video::SColor(255, 130, 55, 50);
        p.t10 = irr::video::SColor(255, 180, 82, 74);
        p.num = irr::video::SColor(255, 185, 88, 78);
        p.starMain = irr::video::SColor(255, 44, 34, 33);
        p.starSec = irr::video::SColor(255, 32, 26, 26);
        p.starNorth = irr::video::SColor(255, 150, 60, 54);
        p.hub = irr::video::SColor(255, 8, 8, 10);
        p.letter = irr::video::SColor(255, 255, 255, 255);
        p.lubber = irr::video::SColor(255, 210, 120, 60);
        p.led = irr::video::SColor(255, 232, 54, 44);
        p.ledBox = irr::video::SColor(255, 6, 6, 7);
        p.ledEdge = irr::video::SColor(255, 0, 0, 0);
    }

    // --- face: surround, white/dark card, red band, inner field --------------------------
    driver->draw2DRectangle(p.bezel, irr::core::rect<irr::s32>(0, 0, sw, sh));
    fillCircle(driver, cx, cy, (irr::s32)R, p.card);
    fillCircle(driver, cx, cy, (irr::s32)(R * 0.80f), p.band);    // red band ...
    fillCircle(driver, cx, cy, (irr::s32)(R * 0.75f), p.field);   // ... over the inner field

    // --- graduated ring + upright numerals ----------------------------------------------
    for (int i = 0; i < 360; ++i) {
        const irr::f32 a = ((irr::f32)i - heading) * irr::core::DEGTORAD;   // 0 = up
        const irr::f32 ux = sinf(a);
        const irr::f32 uy = -cosf(a);
        const bool is10 = (i % 10 == 0);
        const bool is5 = (i % 5 == 0);

        irr::f32 rin, wth; irr::video::SColor col;
        if (is10) { rin = 0.88f; wth = 2.6f; col = p.t10; }
        else if (is5) { rin = 0.91f; wth = 1.8f; col = p.t5; }
        else { rin = 0.94f; wth = 1.0f; col = p.tMinor; }

        drawThickLine(driver,
            irr::core::vector2df(cx + ux * R * rin, cy + uy * R * rin),
            irr::core::vector2df(cx + ux * R * 0.995f, cy + uy * R * 0.995f), wth, col);

        if (font && is10) {
            wchar_t buf[8];
            swprintf(buf, 8, L"%03d", i);
            irr::core::dimension2du dim = font->getDimension(buf);
            irr::s32 lx = cx + (irr::s32)(ux * R * 0.845f);
            irr::s32 ly = cy + (irr::s32)(uy * R * 0.845f);
            font->draw(buf,
                irr::core::rect<irr::s32>(lx - (irr::s32)dim.Width / 2, ly - (irr::s32)dim.Height / 2,
                    lx + (irr::s32)dim.Width / 2, ly + (irr::s32)dim.Height / 2), p.num);
        }
    }

    // --- compass-rose star: 8 short + 8 long points, North point emphasised --------------
    const irr::f32 Rhub = R * 0.30f;
    for (int k = 0; k < 16; ++k) {
        const bool primary = (k % 2 == 0);              // 0,45,90,... are primary (long)
        const irr::f32 bearing = (irr::f32)k * 22.5f;
        const irr::f32 apexR = primary ? R * 0.66f : R * 0.51f;
        const irr::f32 beta = 8.0f;                     // half-width of the point, degrees
        const irr::f32 aA = (bearing - heading) * irr::core::DEGTORAD;
        const irr::f32 aL = (bearing - beta - heading) * irr::core::DEGTORAD;
        const irr::f32 aR = (bearing + beta - heading) * irr::core::DEGTORAD;
        irr::core::vector2df apex(cx + sinf(aA) * apexR, cy - cosf(aA) * apexR);
        irr::core::vector2df bl(cx + sinf(aL) * Rhub, cy - cosf(aL) * Rhub);
        irr::core::vector2df br(cx + sinf(aR) * Rhub, cy - cosf(aR) * Rhub);
        irr::video::SColor fill = primary ? p.starMain : p.starSec;
        if (k == 0) { fill = p.starNorth; }             // North point stands out
        fillTriangle(driver, apex, bl, br, fill);
    }

    // --- dark centre hub -----------------------------------------------------------------
    fillCircle(driver, cx, cy, (irr::s32)Rhub, p.hub);

    // --- cardinal / intercardinal letters (upright, rotate position with the card) -------
    if (font) {
        const wchar_t* L8[8] = { L"N", L"NE", L"E", L"SE", L"S", L"SW", L"W", L"NW" };
        for (int k = 0; k < 8; ++k) {
            const irr::f32 aL = ((irr::f32)k * 45.0f - heading) * irr::core::DEGTORAD;
            irr::s32 lx = cx + (irr::s32)(sinf(aL) * R * 0.44f);
            irr::s32 ly = cy - (irr::s32)(cosf(aL) * R * 0.44f);
            irr::core::dimension2du dim = font->getDimension(L8[k]);
            font->draw(L8[k],
                irr::core::rect<irr::s32>(lx - (irr::s32)dim.Width / 2, ly - (irr::s32)dim.Height / 2,
                    lx + (irr::s32)dim.Width / 2, ly + (irr::s32)dim.Height / 2), p.letter);
        }
    }

    // --- fixed lubber index at the very top ----------------------------------------------
    fillTriangle(driver,
        irr::core::vector2df((irr::f32)cx - R * 0.030f, cy - R * 1.00f),
        irr::core::vector2df((irr::f32)cx + R * 0.030f, cy - R * 1.00f),
        irr::core::vector2df((irr::f32)cx, cy - R * 0.86f), p.lubber);

    // --- red LED heading readout in the hub ---------------------------------------------
    {
        char asc[16];
        snprintf(asc, sizeof(asc), "%05.1f", heading);   // e.g. 047.5
        std::wstring hs(asc, asc + strlen(asc));

        const irr::s32 digitH = (irr::s32)(Rhub * 0.62f);
        const irr::s32 numW = sevenSegWidth(hs, digitH);
        const irr::s32 x0 = cx - numW / 2;
        const irr::s32 y0 = cy - digitH / 2;
        const irr::s32 padX = (irr::s32)(digitH * 0.30f);
        const irr::s32 padY = (irr::s32)(digitH * 0.26f);
        irr::core::rect<irr::s32> boxR(x0 - padX, y0 - padY, x0 + numW + padX, y0 + digitH + padY);
        driver->draw2DRectangle(p.ledBox, boxR);
        driver->draw2DRectangleOutline(boxR, p.ledEdge);
        drawSevenSegString(driver, hs, irr::core::vector2d<irr::s32>(x0, y0), digitH, p.led);
    }
}