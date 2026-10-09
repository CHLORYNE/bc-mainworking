/*   NAUTITECH - Simulateur de Navigation
     Instrument console for the main bridge view (Bridge Command fork).

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#ifndef __GUI_INSTRUMENT_PANEL_HPP_INCLUDED__
#define __GUI_INSTRUMENT_PANEL_HPP_INCLUDED__

     //KYARA: bridge instrument console, drawn entirely with Irrlicht 2D primitives (no textures).
     //One element draws the whole console - bezel panel + every gauge - so it can be hidden/shown in
     //one call and costs one GUI draw per frame. It displays data only; it takes no input.
     //
     //Instruments (each shown only if the ship carries it - see setFit):
     //  BARRE     rudder angle indicator (actual rudder + helm order marker)
     //  CAP       gyro repeater (rotating card, lubber line, COG marker, digital heading)
     //  GIRATION  rate-of-turn indicator (only if the ship has one)
     //  LOCH      speed log (speed through water, speed over ground as secondary)
     //  NAV       GPS / echo-sounder display (position, COG/SOG, time, depth)

#include "IGUIElement.h"
#include "IGUIEnvironment.h"
#include "IVideoDriver.h"
#include "S3DVertex.h"
#include "GUIPanelDraw.hpp"
#include "irrString.h"
#include <vector>

namespace irr
{
    namespace gui
    {
        //Everything the console shows. Units are the ones a mariner reads, NOT simulator internals -
        //the conversion happens once, in GUIMain, so the gauges never have to guess.
        struct InstrumentData
        {
            f32 heading = 0;      //deg true, 0..360
            f32 cog = 0;          //deg true, 0..360
            f32 stwKn = 0;        //speed through water, knots (negative = going astern)
            f32 sogKn = 0;        //speed over ground, knots
            f32 rudder = 0;       //actual rudder angle, deg (negative = port)
            f32 rudderOrder = 0;  //helm order, deg (negative = port)
            f32 rotDegMin = 0;    //rate of turn, deg/min (positive = turning to starboard)
            f32 portRPM = 0;      //port shaft, rev/min (negative = astern)
            f32 stbdRPM = 0;      //starboard shaft, rev/min
            f32 windDirTrue = 0;  //deg true, the direction the wind blows FROM
            f32 windSpeedKn = 0;  //true wind speed, knots
            f32 depth = 0;        //metres under the transducer
            f32 lat = 0;          //decimal degrees, +N
            f32 lon = 0;          //decimal degrees, +E
            f32 tideHeight = 0;   //metres
            core::stringw timeText;
            u32 fps = 0;
            bool paused = false;
            f32 pitchDeg = 0;     //KYARA HOULE: + bow up (tangage)
            f32 rollDeg = 0;      //KYARA HOULE: + heeled to starboard (gite / roulis)
            u32 timeMs = 0;       //KYARA HOULE: simulator clock, for the 2-minute peak markers
            bool gyroLost = false;      //gyro failure: heading frozen, "GYRO HS" on the compass
            bool gpsLost = false;       //GPS failure: last position, shown as lost
            f32 depthAlarmLimit = 0;    //echo sounder alarm, m (0: off)
            bool depthAlarm = false;    //depth under the limit now
            bool mobOn = false;         //man overboard mark: bearing and distance in the GPS box
            f32 mobBrg = 0, mobNm = 0;
        };

        class GUIInstrumentPanel : public IGUIElement
        {
        public:
            GUIInstrumentPanel(IGUIEnvironment* environment, IGUIElement* parent, s32 id, const core::rect<s32>& rectangle);
            virtual ~GUIInstrumentPanel();

            //Which instruments this ship carries. Call once after construction.
            void setFit(bool hasRudderIndicator, bool hasRateOfTurn, bool hasGPS, bool hasDepthSounder, f32 maxSounderDepth, bool showTide);

            //Optional extra dials: shaft tachometer and true wind. maxRPM sets the tachometer scale;
            //singleEngine draws one needle instead of two.
            void setExtraInstruments(bool showRPM, bool showWind, f32 maxRPM, bool singleEngine);

            //KYARA HOULE: TANGAGE (pitch) and GITE (roll clinometer) dials.
            void setMotionInstruments(bool show);

            //KYARA: let the rudder dial act as the helm. Dragging inside it orders a wheel angle,
            //which is how the trainee steers now that the slider along the bottom is gone.
            //maxWheelDeg is the hard-over limit (the wheel scrollbar's range).
            void setHelmControl(bool enabled, f32 maxWheelDeg);

            //Returns true once per new helm order and writes it to wheelDeg (-ve port).
            bool consumeHelmRequest(f32& wheelDeg);

            //Full-scale values. speedMaxKn: top of the log. rotMaxDegMin: each side of zero. rudderMaxDeg: each side.
            void setScales(f32 speedMaxKn, f32 rotMaxDegMin, f32 rudderMaxDeg);

            //Width reserved beside the gauges for buttons/lamps that GUIMain places itself (RADAR, pumps, ack).
            void setStatusColumnWidth(s32 widthPx);

            //Where those buttons should go (absolute screen coordinates). Valid after setFit/setStatusColumnWidth.
            core::rect<s32> getStatusColumnRect() const;

            //Up to maxRows rows of dials, whichever count gives the biggest dials (1 = the classic strip).
            //Used when the console has a window of its own, which may be as tall as a whole screen.
            void setMaxRows(int maxRows);
            //Diameter of the dials in the current layout, pixels.
            f32 getGaugeDiameter() const { return gaugeD; }
            //Font for the dial lettering instead of the skin's (0 = skin font) - bigger dials, bigger text.
            void setOverrideFont(IGUIFont* font);

            void setData(const InstrumentData& data);

            virtual void draw();
            virtual void updateAbsolutePosition();

            //Display only, EXCEPT the rudder dial when it is acting as the helm - everywhere else the
            //panel must never steal clicks from the buttons beside it.
            virtual bool isPointInside(const core::position2d<s32>& point) const;

            //Helm drag handling (only reached via isPointInside above).
            virtual bool OnEvent(const SEvent& event);

        private:
            enum GaugeKind { G_RUDDER, G_COMPASS, G_ROT, G_LOG, G_RPM, G_WIND, G_PITCH, G_ROLL, G_NAV };
            struct Slot
            {
                GaugeKind kind;
                core::rect<f32> box;   //screen area of the instrument
                core::vector2df c;     //centre (round gauges)
                f32 R;                 //outer radius (round gauges)
            };

            void layout();

            //Pass 1 = static geometry (bezels, faces, scales), pass 2 = moving parts (needles, digits).
            void drawRoundBody(const Slot& s);
            void drawScale(const core::vector2df& c, f32 rf, f32 vMin, f32 vMax, f32 a0, f32 a1, f32 major, f32 minor);
            void drawBand(const core::vector2df& c, f32 rf, f32 vMin, f32 vMax, f32 a0, f32 a1, f32 bandFrom, f32 bandTo, video::SColor col);
            void drawScaleLabels(IGUIFont* font, const core::vector2df& c, f32 rf, f32 vMin, f32 vMax, f32 a0, f32 a1, f32 step, bool absolute);
            void drawNeedle(const core::vector2df& c, f32 rf, f32 angleDeg, video::SColor col);

            void drawCompass(const Slot& s, int pass, IGUIFont* font);
            void drawLog(const Slot& s, int pass, IGUIFont* font);
            void drawRudder(const Slot& s, int pass, IGUIFont* font);
            void drawRot(const Slot& s, int pass, IGUIFont* font);
            void drawNav(const Slot& s, int pass, IGUIFont* font);
            void drawRpm(const Slot& s, int pass, IGUIFont* font);
            void drawWind(const Slot& s, int pass, IGUIFont* font);
            void drawPitch(const Slot& s, int pass, IGUIFont* font); //KYARA HOULE
            void drawRoll(const Slot& s, int pass, IGUIFont* font);  //KYARA HOULE
            //KYARA HOULE: filled polygon (convex or star-shaped about its first point), local coords
            //rotated by angleRad (counter-clockwise on screen) and scaled, around 'origin'.
            void drawShape(const core::vector2df& origin, const f32* xy, int n, f32 scale, f32 angleRad, video::SColor col);
            void orderedBand(const core::vector2df& c, f32 rf, f32 angA, f32 angB, video::SColor col);

            //Seven-segment readout. Returns the width it occupies. Handles 0-9 '-' '.' and ' '.
            f32 segWidth(const wchar_t* text, f32 h) const;
            void segText(const wchar_t* text, f32 xLeft, f32 yTop, f32 h, video::SColor on, video::SColor off);
            void segTextCentred(const wchar_t* text, f32 xCentre, f32 yTop, f32 h, video::SColor on, video::SColor off);

            void text(IGUIFont* font, const wchar_t* s, f32 x, f32 y, video::SColor col, bool hCentre = true, bool vCentre = true);

            PanelBatch batch;
            std::vector<Slot> slots;
            InstrumentData data;

            //Rudder dial slot, or 0 when this ship has no rudder dial (used for the helm hit test).
            const Slot* rudderSlot() const;
            //Wheel angle for a click at p, or false if it is outside the dial.
            bool helmAngleFromPoint(const core::position2d<s32>& p, f32& wheelDeg) const;

            bool fitRudder, fitRot, fitGPS, fitSounder, fitTide;
            bool fitRPM = false, fitWind = false, singleEngine = false;
            bool fitMotion = true;       //KYARA HOULE
            //KYARA HOULE: peak markers = largest value seen over the last 2 minutes, kept in
            //one-second buckets: [0] bow up, [1] bow down, [2] starboard, [3] port.
            f32 peakBuckets[4][120];
            u32 peakLastBucket = 0;
            bool peakStarted = false;
            f32 peakValue(int which) const;
            f32 maxRPM = 1000.0f;
            bool helmEnabled = false;
            f32 helmMaxDeg = 30.0f;
            bool helmDragging = false;
            bool helmPending = false;
            f32 helmRequest = 0;
            f32 maxSounderDepth;
            f32 speedMax, rotMax, rudderMax;
            s32 statusWidth;
            core::rect<s32> statusRect; //absolute
            f32 gaugeD;                 //diameter used by the current layout
            int maxRows = 1;
            IGUIFont* overrideFont = 0;
        };

    } // end namespace gui
} // end namespace irr

#endif