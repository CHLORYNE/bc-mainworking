/* Shared drawing helpers for the scenario editors' charts. See ChartDraw.hpp. */
#include "ChartDraw.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "ChartView.hpp"

namespace ChartDraw {

void fillPolygon(irr::video::IVideoDriver* driver, const std::vector<irr::core::position2df>& pts, irr::video::SColor color)
{
    if (pts.size() < 3) { return; }
    std::vector<irr::video::S3DVertex> v;
    std::vector<irr::u16> idx;
    for (size_t i = 0; i < pts.size(); i++) {
        v.push_back(irr::video::S3DVertex(pts[i].X, pts[i].Y, 0.0f, 0, 0, 1, color, 0, 0));
    }
    for (size_t i = 1; i + 1 < pts.size(); i++) {   // triangle fan from point 0 (convex shapes)
        idx.push_back(0); idx.push_back((irr::u16)i); idx.push_back((irr::u16)(i + 1));
    }
    irr::video::SMaterial m;
    m.Lighting = false;
    m.MaterialType = irr::video::EMT_TRANSPARENT_VERTEX_ALPHA;
    driver->setMaterial(m);
    driver->draw2DVertexPrimitiveList(&v[0], (irr::u32)v.size(), &idx[0], (irr::u32)idx.size() / 3,
        irr::video::EVT_STANDARD, irr::scene::EPT_TRIANGLES, irr::video::EIT_16BIT);
}

void polyline(irr::video::IVideoDriver* driver, const std::vector<irr::core::position2di>& pts, irr::video::SColor color, bool dashed, irr::s32 width)
{
    for (size_t i = 0; i + 1 < pts.size(); i++) {
        irr::core::vector2df a((irr::f32)pts[i].X, (irr::f32)pts[i].Y), b((irr::f32)pts[i + 1].X, (irr::f32)pts[i + 1].Y);
        irr::core::vector2df d = b - a;
        irr::f32 len = d.getLength();
        if (len < 1.0f) { continue; }
        irr::core::vector2df u = d / len;
        irr::core::vector2df n(-u.Y, u.X);
        irr::f32 step = dashed ? 14.0f : len;
        irr::f32 on = dashed ? 8.0f : len;
        for (irr::f32 s = 0; s < len; s += step) {
            irr::f32 e = std::min(s + on, len);
            for (irr::s32 w = 0; w < width; w++) {
                irr::f32 off = (irr::f32)w - (irr::f32)(width - 1) / 2.0f;
                irr::core::vector2df p0 = a + u * s + n * off, p1 = a + u * e + n * off;
                driver->draw2DLine(irr::core::position2di((irr::s32)p0.X, (irr::s32)p0.Y),
                    irr::core::position2di((irr::s32)p1.X, (irr::s32)p1.Y), color);
            }
        }
    }
}

static void outline(irr::video::IVideoDriver* driver, const std::vector<irr::core::position2df>& pts, irr::video::SColor edge)
{
    std::vector<irr::core::position2di> o;
    for (size_t i = 0; i < pts.size(); i++) { o.push_back(irr::core::position2di((irr::s32)pts[i].X, (irr::s32)pts[i].Y)); }
    if (!o.empty()) { o.push_back(o[0]); }
    polyline(driver, o, edge, false, 1);
}

void marker(irr::video::IVideoDriver* driver, irr::core::position2di at, int shape, irr::video::SColor fill, irr::video::SColor edge, irr::s32 r)
{
    std::vector<irr::core::position2df> pts;
    irr::f32 fr = (irr::f32)r;
    if (shape == Square) {
        pts.push_back(irr::core::position2df(at.X - fr, at.Y - fr)); pts.push_back(irr::core::position2df(at.X + fr, at.Y - fr));
        pts.push_back(irr::core::position2df(at.X + fr, at.Y + fr)); pts.push_back(irr::core::position2df(at.X - fr, at.Y + fr));
    }
    else if (shape == Diamond) {
        pts.push_back(irr::core::position2df((irr::f32)at.X, at.Y - fr * 1.3f)); pts.push_back(irr::core::position2df(at.X + fr * 1.3f, (irr::f32)at.Y));
        pts.push_back(irr::core::position2df((irr::f32)at.X, at.Y + fr * 1.3f)); pts.push_back(irr::core::position2df(at.X - fr * 1.3f, (irr::f32)at.Y));
    }
    else {
        for (int i = 0; i < 14; i++) {
            irr::f32 a = (irr::f32)i / 14.0f * 6.2832f;
            pts.push_back(irr::core::position2df(at.X + fr * std::cos(a), at.Y + fr * std::sin(a)));
        }
    }
    fillPolygon(driver, pts, fill);
    outline(driver, pts, edge);
}

void ship(irr::video::IVideoDriver* driver, irr::core::position2di c, float heading, irr::video::SColor fill, irr::video::SColor edge, float size)
{
    irr::f32 h = heading * 3.14159265f / 180.0f;
    irr::core::vector2df fwd(std::sin(h), -std::cos(h)), right(std::cos(h), std::sin(h));
    const irr::f32 hull[5][2] = { { 0.0f, 1.0f }, { 0.38f, 0.35f }, { 0.38f, -1.0f }, { -0.38f, -1.0f }, { -0.38f, 0.35f } };
    std::vector<irr::core::position2df> pts;
    for (int i = 0; i < 5; i++) {
        irr::core::vector2df v = irr::core::vector2df((irr::f32)c.X, (irr::f32)c.Y) + fwd * (hull[i][1] * size) + right * (hull[i][0] * size);
        pts.push_back(irr::core::position2df(v.X, v.Y));
    }
    fillPolygon(driver, pts, fill);
    outline(driver, pts, edge);
}

void ring(irr::video::IVideoDriver* driver, irr::core::position2di centre, float radius, irr::video::SColor color)
{
    driver->draw2DPolygon(centre, radius, color, 20);
    driver->draw2DPolygon(centre, radius + 1.0f, color, 20);
}

void text(irr::gui::IGUIFont* font, const std::wstring& s, irr::core::position2di at, irr::video::SColor color,
    const irr::core::recti* clip, bool centred, bool lightBackground)
{
    if (!font) { return; }
    irr::core::dimension2du d = font->getDimension(s.c_str());
    irr::core::recti r(at.X, at.Y, at.X + (irr::s32)d.Width, at.Y + (irr::s32)d.Height);
    if (centred) { r -= irr::core::position2di((irr::s32)d.Width / 2, (irr::s32)d.Height / 2); }
    irr::video::SColor shadow = lightBackground ? irr::video::SColor(210, 255, 255, 255) : irr::video::SColor(200, 0, 0, 0);
    font->draw(s.c_str(), r + irr::core::position2di(1, 1), shadow, false, false, clip);
    font->draw(s.c_str(), r, color, false, false, clip);
}

void scaleBar(irr::video::IVideoDriver* driver, irr::gui::IGUIFont* font, const ChartView& chart, irr::s32 bottomMargin)
{
    const irr::core::recti& vp = chart.getViewport();
    double mpp = chart.metresPerPixel();
    double target = mpp * 150.0;
    double magnitude = std::pow(10.0, std::floor(std::log10(target)));
    double nice = magnitude;
    if (target / magnitude >= 5) { nice = 5 * magnitude; }
    else if (target / magnitude >= 2) { nice = 2 * magnitude; }
    irr::s32 px = (irr::s32)(nice / mpp);
    irr::s32 sx = vp.LowerRightCorner.X - px - 20, sy = vp.LowerRightCorner.Y - bottomMargin - 14;
    irr::video::SColor white(255, 255, 255, 255);
    driver->draw2DRectangle(irr::video::SColor(170, 0, 0, 0), irr::core::recti(sx - 8, sy - 20, sx + px + 8, sy + 6));
    driver->draw2DLine(irr::core::position2di(sx, sy), irr::core::position2di(sx + px, sy), white);
    driver->draw2DLine(irr::core::position2di(sx, sy - 5), irr::core::position2di(sx, sy + 3), white);
    driver->draw2DLine(irr::core::position2di(sx + px, sy - 5), irr::core::position2di(sx + px, sy + 3), white);
    wchar_t buf[32];
    if (nice >= 1000) { swprintf(buf, 32, nice >= 10000 ? L"%.0f km" : L"%.1f km", nice / 1000.0); }
    else { swprintf(buf, 32, L"%.0f m", nice); }
    text(font, buf, irr::core::position2di(sx + px / 2, sy - 11), white, &vp, true);
}

}
