/*   NAUTITECH - Simulateur de Navigation
     Shared look for the tools around the simulator (settings lock, multiplayer hub, scenario editor):
     the launcher's dark navy theme, smooth rounded shapes and a custom-drawn button.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#ifndef __UITHEME_HPP_INCLUDED__
#define __UITHEME_HPP_INCLUDED__

//Header only: shapes are drawn with Irrlicht 2D triangles (GUIPanelDraw.hpp), so edges are smooth
//and nothing depends on image files.

#include "irrlicht.h"
#include "GUIPanelDraw.hpp"
#include <chrono>
#include <string>
#include <vector>
#include <set>
#include <map>

namespace Ui {

    //Colours, as in the launcher.
    const irr::video::SColor background(255, 9, 18, 33);
    const irr::video::SColor backgroundDeep(255, 5, 12, 24);
    const irr::video::SColor panelTop(255, 19, 36, 62);
    const irr::video::SColor panelBottom(255, 13, 26, 46);
    const irr::video::SColor cardTop(255, 24, 43, 71);
    const irr::video::SColor cardBottom(255, 18, 34, 58);
    const irr::video::SColor edge(90, 110, 160, 220);
    const irr::video::SColor rule(55, 140, 180, 230);
    const irr::video::SColor text(255, 238, 243, 250);
    const irr::video::SColor textDim(255, 160, 178, 200);
    const irr::video::SColor textFaint(255, 112, 130, 154);
    const irr::video::SColor accent(255, 64, 156, 240);
    const irr::video::SColor accentHi(255, 120, 190, 255);
    const irr::video::SColor primaryTop(255, 36, 122, 222);
    const irr::video::SColor primaryBottom(255, 16, 78, 168);
    const irr::video::SColor danger(255, 226, 84, 84);
    const irr::video::SColor warning(255, 255, 178, 64);
    const irr::video::SColor success(255, 70, 200, 130);
    const irr::video::SColor field(255, 8, 16, 30);

    inline irr::video::SColor mix(irr::video::SColor a, irr::video::SColor b, irr::f32 t)
    {
        return b.getInterpolated(a, irr::core::clamp(t, 0.0f, 1.0f));
    }

    inline irr::video::SColor alpha(irr::video::SColor c, irr::u32 a)
    {
        c.setAlpha(a);
        return c;
    }

    inline irr::core::rect<irr::f32> toF(const irr::core::rect<irr::s32>& r)
    {
        return irr::core::rect<irr::f32>((irr::f32)r.UpperLeftCorner.X, (irr::f32)r.UpperLeftCorner.Y,
            (irr::f32)r.LowerRightCorner.X, (irr::f32)r.LowerRightCorner.Y);
    }

    inline irr::core::rect<irr::s32> toI(const irr::core::rect<irr::f32>& r)
    {
        return irr::core::rect<irr::s32>((irr::s32)r.UpperLeftCorner.X, (irr::s32)r.UpperLeftCorner.Y,
            (irr::s32)r.LowerRightCorner.X, (irr::s32)r.LowerRightCorner.Y);
    }

    //Rounded rectangle, vertical gradient.
    inline void roundRect(irr::gui::PanelBatch& b, const irr::core::rect<irr::f32>& r, irr::f32 rad, irr::video::SColor top, irr::video::SColor bottom)
    {
        using irr::core::vector2df;
        const irr::f32 x0 = r.UpperLeftCorner.X, y0 = r.UpperLeftCorner.Y, x1 = r.LowerRightCorner.X, y1 = r.LowerRightCorner.Y;
        if (x1 <= x0 || y1 <= y0) { return; }
        rad = irr::core::min_(rad, irr::core::min_((x1 - x0) * 0.5f, (y1 - y0) * 0.5f));
        const irr::f32 h = y1 - y0;
        const irr::video::SColor cTop = mix(top, bottom, rad / h);
        const irr::video::SColor cBot = mix(top, bottom, 1.0f - rad / h);
        b.rectV(irr::core::rect<irr::f32>(x0 + rad, y0, x1 - rad, y0 + rad), top, cTop);
        b.rectV(irr::core::rect<irr::f32>(x0, y0 + rad, x1, y1 - rad), cTop, cBot);
        b.rectV(irr::core::rect<irr::f32>(x0 + rad, y1 - rad, x1 - rad, y1), cBot, bottom);
        b.sector(vector2df(x0 + rad, y0 + rad), 0, rad, 270, 360, cTop, top);
        b.sector(vector2df(x1 - rad, y0 + rad), 0, rad, 0, 90, cTop, top);
        b.sector(vector2df(x1 - rad, y1 - rad), 0, rad, 90, 180, cBot, bottom);
        b.sector(vector2df(x0 + rad, y1 - rad), 0, rad, 180, 270, cBot, bottom);
    }

    inline void roundRectOutline(irr::gui::PanelBatch& b, const irr::core::rect<irr::f32>& r, irr::f32 rad, irr::f32 w, irr::video::SColor col)
    {
        using irr::core::vector2df;
        const irr::f32 x0 = r.UpperLeftCorner.X, y0 = r.UpperLeftCorner.Y, x1 = r.LowerRightCorner.X, y1 = r.LowerRightCorner.Y;
        if (x1 <= x0 || y1 <= y0) { return; }
        rad = irr::core::min_(rad, irr::core::min_((x1 - x0) * 0.5f, (y1 - y0) * 0.5f));
        b.rect(irr::core::rect<irr::f32>(x0 + rad, y0, x1 - rad, y0 + w), col);
        b.rect(irr::core::rect<irr::f32>(x0 + rad, y1 - w, x1 - rad, y1), col);
        b.rect(irr::core::rect<irr::f32>(x0, y0 + rad, x0 + w, y1 - rad), col);
        b.rect(irr::core::rect<irr::f32>(x1 - w, y0 + rad, x1, y1 - rad), col);
        //Corners: the arcs get a soft 1px edge on each side, so their solid core is thinner and centred on the
        //straight edges' line, for the same weight all round.
        const irr::f32 core = irr::core::max_(0.2f, w - 1.1f), mid = rad - w * 0.5f, a = mid - core * 0.5f, z = mid + core * 0.5f;
        b.sector(vector2df(x0 + rad, y0 + rad), a, z, 270, 360, col, col);
        b.sector(vector2df(x1 - rad, y0 + rad), a, z, 0, 90, col, col);
        b.sector(vector2df(x1 - rad, y1 - rad), a, z, 90, 180, col, col);
        b.sector(vector2df(x0 + rad, y1 - rad), a, z, 180, 270, col, col);
    }

    //Card: soft shadow, gradient body, fine outline.
    inline void card(irr::gui::PanelBatch& b, const irr::core::rect<irr::f32>& r, irr::f32 rad = 12,
        irr::video::SColor top = cardTop, irr::video::SColor bottom = cardBottom)
    {
        irr::core::rect<irr::f32> s = r;
        s.UpperLeftCorner += irr::core::vector2df(0, 4);
        s.LowerRightCorner += irr::core::vector2df(0, 6);
        roundRect(b, s, rad, irr::video::SColor(90, 0, 0, 0), irr::video::SColor(90, 0, 0, 0));
        roundRect(b, r, rad, top, bottom);
        roundRectOutline(b, r, rad, 1.0f, edge);
    }

    //Padlock: body and shackle, about 2s wide.
    inline void padlock(irr::gui::PanelBatch& b, irr::core::vector2df c, irr::f32 s, irr::video::SColor col, bool open = false)
    {
        const irr::f32 lw = irr::core::max_(1.5f, s * 0.22f);
        roundRect(b, irr::core::rect<irr::f32>(c.X - s, c.Y - s * 0.15f, c.X + s, c.Y + s * 1.15f), s * 0.25f, col, col);
        const irr::f32 r1 = s * 0.68f, r0 = r1 - lw;
        const irr::f32 shift = open ? s * 0.55f : 0.0f; //an open lock has its shackle lifted
        const irr::core::vector2df top(c.X, c.Y - s * 0.55f - shift);
        b.sector(top, r0, r1, -90, 90, col, col);
        b.rect(irr::core::rect<irr::f32>(c.X - r1, top.Y, c.X - r0, c.Y - s * 0.15f), col);
        if (!open) { b.rect(irr::core::rect<irr::f32>(c.X + r0, top.Y, c.X + r1, c.Y - s * 0.15f), col); }
    }

    //Stations linked to a hub (multiplayer), in a box of half-size s.
    inline void networkIcon(irr::gui::PanelBatch& b, irr::core::vector2df c, irr::f32 s, irr::video::SColor col)
    {
        const irr::f32 lw = irr::core::max_(1.6f, s * 0.13f);
        const irr::core::vector2df hub(c.X, c.Y + s * 0.05f);
        const irr::core::vector2df n[3] = { irr::gui::panelPolar(hub, s * 0.78f, 0), irr::gui::panelPolar(hub, s * 0.78f, 120), irr::gui::panelPolar(hub, s * 0.78f, 240) };
        for (int i = 0; i < 3; i++) { b.line(hub, n[i], lw, col); }
        b.disc(hub, s * 0.24f, col, col);
        for (int i = 0; i < 3; i++) { b.sector(n[i], s * 0.17f, s * 0.17f + lw, 0, 360, col, col); }
    }

    inline irr::f32 textWidth(irr::gui::IGUIFont* font, const std::wstring& t)
    {
        return font ? (irr::f32)font->getDimension(t.c_str()).Width : 8.0f * t.size();
    }

    inline irr::f32 textHeight(irr::gui::IGUIFont* font)
    {
        return font ? (irr::f32)font->getDimension(L"Ag").Height : 14.0f;
    }

    //Bitmap font atlases pack the glyphs tightly: an overhang (the tail of J, j, y...) can reach into the
    //neighbouring glyph's rectangle and show as a stray dot under it. Removes from each glyph's rectangle
    //the ink that belongs to another glyph (connected to more of it in another rectangle). That ink is
    //never drawn with its own glyph, which only shows its own rectangle. Once per font.
    inline void cleanFontAtlas(irr::gui::IGUIFont* font)
    {
        static std::set<irr::gui::IGUIFont*> done;
        if (!font || font->getType() != irr::gui::EGFT_BITMAP || done.count(font)) { return; }
        done.insert(font);
        irr::gui::IGUISpriteBank* bank = static_cast<irr::gui::IGUIFontBitmap*>(font)->getSpriteBank();
        if (!bank || bank->getTextureCount() != 1) { return; }
        irr::video::ITexture* tex = bank->getTexture(0);
        if (!tex || tex->getColorFormat() != irr::video::ECF_A8R8G8B8) { return; }
        const irr::core::array<irr::core::rect<irr::s32> >& rects = bank->getPositions();
        const irr::s32 W = (irr::s32)tex->getSize().Width;
        irr::s32 H = 0;
        for (irr::u32 i = 0; i < rects.size(); i++) { H = irr::core::max_(H, rects[i].LowerRightCorner.Y); }
        H = irr::core::min_(H, (irr::s32)tex->getSize().Height);
        if (W <= 0 || H <= 0) { return; }
        irr::u32* pixels = static_cast<irr::u32*>(tex->lock(irr::video::ETLM_READ_WRITE));
        if (!pixels) { return; }
        const irr::s32 pitch = (irr::s32)(tex->getPitch() / 4);
        std::vector<irr::s32> owner((size_t)W * H, -1);
        for (irr::u32 i = 0; i < rects.size(); i++) {
            for (irr::s32 y = irr::core::max_(0, rects[i].UpperLeftCorner.Y); y < irr::core::min_(H, rects[i].LowerRightCorner.Y); y++) {
                for (irr::s32 x = irr::core::max_(0, rects[i].UpperLeftCorner.X); x < irr::core::min_(W, rects[i].LowerRightCorner.X); x++) {
                    owner[(size_t)y * W + x] = (irr::s32)i;
                }
            }
        }
        //Connected ink (4-neighbours; faint edge pixels left out, so that touching glyphs do not join), each
        //blob given to the rectangle holding most of it. A piece of it in another rectangle is erased only if
        //it is a small part of that rectangle's ink - an intrusion, never a glyph's own body.
        const irr::u32 inkAlpha = 64;
        std::vector<irr::s32> blob((size_t)W * H, -1);
        std::vector<std::map<irr::s32, irr::s32> > blobCounts;
        std::vector<irr::s32> rectInk(rects.size(), 0);
        std::vector<irr::s32> stack;
        for (irr::s32 start = 0; start < W * H; start++) {
            if (blob[start] >= 0 || (pixels[(start / W) * pitch + start % W] >> 24) < inkAlpha) { continue; }
            const irr::s32 id = (irr::s32)blobCounts.size();
            blobCounts.push_back(std::map<irr::s32, irr::s32>());
            stack.push_back(start);
            blob[start] = id;
            while (!stack.empty()) {
                const irr::s32 p = stack.back();
                stack.pop_back();
                blobCounts[id][owner[p]]++;
                if (owner[p] >= 0) { rectInk[owner[p]]++; }
                const irr::s32 px = p % W, py = p / W;
                const irr::s32 next[4][2] = { { px - 1, py }, { px + 1, py }, { px, py - 1 }, { px, py + 1 } };
                for (int k = 0; k < 4; k++) {
                    const irr::s32 nx = next[k][0], ny = next[k][1];
                    if (nx < 0 || ny < 0 || nx >= W || ny >= H) { continue; }
                    const irr::s32 q = ny * W + nx;
                    if (blob[q] >= 0 || (pixels[ny * pitch + nx] >> 24) < inkAlpha) { continue; }
                    blob[q] = id;
                    stack.push_back(q);
                }
            }
        }
        std::vector<irr::s32> best(blobCounts.size(), -1);
        for (size_t i = 0; i < blobCounts.size(); i++) {
            irr::s32 most = -1;
            for (std::map<irr::s32, irr::s32>::iterator it = blobCounts[i].begin(); it != blobCounts[i].end(); ++it) {
                if (it->first >= 0 && it->second > most) { best[i] = it->first; most = it->second; }
            }
        }
        std::vector<char> erased((size_t)W * H, 0);
        for (irr::s32 p = 0; p < W * H; p++) {
            const irr::s32 id = blob[p], r = owner[p];
            if (id < 0 || r < 0 || r == best[id]) { continue; }
            if (blobCounts[id][r] * 4 < rectInk[r]) {
                pixels[(p / W) * pitch + p % W] = 0;
                erased[p] = 1;
            }
        }
        //The faint edge of an erased piece goes with it.
        for (irr::s32 p = 0; p < W * H; p++) {
            if (blob[p] >= 0 || owner[p] < 0 || erased[p]) { continue; }
            const irr::s32 px = p % W, py = p / W;
            bool next = false;
            for (irr::s32 dy = -1; dy <= 1 && !next; dy++) {
                for (irr::s32 dx = -1; dx <= 1 && !next; dx++) {
                    const irr::s32 nx = px + dx, ny = py + dy;
                    if (nx >= 0 && ny >= 0 && nx < W && ny < H && erased[ny * W + nx] && owner[ny * W + nx] == owner[p]) { next = true; }
                }
            }
            if (next) { pixels[py * pitch + px] = 0; }
        }
        tex->unlock();
    }

    enum Align { Left, Centre, Right };

    //Text in a box, vertically centred.
    inline void drawText(irr::gui::IGUIFont* font, const std::wstring& t, const irr::core::rect<irr::f32>& box, irr::video::SColor col,
        Align align = Left, const irr::core::rect<irr::s32>* clip = 0)
    {
        if (!font || t.empty()) { return; }
        const irr::core::dimension2du d = font->getDimension(t.c_str());
        irr::s32 x = (irr::s32)box.UpperLeftCorner.X;
        if (align == Centre) { x = (irr::s32)(box.getCenter().X - d.Width * 0.5f); }
        if (align == Right) { x = (irr::s32)(box.LowerRightCorner.X - d.Width); }
        const irr::s32 y = (irr::s32)(box.getCenter().Y - d.Height * 0.5f);
        font->draw(t.c_str(), irr::core::rect<irr::s32>(x, y, x + (irr::s32)d.Width + 2, y + (irr::s32)d.Height), col, false, false, clip);
    }

    //Word-wrap text into the given width (explicit \n are kept).
    inline std::vector<std::wstring> wrap(irr::gui::IGUIFont* font, const std::wstring& t, irr::f32 width)
    {
        std::vector<std::wstring> lines;
        if (!font) { lines.push_back(t); return lines; }
        std::wstring line, word;
        for (size_t i = 0; i <= t.size(); i++) {
            const wchar_t ch = (i < t.size()) ? t[i] : L' ';
            if (ch == L' ' || ch == L'\n') {
                const std::wstring candidate = line.empty() ? word : line + L" " + word;
                if (!line.empty() && textWidth(font, candidate) > width) {
                    lines.push_back(line);
                    line = word;
                }
                else {
                    line = candidate;
                }
                word.clear();
                if (ch == L'\n') { lines.push_back(line); line.clear(); }
            }
            else {
                word += ch;
            }
        }
        if (!line.empty()) { lines.push_back(line); }
        return lines;
    }

    //Wrapped text from the top of a box; returns the height used.
    inline irr::f32 drawWrapped(irr::gui::IGUIFont* font, const std::wstring& t, const irr::core::rect<irr::f32>& box, irr::video::SColor col,
        irr::f32 lineGap = 2)
    {
        const std::vector<std::wstring> lines = wrap(font, t, box.getWidth());
        const irr::f32 lh = textHeight(font) + lineGap;
        irr::f32 y = box.UpperLeftCorner.Y;
        for (size_t i = 0; i < lines.size(); i++) {
            drawText(font, lines[i], irr::core::rect<irr::f32>(box.UpperLeftCorner.X, y, box.LowerRightCorner.X, y + lh), col);
            y += lh;
        }
        return y - box.UpperLeftCorner.Y;
    }

    //Skin colours for the standard controls (edit boxes, lists, combo boxes, check boxes), flat and dark
    //so that they sit well among the custom-drawn panels.
    inline void applySkin(irr::gui::IGUISkin* skin)
    {
        if (!skin) { return; }
        skin->setColor(irr::gui::EGDC_WINDOW, irr::video::SColor(255, 14, 26, 44));
        skin->setColor(irr::gui::EGDC_3D_FACE, irr::video::SColor(255, 28, 46, 72));
        skin->setColor(irr::gui::EGDC_3D_SHADOW, irr::video::SColor(255, 30, 50, 78));
        skin->setColor(irr::gui::EGDC_3D_DARK_SHADOW, irr::video::SColor(255, 40, 64, 96));
        skin->setColor(irr::gui::EGDC_3D_HIGH_LIGHT, irr::video::SColor(255, 40, 64, 96));
        skin->setColor(irr::gui::EGDC_3D_LIGHT, irr::video::SColor(255, 34, 56, 86));
        skin->setColor(irr::gui::EGDC_ACTIVE_BORDER, irr::video::SColor(255, 40, 120, 210));
        skin->setColor(irr::gui::EGDC_ACTIVE_CAPTION, text);
        skin->setColor(irr::gui::EGDC_INACTIVE_BORDER, irr::video::SColor(255, 28, 46, 72));
        skin->setColor(irr::gui::EGDC_INACTIVE_CAPTION, textDim);
        skin->setColor(irr::gui::EGDC_BUTTON_TEXT, text);
        skin->setColor(irr::gui::EGDC_GRAY_TEXT, textFaint);
        skin->setColor(irr::gui::EGDC_HIGH_LIGHT, irr::video::SColor(255, 36, 110, 200));
        skin->setColor(irr::gui::EGDC_HIGH_LIGHT_TEXT, irr::video::SColor(255, 255, 255, 255));
        skin->setColor(irr::gui::EGDC_EDITABLE, field);
        skin->setColor(irr::gui::EGDC_GRAY_EDITABLE, irr::video::SColor(255, 18, 28, 44));
        skin->setColor(irr::gui::EGDC_FOCUSED_EDITABLE, irr::video::SColor(255, 12, 26, 48));
        skin->setColor(irr::gui::EGDC_SCROLLBAR, irr::video::SColor(255, 12, 22, 38));
        skin->setColor(irr::gui::EGDC_WINDOW_SYMBOL, text);
        skin->setColor(irr::gui::EGDC_ICON, text);
        skin->setColor(irr::gui::EGDC_ICON_HIGH_LIGHT, irr::video::SColor(255, 255, 255, 255));
        skin->setColor(irr::gui::EGDC_TOOLTIP, text);
        skin->setColor(irr::gui::EGDC_TOOLTIP_BACKGROUND, irr::video::SColor(240, 18, 30, 46));
    }

    //Custom-drawn button. Sends EGET_BUTTON_CLICKED like an IGUIButton, so event receivers work as
    //they would with one.
    class Button : public irr::gui::IGUIElement
    {
    public:
        enum Kind { Primary, Secondary, Danger, Quiet };
        //A drawn symbol instead of the label (crisper than font glyphs).
        enum Glyph { NoGlyph, Plus, Minus, ChevronLeft, ChevronRight };

        Button(irr::gui::IGUIEnvironment* env, irr::gui::IGUIElement* parent, irr::s32 id, const irr::core::rect<irr::s32>& r,
            const wchar_t* label, Kind kind = Secondary)
            : irr::gui::IGUIElement(irr::gui::EGUIET_BUTTON, env, parent ? parent : env->getRootGUIElement(), id, r),
            kind(kind), glyph(NoGlyph), font(0), hovered(false), pressed(false), checked(false), hover(0), lastMs(0)
        {
            setTabStop(true);
            setText(label);
        }

        void setKind(Kind k) { kind = k; }
        void setGlyph(Glyph g) { glyph = g; }
        void setFont(irr::gui::IGUIFont* f) { font = f; }
        //A checked button is drawn as selected (for tabs and choices).
        void setChecked(bool c) { checked = c; }
        bool isChecked() const { return checked; }

        virtual bool OnEvent(const irr::SEvent& event)
        {
            if (!isEnabled()) { return IGUIElement::OnEvent(event); }
            if (event.EventType == irr::EET_GUI_EVENT && event.GUIEvent.Caller == this) {
                if (event.GUIEvent.EventType == irr::gui::EGET_ELEMENT_HOVERED) { hovered = true; }
                if (event.GUIEvent.EventType == irr::gui::EGET_ELEMENT_LEFT) { hovered = false; pressed = false; }
            }
            if (event.EventType == irr::EET_MOUSE_INPUT_EVENT) {
                if (event.MouseInput.Event == irr::EMIE_LMOUSE_PRESSED_DOWN) {
                    pressed = true;
                    Environment->setFocus(this);
                    return true;
                }
                if (event.MouseInput.Event == irr::EMIE_LMOUSE_LEFT_UP) {
                    const bool click = pressed && AbsoluteClippingRect.isPointInside(irr::core::position2di(event.MouseInput.X, event.MouseInput.Y));
                    pressed = false;
                    if (click) { fire(); }
                    return true;
                }
            }
            if (event.EventType == irr::EET_KEY_INPUT_EVENT && !event.KeyInput.PressedDown &&
                (event.KeyInput.Key == irr::KEY_RETURN || event.KeyInput.Key == irr::KEY_SPACE)) {
                fire();
                return true;
            }
            return IGUIElement::OnEvent(event);
        }

        virtual void draw()
        {
            if (!IsVisible) { return; }
            irr::video::IVideoDriver* driver = Environment->getVideoDriver();
            //Hover eases in and out over ~120 ms.
            const irr::u32 now = (irr::u32)std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            const irr::f32 dt = lastMs ? (irr::f32)(now - lastMs) / 1000.0f : 0.0f;
            lastMs = now;
            const bool on = hovered && isEnabled();
            hover = irr::core::clamp(hover + (on ? 1.0f : -1.0f) * dt / 0.12f, 0.0f, 1.0f);
            const irr::f32 k = hover;

            const irr::core::rect<irr::f32> r = toF(AbsoluteRect);
            const irr::f32 rad = irr::core::min_(8.0f, r.getHeight() * 0.5f);
            irr::gui::PanelBatch b;
            b.begin(driver);
            irr::video::SColor labelCol = text;
            if (!isEnabled()) {
                roundRect(b, r, rad, irr::video::SColor(255, 26, 38, 56), irr::video::SColor(255, 22, 32, 48));
                roundRectOutline(b, r, rad, 1.0f, irr::video::SColor(50, 150, 180, 220));
                labelCol = textFaint;
            }
            else if (kind == Primary || checked) {
                irr::video::SColor top = mix(primaryTop, irr::video::SColor(255, 66, 150, 240), k);
                irr::video::SColor bottom = mix(primaryBottom, irr::video::SColor(255, 26, 98, 196), k);
                roundRect(b, r, rad, pressed ? bottom : top, bottom);
                roundRectOutline(b, r, rad, 1.0f, irr::video::SColor((irr::u32)(80 + 90 * k), 190, 225, 255));
                labelCol = irr::video::SColor(255, 255, 255, 255);
            }
            else if (kind == Danger) {
                roundRect(b, r, rad, mix(irr::video::SColor(255, 64, 30, 38), irr::video::SColor(255, 150, 44, 50), k),
                    mix(irr::video::SColor(255, 52, 24, 32), irr::video::SColor(255, 124, 34, 40), k));
                roundRectOutline(b, r, rad, 1.0f, irr::video::SColor((irr::u32)(120 + 100 * k), 240, 110, 110));
                labelCol = mix(irr::video::SColor(255, 255, 196, 196), irr::video::SColor(255, 255, 255, 255), k);
            }
            else if (kind == Quiet) {
                if (k > 0.01f) { roundRect(b, r, rad, irr::video::SColor((irr::u32)(70 * k), 120, 180, 255), irr::video::SColor((irr::u32)(70 * k), 120, 180, 255)); }
                labelCol = mix(textDim, text, k);
            }
            else {
                roundRect(b, r, rad, mix(irr::video::SColor(255, 30, 50, 78), irr::video::SColor(255, 40, 66, 102), k),
                    mix(irr::video::SColor(255, 22, 38, 62), irr::video::SColor(255, 30, 52, 82), k));
                roundRectOutline(b, r, rad, 1.0f, mix(irr::video::SColor(110, 120, 170, 230), irr::video::SColor(220, 120, 190, 255), k));
            }
            if (Environment->hasFocus(this) && !pressed && kind != Quiet) {
                irr::core::rect<irr::f32> f = r;
                f.UpperLeftCorner -= irr::core::vector2df(2, 2);
                f.LowerRightCorner += irr::core::vector2df(2, 2);
                roundRectOutline(b, f, rad + 2, 1.0f, irr::video::SColor(120, 150, 205, 255));
            }
            if (glyph != NoGlyph) {
                const irr::core::vector2df c = r.getCenter();
                const irr::f32 g = irr::core::min_(r.getWidth(), r.getHeight()) * 0.2f;
                if (glyph == Plus || glyph == Minus) { b.line(irr::core::vector2df(c.X - g, c.Y), irr::core::vector2df(c.X + g, c.Y), 2.0f, labelCol); }
                if (glyph == Plus) { b.line(irr::core::vector2df(c.X, c.Y - g), irr::core::vector2df(c.X, c.Y + g), 2.0f, labelCol); }
                const irr::f32 dir = (glyph == ChevronLeft) ? -1.0f : 1.0f;
                if (glyph == ChevronLeft || glyph == ChevronRight) {
                    b.line(irr::core::vector2df(c.X - dir * g * 0.5f, c.Y - g), irr::core::vector2df(c.X + dir * g * 0.5f, c.Y), 2.0f, labelCol);
                    b.line(irr::core::vector2df(c.X + dir * g * 0.5f, c.Y), irr::core::vector2df(c.X - dir * g * 0.5f, c.Y + g), 2.0f, labelCol);
                }
            }
            b.flush();
            if (glyph != NoGlyph) { return; }
            irr::gui::IGUIFont* f = font ? font : Environment->getSkin()->getFont();
            const irr::core::rect<irr::s32> clip = AbsoluteClippingRect;
            drawText(f, Text.c_str(), r, labelCol, Centre, &clip);
        }

    private:
        void fire()
        {
            irr::SEvent e;
            e.EventType = irr::EET_GUI_EVENT;
            e.GUIEvent.Caller = this;
            e.GUIEvent.Element = 0;
            e.GUIEvent.EventType = irr::gui::EGET_BUTTON_CLICKED;
            if (Parent) { Parent->OnEvent(e); }
        }

        Kind kind;
        Glyph glyph;
        irr::gui::IGUIFont* font;
        bool hovered, pressed, checked;
        irr::f32 hover;
        irr::u32 lastMs;
    };

    //Rounded card behind a group of controls, optionally with a title. Draws itself, then its children.
    class Card : public irr::gui::IGUIElement
    {
    public:
        Card(irr::gui::IGUIEnvironment* env, irr::gui::IGUIElement* parent, const irr::core::rect<irr::s32>& r, const wchar_t* title = L"",
            irr::gui::IGUIFont* titleFont = 0)
            : irr::gui::IGUIElement(irr::gui::EGUIET_ELEMENT, env, parent ? parent : env->getRootGUIElement(), -1, r),
            titleFont(titleFont)
        {
            setText(title);
            setTabStop(false);
        }

        virtual void draw()
        {
            if (!IsVisible) { return; }
            const irr::core::rect<irr::f32> r = toF(AbsoluteRect);
            irr::gui::PanelBatch b;
            b.begin(Environment->getVideoDriver());
            card(b, r, 12);
            b.flush();
            if (Text.size() > 0) {
                irr::gui::IGUIFont* f = titleFont ? titleFont : Environment->getSkin()->getFont();
                drawText(f, Text.c_str(), irr::core::rect<irr::f32>(r.UpperLeftCorner.X + 16, r.UpperLeftCorner.Y + 8,
                    r.LowerRightCorner.X - 16, r.UpperLeftCorner.Y + 8 + textHeight(f) + 4), accentHi);
            }
            IGUIElement::draw();
        }

    private:
        irr::gui::IGUIFont* titleFont;
    };
}

#endif
