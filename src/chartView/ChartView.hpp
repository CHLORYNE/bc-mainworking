/* Shared chart for the scenario editors (standard editor and fire editor).
   Loads a world's chart and draws it into a viewport with free zoom (about the cursor) and pan.
   Backgrounds ("styles"):
     - nautical chart, day or night palette, generated from the world's height map (depth
       bands, depth contours, coastline, shaded relief) - works for every world;
     - the world's original map image (MapImage / RadarImage in terrain.ini);
     - an optional high-resolution image supplied for the editor: EditorMapImage=<file> in
       terrain.ini, or editor_map.jpg / editor_map.png in the world folder, covering exactly the
       terrain's lat/long extent.
   Positions: lat/long, or X/Z metres east/north of the chart's south-west corner (the frame the
   standard editor and the simulator use). */
#ifndef __CHARTVIEW_HPP_INCLUDED__
#define __CHARTVIEW_HPP_INCLUDED__

#include <string>
#include <vector>

#include "irrlicht.h"
#include "../IncidentConfig.hpp"

class ChartView {
public:
    enum Style { Style_Day = 0, Style_Night = 1, Style_Original = 2, Style_HD = 3 };

    ChartView();
    ~ChartView();

    bool load(irr::IrrlichtDevice* device, const std::string& worldName, std::string& error);
    // Chart bounds from terrain.ini without loading anything else (false if they cannot be read).
    static bool worldBounds(const std::string& worldName, double& south, double& west, double& latExtent, double& longExtent);

    // Background
    void setStyle(int style);          // generates the chart on first use
    int getStyle() const { return style; }
    void nextStyle();                  // cycles through the available backgrounds
    bool hasHDImage() const { return hdTexture != 0; }
    std::wstring styleName() const;
    bool lightBackground() const { return style == Style_Day; }

    // View
    void setViewport(const irr::core::recti& r);
    const irr::core::recti& getViewport() const { return viewport; }
    void centreOn(const IncidentPoint& p);
    void centreOnXZ(double x, double z);
    void fitWorld();
    void zoomAt(irr::core::position2di screen, irr::f32 factor);
    void panPixels(irr::s32 dx, irr::s32 dy);
    double metresPerPixel() const { return mpp; }
    void setMetresPerPixel(double metres);
    IncidentPoint centre() const;
    void centreXZ(double& x, double& z) const { x = cx; z = cz; }

    // Conversions
    irr::core::position2di toScreen(const IncidentPoint& p) const;
    irr::core::position2di toScreenXZ(double x, double z) const;
    IncidentPoint toLatLong(irr::core::position2di screen) const;
    void toXZ(irr::core::position2di screen, double& x, double& z) const;
    double metresBetween(const IncidentPoint& a, const IncidentPoint& b) const;
    double lonToX(double lon) const;
    double latToZ(double lat) const;
    double xToLon(double x) const;
    double zToLat(double z) const;

    // Drawing
    void draw(irr::video::IVideoDriver* driver);
    // Lat/long grid with labels. topMargin / bottomMargin keep labels clear of bars over the chart.
    void drawGraticule(irr::video::IVideoDriver* driver, irr::gui::IGUIFont* font, irr::s32 topMargin, irr::s32 bottomMargin);

    double south, west, latExtent, longExtent;   // chart bounds, degrees
    double widthM, heightM;                      // chart size, metres

private:
    bool loadHeights(irr::IrrlichtDevice* device, const std::string& worldPath, const std::string& terrainFile, bool hdrOnly);
    irr::video::ITexture* buildChart(bool night);
    // Renders the chart for the world rectangle [x0,x1] x [z0,z1] (metres) into an outW x outH image.
    irr::video::IImage* renderRegion(double x0, double z0, double x1, double z1, irr::u32 outW, irr::u32 outH, bool night) const;
    void updateDetail();
    void dropDetail();
    irr::video::ITexture* makeTexture(irr::video::IImage* img, const std::string& name);
    float heightAt(double gx, double gz) const;
    void rememberStyle() const;
    static int recalledStyle();

    irr::video::IVideoDriver* driver;
    irr::ITimer* timer;
    std::string worldName;
    irr::video::ITexture* originalTexture;
    irr::video::ITexture* dayTexture;
    irr::video::ITexture* nightTexture;
    irr::video::ITexture* hdTexture;
    std::vector<irr::video::ITexture*> owned;   // textures this view created and must remove
    std::vector<float> heights;                 // primary terrain, heights[z * hCols + x], z = 0 is the south edge
    int hCols, hRows;
    int style;
    irr::u32 baseW;  // width of the whole-chart texture, pixels
    // Close-up: when zoomed in past the whole-chart texture's resolution, the visible area is
    // re-rendered at screen resolution once the view has settled, so coastlines stay sharp.
    irr::video::ITexture* detailTexture;
    double detailX0, detailZ0, detailX1, detailZ1, detailMpp;
    int detailStyle;
    double lastCx, lastCz, lastMpp;
    irr::u32 lastViewChange;
    double cx, cz;   // view centre, metres
    double mpp;      // metres per screen pixel
    irr::core::recti viewport;
};

#endif
