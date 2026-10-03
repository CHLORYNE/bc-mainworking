//Drawing pieces shared by the launcher's screens: colours, rounded rectangles, line icons.
//Everything is drawn with Irrlicht 2D triangles (GUIPanelDraw.hpp), so edges are smooth at any size.

#ifndef __LAUNCHER_LAUNCHERDRAW_HPP_INCLUDED__
#define __LAUNCHER_LAUNCHERDRAW_HPP_INCLUDED__

#include "irrlicht.h"
#include "../GUIPanelDraw.hpp"
#include <string>
#include <vector>

namespace Theme {
    const irr::video::SColor text(255, 244, 247, 251);
    const irr::video::SColor textDim(255, 178, 192, 208);
    const irr::video::SColor accent(255, 64, 156, 240);
    const irr::video::SColor accentHi(255, 120, 190, 255);
    const irr::video::SColor primaryTop(255, 36, 122, 222);
    const irr::video::SColor primaryBottom(255, 16, 78, 168);
    const irr::video::SColor glass(158, 9, 20, 36);
    const irr::video::SColor glassHover(190, 16, 34, 58);
    const irr::video::SColor border(64, 255, 255, 255);
    const irr::video::SColor danger(255, 214, 72, 72);
    const irr::video::SColor shade(255, 3, 10, 22);   //bottom gradient
}

inline irr::video::SColor mixColour(irr::video::SColor a, irr::video::SColor b, irr::f32 t)
{
    return b.getInterpolated(a, irr::core::clamp(t, 0.0f, 1.0f));
}

//Rounded rectangle, vertical gradient, soft edges.
inline void roundRect(irr::gui::PanelBatch& b, const irr::core::rect<irr::f32>& r, irr::f32 rad, irr::video::SColor top, irr::video::SColor bottom)
{
    using irr::core::vector2df;
    const irr::f32 x0 = r.UpperLeftCorner.X, y0 = r.UpperLeftCorner.Y, x1 = r.LowerRightCorner.X, y1 = r.LowerRightCorner.Y;
    rad = irr::core::min_(rad, irr::core::min_((x1 - x0) * 0.5f, (y1 - y0) * 0.5f));
    const irr::f32 h = y1 - y0;
    const irr::video::SColor cTop = mixColour(top, bottom, rad / h);
    const irr::video::SColor cBot = mixColour(top, bottom, 1.0f - rad / h);
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

enum LauncherIcon { Icon_Helm, Icon_Route, Icon_Flame, Icon_Network, Icon_Gear, Icon_Keys, Icon_Power, Icon_Compass, Icon_Lock, Icon_Back };

//Line icons, drawn in a box of half-size s around c.
inline void drawIcon(irr::gui::PanelBatch& b, LauncherIcon icon, irr::core::vector2df c, irr::f32 s, irr::video::SColor col)
{
    using irr::core::vector2df;
    using irr::gui::panelPolar;
    const irr::f32 lw = irr::core::max_(1.6f, s * 0.13f);
    switch (icon) {
    case Icon_Helm: {   //ship's wheel
        b.sector(c, s * 0.50f, s * 0.50f + lw, 0, 360, col, col);
        b.disc(c, s * 0.20f, col, col);
        for (int i = 0; i < 8; i++) {
            const irr::f32 a = 22.5f + 45.0f * i;
            b.line(panelPolar(c, s * 0.18f, a), panelPolar(c, s * 0.86f, a), lw, col);
            b.disc(panelPolar(c, s * 0.92f, a), lw * 0.85f, col, col);
        }
        break;
    }
    case Icon_Route: {  //planned track with waypoints
        const vector2df p[4] = { vector2df(c.X - s * 0.80f, c.Y + s * 0.62f), vector2df(c.X - s * 0.20f, c.Y + s * 0.05f),
            vector2df(c.X + s * 0.28f, c.Y + s * 0.38f), vector2df(c.X + s * 0.78f, c.Y - s * 0.55f) };
        for (int i = 0; i < 3; i++) { b.line(p[i], p[i + 1], lw, col); }
        for (int i = 0; i < 3; i++) { b.disc(p[i], lw * 1.25f, col, col); }
        b.sector(p[3], s * 0.18f, s * 0.18f + lw, 0, 360, col, col);
        break;
    }
    case Icon_Flame: {  //flame: outer tongues in the icon colour, a warm core inside
        static const irr::f32 outer[][2] = {
            { 0.00f, 1.00f }, { 0.42f, 0.86f }, { 0.66f, 0.52f }, { 0.64f, 0.10f }, { 0.50f, -0.24f }, { 0.36f, -0.02f },
            { 0.30f, -0.48f }, { 0.12f, -0.78f }, { -0.02f, -1.05f }, { -0.18f, -0.62f }, { -0.42f, -0.36f },
            { -0.56f, -0.58f }, { -0.64f, -0.12f }, { -0.66f, 0.40f }, { -0.44f, 0.84f } };
        static const irr::f32 inner[][2] = {
            { 0.00f, 0.98f }, { 0.30f, 0.84f }, { 0.38f, 0.50f }, { 0.24f, 0.10f }, { 0.04f, -0.30f },
            { -0.10f, 0.02f }, { -0.30f, 0.30f }, { -0.34f, 0.70f } };
        const int no = sizeof(outer) / sizeof(outer[0]), ni = sizeof(inner) / sizeof(inner[0]);
        const irr::f32 k = s * 0.90f;
        const vector2df co(c.X, c.Y + 0.30f * k), ci(c.X, c.Y + 0.55f * k);
        for (int i = 0; i < no; i++) {
            const int n = (i + 1) % no;
            b.tri(co, vector2df(c.X + outer[i][0] * k, c.Y + outer[i][1] * k), vector2df(c.X + outer[n][0] * k, c.Y + outer[n][1] * k), col);
        }
        const irr::video::SColor coreCol(col.getAlpha(), 255, 206, 96);
        for (int i = 0; i < ni; i++) {
            const int n = (i + 1) % ni;
            b.tri(ci, vector2df(c.X + inner[i][0] * k, c.Y + inner[i][1] * k), vector2df(c.X + inner[n][0] * k, c.Y + inner[n][1] * k), coreCol);
        }
        break;
    }
    case Icon_Network: {  //stations linked to a hub
        const vector2df hub(c.X, c.Y + s * 0.05f);
        const vector2df n[3] = { panelPolar(hub, s * 0.78f, 0), panelPolar(hub, s * 0.78f, 120), panelPolar(hub, s * 0.78f, 240) };
        for (int i = 0; i < 3; i++) { b.line(hub, n[i], lw, col); }
        b.disc(hub, s * 0.24f, col, col);
        for (int i = 0; i < 3; i++) { b.sector(n[i], s * 0.17f, s * 0.17f + lw, 0, 360, col, col); }
        break;
    }
    case Icon_Gear: {
        for (int i = 0; i < 8; i++) {
            const irr::f32 a = 45.0f * i;
            b.line(panelPolar(c, s * 0.50f, a), panelPolar(c, s * 0.90f, a), s * 0.30f, col);
        }
        b.sector(c, s * 0.28f, s * 0.66f, 0, 360, col, col);
        break;
    }
    case Icon_Keys: {   //keyboard
        const irr::core::rect<irr::f32> r(c.X - s * 0.95f, c.Y - s * 0.58f, c.X + s * 0.95f, c.Y + s * 0.58f);
        roundRectOutline(b, r, s * 0.18f, lw, col);
        for (int row = 0; row < 2; row++) {
            for (int k = 0; k < 4; k++) {
                const irr::f32 x = r.UpperLeftCorner.X + s * (0.38f + 0.40f * k), y = r.UpperLeftCorner.Y + s * (0.38f + 0.36f * row);
                b.rect(irr::core::rect<irr::f32>(x - s * 0.11f, y - s * 0.09f, x + s * 0.11f, y + s * 0.09f), col);
            }
        }
        b.rect(irr::core::rect<irr::f32>(c.X - s * 0.45f, c.Y + s * 0.26f, c.X + s * 0.45f, c.Y + s * 0.40f), col);
        break;
    }
    case Icon_Power: {
        b.sector(c, s * 0.62f, s * 0.62f + lw, 35, 325, col, col);
        b.line(vector2df(c.X, c.Y - s * 0.90f), vector2df(c.X, c.Y - s * 0.15f), lw, col);
        break;
    }
    case Icon_Lock: {   //padlock: body and shackle
        roundRect(b, irr::core::rect<irr::f32>(c.X - s * 0.62f, c.Y - s * 0.12f, c.X + s * 0.62f, c.Y + s * 0.78f), s * 0.16f, col, col);
        b.sector(vector2df(c.X, c.Y - s * 0.20f), s * 0.30f, s * 0.30f + lw, -90, 90, col, col);
        b.rect(irr::core::rect<irr::f32>(c.X - s * 0.30f - lw, c.Y - s * 0.20f, c.X - s * 0.30f, c.Y - s * 0.08f), col);
        b.rect(irr::core::rect<irr::f32>(c.X + s * 0.30f, c.Y - s * 0.20f, c.X + s * 0.30f + lw, c.Y - s * 0.08f), col);
        break;
    }
    case Icon_Back: {   //arrow to the left
        b.line(vector2df(c.X + s * 0.75f, c.Y), vector2df(c.X - s * 0.70f, c.Y), lw, col);
        b.line(vector2df(c.X - s * 0.72f, c.Y), vector2df(c.X - s * 0.18f, c.Y - s * 0.55f), lw, col);
        b.line(vector2df(c.X - s * 0.72f, c.Y), vector2df(c.X - s * 0.18f, c.Y + s * 0.55f), lw, col);
        break;
    }
    case Icon_Compass: {  //compass card: ring, cardinal marks, needle (north half solid)
        b.sector(c, s * 0.86f, s * 0.86f + lw, 0, 360, col, col);
        for (int i = 0; i < 4; i++) {
            b.line(panelPolar(c, s * 0.60f, 90.0f * i), panelPolar(c, s * 0.80f, 90.0f * i), lw, col);
        }
        const vector2df north = panelPolar(c, s * 0.56f, 0), south = panelPolar(c, s * 0.56f, 180);
        const vector2df west = panelPolar(c, s * 0.19f, 270), east = panelPolar(c, s * 0.19f, 90);
        b.tri(north, east, west, col);
        b.tri(south, west, east, irr::video::SColor(col.getAlpha() / 2, col.getRed(), col.getGreen(), col.getBlue()));
        break;
    }
    }
}

//Word-wrap text into the given width.
inline std::vector<std::wstring> wrapText(irr::gui::IGUIFont* font, const std::wstring& text, irr::s32 width)
{
    std::vector<std::wstring> lines;
    if (!font) { lines.push_back(text); return lines; }
    std::wstring line, word;
    for (size_t i = 0; i <= text.size(); i++) {
        const wchar_t ch = (i < text.size()) ? text[i] : L' ';
        if (ch == L' ' || ch == L'\n') {
            const std::wstring candidate = line.empty() ? word : line + L" " + word;
            if (!line.empty() && (irr::s32)font->getDimension(candidate.c_str()).Width > width) {
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

#endif
