/* Shared drawing helpers for the scenario editors' charts: filled polygons, dashed lines, the
   ship symbol, markers, outlined text and the scale bar. */
#ifndef __CHARTDRAW_HPP_INCLUDED__
#define __CHARTDRAW_HPP_INCLUDED__

#include <string>
#include <vector>

#include "irrlicht.h"

class ChartView;

namespace ChartDraw {

enum Shape { Circle = 0, Square = 1, Diamond = 2 };

void fillPolygon(irr::video::IVideoDriver* driver, const std::vector<irr::core::position2df>& pts, irr::video::SColor color);
void polyline(irr::video::IVideoDriver* driver, const std::vector<irr::core::position2di>& pts, irr::video::SColor color, bool dashed, irr::s32 width);
void marker(irr::video::IVideoDriver* driver, irr::core::position2di at, int shape, irr::video::SColor fill, irr::video::SColor edge, irr::s32 r);
// Hull outline pointing along heading (degrees true), size = half-length in pixels.
void ship(irr::video::IVideoDriver* driver, irr::core::position2di centre, float heading, irr::video::SColor fill, irr::video::SColor edge, float size);
void ring(irr::video::IVideoDriver* driver, irr::core::position2di centre, float radius, irr::video::SColor color);
// Text with a one-pixel shadow (dark shadow, or light on a light chart).
void text(irr::gui::IGUIFont* font, const std::wstring& s, irr::core::position2di at, irr::video::SColor color,
    const irr::core::recti* clip, bool centred = false, bool lightBackground = false);
// Scale bar in the bottom-right corner of the chart's viewport, above bottomMargin pixels.
void scaleBar(irr::video::IVideoDriver* driver, irr::gui::IGUIFont* font, const ChartView& chart, irr::s32 bottomMargin);

}

#endif
