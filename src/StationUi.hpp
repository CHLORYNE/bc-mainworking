/*   NAUTITECH - Simulateur de Navigation
     The look of the instructor station windows (CONTROLES, METEO): a navy console page with amber
     marks, a header strip, a column of numbered pages, rows of segmented keys, framed boxes on the
     right and a status footer.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#ifndef __STATIONUI_HPP_INCLUDED__
#define __STATIONUI_HPP_INCLUDED__

#include "irrlicht.h"
#include <string>
#include <vector>

class HudFont;
namespace bridge { class PanelDock; }

namespace station
{
    typedef irr::core::rect<irr::f32> Rect;
    typedef irr::video::SColor Colour;

    //The station's own colours: navy and amber by day and at dusk, black and amber at night (dark
    //adaptation), green on black in the digital mode. They follow bridge::currentMode().
    struct Colours
    {
        Colour bg, nav, raised, navSel, rule;
        Colour edge, edge2;
        Colour text, dim, faint;
        Colour accent, accentDim, accentText, tagText;
        Colour accentInk;   //the accent as lettering on the background (section names, the page in view)
        Colour ok, warn, err, errDim, warnDim;
        Colour key, hover, selRow;
    };
    const Colours& colours();

    enum Tone { TONE_NONE = 0, TONE_OK = 1, TONE_ERR = 2, TONE_WARN = 3 };
    Colour toneColour(int tone);
    //What a state's words say: "en panne", "perte" are faults, "reduite", "derive" warnings,
    //"en service" is good, anything else says nothing
    int toneOfWords(const std::wstring& text);

    //The faces, at the size of the window (k)
    struct Fonts
    {
        HudFont* title = 0;      //header title
        HudFont* tag = 0;        //the code block of the header
        HudFont* nav = 0;        //page names
        HudFont* navNum = 0;     //page numbers, footer
        HudFont* label = 0;      //row labels
        HudFont* state = 0;      //states, captions
        HudFont* key = 0;        //key captions
        HudFont* section = 0;    //section titles, group headers
        HudFont* small = 0;      //box headers, table headers, cell captions
        HudFont* big = 0;        //readings
        HudFont* unit = 0;       //their units
        HudFont* text = 0;       //explanations
        HudFont* strong = 0;     //the title of an explanation, readout values
        void load(irr::gui::IGUIEnvironment* env, irr::f32 k);
        void free();
        ~Fonts() { free(); }
    };

    //Drawing
    irr::core::rect<irr::s32> toI(const Rect& r);
    void fill(irr::video::IVideoDriver* d, const Rect& r, Colour c, const irr::core::rect<irr::s32>* clip);
    void frame(irr::video::IVideoDriver* d, const Rect& r, Colour c, const irr::core::rect<irr::s32>* clip);
    void hline(irr::video::IVideoDriver* d, irr::f32 x0, irr::f32 x1, irr::f32 y, Colour c, const irr::core::rect<irr::s32>* clip);
    void vline(irr::video::IVideoDriver* d, irr::f32 x, irr::f32 y0, irr::f32 y1, Colour c, const irr::core::rect<irr::s32>* clip);
    void lamp(irr::video::IVideoDriver* d, irr::f32 cx, irr::f32 cy, irr::f32 size, Colour c, const irr::core::rect<irr::s32>* clip);
    irr::core::rect<irr::s32> clipOf(const Rect& r, const irr::core::rect<irr::s32>& outer);

    //A row of segmented keys (one engaged), or one key alone
    struct Segment
    {
        std::wstring text;
        bool on = false;
        int tone = TONE_NONE;    //the colour of the engaged key: amber, or red / yellow for a fault
        bool hover = false;
        bool down = false;
        bool enabled = true;
    };
    enum SegmentAlign { ALIGN_LEFT, ALIGN_RIGHT, ALIGN_FILL };
    irr::f32 segmentWidth(const Fonts& f, const std::wstring& text, irr::f32 k);
    std::vector<Rect> layoutSegments(const Fonts& f, irr::f32 k, const std::vector<std::wstring>& texts, const Rect& area,
        int align, irr::f32 height);
    //True when the keys fit the area at their own width
    bool segmentsFit(const Fonts& f, irr::f32 k, const std::vector<std::wstring>& texts, irr::f32 width);
    void drawSegments(irr::video::IVideoDriver* d, const Fonts& f, irr::f32 k, const std::vector<Rect>& rects,
        const std::vector<Segment>& segs, const irr::core::rect<irr::s32>& clip);
    void drawKey(irr::video::IVideoDriver* d, const Fonts& f, irr::f32 k, const Rect& r, const std::wstring& text, bool on,
        bool hover, bool down, const irr::core::rect<irr::s32>& clip);
    //A value with an arrow at each end, for long lists
    void drawStepper(irr::video::IVideoDriver* d, const Fonts& f, irr::f32 k, const Rect& r, const std::wstring& text, int hoverSide,
        const irr::core::rect<irr::s32>& clip);
    //A thin track with a fader cap; t from 0 to 1
    void drawFader(irr::video::IVideoDriver* d, irr::f32 k, const Rect& track, irr::f32 t, bool active, const irr::core::rect<irr::s32>& clip);

    //Section title in amber, a rule after it and the number of items
    void drawSection(irr::video::IVideoDriver* d, const Fonts& f, irr::f32 k, const Rect& r, const std::wstring& label,
        const std::wstring& count, const irr::core::rect<irr::s32>& clip);
    //Background of a row: alternate shading, hover, the hairline under it, the mark of the row in use
    void drawRowBack(irr::video::IVideoDriver* d, irr::f32 k, const Rect& r, bool odd, bool hover, bool active,
        const irr::core::rect<irr::s32>& clip);
    //The state of a row: a lamp and words
    void drawState(irr::video::IVideoDriver* d, const Fonts& f, irr::f32 k, const Rect& r, const std::wstring& text, int tone,
        bool showLamp, const irr::core::rect<irr::s32>& clip);
    //A framed box with its header; returns the area under the header
    Rect drawBox(irr::video::IVideoDriver* d, const Fonts& f, irr::f32 k, const Rect& r, const std::wstring& title,
        const irr::core::rect<irr::s32>& clip);
    irr::f32 boxHeaderHeight(irr::f32 k);
    //The explanation: a title and wrapped text; returns the height used
    irr::f32 drawDetail(irr::video::IVideoDriver* d, const Fonts& f, irr::f32 k, const Rect& r, const std::wstring& title,
        const std::wstring& text, const irr::core::rect<irr::s32>& clip);
    irr::f32 detailHeight(const Fonts& f, irr::f32 k, irr::f32 width, const std::wstring& title, const std::wstring& text);
    void drawScrollBar(irr::video::IVideoDriver* d, irr::f32 k, const Rect& area, irr::f32 total, irr::f32 scroll,
        const irr::core::rect<irr::s32>& clip);

    //A label and its value, in the header (with a lamp) and the footer
    struct Status
    {
        std::wstring label, value;
        int tone;
    };

    //The window around the pages: the header strip (code, title, states, dock key, close key),
    //the column of pages (a grid on top when docked narrow), the side column (a strip at the
    //bottom when narrow) and the footer.
    class Frame
    {
    public:
        struct Page
        {
            std::wstring name, group;
            int badge = 0;           //count shown after the name (faults), 0 for none
        };

        std::wstring code;           //"CTL 01"
        std::wstring title;          //"PUPITRE INSTRUCTEUR"
        std::wstring subtitle;       //"CONTROLES"
        std::vector<Page> pages;
        Fonts fonts;
        irr::f32 k = 1.0f;
        bool compact = false;
        Rect window, header, navArea, main, side, footer;
        int hoveredPage = -1;
        bool hoveredClose = false;
        bool showKeys = true;        //dock and close keys (not on the instructor screen, where the window stays)
        bool forceCompact = false;   //pages on top and boxes underneath unless the window is clearly wide

        //sideWidth: the side column; sideHeight: the strip under the pages when narrow (both
        //before scaling). Reloads the fonts when the scale changes.
        void layout(irr::gui::IGUIEnvironment* env, const irr::core::rect<irr::s32>& area, irr::f32 sideWidth, irr::f32 sideHeight);
        Rect pageRect(int i) const;
        int pageAt(const irr::core::vector2df& p) const;
        Rect closeRect() const;
        Rect dockRect(int place) const;
        //Window, header, pages, side background; then the footer and the corner marks
        void drawBack(irr::video::IVideoDriver* d, const bridge::PanelDock& dock, int selected, const std::vector<Status>& states,
            const irr::core::rect<irr::s32>& clip);
        void drawFooter(irr::video::IVideoDriver* d, const std::vector<Status>& items, const std::wstring& hints,
            const irr::core::rect<irr::s32>& clip);
        //Mouse over the header's keys and the pages
        void hover(const irr::core::vector2df& p);
    };
}

#endif
