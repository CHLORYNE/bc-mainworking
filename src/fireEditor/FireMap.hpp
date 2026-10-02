/* SCENARIO INCENDIE - fire scenario editor: the chart. Loads the world's map image as one texture
   and draws it scaled into a viewport, with free zoom (mouse wheel, about the cursor) and pan.
   Positions are handled as lat/long; X/Z are metres east/north of the chart's SW corner, as in
   the standard editor. */
#ifndef __FIREMAP_HPP_INCLUDED__
#define __FIREMAP_HPP_INCLUDED__

#include <string>

#include "irrlicht.h"
#include "../IncidentConfig.hpp"

class FireMap {
public:
    FireMap();
    ~FireMap();

    bool load(irr::IrrlichtDevice* device, const std::string& worldName, std::string& error);

    void setViewport(const irr::core::recti& r);
    const irr::core::recti& getViewport() const { return viewport; }
    void centreOn(const IncidentPoint& p);
    void fitWorld();                                       // whole chart in view
    void zoomAt(irr::core::position2di screen, irr::f32 factor);
    void panPixels(irr::s32 dx, irr::s32 dy);

    irr::core::position2di toScreen(const IncidentPoint& p) const;
    IncidentPoint toLatLong(irr::core::position2di screen) const;
    double metresPerPixel() const { return mpp; }
    void setMetresPerPixel(double metres);
    double metresBetween(const IncidentPoint& a, const IncidentPoint& b) const;
    IncidentPoint centre() const;

    void draw(irr::video::IVideoDriver* driver);

    double south, west, latExtent, longExtent;   // chart bounds, degrees

private:
    double lonToX(double lon) const;
    double latToZ(double lat) const;
    double xToLon(double x) const;
    double zToLat(double z) const;

    irr::video::ITexture* texture;
    irr::video::IVideoDriver* driver;
    double widthM, heightM;   // chart size in metres
    double cx, cz;            // view centre, metres
    double mpp;               // metres per screen pixel
    irr::core::recti viewport;
};

#endif
