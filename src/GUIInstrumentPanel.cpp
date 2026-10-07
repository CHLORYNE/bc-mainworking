/*   NAUTITECH - Simulateur de Navigation
     Instrument console for the main bridge view (Bridge Command fork).

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#include "GUIInstrumentPanel.hpp"
#include "BridgeSkin.hpp"

#include "IGUISkin.h"
#include "IGUIFont.h"
#include "SMaterial.h"
#include <cmath>
#include <cwchar>
#include <cstdio>

namespace irr
{
    namespace gui
    {

        namespace
        {
            //---------------------------------------------------------------------------------------------
            //KYARA: every label the console shows, in one place. Change the wording here only.
            const wchar_t* TXT_RUDDER = L"BARRE";
            const wchar_t* TXT_COMPASS = L"CAP";
            const wchar_t* TXT_ROT = L"GIRATION";
            const wchar_t* TXT_LOG = L"LOCH";
            const wchar_t* TXT_GPS = L"GPS";
            const wchar_t* TXT_NAV = L"NAV";
            const wchar_t* TXT_SOUNDER = L"SONDEUR";
            const wchar_t* TXT_TIDE = L"MAR\u00C9E";
            const wchar_t* TXT_PAUSE = L"PAUSE";
            const wchar_t* TXT_KNOTS = L"kn";     //IMO SN.1/Circ.243 unit abbreviation for knots
            const wchar_t* TXT_GROUND = L"SOG";    //IMO abbreviation for speed over ground
            const wchar_t* TXT_ORDER = L"ORDRE";  //helm order
            const wchar_t* TXT_PORT = L"BD";     //bâbord
            const wchar_t* TXT_STBD = L"TD";     //tribord
            const wchar_t* TXT_PER_MIN = L"\u00B0/min";
            const wchar_t* TXT_RPM = L"MOTEURS";
            const wchar_t* TXT_RPM_UNIT = L"MOTEURS  tr/min x100";
            const wchar_t* TXT_WIND = L"VENT";
            const wchar_t* TXT_PITCH = L"TANGAGE";            //KYARA HOULE
            const wchar_t* TXT_ROLL = L"G\u00CETE";           //KYARA HOULE (clinometre)
            //Short on purpose: anything longer runs into the scale numbers on a small dial.
            const wchar_t* TXT_BOW_UP = L"HAUT";               //KYARA HOULE (etrave haute)
            const wchar_t* TXT_BOW_DOWN = L"BAS";              //KYARA HOULE (etrave basse)
            const wchar_t* TXT_NO_DIR = L"---\u00B0";  //direction unavailable - dashed out, never guessed
            const wchar_t* TXT_ASTERN = L"AR";     //arri\u00E8re
            const wchar_t* TXT_NO_DEPTH = L"---";
            const wchar_t* CARDINALS[4] = { L"N", L"E", L"S", L"W" };

            //---------------------------------------------------------------------------------------------
            //Palette: day (light dials, dark figures, LCD readouts), dusk (dark dials, white figures) and
            //night (black dials, dim amber), switched with bridge::currentMode() - see applyPalette().
            video::SColor PANEL_TOP, PANEL_BOTTOM, PANEL_EDGE_HI, PANEL_EDGE_LO, BEZEL_TOP, BEZEL_BOTTOM, BEZEL_INNER, FACE_CENTRE;
            video::SColor FACE_EDGE, SCALE_WHITE, TEXT_TITLE, TEXT_DIM, NEEDLE, NEEDLE_SHADOW, HUB_OUTER, HUB_INNER;
            video::SColor BAND_PORT, BAND_STBD, BAND_ASTERN, LUBBER, COG_MARK, ORDER_MARK, LCD_BG, LCD_EDGE;
            video::SColor LCD_HEADER, DIGIT_WHITE, DIGIT_RED, DIGIT_GREEN, DIGIT_AMBER, NAV_LABEL, NAV_VALUE, HULL_MARK;
            video::SColor WIND_BARB, SHIP_HULL, SHIP_HOUSE, SCREW_HI, SCREW_LO, SCREW_SLOT, STATUS_BG;
            //Backlight: 0 by day (printed dial), up to 1 at night - markings, needles and digits glow.
            f32 GLOW = 0.0f;

            void applyPalette(int mode)
            {
                if (mode < 0 || mode > 2) { mode = bridge::MODE_DUSK; }
                const u32 c[][3][3] = {
                    { {227, 232, 238}, {28, 42, 60}, {15, 23, 34} }, //PANEL_TOP
                    { {211, 218, 226}, {22, 33, 48}, {11, 17, 26} }, //PANEL_BOTTOM
                    { {245, 247, 250}, {52, 70, 94}, {29, 40, 54} }, //PANEL_EDGE_HI
                    { {160, 172, 186}, {8, 12, 18}, {3, 5, 8} }, //PANEL_EDGE_LO
                    { {214, 220, 228}, {122, 134, 150}, {52, 58, 68} }, //BEZEL_TOP
                    { {140, 150, 162}, {40, 48, 60}, {18, 22, 28} }, //BEZEL_BOTTOM
                    { {110, 120, 132}, {10, 12, 16}, {4, 5, 7} }, //BEZEL_INNER
                    { {252, 252, 253}, {38, 44, 54}, {26, 19, 12} }, //FACE_CENTRE
                    { {230, 234, 239}, {8, 10, 14}, {2, 3, 4} }, //FACE_EDGE
                    { {20, 26, 34}, {246, 249, 253}, {255, 180, 76} }, //SCALE_WHITE
                    { {52, 62, 74}, {222, 228, 238}, {236, 162, 78} }, //TEXT_TITLE
                    { {96, 108, 122}, {150, 162, 176}, {168, 120, 66} }, //TEXT_DIM
                    { {224, 82, 0}, {255, 150, 24}, {255, 118, 28} }, //NEEDLE
                    { {120, 128, 138}, {58, 61, 68}, {40, 36, 30} }, //HUB_OUTER
                    { {40, 44, 50}, {20, 21, 24}, {10, 9, 8} }, //HUB_INNER
                    { {210, 40, 40}, {236, 56, 50}, {214, 56, 42} }, //BAND_PORT
                    { {30, 150, 70}, {52, 210, 92}, {60, 180, 84} }, //BAND_STBD
                    { {200, 50, 45}, {226, 56, 52}, {204, 54, 42} }, //BAND_ASTERN
                    { {210, 30, 25}, {255, 64, 48}, {255, 74, 44} }, //LUBBER
                    { {0, 120, 210}, {90, 214, 255}, {96, 184, 236} }, //COG_MARK
                    { {220, 150, 0}, {255, 222, 64}, {255, 204, 80} }, //ORDER_MARK
                    { {186, 199, 180}, {7, 10, 12}, {8, 6, 3} }, //LCD_BG
                    { {120, 132, 118}, {64, 70, 78}, {40, 32, 22} }, //LCD_EDGE
                    { {170, 184, 164}, {26, 34, 40}, {18, 13, 8} }, //LCD_HEADER
                    { {22, 28, 24}, {246, 250, 255}, {255, 190, 84} }, //DIGIT_WHITE
                    { {170, 20, 20}, {255, 96, 82}, {255, 98, 66} }, //DIGIT_RED
                    { {10, 110, 40}, {90, 240, 130}, {130, 226, 104} }, //DIGIT_GREEN
                    { {22, 28, 24}, {255, 200, 72}, {255, 196, 86} }, //DIGIT_AMBER
                    { {56, 78, 68}, {132, 196, 216}, {206, 148, 80} }, //NAV_LABEL
                    { {20, 28, 24}, {238, 246, 250}, {255, 190, 100} }, //NAV_VALUE
                    { {160, 168, 178}, {92, 99, 110}, {60, 52, 40} }, //HULL_MARK
                    { {0, 110, 200}, {140, 222, 255}, {255, 178, 76} }, //WIND_BARB
                    { {96, 104, 114}, {166, 174, 186}, {150, 118, 76} }, //SHIP_HULL
                    { {62, 68, 76}, {220, 226, 234}, {200, 156, 98} }, //SHIP_HOUSE
                    { {244, 246, 248}, {130, 138, 150}, {60, 62, 66} }, //SCREW_HI
                    { {150, 158, 168}, {50, 56, 66}, {20, 22, 26} }, //SCREW_LO
                    { {110, 118, 128}, {24, 28, 34}, {8, 9, 10} }, //SCREW_SLOT
                    { {200, 207, 215}, {14, 22, 34}, {7, 11, 17} }, //STATUS_BG
                };
                video::SColor* target[] = { &PANEL_TOP, &PANEL_BOTTOM, &PANEL_EDGE_HI, &PANEL_EDGE_LO, &BEZEL_TOP, &BEZEL_BOTTOM, &BEZEL_INNER, &FACE_CENTRE, &FACE_EDGE, &SCALE_WHITE, &TEXT_TITLE, &TEXT_DIM, &NEEDLE, &HUB_OUTER, &HUB_INNER, &BAND_PORT, &BAND_STBD, &BAND_ASTERN, &LUBBER, &COG_MARK, &ORDER_MARK, &LCD_BG, &LCD_EDGE, &LCD_HEADER, &DIGIT_WHITE, &DIGIT_RED, &DIGIT_GREEN, &DIGIT_AMBER, &NAV_LABEL, &NAV_VALUE, &HULL_MARK, &WIND_BARB, &SHIP_HULL, &SHIP_HOUSE, &SCREW_HI, &SCREW_LO, &SCREW_SLOT, &STATUS_BG };
                for (size_t i = 0; i < sizeof(target) / sizeof(target[0]); i++) {
                    *target[i] = video::SColor(255, c[i][mode][0], c[i][mode][1], c[i][mode][2]);
                }
                NEEDLE_SHADOW = video::SColor(mode == bridge::MODE_DAY ? 50 : 90, 0, 0, 0);
                GLOW = (mode == bridge::MODE_NIGHT) ? 1.0f : (mode == bridge::MODE_DUSK ? 0.7f : 0.0f);
            }

            //Soft light around a stroke from p0 to p1 of width w: fades from 'strength' x GLOW alpha at
            //the stroke's edge to nothing a little way out. Nothing by day.
            void glowHalo(PanelBatch& b, const core::vector2df& p0, const core::vector2df& p1, f32 w, video::SColor col, f32 strength = 1.0f, f32 reach = 0.0f)
            {
                if (GLOW <= 0.0f) { return; }
                core::vector2df d = p1 - p0;
                const f32 len = d.getLength();
                if (len < 1e-4f) { return; }
                d /= len;
                if (reach <= 0.0f) { reach = core::max_(1.5f, w * 1.2f); }
                const core::vector2df n(-d.Y, d.X);
                const f32 hw = w * 0.5f;
                const video::SColor in((u32)core::clamp(GLOW * strength * 120.0f, 0.0f, 255.0f), col.getRed(), col.getGreen(), col.getBlue());
                const video::SColor out(0, col.getRed(), col.getGreen(), col.getBlue());
                const core::vector2df a = p0 - d * reach, e = p1 + d * reach;
                //Both sides and both ends fade out; nothing under the stroke itself.
                b.quad(p0 + n * hw, in, p1 + n * hw, in, p1 + n * (hw + reach), out, p0 + n * (hw + reach), out);
                b.quad(p0 - n * (hw + reach), out, p1 - n * (hw + reach), out, p1 - n * hw, in, p0 - n * hw, in);
                b.quad(a + n * hw, out, p0 + n * hw, in, p0 - n * hw, in, a - n * hw, out);
                b.quad(p1 + n * hw, in, e + n * hw, out, e - n * hw, out, p1 - n * hw, in);
            }

            //A lit marking: the halo, then the stroke itself.
            void glowLine(PanelBatch& b, const core::vector2df& p0, const core::vector2df& p1, f32 w, video::SColor col)
            {
                glowHalo(b, p0, p1, w, col, 0.55f);
                b.line(p0, p1, w, col);
            }

            inline video::SColor ghostOf(video::SColor c) { return video::SColor(20, c.getRed(), c.getGreen(), c.getBlue()); }

            inline f32 norm360(f32 a)
            {
                a = fmodf(a, 360.0f);
                if (a < 0) { a += 360.0f; }
                return a;
            }

            inline f32 valueToAngle(f32 v, f32 vMin, f32 vMax, f32 a0, f32 a1)
            {
                const f32 t = (vMax > vMin) ? (v - vMin) / (vMax - vMin) : 0.0f;
                return a0 + t * (a1 - a0);
            }

            //1, 2, 2.5, 5, 10 x 10^n - the smallest "round" step >= x.
            f32 niceStep(f32 x)
            {
                if (x <= 0) { return 1.0f; }
                const f32 p = powf(10.0f, floorf(log10f(x)));
                const f32 m = x / p;
                f32 n = 10.0f;
                if (m <= 1.0f) n = 1.0f; else if (m <= 2.0f) n = 2.0f; else if (m <= 2.5f) n = 2.5f; else if (m <= 5.0f) n = 5.0f;
                return n * p;
            }

            //Minor tick spacing that divides a major step into a readable number of ticks.
            f32 minorFor(f32 major)
            {
                const f32 p = powf(10.0f, floorf(log10f(major)));
                const f32 m = major / p;
                if (fabsf(m - 2.0f) < 0.01f) { return major / 4.0f; }
                if (fabsf(m - 2.5f) < 0.01f) { return major / 5.0f; }
                return major / 5.0f;
            }

            //KYARA: with several optional dials on a small screen the face gets tight. When a dial is
            //small relative to the font, the secondary lines (units, sub-readouts) are dropped rather than
            //allowed to overlap. The needle, the scale and the main readout always stay.
            inline bool compactDial(f32 rf, IGUIFont* font)
            {
                const f32 fh = font ? (f32)font->getDimension(L"0Ag").Height : 14.0f;
                return rf < fh * 5.0f;
            }

            //Seven-segment masks, bit0=a (top) .. bit6=g (middle).
            const u8 SEG_MASK[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };
        }

        //=================================================================================================
        // Element
        //=================================================================================================

        GUIInstrumentPanel::GUIInstrumentPanel(IGUIEnvironment* environment, IGUIElement* parent, s32 id, const core::rect<s32>& rectangle)
            : IGUIElement(EGUIET_ELEMENT, environment, parent, id, rectangle),
            fitRudder(true), fitRot(false), fitGPS(true), fitSounder(true), fitTide(false),
            maxSounderDepth(1000.0f), speedMax(20.0f), rotMax(60.0f), rudderMax(35.0f),
            statusWidth(0), gaugeD(0)
        {
            for (int k = 0; k < 4; k++) { for (int b = 0; b < 120; b++) { peakBuckets[k][b] = 0.0f; } } //KYARA HOULE
#ifdef _DEBUG
            setDebugName("GUIInstrumentPanel");
#endif
            setTabStop(false);
            layout();
        }

        GUIInstrumentPanel::~GUIInstrumentPanel()
        {
        }

        void GUIInstrumentPanel::setFit(bool hasRudderIndicator, bool hasRateOfTurn, bool hasGPS, bool hasDepthSounder, f32 maxDepth, bool showTide)
        {
            fitRudder = hasRudderIndicator;
            fitRot = hasRateOfTurn;
            fitGPS = hasGPS;
            fitSounder = hasDepthSounder;
            maxSounderDepth = maxDepth;
            fitTide = showTide;
            layout();
        }

        void GUIInstrumentPanel::setExtraInstruments(bool showRPM, bool showWind, f32 rpmScale, bool oneEngine)
        {
            fitRPM = showRPM;
            fitWind = showWind;
            singleEngine = oneEngine;
            if (rpmScale > 0) { maxRPM = rpmScale; }
            layout();
        }

        void GUIInstrumentPanel::setMotionInstruments(bool show)
        {
            fitMotion = show;
            layout();
        }
        void GUIInstrumentPanel::setHelmControl(bool enabled, f32 maxWheelDeg)
        {
            helmEnabled = enabled;
            if (maxWheelDeg > 0) { helmMaxDeg = maxWheelDeg; }
        }

        bool GUIInstrumentPanel::consumeHelmRequest(f32& wheelDeg)
        {
            if (!helmPending) { return false; }
            wheelDeg = helmRequest;
            helmPending = false;
            return true;
        }

        const GUIInstrumentPanel::Slot* GUIInstrumentPanel::rudderSlot() const
        {
            for (size_t i = 0; i < slots.size(); i++) {
                if (slots[i].kind == G_RUDDER) { return &slots[i]; }
            }
            return 0;
        }

        bool GUIInstrumentPanel::helmAngleFromPoint(const core::position2d<s32>& p, f32& wheelDeg) const
        {
            const Slot* s = rudderSlot();
            if (!s) { return false; }
            const core::vector2df d((f32)p.X - s->c.X, (f32)p.Y - s->c.Y);
            const f32 dist = d.getLength();
            //Ignore the hub (where the angle is meaningless and jumpy) and anything off the dial.
            if (dist > s->R || dist < s->R * 0.15f) { return false; }

            //Same mapping as the dial face: the scale runs a0..a1 across the full rudder range, so where
            //you click on the arc is the angle you order.
            const f32 a0 = -100.0f, a1 = 100.0f;
            f32 a = atan2f(d.X, -d.Y) * core::RADTODEG; //deg clockwise from 12 o'clock
            //The bottom of the face carries no scale - a click there is not a helm order, so it must not
            //clamp to hard over. Only a small overshoot past each end is accepted.
            if (fabsf(a) > 108.0f) { return false; }
            a = panelClamp(a, a0, a1);
            wheelDeg = panelClamp((a - a0) / (a1 - a0) * 2.0f * helmMaxDeg - helmMaxDeg, -helmMaxDeg, helmMaxDeg);
            return true;
        }

        bool GUIInstrumentPanel::isPointInside(const core::position2d<s32>& point) const
        {
            if (!helmEnabled || !IsVisible) { return false; }
            f32 unused;
            return helmAngleFromPoint(point, unused);
        }

        bool GUIInstrumentPanel::OnEvent(const SEvent& event)
        {
            if (!helmEnabled || !IsVisible || !isEnabled()) { return IGUIElement::OnEvent(event); }

            if (event.EventType == EET_MOUSE_INPUT_EVENT) {
                const core::position2d<s32> p(event.MouseInput.X, event.MouseInput.Y);
                switch (event.MouseInput.Event) {
                case EMIE_LMOUSE_PRESSED_DOWN:
                case EMIE_RMOUSE_PRESSED_DOWN:
                {
                    f32 w;
                    if (helmAngleFromPoint(p, w)) {
                        helmDragging = true;
                        helmRequest = w;
                        helmPending = true;
                        Environment->setFocus(this);
                        return true;
                    }
                    break;
                }
                case EMIE_LMOUSE_LEFT_UP:
                case EMIE_RMOUSE_LEFT_UP:
                    helmDragging = false;
                    Environment->removeFocus(this);
                    return true;
                case EMIE_MOUSE_MOVED:
                {
                    if (!event.MouseInput.isLeftPressed() && !event.MouseInput.isRightPressed()) {
                        helmDragging = false;
                        break;
                    }
                    f32 w;
                    if (helmDragging && helmAngleFromPoint(p, w)) {
                        helmRequest = w;
                        helmPending = true;
                        return true;
                    }
                    break;
                }
                default:
                    break;
                }
            }
            else if (event.EventType == EET_GUI_EVENT && event.GUIEvent.EventType == EGET_ELEMENT_FOCUS_LOST
                && event.GUIEvent.Caller == this) {
                helmDragging = false;
            }

            return IGUIElement::OnEvent(event);
        }

        void GUIInstrumentPanel::setScales(f32 speedMaxKn, f32 rotMaxDegMin, f32 rudderMaxDeg)
        {
            if (speedMaxKn > 0) speedMax = speedMaxKn;
            if (rotMaxDegMin > 0) rotMax = rotMaxDegMin;
            if (rudderMaxDeg > 0) rudderMax = rudderMaxDeg;
        }

        void GUIInstrumentPanel::setStatusColumnWidth(s32 widthPx)
        {
            statusWidth = widthPx > 0 ? widthPx : 0;
            layout();
        }

        core::rect<s32> GUIInstrumentPanel::getStatusColumnRect() const
        {
            return statusRect;
        }

        void GUIInstrumentPanel::setData(const InstrumentData& d)
        {
            data = d;
            data.heading = norm360(data.heading);
            data.cog = norm360(data.cog);

            //KYARA HOULE: 2-minute peak markers
            const u32 bucket = d.timeMs / 1000;
            if (!peakStarted || bucket < peakLastBucket || bucket - peakLastBucket >= 120) {
                for (int k = 0; k < 4; k++) { for (int b = 0; b < 120; b++) { peakBuckets[k][b] = 0.0f; } }
                peakStarted = true;
            }
            else {
                for (u32 b = peakLastBucket + 1; b <= bucket; b++) {
                    for (int k = 0; k < 4; k++) { peakBuckets[k][b % 120] = 0.0f; }
                }
            }
            peakLastBucket = bucket;
            const u32 bi = bucket % 120;
            const f32 vals[4] = { d.pitchDeg, -d.pitchDeg, d.rollDeg, -d.rollDeg };
            for (int k = 0; k < 4; k++) {
                if (vals[k] > peakBuckets[k][bi]) { peakBuckets[k][bi] = vals[k]; }
            }
        }

        void GUIInstrumentPanel::updateAbsolutePosition()
        {
            IGUIElement::updateAbsolutePosition();
            layout();
        }

        void GUIInstrumentPanel::layout()
        {
            //KYARA: sized from the panel HEIGHT, positioned by CENTRING. On the Eyefinity canvas the width
            //is three screens, so anything sized from the width would stretch; the dials would turn into
            //ovals. Here the diameter is capped by the height and the whole group (dials + status column)
            //is centred, which puts it on the middle TV - right in front of the trainee.
            slots.clear();

            const f32 W = (f32)AbsoluteRect.getWidth();
            const f32 H = (f32)AbsoluteRect.getHeight();
            if (W <= 0 || H <= 0) { return; }

            std::vector<GaugeKind> kinds;
            if (fitRudder) kinds.push_back(G_RUDDER);
            kinds.push_back(G_COMPASS);
            if (fitRot) kinds.push_back(G_ROT);
            kinds.push_back(G_LOG);
            if (fitRPM) kinds.push_back(G_RPM);
            if (fitWind) kinds.push_back(G_WIND);
            if (fitMotion) { kinds.push_back(G_PITCH); kinds.push_back(G_ROLL); } //KYARA HOULE
            kinds.push_back(G_NAV);

            const f32 NAV_WEIGHT = 1.45f;
            const f32 GAP_K = 0.07f;
            auto weightOf = [&](GaugeKind k) { return (k == G_NAV) ? NAV_WEIGHT : 1.0f; };

            f32 sumWeights = 0;
            for (size_t i = 0; i < kinds.size(); i++) { sumWeights += weightOf(kinds[i]); }

            const f32 pad = core::max_(4.0f, core::min_(H, W * 0.25f) * 0.045f);
            const f32 statusSpace = (statusWidth > 0) ? (f32)statusWidth + 1.5f * pad : 0.0f;
            const f32 availW = W - 2.0f * pad - statusSpace;

            //Rows: split the dials in order, each row holding about the same width of instruments, and
            //keep the row count that gives the biggest diameter. One row unless setMaxRows allows more.
            std::vector<size_t> bestStarts(1, 0);
            f32 D = 0;
            const int rowLimit = core::max_(1, core::min_(maxRows, (int)kinds.size()));
            for (int rows = 1; rows <= rowLimit; rows++) {
                std::vector<size_t> starts(1, 0);
                f32 acc = 0;
                const f32 target = sumWeights / (f32)rows;
                for (size_t i = 0; i < kinds.size(); i++) {
                    const f32 w = weightOf(kinds[i]);
                    if ((int)starts.size() < rows && acc > 0 && acc + w * 0.5f > target * (f32)starts.size()) {
                        starts.push_back(i);
                    }
                    acc += w;
                }
                const f32 rowGapK = 0.10f;
                f32 d = (H - 2.0f * pad) / ((f32)starts.size() + rowGapK * (f32)(starts.size() - 1));
                for (size_t r = 0; r < starts.size(); r++) {
                    const size_t from = starts[r], to = (r + 1 < starts.size()) ? starts[r + 1] : kinds.size();
                    f32 rowWeight = 0;
                    for (size_t i = from; i < to; i++) { rowWeight += weightOf(kinds[i]); }
                    const f32 dRow = availW / (rowWeight + GAP_K * (f32)(to - from - 1));
                    if (dRow < d) d = dRow;
                }
                if (d > D * 1.02f) {   //a clearly bigger dial is worth another row
                    D = d;
                    bestStarts = starts;
                }
            }
            if (D < 40.0f) D = 40.0f;
            gaugeD = D;
            const f32 gap = GAP_K * D;
            const size_t rowCount = bestStarts.size();
            const f32 rowGap = 0.10f * D;
            const f32 blockH = D * (f32)rowCount + rowGap * (f32)(rowCount - 1);

            //Centre on the SCREEN (= the middle TV on Eyefinity), but never leave the panel. In a
            //window of its own the panel is the screen.
            f32 screenCentreX = (f32)AbsoluteRect.getCenter().X;
            if (Environment && Environment->getRootGUIElement() && Parent == Environment->getRootGUIElement()) {
                screenCentreX = (f32)Environment->getRootGUIElement()->getAbsolutePosition().getCenter().X;
            }
            const f32 panelL = (f32)AbsoluteRect.UpperLeftCorner.X + pad;
            const f32 panelR = (f32)AbsoluteRect.LowerRightCorner.X - pad;
            const f32 blockTop = (f32)AbsoluteRect.UpperLeftCorner.Y + (H - blockH) * 0.5f;

            //Every row is centred on the same axis; the status column sits right of the widest row.
            f32 widest = 0;
            for (size_t r = 0; r < rowCount; r++) {
                const size_t from = bestStarts[r], to = (r + 1 < rowCount) ? bestStarts[r + 1] : kinds.size();
                f32 rw = gap * (f32)(to - from - 1);
                for (size_t i = from; i < to; i++) { rw += weightOf(kinds[i]) * D; }
                if (rw > widest) widest = rw;
            }
            const f32 groupW = widest + statusSpace;
            f32 groupX = screenCentreX - groupW * 0.5f;
            if (groupX + groupW > panelR) groupX = panelR - groupW;
            if (groupX < panelL) groupX = panelL;

            for (size_t r = 0; r < rowCount; r++) {
                const size_t from = bestStarts[r], to = (r + 1 < rowCount) ? bestStarts[r + 1] : kinds.size();
                f32 rw = gap * (f32)(to - from - 1);
                for (size_t i = from; i < to; i++) { rw += weightOf(kinds[i]) * D; }
                f32 x = groupX + (widest - rw) * 0.5f;
                const f32 yTop = blockTop + (f32)r * (D + rowGap);
                for (size_t i = from; i < to; i++) {
                    Slot s;
                    s.kind = kinds[i];
                    const f32 w = weightOf(kinds[i]) * D;
                    s.box = core::rect<f32>(x, yTop, x + w, yTop + D);
                    s.c = core::vector2df(x + w * 0.5f, yTop + D * 0.5f);
                    s.R = D * 0.5f;
                    slots.push_back(s);
                    x += w + gap;
                }
            }

            if (statusWidth > 0) {
                const f32 sx = groupX + widest + 1.5f * pad;
                const f32 sTop = (f32)AbsoluteRect.UpperLeftCorner.Y + (H - D) * 0.5f;
                statusRect = core::rect<s32>((s32)sx, (s32)sTop, (s32)(sx + statusWidth), (s32)(sTop + D));
            }
            else {
                statusRect = core::rect<s32>(0, 0, 0, 0);
            }
        }

        void GUIInstrumentPanel::setMaxRows(int rows)
        {
            maxRows = core::max_(1, rows);
            layout();
        }

        void GUIInstrumentPanel::setOverrideFont(IGUIFont* font)
        {
            overrideFont = font;
        }

        //-------------------------------------------------------------------------------------------------
        // Text & seven-segment helpers
        //-------------------------------------------------------------------------------------------------

        void GUIInstrumentPanel::text(IGUIFont* font, const wchar_t* s, f32 x, f32 y, video::SColor col, bool hCentre, bool vCentre)
        {
            if (!font || !s) { return; }
            const core::dimension2du d = font->getDimension(s);
            s32 l = (s32)x, t = (s32)y;
            if (hCentre) { l = (s32)(x - d.Width * 0.5f); }
            if (vCentre) { t = (s32)(y - d.Height * 0.5f); }
            //Backlit lettering: a faint copy one pixel out on each side
            if (GLOW > 0.0f && col.getAlpha() == 255) {
                const video::SColor halo((u32)(GLOW * 42.0f), col.getRed(), col.getGreen(), col.getBlue());
                const s32 ox[4] = { -1, 1, 0, 0 }, oy[4] = { 0, 0, -1, 1 };
                for (int k = 0; k < 4; k++) {
                    const s32 l2 = l + ox[k], t2 = t + oy[k];
                    font->draw(core::stringw(s), core::rect<s32>(l2, t2, l2 + (s32)d.Width + 2, t2 + (s32)d.Height + 2), halo, false, false, &AbsoluteClippingRect);
                }
            }
            font->draw(core::stringw(s), core::rect<s32>(l, t, l + (s32)d.Width + 2, t + (s32)d.Height + 2), col, false, false, &AbsoluteClippingRect);
        }

        f32 GUIInstrumentPanel::segWidth(const wchar_t* s, f32 h) const
        {
            const f32 cellW = 0.50f * h, space = 0.17f * h, dotW = 0.13f * h + 0.14f * h;
            f32 w = 0;
            for (const wchar_t* p = s; *p; p++) { w += (*p == L'.') ? dotW : (cellW + space); }
            if (w > 0 && s[0] && s[wcslen(s) - 1] != L'.') { w -= space; }
            return w;
        }

        void GUIInstrumentPanel::segText(const wchar_t* s, f32 xLeft, f32 yTop, f32 h, video::SColor on, video::SColor off)
        {
            const f32 w = 0.50f * h;       //digit cell width
            const f32 t = 0.14f * h;       //segment thickness
            const f32 g = 0.022f * h;      //gap between segments
            const f32 space = 0.17f * h;   //between digits
            const f32 skew = 0.09f;        //LCD-style italic slant
            const f32 ht = t * 0.5f;

            //One segment = a bevelled hexagon. Defined in an upright cell, then slanted.
            auto P = [&](f32 cx, f32 x, f32 y) { return core::vector2df(cx + x + skew * (h - y), yTop + y); };
            auto hSeg = [&](f32 cx, f32 y, video::SColor col) {
                const f32 xl = ht + g, xr = w - ht - g;
                const core::vector2df p0 = P(cx, xl, y), p1 = P(cx, xl + ht, y - ht), p2 = P(cx, xr - ht, y - ht),
                    p3 = P(cx, xr, y), p4 = P(cx, xr - ht, y + ht), p5 = P(cx, xl + ht, y + ht);
                batch.tri(p0, p1, p2, col); batch.tri(p0, p2, p3, col); batch.tri(p0, p3, p4, col); batch.tri(p0, p4, p5, col);
                };
            auto vSeg = [&](f32 cx, f32 x, f32 yt, f32 yb, video::SColor col) {
                const core::vector2df p0 = P(cx, x, yt), p1 = P(cx, x + ht, yt + ht), p2 = P(cx, x + ht, yb - ht),
                    p3 = P(cx, x, yb), p4 = P(cx, x - ht, yb - ht), p5 = P(cx, x - ht, yt + ht);
                batch.tri(p0, p1, p2, col); batch.tri(p0, p2, p3, col); batch.tri(p0, p3, p4, col); batch.tri(p0, p4, p5, col);
                };
            //ghost = true: draw the given segments in the 'off' colour; false: draw them lit.
            auto digit = [&](f32 cx, u8 mask, bool ghost) {
                //Backlit LCD: each lit segment bleeds a little light around it
                if (!ghost && GLOW > 0.0f) {
                    for (int seg = 0; seg < 7; seg++) {
                        if (!((mask >> seg) & 1)) continue;
                        const f32 xl = ht + g, xr = w - ht - g;
                        switch (seg) {
                        case 0: glowHalo(batch, P(cx, xl, ht), P(cx, xr, ht), t, on, 0.5f); break;
                        case 1: glowHalo(batch, P(cx, w - ht, ht + g), P(cx, w - ht, h * 0.5f - g), t, on, 0.5f); break;
                        case 2: glowHalo(batch, P(cx, w - ht, h * 0.5f + g), P(cx, w - ht, h - ht - g), t, on, 0.5f); break;
                        case 3: glowHalo(batch, P(cx, xl, h - ht), P(cx, xr, h - ht), t, on, 0.5f); break;
                        case 4: glowHalo(batch, P(cx, ht, h * 0.5f + g), P(cx, ht, h - ht - g), t, on, 0.5f); break;
                        case 5: glowHalo(batch, P(cx, ht, ht + g), P(cx, ht, h * 0.5f - g), t, on, 0.5f); break;
                        case 6: glowHalo(batch, P(cx, xl, h * 0.5f), P(cx, xr, h * 0.5f), t, on, 0.5f); break;
                        }
                    }
                }
                for (int seg = 0; seg < 7; seg++) {
                    if (!((mask >> seg) & 1)) continue;
                    const video::SColor col = ghost ? off : on;
                    switch (seg) {
                    case 0: hSeg(cx, ht, col); break;                                   //a top
                    case 1: vSeg(cx, w - ht, ht + g, h * 0.5f - g, col); break;         //b upper right
                    case 2: vSeg(cx, w - ht, h * 0.5f + g, h - ht - g, col); break;     //c lower right
                    case 3: hSeg(cx, h - ht, col); break;                               //d bottom
                    case 4: vSeg(cx, ht, h * 0.5f + g, h - ht - g, col); break;         //e lower left
                    case 5: vSeg(cx, ht, ht + g, h * 0.5f - g, col); break;             //f upper left
                    case 6: hSeg(cx, h * 0.5f, col); break;                             //g middle
                    }
                }
                };

            f32 x = xLeft;
            for (const wchar_t* p = s; *p; p++) {
                const wchar_t ch = *p;
                if (ch == L'.') {
                    const f32 ds = t * 1.05f;
                    batch.rect(core::rect<f32>(x + 0.02f * h, yTop + h - ds, x + 0.02f * h + ds, yTop + h), on);
                    x += 0.13f * h + 0.14f * h;
                    continue;
                }
                u8 mask = 0;
                if (ch >= L'0' && ch <= L'9') mask = SEG_MASK[ch - L'0'];
                else if (ch == L'-') mask = 0x40;
                //Unlit segments stay faintly visible, like a real LCD - it reads as an instrument, not as text.
                digit(x, 0x7F, true);            //ghost
                if (mask) digit(x, mask, false); //lit
                x += w + space;
            }
        }

        void GUIInstrumentPanel::segTextCentred(const wchar_t* s, f32 xCentre, f32 yTop, f32 h, video::SColor on, video::SColor off)
        {
            segText(s, xCentre - segWidth(s, h) * 0.5f, yTop, h, on, off);
        }

        //-------------------------------------------------------------------------------------------------
        // Shared dial parts
        //-------------------------------------------------------------------------------------------------

        void GUIInstrumentPanel::drawRoundBody(const Slot& s)
        {
            const core::vector2df& c = s.c;
            const f32 R = s.R;
            //Soft drop shadow, slightly below: the dial sits proud of the panel.
            const core::vector2df sc(c.X, c.Y + R * 0.035f);
            batch.sector(sc, 0, R * 0.96f, 0, 360, video::SColor(120, 0, 0, 0), video::SColor(120, 0, 0, 0), false);
            batch.sector(sc, R * 0.96f, R * 1.07f, 0, 360, video::SColor(120, 0, 0, 0), video::SColor(0, 0, 0, 0), false);
            //Brushed bezel, then a dark lip, then the face with a gentle radial gradient.
            batch.bezelRing(c, R * 0.895f, R, BEZEL_TOP, BEZEL_BOTTOM);
            batch.sector(c, R * 0.875f, R * 0.895f, 0, 360, BEZEL_INNER, BEZEL_INNER, false);
            batch.disc(c, R * 0.876f, FACE_CENTRE, FACE_EDGE);
        }

        void GUIInstrumentPanel::drawScale(const core::vector2df& c, f32 rf, f32 vMin, f32 vMax, f32 a0, f32 a1, f32 major, f32 minor)
        {
            const f32 wMajor = core::max_(1.6f, rf * 0.020f);
            const f32 wMinor = core::max_(1.0f, rf * 0.010f);
            const f32 rOut = rf * 0.925f;
            const s32 nMinor = (s32)floorf((vMax - vMin) / minor + 0.5f);
            for (s32 i = 0; i <= nMinor; i++) {
                const f32 v = vMin + minor * (f32)i;
                const f32 k = v / major;
                const bool isMajor = fabsf(k - floorf(k + 0.5f)) < 0.001f;
                const f32 a = valueToAngle(v, vMin, vMax, a0, a1);
                glowLine(batch, panelPolar(c, isMajor ? rf * 0.78f : rf * 0.855f, a), panelPolar(c, rOut, a), isMajor ? wMajor : wMinor, SCALE_WHITE);
            }
        }

        void GUIInstrumentPanel::drawBand(const core::vector2df& c, f32 rf, f32 vMin, f32 vMax, f32 a0, f32 a1, f32 bandFrom, f32 bandTo, video::SColor col)
        {
            const f32 aa = valueToAngle(panelClamp(bandFrom, vMin, vMax), vMin, vMax, a0, a1);
            const f32 ab = valueToAngle(panelClamp(bandTo, vMin, vMax), vMin, vMax, a0, a1);
            batch.sector(c, rf * 0.935f, rf * 0.985f, aa, ab, col, col, true);
        }

        void GUIInstrumentPanel::drawScaleLabels(IGUIFont* font, const core::vector2df& c, f32 rf, f32 vMin, f32 vMax, f32 a0, f32 a1, f32 step, bool absolute)
        {
            const s32 first = (s32)ceilf(vMin / step - 0.001f);
            const s32 last = (s32)floorf(vMax / step + 0.001f);
            for (s32 i = first; i <= last; i++) {
                const f32 v = step * (f32)i;
                const f32 shown = absolute ? fabsf(v) : v;
                wchar_t buf[16];
                if (fabsf(shown - floorf(shown + 0.5f)) < 0.001f) swprintf(buf, 16, L"%d", (int)floorf(shown + 0.5f));
                else swprintf(buf, 16, L"%.1f", shown);
                const core::vector2df p = panelPolar(c, rf * 0.62f, valueToAngle(v, vMin, vMax, a0, a1));
                text(font, buf, p.X, p.Y, SCALE_WHITE);
            }
        }

        void GUIInstrumentPanel::drawNeedle(const core::vector2df& c, f32 rf, f32 angleDeg, video::SColor col)
        {
            //Tapered pointer with a short counterweight tail, a drop shadow, and a two-tone hub cap.
            const f32 a = angleDeg * core::DEGTORAD;
            const core::vector2df u(sinf(a), -cosf(a));      //along the needle
            const core::vector2df n(cosf(a), sinf(a));       //across it
            const f32 len = rf * 0.84f, tail = rf * 0.20f;
            const f32 wBase = core::max_(2.5f, rf * 0.050f), wTip = core::max_(0.8f, rf * 0.010f), wTail = rf * 0.034f;

            glowHalo(batch, c - u * tail * 0.6f, c + u * len * 0.92f, wTip * 2.0f, col, 0.45f, wBase * 1.1f);
            for (int pass = 0; pass < 2; pass++) {
                const core::vector2df off = (pass == 0) ? core::vector2df(rf * 0.018f, rf * 0.030f) : core::vector2df(0, 0);
                const video::SColor cc = (pass == 0) ? NEEDLE_SHADOW : col;
                const core::vector2df o = c + off;
                const core::vector2df tipL = o + u * len + n * wTip, tipR = o + u * len - n * wTip;
                const core::vector2df baseL = o + n * wBase, baseR = o - n * wBase;
                const core::vector2df tailL = o - u * tail + n * wTail, tailR = o - u * tail - n * wTail;
                batch.quad(baseL, tipL, tipR, baseR, cc);
                batch.quad(tailL, baseL, baseR, tailR, cc);
            }
            batch.disc(c, rf * 0.095f, HUB_OUTER, HUB_OUTER);
            batch.disc(c, rf * 0.060f, HUB_INNER, HUB_OUTER);
        }

        //-------------------------------------------------------------------------------------------------
        // CAP - gyro repeater
        //-------------------------------------------------------------------------------------------------

        void GUIInstrumentPanel::drawCompass(const Slot& s, int pass, IGUIFont* font)
        {
            const core::vector2df& c = s.c;
            const f32 rf = s.R * 0.876f;
            const f32 hdg = data.heading;

            const f32 dh = rf * 0.20f; //digit height
            wchar_t hbuf[16];
            swprintf(hbuf, 16, L"%05.1f", hdg + 0.05f >= 360.0f ? 0.0f : hdg);
            const f32 dw = segWidth(hbuf, dh);
            const f32 wy = c.Y + rf * 0.02f; //window top

            if (pass == 1) {
                drawRoundBody(s);
                //The card rotates under a fixed lubber line: bearing b is drawn at screen angle (b - heading).
                const f32 wMajor = core::max_(1.6f, rf * 0.020f);
                const f32 wMinor = core::max_(1.0f, rf * 0.010f);
                for (int b = 0; b < 360; b += 5) {
                    const f32 a = (f32)b - hdg;
                    const bool ten = (b % 10) == 0;
                    const bool thirty = (b % 30) == 0;
                    const f32 r0 = thirty ? rf * 0.76f : (ten ? rf * 0.82f : rf * 0.87f);
                    glowLine(batch, panelPolar(c, r0, a), panelPolar(c, rf * 0.935f, a), (thirty || ten) ? wMajor : wMinor, SCALE_WHITE);
                }
                //Digital heading window
                const f32 p = dh * 0.28f;
                batch.rect(core::rect<f32>(c.X - dw * 0.5f - p - 1, wy - p - 1, c.X + dw * 0.5f + p + 1, wy + dh + p + 1), LCD_EDGE);
                batch.rect(core::rect<f32>(c.X - dw * 0.5f - p, wy - p, c.X + dw * 0.5f + p, wy + dh + p), LCD_BG);
                segTextCentred(hbuf, c.X, wy, dh, DIGIT_WHITE, ghostOf(DIGIT_WHITE));
            }
            else if (pass == 2) {
                for (int b = 0; b < 360; b += 30) {
                    const core::vector2df p = panelPolar(c, rf * 0.63f, (f32)b - hdg);
                    if (b % 90 == 0) {
                        text(font, CARDINALS[b / 90], p.X, p.Y, b == 0 ? LUBBER : SCALE_WHITE);
                    }
                    else {
                        wchar_t buf[8];
                        swprintf(buf, 8, L"%03d", b);
                        text(font, buf, p.X, p.Y, SCALE_WHITE);
                    }
                }
                text(font, TXT_COMPASS, c.X, c.Y - rf * 0.30f, TEXT_TITLE);
                text(font, L"\u00B0", c.X + dw * 0.5f + dh * 0.45f, wy + dh * 0.15f, DIGIT_WHITE);
                if (data.sogKn > 0.3f) {
                    wchar_t buf[24];
                    swprintf(buf, 24, L"COG %03d\u00B0", ((int)(data.cog + 0.5f)) % 360);
                    text(font, buf, c.X, c.Y + rf * 0.43f, COG_MARK);
                }
            }
            else {
                //Lubber line: the ship's head, fixed at 12 o'clock.
                glowLine(batch, panelPolar(c, rf * 0.70f, 0), panelPolar(c, rf * 0.985f, 0), core::max_(2.0f, rf * 0.022f), LUBBER);
                batch.tri(panelPolar(c, rf * 0.985f, -4.5f), panelPolar(c, rf * 0.985f, 4.5f), panelPolar(c, rf * 0.90f, 0), LUBBER);
                //COG marker - only meaningful when actually making way.
                if (data.sogKn > 0.3f) {
                    const f32 a = data.cog - hdg;
                    batch.tri(panelPolar(c, rf * 0.99f, a - 4.0f), panelPolar(c, rf * 0.99f, a + 4.0f), panelPolar(c, rf * 0.89f, a), COG_MARK);
                }
            }
        }

        //-------------------------------------------------------------------------------------------------
        // LOCH - speed log
        //-------------------------------------------------------------------------------------------------

        void GUIInstrumentPanel::drawLog(const Slot& s, int pass, IGUIFont* font)
        {
            const core::vector2df& c = s.c;
            const f32 rf = s.R * 0.876f;
            const f32 major = niceStep(speedMax / 4.0f);
            const f32 vMax = major * ceilf(speedMax / major - 0.001f);
            const f32 vMin = -major; //one major step astern
            const f32 a0 = -125.0f, a1 = 125.0f;

            const f32 dh = rf * 0.20f;
            wchar_t buf[16];
            swprintf(buf, 16, L"%.1f", data.stwKn);
            const f32 unitW = font ? (f32)font->getDimension(TXT_KNOTS).Width + dh * 0.45f : dh * 0.6f;
            const f32 dw = core::max_(segWidth(L"00.0", dh), segWidth(buf, dh)) + unitW;
            const f32 wy = c.Y + rf * 0.17f;

            if (pass == 1) {
                drawRoundBody(s);
                drawBand(c, rf, vMin, vMax, a0, a1, vMin, 0.0f, BAND_ASTERN);
                drawScale(c, rf, vMin, vMax, a0, a1, major, minorFor(major));
                const f32 p = dh * 0.28f;
                batch.rect(core::rect<f32>(c.X - dw * 0.5f - p - 1, wy - p - 1, c.X + dw * 0.5f + p + 1, wy + dh + p + 1), LCD_EDGE);
                batch.rect(core::rect<f32>(c.X - dw * 0.5f - p, wy - p, c.X + dw * 0.5f + p, wy + dh + p), LCD_BG);
                //Right-aligned, like a real display: the decimal point does not wander as the value changes.
                const video::SColor dc = (data.stwKn < -0.05f) ? DIGIT_RED : DIGIT_WHITE;
                segText(buf, c.X + dw * 0.5f - unitW - segWidth(buf, dh), wy, dh, dc, ghostOf(dc));
            }
            else if (pass == 2) {
                drawScaleLabels(font, c, rf, vMin, vMax, a0, a1, major, false);
                text(font, TXT_LOG, c.X, c.Y - rf * 0.34f, TEXT_TITLE);
                text(font, TXT_KNOTS, c.X + dw * 0.5f - unitW + dh * 0.12f, wy + dh * 0.62f, TEXT_DIM, false, true);
                wchar_t sb[32];
                swprintf(sb, 32, L"%ls %.1f %ls", TXT_GROUND, data.sogKn, TXT_KNOTS);
                text(font, sb, c.X, c.Y + rf * 0.64f, COG_MARK);
            }
            else {
                drawNeedle(c, rf, valueToAngle(panelClamp(data.stwKn, vMin - major * 0.08f, vMax + major * 0.08f), vMin, vMax, a0, a1), NEEDLE);
            }
        }

        //-------------------------------------------------------------------------------------------------
        // BARRE - rudder angle indicator
        //-------------------------------------------------------------------------------------------------

        void GUIInstrumentPanel::drawRudder(const Slot& s, int pass, IGUIFont* font)
        {
            const core::vector2df& c = s.c;
            const f32 rf = s.R * 0.876f;
            const f32 vMax = rudderMax, vMin = -rudderMax;
            const f32 a0 = -100.0f, a1 = 100.0f;
            const f32 major = (rudderMax > 45.0f) ? 20.0f : 10.0f;

            const f32 dh = rf * 0.21f;
            const int deg = (int)floorf(fabsf(data.rudder) + 0.5f);
            wchar_t buf[16];
            swprintf(buf, 16, L"%d", deg);
            const f32 unitW = font ? (f32)font->getDimension(L"\u00B0").Width + dh * 0.45f : dh * 0.5f;
            const f32 dw = segWidth(L"00", dh) + unitW;
            const f32 wy = c.Y + rf * 0.38f;
            const video::SColor dc = (deg == 0) ? DIGIT_WHITE : (data.rudder < 0 ? DIGIT_RED : DIGIT_GREEN);

            if (pass == 1) {
                drawRoundBody(s);
                drawBand(c, rf, vMin, vMax, a0, a1, vMin, 0.0f, BAND_PORT);
                drawBand(c, rf, vMin, vMax, a0, a1, 0.0f, vMax, BAND_STBD);
                drawScale(c, rf, vMin, vMax, a0, a1, major, major / 2.0f);
                const f32 p = dh * 0.28f;
                batch.rect(core::rect<f32>(c.X - dw * 0.5f - p - 1, wy - p - 1, c.X + dw * 0.5f + p + 1, wy + dh + p + 1), LCD_EDGE);
                batch.rect(core::rect<f32>(c.X - dw * 0.5f - p, wy - p, c.X + dw * 0.5f + p, wy + dh + p), LCD_BG);
                segText(buf, c.X + dw * 0.5f - unitW - segWidth(buf, dh), wy, dh, dc, ghostOf(dc));
            }
            else if (pass == 2) {
                drawScaleLabels(font, c, rf, vMin, vMax, a0, a1, major, true);
                text(font, TXT_RUDDER, c.X, c.Y + rf * 0.245f, TEXT_TITLE);
                text(font, L"\u00B0", c.X + dw * 0.5f - unitW + dh * 0.12f, wy + dh * 0.55f, TEXT_DIM, false, true);
                //Side labels on the upper face, as printed on a real rudder indicator
                const core::vector2df pl = panelPolar(c, rf * 0.40f, -48.0f), pr = panelPolar(c, rf * 0.40f, 48.0f);
                text(font, TXT_PORT, pl.X, pl.Y, DIGIT_RED);
                text(font, TXT_STBD, pr.X, pr.Y, DIGIT_GREEN);
                //Helm order in words, so the officer can check "starboard fifteen" at a glance
                const int ord = (int)floorf(fabsf(data.rudderOrder) + 0.5f);
                wchar_t ob[32];
                if (ord == 0) swprintf(ob, 32, L"%ls 0\u00B0", TXT_ORDER);
                else swprintf(ob, 32, L"%ls %d\u00B0 %ls", TXT_ORDER, ord, data.rudderOrder < 0 ? TXT_PORT : TXT_STBD);
                text(font, ob, c.X, c.Y + rf * 0.77f, ORDER_MARK);
            }
            else {
                //Helm order marker outside the scale, needle = actual rudder. The gap between them IS the
                //steering gear lag - exactly what a trainee should learn to watch.
                const f32 ao = valueToAngle(panelClamp(data.rudderOrder, vMin, vMax), vMin, vMax, a0, a1);
                batch.tri(panelPolar(c, rf * 1.00f, ao - 4.5f), panelPolar(c, rf * 1.00f, ao + 4.5f), panelPolar(c, rf * 0.90f, ao), ORDER_MARK);
                drawNeedle(c, rf, valueToAngle(panelClamp(data.rudder, vMin * 1.03f, vMax * 1.03f), vMin, vMax, a0, a1), NEEDLE);
            }
        }

        //-------------------------------------------------------------------------------------------------
        // GIRATION - rate of turn
        //-------------------------------------------------------------------------------------------------

        void GUIInstrumentPanel::drawRot(const Slot& s, int pass, IGUIFont* font)
        {
            const core::vector2df& c = s.c;
            const f32 rf = s.R * 0.876f;
            const f32 major = niceStep(rotMax / 3.0f);
            const f32 vMax = major * ceilf(rotMax / major - 0.001f), vMin = -vMax;
            const f32 a0 = -120.0f, a1 = 120.0f;

            const f32 dh = rf * 0.20f;
            const int rot = (int)floorf(fabsf(data.rotDegMin) + 0.5f);
            wchar_t buf[16];
            swprintf(buf, 16, L"%d", rot);
            const f32 dw = segWidth(L"000", dh);
            const f32 wy = c.Y + rf * 0.17f;
            const video::SColor dc = (rot == 0) ? DIGIT_WHITE : (data.rotDegMin < 0 ? DIGIT_RED : DIGIT_GREEN);

            if (pass == 1) {
                drawRoundBody(s);
                drawBand(c, rf, vMin, vMax, a0, a1, vMin, 0.0f, BAND_PORT);
                drawBand(c, rf, vMin, vMax, a0, a1, 0.0f, vMax, BAND_STBD);
                drawScale(c, rf, vMin, vMax, a0, a1, major, minorFor(major));
                const f32 p = dh * 0.28f;
                batch.rect(core::rect<f32>(c.X - dw * 0.5f - p - 1, wy - p - 1, c.X + dw * 0.5f + p + 1, wy + dh + p + 1), LCD_EDGE);
                batch.rect(core::rect<f32>(c.X - dw * 0.5f - p, wy - p, c.X + dw * 0.5f + p, wy + dh + p), LCD_BG);
                segText(buf, c.X + dw * 0.5f - segWidth(buf, dh), wy, dh, dc, ghostOf(dc));
            }
            else if (pass == 2) {
                drawScaleLabels(font, c, rf, vMin, vMax, a0, a1, major, true);
                text(font, TXT_PER_MIN, c.X, c.Y + rf * 0.52f, TEXT_DIM);
                text(font, TXT_ROT, c.X, c.Y + rf * 0.71f, TEXT_TITLE);
            }
            else {
                drawNeedle(c, rf, valueToAngle(panelClamp(data.rotDegMin, vMin * 1.04f, vMax * 1.04f), vMin, vMax, a0, a1), NEEDLE);
            }
        }

        //-------------------------------------------------------------------------------------------------
        // MOTEURS - shaft tachometer (two needles: port red, starboard green)
        //-------------------------------------------------------------------------------------------------

        void GUIInstrumentPanel::drawRpm(const Slot& s, int pass, IGUIFont* font)
        {
            const core::vector2df& c = s.c;
            const f32 rf = s.R * 0.876f;
            //Scale in HUNDREDS of rev/min, like a real tachometer - keeps the dial numbers to two digits.
            const f32 scale = 100.0f;
            const f32 major = niceStep(maxRPM / scale / 4.0f);
            const f32 vMax = major * ceilf(maxRPM / scale / major - 0.001f);
            const f32 vMin = -major;
            const f32 a0 = -125.0f, a1 = 125.0f;

            const f32 dh = rf * 0.145f;
            const f32 wy = c.Y + rf * 0.26f;
            wchar_t pbuf[16], sbuf[16];
            swprintf(pbuf, 16, L"%d", (int)floorf(fabsf(data.portRPM) + 0.5f));
            swprintf(sbuf, 16, L"%d", (int)floorf(fabsf(data.stbdRPM) + 0.5f));
            const f32 dw = segWidth(L"0000", dh);

            if (pass == 1) {
                drawRoundBody(s);
                drawBand(c, rf, vMin, vMax, a0, a1, vMin, 0.0f, BAND_ASTERN);
                drawScale(c, rf, vMin, vMax, a0, a1, major, minorFor(major));
                //Readouts stacked - port above starboard. Stacking keeps the window narrow, so it stays
                //clear of the scale numbers on either side of the dial.
                if (compactDial(rf, font)) { return; }
                const f32 p = dh * 0.30f;
                const f32 rows = singleEngine ? 1.0f : 2.0f;
                const f32 rowGap = dh * 0.25f;
                const f32 boxH = rows * dh + (rows - 1.0f) * rowGap;
                batch.rect(core::rect<f32>(c.X - dw * 0.5f - p - 1, wy - p - 1, c.X + dw * 0.5f + p + 1, wy + boxH + p + 1), LCD_EDGE);
                batch.rect(core::rect<f32>(c.X - dw * 0.5f - p, wy - p, c.X + dw * 0.5f + p, wy + boxH + p), LCD_BG);
                for (int e = 0; e < (singleEngine ? 1 : 2); e++) {
                    const wchar_t* t = (e == 0) ? pbuf : sbuf;
                    const video::SColor dc = (e == 0) ? DIGIT_RED : DIGIT_GREEN;
                    segText(t, c.X + dw * 0.5f - segWidth(t, dh), wy + (f32)e * (dh + rowGap), dh, dc, ghostOf(dc));
                }
            }
            else if (pass == 2) {
                drawScaleLabels(font, c, rf, vMin, vMax, a0, a1, major, true);
                text(font, compactDial(rf, font) ? TXT_RPM : TXT_RPM_UNIT, c.X, c.Y + rf * 0.66f, TEXT_TITLE);
                //Astern reminder on the red band, so a needle below zero is unambiguous
                const core::vector2df pb = panelPolar(c, rf * 0.80f, -113.0f);
                text(font, TXT_ASTERN, pb.X, pb.Y, BAND_ASTERN);
            }
            else {
                //Port needle first so the starboard one sits on top when they overlap.
                drawNeedle(c, rf, valueToAngle(panelClamp(data.portRPM / scale, vMin, vMax), vMin, vMax, a0, a1), BAND_PORT);
                if (!singleEngine) {
                    drawNeedle(c, rf, valueToAngle(panelClamp(data.stbdRPM / scale, vMin, vMax), vMin, vMax, a0, a1), BAND_STBD);
                }
            }
        }

        //-------------------------------------------------------------------------------------------------
        // VENT - true wind (arrow shows where the wind comes FROM, relative to the ship)
        //-------------------------------------------------------------------------------------------------

        void GUIInstrumentPanel::drawWind(const Slot& s, int pass, IGUIFont* font)
        {
            const core::vector2df& c = s.c;
            const f32 rf = s.R * 0.876f;
            //KYARA: a calm has no direction. Wind direction stays at whatever the scenario or the slider
            //last held (000 by default), so drawing the arrow would point somewhere definite - and read as
            //a real wind - when there is none. Below the threshold the dial shows CALME and no arrow.
            const bool calm = data.windSpeedKn < 0.1f;
            //Relative angle: 0 = dead ahead, 90 = on the starboard beam.
            const f32 rel = norm360(data.windDirTrue - data.heading);

            const f32 dh = rf * 0.19f;
            const f32 wy = c.Y + rf * 0.20f;
            wchar_t buf[16];
            swprintf(buf, 16, L"%.1f", data.windSpeedKn);
            const f32 unitW = font ? (f32)font->getDimension(TXT_KNOTS).Width + dh * 0.45f : dh * 0.6f;
            const f32 dw = core::max_(segWidth(L"00.0", dh), segWidth(buf, dh)) + unitW;

            if (pass == 1) {
                drawRoundBody(s);
                //Fixed card: the ship stays pointing up, marks every 10 deg, longer every 30.
                const f32 wMajor = core::max_(1.6f, rf * 0.020f);
                const f32 wMinor = core::max_(1.0f, rf * 0.010f);
                for (int b = 0; b < 360; b += 10) {
                    const bool thirty = (b % 30) == 0;
                    glowLine(batch, panelPolar(c, thirty ? rf * 0.78f : rf * 0.86f, (f32)b), panelPolar(c, rf * 0.935f, (f32)b),
                        thirty ? wMajor : wMinor, SCALE_WHITE);
                }
                //Little ship outline at the centre, so "relative to the bow" needs no explaining.
                const f32 hl = rf * 0.26f, hw = rf * 0.10f;
                const video::SColor hull = HULL_MARK;
                batch.tri(core::vector2df(c.X, c.Y - hl), core::vector2df(c.X + hw, c.Y - hl * 0.25f), core::vector2df(c.X + hw, c.Y + hl * 0.7f), hull);
                batch.tri(core::vector2df(c.X, c.Y - hl), core::vector2df(c.X + hw, c.Y + hl * 0.7f), core::vector2df(c.X - hw, c.Y + hl * 0.7f), hull);
                batch.tri(core::vector2df(c.X, c.Y - hl), core::vector2df(c.X - hw, c.Y + hl * 0.7f), core::vector2df(c.X - hw, c.Y - hl * 0.25f), hull);
                const f32 p = dh * 0.28f;
                batch.rect(core::rect<f32>(c.X - dw * 0.5f - p - 1, wy - p - 1, c.X + dw * 0.5f + p + 1, wy + dh + p + 1), LCD_EDGE);
                batch.rect(core::rect<f32>(c.X - dw * 0.5f - p, wy - p, c.X + dw * 0.5f + p, wy + dh + p), LCD_BG);
                segText(buf, c.X + dw * 0.5f - unitW - segWidth(buf, dh), wy, dh, DIGIT_WHITE, ghostOf(DIGIT_WHITE));
            }
            else if (pass == 2) {
                //Only the quarter marks are labelled - enough to read the angle, no crowding.
                const bool compact = compactDial(rf, font);
                const int labelled[4] = { 45, 90, 270, 315 };
                for (int i = 0; i < (compact ? 0 : 4); i++) {
                    const int b = labelled[i];
                    wchar_t lb[8];
                    swprintf(lb, 8, L"%d", b > 180 ? 360 - b : b); //relative angle, port and starboard alike
                    const core::vector2df p = panelPolar(c, rf * 0.68f, (f32)b);
                    text(font, lb, p.X, p.Y, SCALE_WHITE);
                }
                text(font, TXT_KNOTS, c.X + dw * 0.5f - unitW + dh * 0.12f, wy + dh * 0.62f, TEXT_DIM, false, true);
                if (calm) {
                    //A calm has no direction, so the direction field is dashed out rather than showing a
                    //bearing that is only the last value the scenario held.
                    text(font, TXT_NO_DIR, c.X, c.Y + rf * 0.52f, TEXT_DIM);
                }
                else if (!compact) {
                    wchar_t tb[24];
                    swprintf(tb, 24, L"%03d\u00B0 %ls", ((int)(data.windDirTrue + 0.5f)) % 360,
                        rel < 180.0f ? TXT_STBD : TXT_PORT);
                    text(font, tb, c.X, c.Y + rf * 0.52f, COG_MARK);
                }
                text(font, TXT_WIND, c.X, c.Y + rf * 0.68f, TEXT_TITLE);
            }
            else if (calm) {
                //Nothing directional is drawn. (IMO SN.1/Circ.243/Rev.2 annex 1 shows a calm as a circle
                //round the station symbol, but that is the WMO plotting model for chart symbols; on a dial
                //face the equivalent is the dashed direction field above, which stays uncluttered.)
            }
            else {
                //Wind barb pointing at the bearing the wind comes from, with the tail towards the centre.
                const video::SColor wc = WIND_BARB;
                const core::vector2df tip = panelPolar(c, rf * 0.92f, rel);
                const core::vector2df base = panelPolar(c, rf * 0.50f, rel);
                glowLine(batch, base, tip, core::max_(2.0f, rf * 0.045f), wc);
                batch.tri(tip, panelPolar(c, rf * 0.66f, rel - 9.0f), panelPolar(c, rf * 0.66f, rel + 9.0f), wc);
            }
        }

        //-------------------------------------------------------------------------------------------------
        // NAV - GPS / echo sounder
        //-------------------------------------------------------------------------------------------------

        void GUIInstrumentPanel::drawNav(const Slot& s, int pass, IGUIFont* font)
        {
            const core::rect<f32>& b = s.box;
            const f32 D = b.getHeight();
            const f32 bw = D * 0.035f;                       //bezel
            const core::rect<f32> in(b.UpperLeftCorner.X + bw, b.UpperLeftCorner.Y + bw, b.LowerRightCorner.X - bw, b.LowerRightCorner.Y - bw);
            const f32 fh = font ? (f32)font->getDimension(L"0Ag").Height : 14.0f;
            const f32 padX = fh * 0.8f;
            const f32 headerH = fh * 1.75f;
            const f32 lineH = fh * 1.45f;

            //Vertical layout, top down
            f32 y = in.UpperLeftCorner.Y + headerH + fh * 0.5f;
            const f32 yLat = y;           if (fitGPS) y += lineH;
            const f32 yLon = y;           if (fitGPS) y += lineH;
            const f32 yCog = y;           if (fitGPS) y += lineH;
            const f32 ySep = y + fh * 0.15f;
            const f32 yFooter = in.LowerRightCorner.Y - lineH * 0.65f;
            const f32 footerTop = yFooter - fh * 0.9f;
            const f32 depthTop = ySep + fh * 0.5f;
            //Clamped so a short box (many gauges, or a small screen) shrinks the digits instead of
            //letting them run over the tide/FPS line.
            const f32 depthH = core::max_(0.0f, core::min_(D * 0.30f, footerTop - depthTop));

            wchar_t depthBuf[16];
            const bool depthValid = data.depth <= maxSounderDepth && data.depth >= 0.0f;
            if (depthValid) swprintf(depthBuf, 16, L"%.1f", data.depth);

            if (pass == 1) {
                //Shadow, steel frame, LCD glass
                batch.rect(core::rect<f32>(b.UpperLeftCorner.X + 2, b.UpperLeftCorner.Y + D * 0.03f, b.LowerRightCorner.X + 2, b.LowerRightCorner.Y + D * 0.03f), video::SColor(110, 0, 0, 0));
                batch.quad(b.UpperLeftCorner, BEZEL_TOP, core::vector2df(b.LowerRightCorner.X, b.UpperLeftCorner.Y), BEZEL_TOP,
                    b.LowerRightCorner, BEZEL_BOTTOM, core::vector2df(b.UpperLeftCorner.X, b.LowerRightCorner.Y), BEZEL_BOTTOM);
                batch.rect(core::rect<f32>(in.UpperLeftCorner.X - 1, in.UpperLeftCorner.Y - 1, in.LowerRightCorner.X + 1, in.LowerRightCorner.Y + 1), BEZEL_INNER);
                batch.rect(in, LCD_BG);
                batch.rect(core::rect<f32>(in.UpperLeftCorner.X, in.UpperLeftCorner.Y, in.LowerRightCorner.X, in.UpperLeftCorner.Y + headerH), LCD_HEADER);
                if (fitSounder) {
                    batch.line(core::vector2df(in.UpperLeftCorner.X + padX, ySep), core::vector2df(in.LowerRightCorner.X - padX, ySep), 1.0f, LCD_EDGE);
                    if (depthH > 8.0f) {
                        if (depthValid) {
                            const f32 dRight = in.LowerRightCorner.X - padX - fh * 1.6f;
                            segText(depthBuf, dRight - segWidth(depthBuf, depthH), depthTop, depthH, DIGIT_AMBER, ghostOf(DIGIT_AMBER));
                        }
                    }
                }
            }
            else if (pass == 2) {
                const f32 hy = in.UpperLeftCorner.Y + headerH * 0.5f;
                text(font, fitGPS ? TXT_GPS : TXT_NAV, in.UpperLeftCorner.X + padX, hy, NAV_LABEL, false, true);
                if (font) {
                    const core::dimension2du td = font->getDimension(data.timeText.c_str());
                    text(font, data.timeText.c_str(), in.LowerRightCorner.X - padX - td.Width, hy, NAV_VALUE, false, true);
                }

                if (fitGPS) {
                    const f32 valX = in.UpperLeftCorner.X + padX + (font ? (f32)font->getDimension(L"LON  ").Width : fh * 3);
                    const f32 la = fabsf(data.lat), lo = fabsf(data.lon);
                    const int laD = (int)la, loD = (int)lo;
                    wchar_t lb[40], ob[40];
                    swprintf(lb, 40, L"%02d\u00B0%06.3f'%lc", laD, (la - laD) * 60.0f, data.lat >= 0 ? L'N' : L'S');
                    swprintf(ob, 40, L"%03d\u00B0%06.3f'%lc", loD, (lo - loD) * 60.0f, data.lon >= 0 ? L'E' : L'W');
                    text(font, L"LAT", in.UpperLeftCorner.X + padX, yLat, NAV_LABEL, false, true);
                    text(font, lb, valX, yLat, NAV_VALUE, false, true);
                    text(font, L"LON", in.UpperLeftCorner.X + padX, yLon, NAV_LABEL, false, true);
                    text(font, ob, valX, yLon, NAV_VALUE, false, true);

                    wchar_t cb[24], sb[24];
                    swprintf(cb, 24, L"%03d\u00B0", ((int)(data.cog + 0.5f)) % 360);
                    swprintf(sb, 24, L"%.1f %ls", data.sogKn, TXT_KNOTS);
                    const f32 midX = (in.UpperLeftCorner.X + in.LowerRightCorner.X) * 0.5f;
                    text(font, L"COG", in.UpperLeftCorner.X + padX, yCog, NAV_LABEL, false, true);
                    text(font, cb, valX, yCog, NAV_VALUE, false, true);
                    text(font, L"SOG", midX, yCog, NAV_LABEL, false, true);
                    text(font, sb, midX + (valX - in.UpperLeftCorner.X - padX), yCog, NAV_VALUE, false, true);
                }

                if (fitSounder) {
                    text(font, TXT_SOUNDER, in.UpperLeftCorner.X + padX, depthTop + depthH * 0.5f, NAV_LABEL, false, true);
                    if (depthValid) {
                        text(font, L"m", in.LowerRightCorner.X - padX - fh * 0.9f, depthTop + depthH - fh * 0.5f, DIGIT_AMBER);
                    }
                    else {
                        text(font, TXT_NO_DEPTH, (in.UpperLeftCorner.X + in.LowerRightCorner.X) * 0.5f, depthTop + depthH * 0.5f, DIGIT_AMBER);
                    }
                }

                //Footer: tide if the scenario shows it, frame rate small and dim for the instructor.
                if (fitTide) {
                    wchar_t tb[32];
                    swprintf(tb, 32, L"%ls %+.1f m", TXT_TIDE, data.tideHeight);
                    text(font, tb, in.UpperLeftCorner.X + padX, yFooter, NAV_LABEL, false, true);
                }
                //PAUSE in the footer centre - the header is fully used by the date/time.
                if (data.paused) { text(font, TXT_PAUSE, (in.UpperLeftCorner.X + in.LowerRightCorner.X) * 0.5f, yFooter, DIGIT_AMBER); }
                wchar_t fb[16];
                swprintf(fb, 16, L"%u fps", data.fps);
                if (font) {
                    const core::dimension2du fd = font->getDimension(fb);
                    text(font, fb, in.LowerRightCorner.X - padX - fd.Width, yFooter, TEXT_DIM, false, true);
                }
            }
        }

        //-------------------------------------------------------------------------------------------------
        // draw
        //-------------------------------------------------------------------------------------------------


        //-------------------------------------------------------------------------------------------------
        // KYARA HOULE: TANGAGE (pitch) and GITE (roll clinometer)
        //-------------------------------------------------------------------------------------------------

        f32 GUIInstrumentPanel::peakValue(int which) const
        {
            f32 m = 0.0f;
            for (int b = 0; b < 120; b++) { if (peakBuckets[which][b] > m) { m = peakBuckets[which][b]; } }
            return m;
        }

        void GUIInstrumentPanel::drawShape(const core::vector2df& o, const f32* xy, int n, f32 scale, f32 ang, video::SColor col)
        {
            if (n < 3) { return; }
            const f32 ca = cosf(ang), sa = sinf(ang);
            //local x right, y UP -> screen (y down)
            auto P = [&](int i) {
                const f32 x = xy[2 * i], y = xy[2 * i + 1];
                return core::vector2df(o.X + (x * ca - y * sa) * scale, o.Y - (x * sa + y * ca) * scale);
                };
            //fan from point 0 (an interior point) round the outline, closed back to point 1
            const core::vector2df p0 = P(0);
            for (int i = 1; i < n; i++) { batch.tri(p0, P(i), P((i + 1 < n) ? i + 1 : 1), col); }
        }

        void GUIInstrumentPanel::orderedBand(const core::vector2df& c, f32 rf, f32 angA, f32 angB, video::SColor col)
        {
            const f32 lo = angA < angB ? angA : angB;
            const f32 hi = angA < angB ? angB : angA;
            batch.sector(c, rf * 0.935f, rf * 0.985f, lo, hi, col, col, true);
        }

        //TANGAGE: side view, bow to the RIGHT. The scale is on the right-hand arc, bow up = up, and it
        //is expanded (10 deg of pitch = 50 deg of dial) so one or two degrees are still readable. The
        //silhouette turns with the needle, so the two always agree. Title at the top, readout below
        //the silhouette - the left half and the top stay clear of the scale numbers.
        void GUIInstrumentPanel::drawPitch(const Slot& s, int pass, IGUIFont* font)
        {
            const core::vector2df& c = s.c;
            const f32 rf = s.R * 0.876f;
            //+/-10 deg of scale: beyond that the needle just parks at the end. Five numbers over a
            //100 deg arc is as many as fit without them running into each other.
            const f32 vMax = 10.0f, vMin = -10.0f;
            const f32 a0 = 140.0f, a1 = 40.0f;           //bow down at 5 o'clock, bow up at 1 o'clock
            const f32 v = data.pitchDeg;
            const f32 needleA = valueToAngle(panelClamp(v, vMin * 1.04f, vMax * 1.04f), vMin, vMax, a0, a1);

            const f32 dh = rf * 0.155f;
            const f32 av = fabsf(v);
            wchar_t buf[16];
            swprintf(buf, 16, L"%.1f", av);
            const f32 dw = segWidth(L"00.0", dh);
            const f32 wy = c.Y + rf * 0.30f;             //readout below the silhouette, on the dial centreline
            const video::SColor dc = (av < 0.05f) ? DIGIT_WHITE : ((av >= 7.0f) ? DIGIT_RED : DIGIT_AMBER);
            const bool compact = compactDial(rf, font);

            if (pass == 1) {
                drawRoundBody(s);
                //danger sectors at the ends of the scale
                orderedBand(c, rf, valueToAngle(7.0f, vMin, vMax, a0, a1), a1, BAND_ASTERN);
                orderedBand(c, rf, a0, valueToAngle(-7.0f, vMin, vMax, a0, a1), BAND_ASTERN);
                drawScale(c, rf, vMin, vMax, a0, a1, 5.0f, 1.0f);

                //fixed horizon reference, on the free left-hand side
                glowLine(batch, core::vector2df(c.X - rf * 0.78f, c.Y), core::vector2df(c.X - rf * 0.55f, c.Y),
                    core::max_(1.0f, rf * 0.012f), TEXT_DIM);

                //ship silhouette, side view (x forward, y up), turning with the needle
                const f32 ang = (90.0f - needleA) * core::DEGTORAD;
                const core::vector2df o(c.X, c.Y - rf * 0.10f);
                const f32 hull[] = { 0.0f, -0.02f,  -0.40f, 0.02f,  -0.36f, -0.08f,  0.24f, -0.09f,  0.42f, 0.06f,  0.36f, 0.03f };
                const f32 house[] = { -0.20f, 0.02f,  -0.31f, 0.02f,  -0.31f, 0.12f,  -0.09f, 0.12f,  -0.09f, 0.02f };
                const f32 bridge[] = { -0.18f, 0.12f,  -0.28f, 0.12f,  -0.28f, 0.17f,  -0.11f, 0.17f,  -0.11f, 0.12f };
                drawShape(o, hull, 6, rf * 0.85f, ang, SHIP_HULL);
                drawShape(o, house, 5, rf * 0.85f, ang, SHIP_HOUSE);
                drawShape(o, bridge, 5, rf * 0.85f, ang, SHIP_HOUSE);

                const f32 p = dh * 0.28f;
                batch.rect(core::rect<f32>(c.X - dw * 0.5f - p - 1, wy - p - 1, c.X + dw * 0.5f + p + 1, wy + dh + p + 1), LCD_EDGE);
                batch.rect(core::rect<f32>(c.X - dw * 0.5f - p, wy - p, c.X + dw * 0.5f + p, wy + dh + p), LCD_BG);
                segText(buf, c.X + dw * 0.5f - segWidth(buf, dh), wy, dh, dc, ghostOf(dc));
            }
            else if (pass == 2) {
                //Numbers every 5 deg; ticks every 1 deg.
                drawScaleLabels(font, c, rf, vMin, vMax, a0, a1, 5.0f, true);
                text(font, TXT_PITCH, c.X, c.Y - rf * 0.74f, TEXT_TITLE);
                //AV HAUT / AV BAS sits between the readout and the rim, clear of the scale numbers.
                if (!compact && av >= 0.05f) {
                    text(font, v > 0 ? TXT_BOW_UP : TXT_BOW_DOWN, c.X, wy + dh + rf * 0.17f, TEXT_DIM);
                }
            }
            else {
                //2-minute peaks (yellow ticks), then the needle
                const f32 wT = core::max_(2.0f, rf * 0.03f);
                const f32 up = peakValue(0), down = peakValue(1);
                if (up > 0.2f) {
                    const f32 a = valueToAngle(panelClamp(up, 0.0f, vMax), vMin, vMax, a0, a1);
                    glowLine(batch, panelPolar(c, rf * 0.80f, a), panelPolar(c, rf * 0.99f, a), wT, ORDER_MARK);
                }
                if (down > 0.2f) {
                    const f32 a = valueToAngle(panelClamp(-down, vMin, 0.0f), vMin, vMax, a0, a1);
                    glowLine(batch, panelPolar(c, rf * 0.80f, a), panelPolar(c, rf * 0.99f, a), wT, ORDER_MARK);
                }
                drawNeedle(c, rf, needleA, NEEDLE);
            }
        }

        //GITE: pendulum clinometer seen facing forward. Scale along the bottom, BD (port) on the left,
        //TD (starboard) on the right; the pendulum swings towards the side she is heeled to, exactly
        //as a real one does. Everything written (title, readout, BD/TD) is in the upper half, which
        //the needle never reaches.
        void GUIInstrumentPanel::drawRoll(const Slot& s, int pass, IGUIFont* font)
        {
            const core::vector2df& c = s.c;
            const f32 rf = s.R * 0.876f;
            const f32 vMax = 45.0f, vMin = -45.0f;
            const f32 a0 = 240.0f, a1 = 120.0f;          //port 45 at 8 o'clock, starboard 45 at 4 o'clock
            const f32 v = data.rollDeg;
            const f32 needleA = valueToAngle(panelClamp(v, vMin * 1.04f, vMax * 1.04f), vMin, vMax, a0, a1);

            const f32 dh = rf * 0.155f;
            const f32 av = fabsf(v);
            wchar_t buf[16];
            swprintf(buf, 16, L"%.1f", av);
            const f32 dw = segWidth(L"00.0", dh);
            //Everything readable is stacked in the upper half, which the pendulum never enters:
            //title, then the readout, then BD / TD. The lower half belongs to the scale and needle.
            const f32 wy = c.Y - rf * 0.46f;
            const video::SColor dc = (av < 0.05f) ? DIGIT_WHITE : (v < 0 ? DIGIT_RED : DIGIT_GREEN);

            if (pass == 1) {
                drawRoundBody(s);
                orderedBand(c, rf, a0, valueToAngle(-20.0f, vMin, vMax, a0, a1), BAND_PORT);
                orderedBand(c, rf, valueToAngle(20.0f, vMin, vMax, a0, a1), a1, BAND_STBD);
                drawScale(c, rf, vMin, vMax, a0, a1, 10.0f, 5.0f);

                const f32 p = dh * 0.28f;
                batch.rect(core::rect<f32>(c.X - dw * 0.5f - p - 1, wy - p - 1, c.X + dw * 0.5f + p + 1, wy + dh + p + 1), LCD_EDGE);
                batch.rect(core::rect<f32>(c.X - dw * 0.5f - p, wy - p, c.X + dw * 0.5f + p, wy + dh + p), LCD_BG);
                segText(buf, c.X + dw * 0.5f - segWidth(buf, dh), wy, dh, dc, ghostOf(dc));
            }
            else if (pass == 2) {
                //Numbers every 20 deg only; ticks stay every 10 (major) and 5 (minor).
                drawScaleLabels(font, c, rf, vMin, vMax, a0, a1, 20.0f, true);
                text(font, TXT_ROLL, c.X, c.Y - rf * 0.74f, TEXT_TITLE);
                if (av >= 0.05f) {
                    text(font, v < 0 ? TXT_PORT : TXT_STBD, c.X + dw * 0.5f + dh * 1.2f, wy + dh * 0.5f,
                        v < 0 ? BAND_PORT : BAND_STBD);
                }
            }
            else {
                const f32 wT = core::max_(2.0f, rf * 0.03f);
                const f32 stbd = peakValue(2), port = peakValue(3);
                if (stbd > 0.3f) {
                    const f32 a = valueToAngle(panelClamp(stbd, 0.0f, vMax), vMin, vMax, a0, a1);
                    glowLine(batch, panelPolar(c, rf * 0.80f, a), panelPolar(c, rf * 0.99f, a), wT, ORDER_MARK);
                }
                if (port > 0.3f) {
                    const f32 a = valueToAngle(panelClamp(-port, vMin, 0.0f), vMin, vMax, a0, a1);
                    glowLine(batch, panelPolar(c, rf * 0.80f, a), panelPolar(c, rf * 0.99f, a), wT, ORDER_MARK);
                }
                drawNeedle(c, rf, needleA, NEEDLE);
            }
        }

        void GUIInstrumentPanel::draw()
        {
            if (!IsVisible) { return; }

            //Day, dusk or night colours, whichever the bridge is in
            static int paletteShown = -1;
            if (paletteShown != bridge::currentMode()) {
                paletteShown = bridge::currentMode();
                applyPalette(paletteShown);
            }

            video::IVideoDriver* driver = Environment->getVideoDriver();
            IGUISkin* skin = Environment->getSkin();
            IGUIFont* font = overrideFont ? overrideFont : (skin ? skin->getFont() : 0);

            //Console plate: vertical gradient with a lit top edge and a shadowed bottom edge.
            const core::rect<s32>& r = AbsoluteRect;
            driver->draw2DRectangle(r, PANEL_TOP, PANEL_TOP, PANEL_BOTTOM, PANEL_BOTTOM, &AbsoluteClippingRect);
            driver->draw2DRectangle(PANEL_EDGE_HI, core::rect<s32>(r.UpperLeftCorner.X, r.UpperLeftCorner.Y, r.LowerRightCorner.X, r.UpperLeftCorner.Y + 1), &AbsoluteClippingRect);
            driver->draw2DRectangle(PANEL_EDGE_LO, core::rect<s32>(r.UpperLeftCorner.X, r.LowerRightCorner.Y - 1, r.LowerRightCorner.X, r.LowerRightCorner.Y), &AbsoluteClippingRect);

            //Pass 1: static geometry and digits (below the text and the needles)
            batch.begin(driver);
            {
                //Four panel screws - small, but they are what makes it read as hardware.
                const f32 sr = core::max_(3.0f, r.getHeight() * 0.014f);
                const f32 in = sr * 2.6f;
                const core::vector2df screws[4] = {
                    core::vector2df(r.UpperLeftCorner.X + in, r.UpperLeftCorner.Y + in), core::vector2df(r.LowerRightCorner.X - in, r.UpperLeftCorner.Y + in),
                    core::vector2df(r.UpperLeftCorner.X + in, r.LowerRightCorner.Y - in), core::vector2df(r.LowerRightCorner.X - in, r.LowerRightCorner.Y - in) };
                for (int i = 0; i < 4; i++) {
                    batch.disc(screws[i], sr, SCREW_HI, SCREW_LO);
                    batch.line(screws[i] - core::vector2df(sr * 0.6f, sr * 0.6f), screws[i] + core::vector2df(sr * 0.6f, sr * 0.6f), core::max_(1.0f, sr * 0.3f), SCREW_SLOT);
                }
                //Recess behind the status column
                if (statusRect.getWidth() > 0) {
                    const f32 m = (f32)r.getHeight() * 0.02f;
                    const core::rect<f32> sr2((f32)statusRect.UpperLeftCorner.X - m, (f32)statusRect.UpperLeftCorner.Y - m, (f32)statusRect.LowerRightCorner.X + m, (f32)statusRect.LowerRightCorner.Y + m);
                    batch.rect(core::rect<f32>(sr2.UpperLeftCorner.X - 1, sr2.UpperLeftCorner.Y - 1, sr2.LowerRightCorner.X + 1, sr2.LowerRightCorner.Y + 1), PANEL_EDGE_LO);
                    batch.rect(sr2, STATUS_BG);
                }
            }
            for (size_t i = 0; i < slots.size(); i++) {
                switch (slots[i].kind) {
                case G_COMPASS: drawCompass(slots[i], 1, font); break;
                case G_LOG:     drawLog(slots[i], 1, font); break;
                case G_RUDDER:  drawRudder(slots[i], 1, font); break;
                case G_ROT:     drawRot(slots[i], 1, font); break;
                case G_RPM:     drawRpm(slots[i], 1, font); break;
                case G_WIND:    drawWind(slots[i], 1, font); break;
                case G_PITCH:   drawPitch(slots[i], 1, font); break; //KYARA HOULE
                case G_ROLL:    drawRoll(slots[i], 1, font); break;  //KYARA HOULE
                case G_NAV:     drawNav(slots[i], 1, font); break;
                }
            }
            batch.flush();

            //Pass 2: text
            for (size_t i = 0; i < slots.size(); i++) {
                switch (slots[i].kind) {
                case G_COMPASS: drawCompass(slots[i], 2, font); break;
                case G_LOG:     drawLog(slots[i], 2, font); break;
                case G_RUDDER:  drawRudder(slots[i], 2, font); break;
                case G_ROT:     drawRot(slots[i], 2, font); break;
                case G_RPM:     drawRpm(slots[i], 2, font); break;
                case G_WIND:    drawWind(slots[i], 2, font); break;
                case G_PITCH:   drawPitch(slots[i], 2, font); break; //KYARA HOULE
                case G_ROLL:    drawRoll(slots[i], 2, font); break;  //KYARA HOULE
                case G_NAV:     drawNav(slots[i], 2, font); break;
                }
            }

            //Pass 3: needles and markers, on top of everything
            batch.begin(driver);
            for (size_t i = 0; i < slots.size(); i++) {
                switch (slots[i].kind) {
                case G_COMPASS: drawCompass(slots[i], 3, font); break;
                case G_LOG:     drawLog(slots[i], 3, font); break;
                case G_RUDDER:  drawRudder(slots[i], 3, font); break;
                case G_ROT:     drawRot(slots[i], 3, font); break;
                case G_RPM:     drawRpm(slots[i], 3, font); break;
                case G_WIND:    drawWind(slots[i], 3, font); break;
                case G_PITCH:   drawPitch(slots[i], 3, font); break; //KYARA HOULE
                case G_ROLL:    drawRoll(slots[i], 3, font); break;  //KYARA HOULE
                case G_NAV:     break;
                }
            }
            batch.flush();

            IGUIElement::draw();
        }

    } // end namespace gui
} // end namespace irr