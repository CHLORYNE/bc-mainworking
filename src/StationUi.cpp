/*   NAUTITECH - Simulateur de Navigation
     The look of the instructor station windows. See StationUi.hpp.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#include "StationUi.hpp"
#include "BridgeSkin.hpp"
#include "PanelDock.hpp"
#include "GUIPanelDraw.hpp"
#include "launcher/HudText.hpp"

#include <cmath>
#include <cwchar>
#include <algorithm>

namespace station
{
    namespace
    {
        Colour rgb(irr::u32 r, irr::u32 g, irr::u32 b) { return Colour(255, r, g, b); }

        Colours makeNavy()
        {
            Colours c;
            c.bg = rgb(14, 20, 28); c.nav = rgb(11, 16, 23); c.raised = rgb(8, 12, 18); c.navSel = rgb(24, 34, 46); c.rule = rgb(28, 39, 52);
            c.edge = rgb(44, 58, 74); c.edge2 = rgb(30, 42, 55);
            c.text = rgb(214, 224, 234); c.dim = rgb(128, 148, 168); c.faint = rgb(79, 98, 118);
            c.accent = rgb(232, 163, 58); c.accentDim = rgb(74, 52, 20); c.accentText = rgb(255, 220, 166); c.tagText = rgb(26, 18, 6);
            c.ok = rgb(76, 200, 116); c.warn = rgb(240, 198, 74); c.err = rgb(226, 81, 63); c.errDim = rgb(91, 31, 25); c.warnDim = rgb(78, 64, 22);
            c.key = rgb(15, 22, 32); c.hover = rgb(24, 34, 47); c.selRow = rgb(58, 42, 16);
            return c;
        }
        Colours makeNight()
        {
            Colours c;
            c.bg = rgb(8, 8, 9); c.nav = rgb(6, 6, 7); c.raised = rgb(4, 4, 5); c.navSel = rgb(22, 18, 14); c.rule = rgb(30, 24, 18);
            c.edge = rgb(60, 46, 32); c.edge2 = rgb(38, 30, 22);
            c.text = rgb(214, 150, 82); c.dim = rgb(150, 104, 58); c.faint = rgb(92, 66, 40);
            c.accent = rgb(206, 124, 44); c.accentDim = rgb(60, 36, 12); c.accentText = rgb(240, 180, 100); c.tagText = rgb(12, 8, 4);
            c.ok = rgb(124, 170, 92); c.warn = rgb(214, 170, 70); c.err = rgb(204, 84, 62); c.errDim = rgb(64, 22, 16); c.warnDim = rgb(58, 44, 16);
            c.key = rgb(10, 10, 11); c.hover = rgb(22, 18, 14); c.selRow = rgb(44, 28, 10);
            return c;
        }
        Colours makeDigital()
        {
            Colours c;
            c.bg = rgb(2, 7, 3); c.nav = rgb(1, 5, 2); c.raised = rgb(0, 3, 1); c.navSel = rgb(8, 26, 12); c.rule = rgb(12, 36, 16);
            c.edge = rgb(24, 80, 34); c.edge2 = rgb(14, 48, 20);
            c.text = rgb(96, 236, 110); c.dim = rgb(52, 150, 64); c.faint = rgb(30, 92, 40);
            c.accent = rgb(220, 198, 60); c.accentDim = rgb(50, 46, 10); c.accentText = rgb(246, 230, 120); c.tagText = rgb(10, 10, 2);
            c.ok = rgb(96, 236, 110); c.warn = rgb(230, 200, 60); c.err = rgb(255, 100, 80); c.errDim = rgb(70, 20, 14); c.warnDim = rgb(52, 46, 10);
            c.key = rgb(2, 9, 4); c.hover = rgb(8, 26, 12); c.selRow = rgb(40, 38, 8);
            return c;
        }

        Colour withAlpha(Colour c, irr::u32 a)
        {
            c.setAlpha(a);
            return c;
        }

        std::wstring lower(const std::wstring& s)
        {
            std::wstring out(s);
            for (size_t i = 0; i < out.size(); i++) {
                const wchar_t c = out[i];
                if ((c >= L'A' && c <= L'Z') || (c >= 0xC0 && c <= 0xDE && c != 0xD7)) { out[i] = (wchar_t)(c + 32); }
            }
            return out;
        }
    }

    const Colours& colours()
    {
        static const Colours navy = makeNavy(), night = makeNight(), digital = makeDigital();
        switch (bridge::currentMode()) {
        case bridge::MODE_NIGHT: return night;
        case bridge::MODE_DIGITAL: return digital;
        default: return navy;
        }
    }

    Colour toneColour(int tone)
    {
        const Colours& c = colours();
        switch (tone) {
        case TONE_OK: return c.ok;
        case TONE_ERR: return c.err;
        case TONE_WARN: return c.warn;
        default: return c.text;
        }
    }

    int toneOfWords(const std::wstring& text)
    {
        const std::wstring t = lower(text);
        if (t.find(L"panne") != std::wstring::npos || t.find(L"perte") != std::wstring::npos || t.find(L"bloqu") != std::wstring::npos) { return TONE_ERR; }
        if (t.find(L"r\u00E9duite") != std::wstring::npos || t.find(L"d\u00E9rive") != std::wstring::npos) { return TONE_WARN; }
        if (t.find(L"service") != std::wstring::npos) { return TONE_OK; }
        return TONE_NONE;
    }

    //---------------------------------------------------------------------------------------------
    //Fonts
    //---------------------------------------------------------------------------------------------

    void Fonts::load(irr::gui::IGUIEnvironment* env, irr::f32 k)
    {
        free();
        irr::video::IVideoDriver* driver = env->getVideoDriver();
        irr::gui::IGUIFont* fallback = env->getSkin() ? env->getSkin()->getFont() : 0;
        const std::string folder = "media/fonts/barlow-condensed/";
        const std::string regular = folder + "BarlowCondensed-Regular.ttf";
        const std::string medium = folder + "BarlowCondensed-Medium.ttf";
        const std::string semi = folder + "BarlowCondensed-SemiBold.ttf";
        title = new HudFont(driver, semi, 18 * k, fallback);
        tag = new HudFont(driver, semi, 15 * k, fallback);
        nav = new HudFont(driver, medium, 16 * k, fallback);
        navNum = new HudFont(driver, medium, 13.5f * k, fallback);
        label = new HudFont(driver, medium, 16 * k, fallback);
        state = new HudFont(driver, semi, 14 * k, fallback);
        key = new HudFont(driver, medium, 14 * k, fallback);
        section = new HudFont(driver, semi, 13 * k, fallback);
        small = new HudFont(driver, semi, 12 * k, fallback);
        big = new HudFont(driver, semi, 25 * k, fallback);
        unit = new HudFont(driver, medium, 13.5f * k, fallback);
        text = new HudFont(driver, regular, 15 * k, fallback);
        strong = new HudFont(driver, semi, 15 * k, fallback);
    }

    void Fonts::free()
    {
        HudFont** all[] = { &title, &tag, &nav, &navNum, &label, &state, &key, &section, &small, &big, &unit, &text, &strong };
        for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) {
            delete *all[i];
            *all[i] = 0;
        }
    }

    //---------------------------------------------------------------------------------------------
    //Drawing
    //---------------------------------------------------------------------------------------------

    irr::core::rect<irr::s32> toI(const Rect& r)
    {
        return irr::core::rect<irr::s32>((irr::s32)floorf(r.UpperLeftCorner.X + 0.5f), (irr::s32)floorf(r.UpperLeftCorner.Y + 0.5f),
            (irr::s32)floorf(r.LowerRightCorner.X + 0.5f), (irr::s32)floorf(r.LowerRightCorner.Y + 0.5f));
    }

    void fill(irr::video::IVideoDriver* d, const Rect& r, Colour c, const irr::core::rect<irr::s32>* clip)
    {
        d->draw2DRectangle(c, toI(r), clip);
    }

    void frame(irr::video::IVideoDriver* d, const Rect& rf, Colour c, const irr::core::rect<irr::s32>* clip)
    {
        const irr::core::rect<irr::s32> r = toI(rf);
        d->draw2DRectangle(c, irr::core::rect<irr::s32>(r.UpperLeftCorner.X, r.UpperLeftCorner.Y, r.LowerRightCorner.X, r.UpperLeftCorner.Y + 1), clip);
        d->draw2DRectangle(c, irr::core::rect<irr::s32>(r.UpperLeftCorner.X, r.LowerRightCorner.Y - 1, r.LowerRightCorner.X, r.LowerRightCorner.Y), clip);
        d->draw2DRectangle(c, irr::core::rect<irr::s32>(r.UpperLeftCorner.X, r.UpperLeftCorner.Y, r.UpperLeftCorner.X + 1, r.LowerRightCorner.Y), clip);
        d->draw2DRectangle(c, irr::core::rect<irr::s32>(r.LowerRightCorner.X - 1, r.UpperLeftCorner.Y, r.LowerRightCorner.X, r.LowerRightCorner.Y), clip);
    }

    void hline(irr::video::IVideoDriver* d, irr::f32 x0, irr::f32 x1, irr::f32 y, Colour c, const irr::core::rect<irr::s32>* clip)
    {
        const irr::s32 iy = (irr::s32)floorf(y + 0.5f);
        d->draw2DRectangle(c, irr::core::rect<irr::s32>((irr::s32)floorf(x0 + 0.5f), iy, (irr::s32)floorf(x1 + 0.5f), iy + 1), clip);
    }

    void vline(irr::video::IVideoDriver* d, irr::f32 x, irr::f32 y0, irr::f32 y1, Colour c, const irr::core::rect<irr::s32>* clip)
    {
        const irr::s32 ix = (irr::s32)floorf(x + 0.5f);
        d->draw2DRectangle(c, irr::core::rect<irr::s32>(ix, (irr::s32)floorf(y0 + 0.5f), ix + 1, (irr::s32)floorf(y1 + 0.5f)), clip);
    }

    void lamp(irr::video::IVideoDriver* d, irr::f32 cx, irr::f32 cy, irr::f32 size, Colour c, const irr::core::rect<irr::s32>* clip)
    {
        const irr::f32 h = std::max(2.0f, size) * 0.5f;
        fill(d, Rect(cx - h, cy - h, cx + h, cy + h), c, clip);
    }

    irr::core::rect<irr::s32> clipOf(const Rect& r, const irr::core::rect<irr::s32>& outer)
    {
        irr::core::rect<irr::s32> c = toI(r);
        c.clipAgainst(outer);
        return c;
    }

    irr::f32 segmentWidth(const Fonts& f, const std::wstring& text, irr::f32 k)
    {
        return (f.key ? f.key->width(hudUpper(text), 1.0f * k) : 60 * k) + 24 * k;
    }

    bool segmentsFit(const Fonts& f, irr::f32 k, const std::vector<std::wstring>& texts, irr::f32 width)
    {
        irr::f32 total = 0;
        for (size_t i = 0; i < texts.size(); i++) { total += segmentWidth(f, texts[i], k); }
        return total <= width;
    }

    std::vector<Rect> layoutSegments(const Fonts& f, irr::f32 k, const std::vector<std::wstring>& texts, const Rect& area,
        int align, irr::f32 height)
    {
        std::vector<Rect> out;
        const size_t n = texts.size();
        if (n == 0) { return out; }
        std::vector<irr::f32> w(n);
        irr::f32 total = 0;
        for (size_t i = 0; i < n; i++) {
            w[i] = (align == ALIGN_FILL) ? area.getWidth() / n : segmentWidth(f, texts[i], k);
            total += w[i];
        }
        if (total > area.getWidth() && total > 0) {
            //Too wide: every key narrower in proportion (their captions are clipped)
            const irr::f32 s = area.getWidth() / total;
            for (size_t i = 0; i < n; i++) { w[i] *= s; }
            total = area.getWidth();
        }
        irr::f32 x = (align == ALIGN_RIGHT) ? area.LowerRightCorner.X - total : area.UpperLeftCorner.X;
        const irr::f32 y0 = floorf(area.getCenter().Y - height * 0.5f + 0.5f);
        for (size_t i = 0; i < n; i++) {
            out.push_back(Rect(floorf(x + 0.5f), y0, floorf(x + w[i] + 0.5f), y0 + floorf(height + 0.5f)));
            x += w[i];
        }
        return out;
    }

    void drawSegments(irr::video::IVideoDriver* d, const Fonts& f, irr::f32 k, const std::vector<Rect>& rects,
        const std::vector<Segment>& segs, const irr::core::rect<irr::s32>& clip)
    {
        const Colours& c = colours();
        if (rects.empty()) { return; }
        for (size_t i = 0; i < rects.size() && i < segs.size(); i++) {
            const Rect& r = rects[i];
            const Segment& s = segs[i];
            Colour face = c.key;
            Colour line = c.accent;
            if (s.on && !s.enabled) { face = c.hover; }
            else if (s.on) {
                face = s.tone == TONE_ERR ? c.errDim : (s.tone == TONE_WARN ? c.warnDim : c.accentDim);
                line = s.tone == TONE_ERR ? c.err : (s.tone == TONE_WARN ? c.warn : c.accent);
            }
            else if (s.down) { face = c.selRow; }
            else if (s.hover && s.enabled) { face = c.hover; }
            fill(d, r, face, &clip);
            if ((s.on && s.enabled) || s.down) { fill(d, Rect(r.UpperLeftCorner.X, r.UpperLeftCorner.Y, r.LowerRightCorner.X, r.UpperLeftCorner.Y + std::max(1.0f, 2 * k)), line, &clip); }
            if (i > 0) { vline(d, r.UpperLeftCorner.X, r.UpperLeftCorner.Y, r.LowerRightCorner.Y, c.edge, &clip); }
            Colour ink = c.dim;
            if (!s.enabled) { ink = c.faint; }
            else if (s.on) { ink = (s.tone == TONE_ERR || s.tone == TONE_WARN) ? c.text : c.accentText; }
            else if (s.hover) { ink = c.text; }
            const irr::core::rect<irr::s32> sc = clipOf(r, clip);
            f.key->drawIn(hudUpper(s.text), Rect(r.UpperLeftCorner.X + 4 * k, r.UpperLeftCorner.Y, r.LowerRightCorner.X - 4 * k, r.LowerRightCorner.Y + 1 * k),
                ink, HudFont::Centre, 1.0f * k, &sc);
        }
        const Rect all(rects.front().UpperLeftCorner.X, rects.front().UpperLeftCorner.Y, rects.back().LowerRightCorner.X, rects.back().LowerRightCorner.Y);
        frame(d, all, c.edge, &clip);
    }

    void drawKey(irr::video::IVideoDriver* d, const Fonts& f, irr::f32 k, const Rect& r, const std::wstring& text, bool on,
        bool hover, bool down, const irr::core::rect<irr::s32>& clip)
    {
        const Colours& c = colours();
        fill(d, r, down ? c.selRow : (on ? c.accentDim : (hover ? c.hover : c.key)), &clip);
        if (on || down) { fill(d, Rect(r.UpperLeftCorner.X, r.UpperLeftCorner.Y, r.LowerRightCorner.X, r.UpperLeftCorner.Y + std::max(1.0f, 2 * k)), c.accent, &clip); }
        frame(d, r, hover ? c.dim : c.edge, &clip);
        const irr::core::rect<irr::s32> sc = clipOf(r, clip);
        f.state->drawIn(hudUpper(text), Rect(r.UpperLeftCorner.X + 6 * k, r.UpperLeftCorner.Y, r.LowerRightCorner.X - 6 * k, r.LowerRightCorner.Y + 1 * k),
            on ? c.accentText : c.text, HudFont::Centre, 1.6f * k, &sc);
    }

    void drawStepper(irr::video::IVideoDriver* d, const Fonts& f, irr::f32 k, const Rect& r, const std::wstring& text, int hoverSide,
        const irr::core::rect<irr::s32>& clip)
    {
        const Colours& c = colours();
        const irr::f32 zone = 24 * k;
        fill(d, r, c.key, &clip);
        if (hoverSide < 0) { fill(d, Rect(r.UpperLeftCorner.X, r.UpperLeftCorner.Y, r.UpperLeftCorner.X + zone, r.LowerRightCorner.Y), c.hover, &clip); }
        if (hoverSide > 0) { fill(d, Rect(r.LowerRightCorner.X - zone, r.UpperLeftCorner.Y, r.LowerRightCorner.X, r.LowerRightCorner.Y), c.hover, &clip); }
        vline(d, r.UpperLeftCorner.X + zone, r.UpperLeftCorner.Y, r.LowerRightCorner.Y, c.edge, &clip);
        vline(d, r.LowerRightCorner.X - zone - 1, r.UpperLeftCorner.Y, r.LowerRightCorner.Y, c.edge, &clip);
        frame(d, r, c.edge, &clip);
        const irr::f32 cy = r.getCenter().Y, a = 4 * k;
        const irr::f32 lx = r.UpperLeftCorner.X + zone * 0.5f, rx = r.LowerRightCorner.X - zone * 0.5f;
        irr::gui::PanelBatch b;
        b.begin(d);
        b.tri(irr::core::vector2df(lx - a * 0.7f, cy), irr::core::vector2df(lx + a * 0.7f, cy - a), irr::core::vector2df(lx + a * 0.7f, cy + a), hoverSide < 0 ? c.accent : c.dim);
        b.tri(irr::core::vector2df(rx + a * 0.7f, cy), irr::core::vector2df(rx - a * 0.7f, cy + a), irr::core::vector2df(rx - a * 0.7f, cy - a), hoverSide > 0 ? c.accent : c.dim);
        b.flush();
        const Rect mid(r.UpperLeftCorner.X + zone + 4 * k, r.UpperLeftCorner.Y, r.LowerRightCorner.X - zone - 4 * k, r.LowerRightCorner.Y + 1 * k);
        const irr::core::rect<irr::s32> sc = clipOf(mid, clip);
        f.key->drawIn(hudUpper(text), mid, c.text, HudFont::Centre, 1.0f * k, &sc);
    }

    void drawFader(irr::video::IVideoDriver* d, irr::f32 k, const Rect& track, irr::f32 t, bool active, const irr::core::rect<irr::s32>& clip)
    {
        const Colours& c = colours();
        t = irr::core::clamp(t, 0.0f, 1.0f);
        const irr::f32 cy = track.getCenter().Y;
        const irr::f32 th = std::max(1.0f, 1.0f * k);
        const irr::f32 kx = track.UpperLeftCorner.X + t * track.getWidth();
        fill(d, Rect(track.UpperLeftCorner.X, cy - th, track.LowerRightCorner.X, cy + th), c.edge, &clip);
        fill(d, Rect(track.UpperLeftCorner.X, cy - th, kx, cy + th), active ? c.accent : withAlpha(c.accent, 200), &clip);
        //Ticks at the quarters
        for (int i = 0; i <= 4; i++) {
            const irr::f32 x = track.UpperLeftCorner.X + track.getWidth() * i / 4.0f;
            vline(d, x, cy + 4 * k, cy + 7 * k, c.faint, &clip);
        }
        const irr::f32 w = 4 * k, h = 8 * k;
        const Rect cap(kx - w, cy - h, kx + w, cy + h);
        fill(d, cap, active ? c.accent : c.text, &clip);
        fill(d, Rect(kx - 0.5f * std::max(1.0f, k), cy - h + 3 * k, kx + 0.5f * std::max(1.0f, k), cy + h - 3 * k), c.bg, &clip);
    }

    void drawSection(irr::video::IVideoDriver* d, const Fonts& f, irr::f32 k, const Rect& r, const std::wstring& label,
        const std::wstring& count, const irr::core::rect<irr::s32>& clip)
    {
        const Colours& c = colours();
        const irr::f32 y0 = r.LowerRightCorner.Y - 20 * k, y1 = r.LowerRightCorner.Y - 2 * k;
        const std::wstring text = hudUpper(label);
        const irr::f32 tw = f.section->width(text, 2.5f * k);
        f.section->drawIn(text, Rect(r.UpperLeftCorner.X, y0, r.UpperLeftCorner.X + tw + 4 * k, y1), c.accent, HudFont::Left, 2.5f * k, &clip);
        irr::f32 x = r.UpperLeftCorner.X + tw + 10 * k;
        if (!count.empty()) {
            const irr::f32 cw = f.small->width(count, 1.0f * k);
            f.small->drawIn(count, Rect(x, y0, x + cw + 4 * k, y1), c.faint, HudFont::Left, 1.0f * k, &clip);
            x += cw + 10 * k;
        }
        if (x < r.LowerRightCorner.X) { hline(d, x, r.LowerRightCorner.X, (y0 + y1) * 0.5f, c.edge2, &clip); }
    }

    void drawRowBack(irr::video::IVideoDriver* d, irr::f32 k, const Rect& r, bool odd, bool hover, bool active,
        const irr::core::rect<irr::s32>& clip)
    {
        const Colours& c = colours();
        if (hover) { fill(d, r, c.hover, &clip); }
        else if (odd) { fill(d, r, Colour(7, 255, 255, 255), &clip); }
        if (active) { fill(d, Rect(r.UpperLeftCorner.X, r.UpperLeftCorner.Y, r.UpperLeftCorner.X + std::max(2.0f, 2 * k), r.LowerRightCorner.Y), c.accent, &clip); }
        hline(d, r.UpperLeftCorner.X, r.LowerRightCorner.X, r.LowerRightCorner.Y - 1, c.rule, &clip);
    }

    void drawState(irr::video::IVideoDriver* d, const Fonts& f, irr::f32 k, const Rect& r, const std::wstring& text, int tone,
        bool showLamp, const irr::core::rect<irr::s32>& clip)
    {
        const Colours& c = colours();
        irr::f32 x = r.UpperLeftCorner.X;
        if (showLamp && tone != TONE_NONE) {
            lamp(d, x + 4 * k, r.getCenter().Y, 7 * k, toneColour(tone), &clip);
            x += 14 * k;
        }
        const irr::core::rect<irr::s32> sc = clipOf(Rect(x, r.UpperLeftCorner.Y, r.LowerRightCorner.X, r.LowerRightCorner.Y), clip);
        f.state->drawIn(hudUpper(text), Rect(x, r.UpperLeftCorner.Y, r.LowerRightCorner.X, r.LowerRightCorner.Y + 1 * k),
            tone == TONE_NONE ? c.dim : toneColour(tone), HudFont::Left, 1.4f * k, &sc);
    }

    irr::f32 boxHeaderHeight(irr::f32 k) { return 26 * k; }

    Rect drawBox(irr::video::IVideoDriver* d, const Fonts& f, irr::f32 k, const Rect& r, const std::wstring& title,
        const irr::core::rect<irr::s32>& clip)
    {
        const Colours& c = colours();
        const irr::f32 hh = boxHeaderHeight(k);
        const Rect head(r.UpperLeftCorner.X, r.UpperLeftCorner.Y, r.LowerRightCorner.X, r.UpperLeftCorner.Y + hh);
        fill(d, r, withAlpha(c.bg, 255), &clip);
        fill(d, head, c.raised, &clip);
        hline(d, r.UpperLeftCorner.X, r.LowerRightCorner.X, head.LowerRightCorner.Y - 1, c.edge2, &clip);
        frame(d, r, c.edge2, &clip);
        const irr::core::rect<irr::s32> sc = clipOf(head, clip);
        f.small->drawIn(hudUpper(title), Rect(head.UpperLeftCorner.X + 10 * k, head.UpperLeftCorner.Y, head.LowerRightCorner.X - 8 * k, head.LowerRightCorner.Y),
            c.dim, HudFont::Left, 2.5f * k, &sc);
        return Rect(r.UpperLeftCorner.X + 1, head.LowerRightCorner.Y, r.LowerRightCorner.X - 1, r.LowerRightCorner.Y - 1);
    }

    irr::f32 detailHeight(const Fonts& f, irr::f32 k, irr::f32 width, const std::wstring& title, const std::wstring& text)
    {
        const irr::f32 lh = f.text->lineHeight() + 3 * k;
        const size_t lines = f.text->wrap(text, width - 20 * k).size();
        return 10 * k + (title.empty() ? 0 : 24 * k) + lines * lh + 10 * k;
    }

    irr::f32 drawDetail(irr::video::IVideoDriver* d, const Fonts& f, irr::f32 k, const Rect& r, const std::wstring& title,
        const std::wstring& text, const irr::core::rect<irr::s32>& clip)
    {
        const Colours& c = colours();
        const irr::core::rect<irr::s32> sc = clipOf(r, clip);
        const irr::f32 x0 = r.UpperLeftCorner.X + 10 * k, x1 = r.LowerRightCorner.X - 10 * k;
        irr::f32 y = r.UpperLeftCorner.Y + 10 * k;
        if (!title.empty()) {
            f.strong->drawIn(hudUpper(title), Rect(x0, y, x1, y + 20 * k), c.text, HudFont::Left, 1.4f * k, &sc);
            y += 24 * k;
        }
        const irr::f32 lh = f.text->lineHeight() + 3 * k;
        const Colour ink = c.text.getInterpolated(c.dim, 0.55f);
        const std::vector<std::wstring> lines = f.text->wrap(text, x1 - x0);
        for (size_t i = 0; i < lines.size() && y + lh <= r.LowerRightCorner.Y + 2 * k; i++, y += lh) {
            f.text->drawIn(lines[i], Rect(x0, y, x1, y + lh), ink, HudFont::Left, 0, &sc);
        }
        return y + 10 * k - r.UpperLeftCorner.Y;
    }

    void drawScrollBar(irr::video::IVideoDriver* d, irr::f32 k, const Rect& area, irr::f32 total, irr::f32 scroll,
        const irr::core::rect<irr::s32>& clip)
    {
        if (total <= area.getHeight() + 1) { return; }
        const Colours& c = colours();
        const irr::f32 x = area.LowerRightCorner.X - 4 * k;
        fill(d, Rect(x - 1 * k, area.UpperLeftCorner.Y, x + 1 * k, area.LowerRightCorner.Y), c.edge2, &clip);
        const irr::f32 thumbH = std::max(30 * k, area.getHeight() * area.getHeight() / total);
        const irr::f32 thumbY = area.UpperLeftCorner.Y + (area.getHeight() - thumbH) * scroll / std::max(1.0f, total - area.getHeight());
        fill(d, Rect(x - 1.5f * k, thumbY, x + 1.5f * k, thumbY + thumbH), c.dim, &clip);
    }

    //---------------------------------------------------------------------------------------------
    //Frame
    //---------------------------------------------------------------------------------------------

    void Frame::layout(irr::gui::IGUIEnvironment* env, const irr::core::rect<irr::s32>& area, irr::f32 sideWidth, irr::f32 sideHeight)
    {
        window = Rect((irr::f32)area.UpperLeftCorner.X, (irr::f32)area.UpperLeftCorner.Y, (irr::f32)area.LowerRightCorner.X, (irr::f32)area.LowerRightCorner.Y);
        const irr::f32 W = window.getWidth(), H = window.getHeight();
        if (W < 10 || H < 10) { return; }
        compact = W < H * 1.15f;
        const irr::f32 newK = compact ? irr::core::clamp(std::min(H / 860.0f, W / 520.0f), 0.85f, 2.0f)
                                      : irr::core::clamp(std::min(H / 660.0f, W / 1240.0f), 0.85f, 2.0f);
        if (fabsf(newK - k) > 0.01f || !fonts.title) {
            k = newK;
            fonts.load(env, k);
        }
        const irr::f32 x0 = window.UpperLeftCorner.X, y0 = window.UpperLeftCorner.Y, x1 = window.LowerRightCorner.X, y1 = window.LowerRightCorner.Y;
        header = Rect(x0, y0, x1, y0 + floorf(36 * k));
        footer = Rect(x0, y1 - floorf(26 * k), x1, y1);
        if (!compact) {
            navArea = Rect(x0, header.LowerRightCorner.Y, x0 + floorf(210 * k), footer.UpperLeftCorner.Y);
            side = Rect(x1 - floorf(sideWidth * k), header.LowerRightCorner.Y, x1, footer.UpperLeftCorner.Y);
            main = Rect(navArea.LowerRightCorner.X, header.LowerRightCorner.Y, side.UpperLeftCorner.X, footer.UpperLeftCorner.Y);
        }
        else {
            const int n = std::max(1, (int)pages.size());
            const int cols = n <= 6 ? 3 : 4;
            const int rows = (n + cols - 1) / cols;
            navArea = Rect(x0, header.LowerRightCorner.Y, x1, header.LowerRightCorner.Y + floorf(rows * 30 * k + 12 * k));
            const irr::f32 room = footer.UpperLeftCorner.Y - navArea.LowerRightCorner.Y;
            const irr::f32 sh = floorf(std::min(sideHeight * k, room * 0.48f));
            side = Rect(x0, footer.UpperLeftCorner.Y - sh, x1, footer.UpperLeftCorner.Y);
            main = Rect(x0, navArea.LowerRightCorner.Y, x1, side.UpperLeftCorner.Y);
        }
    }

    Rect Frame::pageRect(int i) const
    {
        if (compact) {
            const int n = std::max(1, (int)pages.size());
            const int cols = n <= 6 ? 3 : 4;
            const irr::f32 pad = 8 * k, gap = 4 * k;
            const irr::f32 w = (navArea.getWidth() - 2 * pad - (cols - 1) * gap) / cols;
            const irr::f32 h = 26 * k;
            const irr::f32 x = navArea.UpperLeftCorner.X + pad + (i % cols) * (w + gap);
            const irr::f32 y = navArea.UpperLeftCorner.Y + pad + (i / cols) * (h + gap);
            return Rect(floorf(x), floorf(y), floorf(x + w), floorf(y + h));
        }
        irr::f32 y = navArea.UpperLeftCorner.Y + 6 * k;
        for (int j = 0; j < (int)pages.size(); j++) {
            if (!pages[j].group.empty() && (j == 0 || pages[j].group != pages[j - 1].group)) { y += 30 * k; }
            if (j == i) { break; }
            y += 34 * k;
        }
        return Rect(navArea.UpperLeftCorner.X, floorf(y), navArea.LowerRightCorner.X - 1, floorf(y + 34 * k));
    }

    int Frame::pageAt(const irr::core::vector2df& p) const
    {
        if (!navArea.isPointInside(p)) { return -1; }
        for (int i = 0; i < (int)pages.size(); i++) {
            if (pageRect(i).isPointInside(p)) { return i; }
        }
        return -1;
    }

    Rect Frame::closeRect() const
    {
        const irr::f32 s = floorf(24 * k);
        const irr::f32 xr = header.LowerRightCorner.X - floorf(10 * k);
        const irr::f32 yt = floorf(header.getCenter().Y - s * 0.5f);
        return Rect(xr - s, yt, xr, yt + s);
    }

    Rect Frame::dockRect(int place) const
    {
        const Rect cr = closeRect();
        const irr::f32 tw = fonts.key ? fonts.key->width(bridge::dockCaption(place), 1.5f * k) : 70 * k;
        const irr::f32 xr = cr.UpperLeftCorner.X - floorf(8 * k);
        return Rect(floorf(xr - (16 * k + 8 * k + tw + 20 * k)), cr.UpperLeftCorner.Y, xr, cr.LowerRightCorner.Y);
    }

    void Frame::hover(const irr::core::vector2df& p)
    {
        hoveredPage = pageAt(p);
        hoveredClose = closeRect().isPointInside(p);
    }

    void Frame::drawBack(irr::video::IVideoDriver* d, const bridge::PanelDock& dock, int selected, const std::vector<Status>& states,
        const irr::core::rect<irr::s32>& clip)
    {
        const Colours& c = colours();
        //Window and header strip
        fill(d, window, withAlpha(c.bg, 246), &clip);
        fill(d, header, c.raised, &clip);
        hline(d, header.UpperLeftCorner.X, header.LowerRightCorner.X, header.LowerRightCorner.Y - 1, c.edge, &clip);

        //Code block, title
        const irr::f32 tagW = floorf(fonts.tag->width(code, 1.5f * k) + 26 * k);
        const Rect tag(header.UpperLeftCorner.X, header.UpperLeftCorner.Y, header.UpperLeftCorner.X + tagW, header.LowerRightCorner.Y - 1);
        fill(d, tag, c.accent, &clip);
        fonts.tag->drawIn(code, Rect(tag.UpperLeftCorner.X, tag.UpperLeftCorner.Y, tag.LowerRightCorner.X, tag.LowerRightCorner.Y + 1 * k), c.tagText, HudFont::Centre, 1.5f * k, &clip);
        irr::f32 tx = tag.LowerRightCorner.X + 14 * k;
        const Rect hb(tx, header.UpperLeftCorner.Y, header.LowerRightCorner.X, header.LowerRightCorner.Y + 1 * k);
        const Rect cr = closeRect();
        const Rect dr = dockRect(dock.place);
        if (!compact) {
            const std::wstring t1 = hudUpper(title);
            const irr::f32 w1 = fonts.title->width(t1, 2.5f * k);
            fonts.title->drawIn(t1, hb, c.text, HudFont::Left, 2.5f * k, &clip);
            tx += w1 + 8 * k;
            const std::wstring t2 = L"/ " + hudUpper(subtitle);
            fonts.title->drawIn(t2, Rect(tx, hb.UpperLeftCorner.Y, hb.LowerRightCorner.X, hb.LowerRightCorner.Y), c.dim, HudFont::Left, 2.0f * k, &clip);
            tx += fonts.title->width(t2, 2.0f * k);
        }
        else {
            const std::wstring t2 = hudUpper(subtitle);
            fonts.title->drawIn(t2, hb, c.text, HudFont::Left, 2.0f * k, &clip);
            tx += fonts.title->width(t2, 2.0f * k);
        }

        //Close key
        fill(d, cr, hoveredClose ? c.err : c.key, &clip);
        frame(d, cr, hoveredClose ? c.err : c.edge, &clip);
        {
            const Colour xc = hoveredClose ? c.text : c.dim;
            const irr::f32 in = cr.getWidth() * 0.33f;
            irr::gui::PanelBatch b;
            b.begin(d);
            b.line(irr::core::vector2df(cr.UpperLeftCorner.X + in, cr.UpperLeftCorner.Y + in), irr::core::vector2df(cr.LowerRightCorner.X - in, cr.LowerRightCorner.Y - in), std::max(1.2f, 1.6f * k), xc);
            b.line(irr::core::vector2df(cr.LowerRightCorner.X - in, cr.UpperLeftCorner.Y + in), irr::core::vector2df(cr.UpperLeftCorner.X + in, cr.LowerRightCorner.Y - in), std::max(1.2f, 1.6f * k), xc);
            b.flush();
        }

        //Dock key: a small screen showing where the window goes next, and the words
        {
            const bool hov = dock.isHovered();
            fill(d, dr, hov ? c.hover : c.key, &clip);
            frame(d, dr, hov ? c.dim : c.edge, &clip);
            const irr::f32 iw = 16 * k, ih = 11 * k;
            const irr::f32 ix = dr.UpperLeftCorner.X + 9 * k, iy = floorf(dr.getCenter().Y - ih * 0.5f);
            frame(d, Rect(ix, iy, ix + iw, iy + ih), hov ? c.text : c.dim, &clip);
            Rect win;
            const int next = bridge::nextPanelPlace(dock.place);
            if (next == bridge::PLACE_RIGHT) { win = Rect(ix + iw * 0.6f, iy + 2 * k, ix + iw - 2 * k, iy + ih - 2 * k); }
            else if (next == bridge::PLACE_LEFT) { win = Rect(ix + 2 * k, iy + 2 * k, ix + iw * 0.4f, iy + ih - 2 * k); }
            else { win = Rect(ix + iw * 0.25f, iy + 3 * k, ix + iw * 0.75f, iy + ih - 3 * k); }
            fill(d, win, c.accent, &clip);
            fonts.key->drawIn(bridge::dockCaption(dock.place), Rect(ix + iw + 8 * k, dr.UpperLeftCorner.Y, dr.LowerRightCorner.X - 6 * k, dr.LowerRightCorner.Y + 1 * k),
                hov ? c.text : c.dim, HudFont::Left, 1.5f * k, &clip);
        }

        //States, the first ones first when there is not room for all, the last one nearest the dock key
        std::vector<irr::f32> widths;
        irr::f32 used = 0;
        const irr::f32 room = dr.UpperLeftCorner.X - 18 * k - (tx + 24 * k);
        for (size_t i = 0; i < states.size(); i++) {
            const Status& s = states[i];
            const irr::f32 lw = s.label.empty() ? 0 : fonts.key->width(hudUpper(s.label), 1.5f * k) + 6 * k;
            const irr::f32 w = (s.tone != TONE_NONE ? 14 * k : 0) + lw + fonts.state->width(hudUpper(s.value), 1.0f * k);
            if (used + w > room) { break; }
            widths.push_back(w);
            used += w + 18 * k;
        }
        irr::f32 sx = dr.UpperLeftCorner.X - 18 * k - (used > 0 ? used - 18 * k : 0);
        for (size_t i = 0; i < widths.size(); i++) {
            const Status& s = states[i];
            const std::wstring label = hudUpper(s.label), value = hudUpper(s.value);
            irr::f32 x = sx;
            if (s.tone != TONE_NONE) { lamp(d, x + 4 * k, header.getCenter().Y, 8 * k, toneColour(s.tone), &clip); x += 14 * k; }
            if (!label.empty()) {
                const irr::f32 lw = fonts.key->width(label, 1.5f * k);
                fonts.key->drawIn(label, Rect(x, header.UpperLeftCorner.Y, x + lw + 2 * k, header.LowerRightCorner.Y + 1 * k), c.dim, HudFont::Left, 1.5f * k, &clip);
                x += lw + 6 * k;
            }
            fonts.state->drawIn(value, Rect(x, header.UpperLeftCorner.Y, sx + widths[i] + 2 * k, header.LowerRightCorner.Y + 1 * k), c.text, HudFont::Left, 1.0f * k, &clip);
            sx += widths[i] + 18 * k;
        }

        //Pages
        fill(d, navArea, c.nav, &clip);
        if (!compact) {
            vline(d, navArea.LowerRightCorner.X - 1, navArea.UpperLeftCorner.Y, navArea.LowerRightCorner.Y, c.edge2, &clip);
            for (int i = 0; i < (int)pages.size(); i++) {
                const Rect r = pageRect(i);
                if (!pages[i].group.empty() && (i == 0 || pages[i].group != pages[i - 1].group)) {
                    fonts.small->drawIn(hudUpper(pages[i].group), Rect(r.UpperLeftCorner.X + 16 * k, r.UpperLeftCorner.Y - 22 * k, r.LowerRightCorner.X, r.UpperLeftCorner.Y - 4 * k),
                        c.faint, HudFont::Left, 2.5f * k, &clip);
                }
                const bool sel = (i == selected), hov = (i == hoveredPage);
                if (sel) {
                    fill(d, r, c.navSel, &clip);
                    fill(d, Rect(r.UpperLeftCorner.X, r.UpperLeftCorner.Y, r.UpperLeftCorner.X + floorf(3 * k), r.LowerRightCorner.Y), c.accent, &clip);
                }
                else if (hov) { fill(d, r, c.hover, &clip); }
                wchar_t num[8];
                swprintf(num, 8, L"%02d", i + 1);
                const Rect rr(r.UpperLeftCorner.X, r.UpperLeftCorner.Y, r.LowerRightCorner.X, r.LowerRightCorner.Y + 1 * k);
                fonts.navNum->drawIn(num, Rect(rr.UpperLeftCorner.X + 16 * k, rr.UpperLeftCorner.Y, rr.UpperLeftCorner.X + 46 * k, rr.LowerRightCorner.Y),
                    sel ? c.accent : c.faint, HudFont::Left, 0.5f * k, &clip);
                const irr::core::rect<irr::s32> nc = clipOf(Rect(rr.UpperLeftCorner.X, rr.UpperLeftCorner.Y, rr.LowerRightCorner.X - 30 * k, rr.LowerRightCorner.Y), clip);
                fonts.nav->drawIn(hudUpper(pages[i].name), Rect(rr.UpperLeftCorner.X + 46 * k, rr.UpperLeftCorner.Y, rr.LowerRightCorner.X - 30 * k, rr.LowerRightCorner.Y),
                    (sel || hov) ? c.text : c.dim, HudFont::Left, 1.2f * k, &nc);
                if (pages[i].badge > 0) {
                    fonts.navNum->drawIn(std::to_wstring(pages[i].badge), Rect(rr.UpperLeftCorner.X, rr.UpperLeftCorner.Y, rr.LowerRightCorner.X - 14 * k, rr.LowerRightCorner.Y),
                        c.err, HudFont::Right, 0, &clip);
                }
            }
        }
        else {
            hline(d, navArea.UpperLeftCorner.X, navArea.LowerRightCorner.X, navArea.LowerRightCorner.Y - 1, c.edge2, &clip);
            for (int i = 0; i < (int)pages.size(); i++) {
                const Rect r = pageRect(i);
                const bool sel = (i == selected), hov = (i == hoveredPage);
                fill(d, r, sel ? c.navSel : (hov ? c.hover : c.key), &clip);
                frame(d, r, sel ? c.edge : c.edge2, &clip);
                if (sel) { fill(d, Rect(r.UpperLeftCorner.X, r.UpperLeftCorner.Y, r.LowerRightCorner.X, r.UpperLeftCorner.Y + floorf(2 * k)), c.accent, &clip); }
                wchar_t num[8];
                swprintf(num, 8, L"%02d", i + 1);
                const Rect rr(r.UpperLeftCorner.X, r.UpperLeftCorner.Y, r.LowerRightCorner.X, r.LowerRightCorner.Y + 1 * k);
                fonts.navNum->drawIn(num, Rect(rr.UpperLeftCorner.X + 8 * k, rr.UpperLeftCorner.Y, rr.UpperLeftCorner.X + 30 * k, rr.LowerRightCorner.Y),
                    sel ? c.accent : c.faint, HudFont::Left, 0.5f * k, &clip);
                const irr::f32 right = rr.LowerRightCorner.X - (pages[i].badge > 0 ? 20 * k : 6 * k);
                const irr::core::rect<irr::s32> nc = clipOf(Rect(rr.UpperLeftCorner.X, rr.UpperLeftCorner.Y, right, rr.LowerRightCorner.Y), clip);
                fonts.key->drawIn(hudUpper(pages[i].name), Rect(rr.UpperLeftCorner.X + 30 * k, rr.UpperLeftCorner.Y, right, rr.LowerRightCorner.Y),
                    (sel || hov) ? c.text : c.dim, HudFont::Left, 1.0f * k, &nc);
                if (pages[i].badge > 0) {
                    fonts.navNum->drawIn(std::to_wstring(pages[i].badge), Rect(rr.UpperLeftCorner.X, rr.UpperLeftCorner.Y, rr.LowerRightCorner.X - 7 * k, rr.LowerRightCorner.Y),
                        c.err, HudFont::Right, 0, &clip);
                }
            }
        }

        //Side column (or strip)
        fill(d, side, c.nav, &clip);
        if (!compact) { vline(d, side.UpperLeftCorner.X, side.UpperLeftCorner.Y, side.LowerRightCorner.Y, c.edge2, &clip); }
        else { hline(d, side.UpperLeftCorner.X, side.LowerRightCorner.X, side.UpperLeftCorner.Y, c.edge2, &clip); }
    }

    void Frame::drawFooter(irr::video::IVideoDriver* d, const std::vector<Status>& items, const std::wstring& hints,
        const irr::core::rect<irr::s32>& clip)
    {
        const Colours& c = colours();
        fill(d, footer, c.raised, &clip);
        hline(d, footer.UpperLeftCorner.X, footer.LowerRightCorner.X, footer.UpperLeftCorner.Y, c.edge, &clip);
        const irr::core::rect<irr::s32> fc = clipOf(Rect(footer.UpperLeftCorner.X, footer.UpperLeftCorner.Y, footer.LowerRightCorner.X - 12 * k, footer.LowerRightCorner.Y), clip);
        const Rect fr(footer.UpperLeftCorner.X, footer.UpperLeftCorner.Y, footer.LowerRightCorner.X, footer.LowerRightCorner.Y + 1 * k);
        irr::f32 x = footer.UpperLeftCorner.X + 12 * k;
        for (size_t i = 0; i < items.size(); i++) {
            const std::wstring label = hudUpper(items[i].label), value = hudUpper(items[i].value);
            if (!label.empty()) {
                const irr::f32 lw = fonts.navNum->width(label, 1.3f * k);
                fonts.navNum->drawIn(label, Rect(x, fr.UpperLeftCorner.Y, x + lw + 2 * k, fr.LowerRightCorner.Y), c.dim, HudFont::Left, 1.3f * k, &fc);
                x += lw + 6 * k;
            }
            if (!value.empty()) {
                const irr::f32 vw = fonts.state->width(value, 0.8f * k);
                fonts.state->drawIn(value, Rect(x, fr.UpperLeftCorner.Y, x + vw + 2 * k, fr.LowerRightCorner.Y), toneColour(items[i].tone), HudFont::Left, 0.8f * k, &fc);
                x += vw;
            }
            x += 20 * k;
        }
        const std::wstring h = hudUpper(hints);
        const irr::f32 hw = fonts.navNum->width(h, 1.3f * k);
        if (!h.empty() && x + hw < footer.LowerRightCorner.X - 12 * k) {
            fonts.navNum->drawIn(h, Rect(footer.LowerRightCorner.X - 12 * k - hw - 2 * k, fr.UpperLeftCorner.Y, footer.LowerRightCorner.X - 12 * k, fr.LowerRightCorner.Y),
                c.dim, HudFont::Right, 1.3f * k, &clip);
        }

        //The frame of the window, with the bracket marks of a console page at its corners
        frame(d, window, c.edge, &clip);
        const irr::f32 m = floorf(14 * k), t = std::max(2.0f, floorf(2 * k));
        const irr::f32 x0 = window.UpperLeftCorner.X, y0 = window.UpperLeftCorner.Y, x1 = window.LowerRightCorner.X, y1 = window.LowerRightCorner.Y;
        fill(d, Rect(x0, y0, x0 + m, y0 + t), c.accent, &clip); fill(d, Rect(x0, y0, x0 + t, y0 + m), c.accent, &clip);
        fill(d, Rect(x1 - m, y0, x1, y0 + t), c.accent, &clip); fill(d, Rect(x1 - t, y0, x1, y0 + m), c.accent, &clip);
        fill(d, Rect(x0, y1 - t, x0 + m, y1), c.accent, &clip); fill(d, Rect(x0, y1 - m, x0 + t, y1), c.accent, &clip);
        fill(d, Rect(x1 - m, y1 - t, x1, y1), c.accent, &clip); fill(d, Rect(x1 - t, y1 - m, x1, y1), c.accent, &clip);
    }
}
