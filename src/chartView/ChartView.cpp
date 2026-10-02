/* Shared chart for the scenario editors. See ChartView.hpp.
   Source kept ASCII: accented text is written with \u escapes. */
#define _CRT_SECURE_NO_WARNINGS
#include "ChartView.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>

#include "../Utilities.hpp"

namespace {

const double kEarthRadiusM = 6.371e6;
const double kPi = 3.14159265358979;

struct RGB { float r, g, b; };

RGB mix(const RGB& a, const RGB& b, float t)
{
    if (t < 0) { t = 0; }
    if (t > 1) { t = 1; }
    RGB o = { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t };
    return o;
}

// Depth bands (m) shared by both palettes; colours run shallow -> deep.
const float kDepthBands[] = { 2.0f, 5.0f, 10.0f, 20.0f, 50.0f };
const float kContours[] = { 2.0f, 5.0f, 10.0f, 20.0f, 50.0f, 100.0f };

struct Palette {
    RGB sea[6];        // per depth band, shallow first
    RGB land[4];       // 0 m, 50 m, 200 m, 500 m
    RGB coast;
    RGB contour;
    RGB outside;       // beyond the chart
};

const Palette kDay = {
    { { 132, 186, 230 }, { 162, 204, 238 }, { 191, 221, 244 }, { 213, 233, 248 }, { 229, 241, 251 }, { 243, 248, 253 } },
    { { 240, 228, 192 }, { 226, 210, 165 }, { 205, 189, 145 }, { 182, 168, 130 } },
    { 92, 80, 58 },
    { 118, 152, 186 },
    { 236, 242, 247 }
};

const Palette kNight = {
    { { 44, 86, 120 }, { 35, 72, 104 }, { 28, 60, 91 }, { 22, 50, 78 }, { 17, 41, 66 }, { 12, 31, 52 } },
    { { 66, 70, 58 }, { 74, 77, 62 }, { 82, 82, 66 }, { 92, 90, 72 } },
    { 205, 194, 150 },
    { 72, 108, 144 },
    { 9, 22, 38 }
};

// Height maps often hold flat areas at exactly a band or contour depth (a bay filled at -10 m, say);
// rounding then flips pixels either side of the level and draws stripes. Levels are compared a few
// centimetres off, so such a plateau falls cleanly on one side.
const float kLevelTolerance = 0.05f;

RGB seaColour(const Palette& p, float depth)
{
    int band = 0;
    while (band < 5 && depth >= kDepthBands[band] + kLevelTolerance) { band++; }
    return p.sea[band];
}

RGB landColour(const Palette& p, float h)
{
    if (h < 50.0f) { return mix(p.land[0], p.land[1], h / 50.0f); }
    if (h < 200.0f) { return mix(p.land[1], p.land[2], (h - 50.0f) / 150.0f); }
    return mix(p.land[2], p.land[3], (h - 200.0f) / 300.0f);
}

bool isLand(float h)
{
    return h >= -kLevelTolerance;
}

int contourBand(float h)
{
    if (isLand(h)) { return -1; }
    float d = -h;
    int band = 0;
    while (band < 6 && d >= kContours[band] + kLevelTolerance) { band++; }
    return band;
}

std::string lowerExt(const std::string& name)
{
    size_t dot = name.find_last_of('.');
    std::string ext = (dot == std::string::npos) ? "" : name.substr(dot);
    Utilities::to_lower(ext);
    return ext;
}

std::string stylePath()
{
    return Utilities::getUserDir() + "editorChartStyle.txt";
}

} // namespace

ChartView::ChartView()
    : south(0), west(0), latExtent(1), longExtent(1), widthM(1), heightM(1),
    driver(0), timer(0), originalTexture(0), dayTexture(0), nightTexture(0), hdTexture(0),
    hCols(0), hRows(0), style(Style_Day), baseW(0),
    detailTexture(0), detailX0(0), detailZ0(0), detailX1(0), detailZ1(0), detailMpp(0), detailStyle(-1),
    lastCx(0), lastCz(0), lastMpp(0), lastViewChange(0), cx(0), cz(0), mpp(10)
{
}

ChartView::~ChartView()
{
    if (driver) {
        for (size_t i = 0; i < owned.size(); i++) { if (owned[i]) { driver->removeTexture(owned[i]); } }
    }
}

bool ChartView::load(irr::IrrlichtDevice* device, const std::string& world, std::string& error)
{
    driver = device->getVideoDriver();
    timer = device->getTimer();
    worldName = world;

    // Same lookup as the standard editor: user folder first, terrain.ini or a lone .hdr height
    // map, the chart image from MapImage (or RadarImage).
    std::string worldPath = "World/" + world;
    std::string userFolder = Utilities::getUserDir();
    if (Utilities::pathExists(userFolder + worldPath)) { worldPath = userFolder + worldPath; }

    std::string terrainFile = worldPath + "/terrain.ini";
    bool usingHdrOnly = false;
    if (!Utilities::pathExists(terrainFile)) {
        irr::io::IFileSystem* fs = device->getFileSystem();
        irr::io::path cwd = fs->getWorkingDirectory();
        if (fs->changeWorkingDirectoryTo(worldPath.c_str())) {
            irr::io::IFileList* list = fs->createFileList();
            if (list) {
                for (irr::u32 i = 0; i < list->getFileCount(); i++) {
                    if (!list->isDirectory(i) && irr::core::hasFileExtension(list->getFileName(i), "hdr")) {
                        terrainFile = worldPath + "/" + list->getFileName(i).c_str();
                        usingHdrOnly = true;
                    }
                }
                list->drop();
            }
        }
        fs->changeWorkingDirectoryTo(cwd);
    }

    IncidentIni::Map terrain;
    if (!IncidentIni::read(terrainFile, terrain)) {
        error = "Impossible de lire " + terrainFile;
        return false;
    }

    std::string mapName;
    if (usingHdrOnly) {
        west = IncidentIni::num(terrain, "left_map_x");
        south = IncidentIni::num(terrain, "lower_map_y");
        longExtent = IncidentIni::num(terrain, "right_map_x") - west;
        latExtent = IncidentIni::num(terrain, "upper_map_y") - south;
        mapName = std::string(device->getFileSystem()->getFileBasename(terrainFile.c_str(), false).c_str()) + ".bmp";
    }
    else {
        west = IncidentIni::num(terrain, "TerrainLong(1)");
        south = IncidentIni::num(terrain, "TerrainLat(1)");
        longExtent = IncidentIni::num(terrain, "TerrainLongExtent(1)");
        latExtent = IncidentIni::num(terrain, "TerrainLatExtent(1)");
        mapName = IncidentIni::str(terrain, "MapImage");
        if (mapName.empty()) { mapName = IncidentIni::str(terrain, "RadarImage"); }
    }

    // A .hdr first height map carries the real bounds.
    std::string heightMap = IncidentIni::str(terrain, "HeightMap(1)");
    if (lowerExt(heightMap) == ".hdr") {
        IncidentIni::Map hdr;
        if (IncidentIni::read(worldPath + "/" + heightMap, hdr)) {
            west = IncidentIni::num(hdr, "left_map_x");
            south = IncidentIni::num(hdr, "lower_map_y");
            longExtent = IncidentIni::num(hdr, "right_map_x") - west;
            latExtent = IncidentIni::num(hdr, "upper_map_y") - south;
        }
    }
    if (longExtent <= 0 || latExtent <= 0) {
        error = "Dimensions de carte invalides dans " + terrainFile;
        return false;
    }
    widthM = longExtent * 2.0 * kPi * kEarthRadiusM * std::cos((south + latExtent / 2.0) * kPi / 180.0) / 360.0;
    heightM = latExtent * 2.0 * kPi * kEarthRadiusM / 360.0;
    cx = widthM / 2.0;
    cz = heightM / 2.0;

    driver->setTextureCreationFlag(irr::video::ETCF_CREATE_MIP_MAPS, false);
    if (!mapName.empty()) {
        originalTexture = driver->getTexture((worldPath + "/" + mapName).c_str());
    }
    // Optional high-resolution background for the editor.
    std::string hdName = IncidentIni::str(terrain, "EditorMapImage");
    const char* hdDefaults[3] = { "editor_map.jpg", "editor_map.png", "editor_map.bmp" };
    for (int i = 0; i < 3 && hdName.empty(); i++) {
        if (Utilities::pathExists(worldPath + "/" + hdDefaults[i])) { hdName = hdDefaults[i]; }
    }
    if (!hdName.empty()) { hdTexture = driver->getTexture((worldPath + "/" + hdName).c_str()); }
    driver->setTextureCreationFlag(irr::video::ETCF_CREATE_MIP_MAPS, true);

    loadHeights(device, worldPath, terrainFile, usingHdrOnly);

    if (!originalTexture && heights.empty() && !hdTexture) {
        error = "Ni image de carte ni carte des hauteurs utilisable pour " + world;
        return false;
    }
    int wanted = recalledStyle();
    if (wanted == Style_HD && !hdTexture) { wanted = Style_Day; }
    setStyle(wanted);
    return true;
}

bool ChartView::worldBounds(const std::string& world, double& s, double& w, double& latE, double& lonE)
{
    std::string worldPath = "World/" + world;
    std::string userFolder = Utilities::getUserDir();
    if (Utilities::pathExists(userFolder + worldPath)) { worldPath = userFolder + worldPath; }
    IncidentIni::Map terrain;
    if (!IncidentIni::read(worldPath + "/terrain.ini", terrain)) { return false; }
    w = IncidentIni::num(terrain, "TerrainLong(1)");
    s = IncidentIni::num(terrain, "TerrainLat(1)");
    lonE = IncidentIni::num(terrain, "TerrainLongExtent(1)");
    latE = IncidentIni::num(terrain, "TerrainLatExtent(1)");
    std::string heightMap = IncidentIni::str(terrain, "HeightMap(1)");
    IncidentIni::Map hdr;
    if (lowerExt(heightMap) == ".hdr" && IncidentIni::read(worldPath + "/" + heightMap, hdr)) {
        w = IncidentIni::num(hdr, "left_map_x");
        s = IncidentIni::num(hdr, "lower_map_y");
        lonE = IncidentIni::num(hdr, "right_map_x") - w;
        latE = IncidentIni::num(hdr, "upper_map_y") - s;
    }
    return lonE > 0 && latE > 0;
}

bool ChartView::loadHeights(irr::IrrlichtDevice* device, const std::string& worldPath, const std::string& terrainFile, bool hdrOnly)
{
    heights.clear();
    hCols = hRows = 0;
    IncidentIni::Map terrain;
    IncidentIni::read(terrainFile, terrain);

    // Mirrors Terrain::load for the primary terrain: heights[z][x], z = 0 at the south edge.
    std::string hmName = hdrOnly ? std::string() : IncidentIni::str(terrain, "HeightMap(1)");
    std::string hdrPath = hdrOnly ? terrainFile : (lowerExt(hmName) == ".hdr" ? worldPath + "/" + hmName : std::string());

    if (!hdrPath.empty()) {
        IncidentIni::Map hdr;
        if (!IncidentIni::read(hdrPath, hdr)) { return false; }
        int rows = (int)IncidentIni::num(hdr, "number_of_rows");
        int cols = (int)IncidentIni::num(hdr, "number_of_columns");
        bool fp = IncidentIni::str(hdr, "data_format") == "float32";
        std::string binPath = hdrPath.substr(0, hdrPath.size() - 3) + "bin";
        std::ifstream in(binPath.c_str(), std::ios::binary);
        if (!in || rows <= 1 || cols <= 1) { return false; }
        heights.assign((size_t)rows * cols, -1000.0f);
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                float v = -1000.0f;
                if (fp) { float f; if (in.read((char*)&f, 4) && std::isfinite(f)) { v = f; } }
                else { short s; if (in.read((char*)&s, 2)) { v = (float)s; } }
                heights[(size_t)r * cols + c] = v;
            }
        }
        hRows = rows; hCols = cols;
        return true;
    }

    std::string ext = lowerExt(hmName);
    std::string path = worldPath + "/" + hmName;
    if (ext == ".f32") {
        int rows = (int)IncidentIni::num(terrain, "TerrainHeightMapRows(1)");
        int cols = (int)IncidentIni::num(terrain, "TerrainHeightMapColumns(1)");
        bool legacy = false;
        if (rows == 0 || cols == 0) {   // legacy square file, stored transposed
            rows = cols = (int)IncidentIni::num(terrain, "TerrainHeightMapSize(1)");
            legacy = true;
        }
        std::ifstream in(path.c_str(), std::ios::binary);
        if (!in || rows <= 1 || cols <= 1) { return false; }
        std::vector<float> raw((size_t)rows * cols, -1000.0f);
        in.read((char*)&raw[0], (std::streamsize)(raw.size() * sizeof(float)));
        // As Terrain::heightMapBinaryToVector: anything not a normal float (NaN, inf, 0) is "no data", deep water.
        for (size_t i = 0; i < raw.size(); i++) { if (!std::isnormal(raw[i])) { raw[i] = -1000.0f; } }
        if (legacy) {
            heights.assign(raw.size(), -1000.0f);
            for (int r = 0; r < rows; r++) {
                for (int c = 0; c < cols; c++) { heights[(size_t)c * rows + r] = raw[(size_t)r * cols + c]; }
            }
            hRows = cols; hCols = rows;
        }
        else {
            heights.swap(raw);
            hRows = rows; hCols = cols;
        }
        return true;
    }

    // Image height map
    irr::video::IImage* img = device->getVideoDriver()->createImageFromFile(path.c_str());
    if (!img) { return false; }
    bool rgb = (int)IncidentIni::num(terrain, "UsesRGB(1)") > 0;
    float maxHeight = (float)IncidentIni::num(terrain, "TerrainMaxHeight(1)");
    float seaDepth = (float)IncidentIni::num(terrain, "SeaMaxDepth(1)");
    int w = (int)img->getDimension().Width, h = (int)img->getDimension().Height;
    heights.assign((size_t)w * h, -1000.0f);
    for (int z = 0; z < h; z++) {
        for (int x = 0; x < w; x++) {
            irr::video::SColor c = img->getPixel(x, h - 1 - z);
            float v;
            if (rgb) { v = (float)c.getRed() * 256.0f + (float)c.getGreen() + (float)c.getBlue() / 256.0f - 32768.0f; }
            else { v = c.getLightness() * (maxHeight + seaDepth) / 255.0f - seaDepth; }
            heights[(size_t)z * w + x] = v;
        }
    }
    img->drop();
    hRows = h; hCols = w;
    return true;
}

float ChartView::heightAt(double gx, double gz) const
{
    if (gx < 0) { gx = 0; }
    if (gz < 0) { gz = 0; }
    if (gx > hCols - 1) { gx = hCols - 1; }
    if (gz > hRows - 1) { gz = hRows - 1; }
    int x0 = (int)gx, z0 = (int)gz;
    int x1 = std::min(x0 + 1, hCols - 1), z1 = std::min(z0 + 1, hRows - 1);
    float fx = (float)(gx - x0), fz = (float)(gz - z0);
    float a = heights[(size_t)z0 * hCols + x0], b = heights[(size_t)z0 * hCols + x1];
    float c = heights[(size_t)z1 * hCols + x0], d = heights[(size_t)z1 * hCols + x1];
    return (a * (1 - fx) + b * fx) * (1 - fz) + (c * (1 - fx) + d * fx) * fz;
}

// Box blur, in place, run twice: close to a Gaussian. Smooths the height field at the scale of the
// height map's own grid, so coastlines and contours come out rounded instead of stair-stepped.
static void blurField(std::vector<float>& f, irr::u32 w, irr::u32 h, int r)
{
    if (r < 1) { return; }
    std::vector<float> tmp(f.size());
    for (int pass = 0; pass < 2; pass++) {
        for (irr::u32 y = 0; y < h; y++) {           // horizontal
            const float* row = &f[(size_t)y * w];
            float* out = &tmp[(size_t)y * w];
            double sum = 0;
            for (int k = -r; k <= r; k++) { sum += row[std::min<int>(std::max(k, 0), (int)w - 1)]; }
            for (irr::u32 x = 0; x < w; x++) {
                out[x] = (float)(sum / (2 * r + 1));
                int add = std::min<int>((int)x + r + 1, (int)w - 1), rem = std::max<int>((int)x - r, 0);
                sum += row[add] - row[rem];
            }
        }
        for (irr::u32 x = 0; x < w; x++) {           // vertical
            double sum = 0;
            for (int k = -r; k <= r; k++) { sum += tmp[(size_t)std::min<int>(std::max(k, 0), (int)h - 1) * w + x]; }
            for (irr::u32 y = 0; y < h; y++) {
                f[(size_t)y * w + x] = (float)(sum / (2 * r + 1));
                int add = std::min<int>((int)y + r + 1, (int)h - 1), rem = std::max<int>((int)y - r, 0);
                sum += tmp[(size_t)add * w + x] - tmp[(size_t)rem * w + x];
            }
        }
    }
}

// Nautical chart from the height map: depth bands, contours, coastline, shaded land.
irr::video::IImage* ChartView::renderRegion(double x0, double z0, double x1, double z1, irr::u32 outW, irr::u32 outH, bool night) const
{
    if (heights.empty() || hCols < 2 || hRows < 2 || outW < 2 || outH < 2) { return 0; }
    const Palette& pal = night ? kNight : kDay;

    // Heights at each pixel centre, from the grid (gx, gz in grid units).
    const double cellX = widthM / (hCols - 1), cellZ = heightM / (hRows - 1);
    const double mx = (x1 - x0) / outW, mz = (z1 - z0) / outH;
    std::vector<float> hb((size_t)outW * outH);
    for (irr::u32 py = 0; py < outH; py++) {
        double gz = (z1 - (py + 0.5) * mz) / cellZ;
        for (irr::u32 px = 0; px < outW; px++) {
            double gx = (x0 + (px + 0.5) * mx) / cellX;
            // Deeper than the last contour shows the same either way; capping it stops "no data"
            // cells (-1000 m) from dragging the shallow bands into hard grid-shaped steps.
            hb[(size_t)py * outW + px] = std::max(-120.0f, heightAt(gx, gz));
        }
    }
    int radius = (int)std::floor(0.55 * cellX / mx + 0.5);
    blurField(hb, outW, outH, std::min(radius, 40));

    irr::video::IImage* img = driver->createImage(irr::video::ECF_A8R8G8B8, irr::core::dimension2du(outW, outH));
    if (!img) { return 0; }
    const float fmx = (float)mx, fmz = (float)mz;
    const float lx = -0.5f, ly = 0.5f, lz = 0.7071f;   // light from the north-west, 45 degrees up
    for (irr::u32 py = 0; py < outH; py++) {
        for (irr::u32 px = 0; px < outW; px++) {
            size_t i = (size_t)py * outW + px;
            float h = hb[i];
            float hr = (px + 1 < outW) ? hb[i + 1] : h;
            float hd = (py + 1 < outH) ? hb[i + outW] : h;
            float hl = (px > 0) ? hb[i - 1] : h;
            float hu = (py > 0) ? hb[i - outW] : h;

            RGB col;
            bool coast = (isLand(h) != isLand(hr)) || (isLand(h) != isLand(hd));
            if (coast) {
                col = pal.coast;
            }
            else if (isLand(h)) {
                // Hillshade, relief exaggerated so low coastal ground still reads.
                float dzdx = (hr - hl) / (2.0f * fmx) * 3.0f;
                float dzdy = (hu - hd) / (2.0f * fmz) * 3.0f;   // northward
                float len = std::sqrt(dzdx * dzdx + dzdy * dzdy + 1.0f);
                float shade = (-dzdx * lx - dzdy * ly + lz) / len / lz;
                shade = 0.62f + 0.38f * shade;
                if (shade < 0.45f) { shade = 0.45f; }
                if (shade > 1.12f) { shade = 1.12f; }
                RGB base = landColour(pal, h);
                col.r = std::min(255.0f, base.r * shade);
                col.g = std::min(255.0f, base.g * shade);
                col.b = std::min(255.0f, base.b * shade);
            }
            else {
                col = seaColour(pal, -h);
                int band = contourBand(h);
                if (band != contourBand(hr) || band != contourBand(hd)) { col = mix(col, pal.contour, 0.85f); }
            }
            img->setPixel(px, py, irr::video::SColor(255, (irr::u32)col.r, (irr::u32)col.g, (irr::u32)col.b));
        }
    }
    return img;
}

irr::video::ITexture* ChartView::makeTexture(irr::video::IImage* img, const std::string& name)
{
    if (!img) { return 0; }
    driver->setTextureCreationFlag(irr::video::ETCF_CREATE_MIP_MAPS, false);
    irr::video::ITexture* tex = driver->addTexture(name.c_str(), img);
    driver->setTextureCreationFlag(irr::video::ETCF_CREATE_MIP_MAPS, true);
    img->drop();
    if (tex) { owned.push_back(tex); }
    return tex;
}

irr::video::ITexture* ChartView::buildChart(bool night)
{
    if (heights.empty()) { return 0; }
    irr::u32 maxTex = std::min(4096u, std::min(driver->getMaxTextureSize().Width, driver->getMaxTextureSize().Height));
    if (maxTex < 512) { maxTex = 512; }
    // About three image pixels per height sample for the whole-chart view.
    irr::u32 base = std::min(maxTex, (irr::u32)std::max(hCols, hRows) * 3);
    irr::u32 outW, outH;
    if (widthM >= heightM) { outW = base; outH = std::max(2u, (irr::u32)(base * heightM / widthM)); }
    else { outH = base; outW = std::max(2u, (irr::u32)(base * widthM / heightM)); }
    baseW = outW;
    return makeTexture(renderRegion(0.0, 0.0, widthM, heightM, outW, outH, night),
        std::string(night ? "chart-night:" : "chart-day:") + worldName);
}

void ChartView::dropDetail()
{
    if (!detailTexture) { return; }
    std::vector<irr::video::ITexture*>::iterator it = std::find(owned.begin(), owned.end(), detailTexture);
    if (it != owned.end()) { owned.erase(it); }
    driver->removeTexture(detailTexture);
    detailTexture = 0;
}

void ChartView::updateDetail()
{
    if ((style != Style_Day && style != Style_Night) || heights.empty() || baseW == 0 || !timer) { return; }
    double basePx = widthM / baseW;   // metres per whole-chart texel
    if (mpp > basePx * 0.8) { return; }

    irr::u32 now = timer->getRealTime();
    if (cx != lastCx || cz != lastCz || mpp != lastMpp) {   // still moving: wait until it settles
        lastCx = cx; lastCz = cz; lastMpp = mpp;
        lastViewChange = now;
        return;
    }
    if (now - lastViewChange < 150) { return; }

    double vx0, vz0, vx1, vz1;
    toXZ(viewport.UpperLeftCorner, vx0, vz1);
    toXZ(viewport.LowerRightCorner, vx1, vz0);
    if (detailTexture && detailStyle == style && std::fabs(detailMpp - mpp) < mpp * 0.01 &&
        vx0 >= detailX0 && vx1 <= detailX1 && vz0 >= detailZ0 && vz1 <= detailZ1) {
        return;   // what is on screen is already covered
    }
    // Visible area plus a margin for panning, clamped to the chart.
    double mxm = (vx1 - vx0) * 0.25, mzm = (vz1 - vz0) * 0.25;
    double x0 = std::max(0.0, vx0 - mxm), x1 = std::min(widthM, vx1 + mxm);
    double z0 = std::max(0.0, vz0 - mzm), z1 = std::min(heightM, vz1 + mzm);
    if (x1 <= x0 || z1 <= z0) { return; }
    irr::u32 outW = (irr::u32)std::min(2048.0, (x1 - x0) / mpp);
    irr::u32 outH = (irr::u32)std::min(2048.0, (z1 - z0) / mpp);
    if (outW < 2 || outH < 2) { return; }

    dropDetail();
    static int counter = 0;
    char name[64];
    snprintf(name, sizeof(name), "chart-detail-%d", ++counter);
    detailTexture = makeTexture(renderRegion(x0, z0, x1, z1, outW, outH, style == Style_Night), name);
    detailX0 = x0; detailX1 = x1; detailZ0 = z0; detailZ1 = z1;
    detailMpp = mpp;
    detailStyle = style;
}

void ChartView::setStyle(int s)
{
    if (s == Style_Day && !dayTexture) { dayTexture = buildChart(false); }
    if (s == Style_Night && !nightTexture) { nightTexture = buildChart(true); }
    if ((s == Style_Day && !dayTexture) || (s == Style_Night && !nightTexture) || (s == Style_HD && !hdTexture)) {
        s = Style_Original;
    }
    if (s == Style_Original && !originalTexture) {
        if (!dayTexture) { dayTexture = buildChart(false); }
        s = dayTexture ? Style_Day : Style_Original;
    }
    style = s;
    rememberStyle();
}

void ChartView::nextStyle()
{
    int s = style;
    for (int tries = 0; tries < 4; tries++) {
        s = (s + 1) % 4;
        if (s == Style_HD && !hdTexture) { continue; }
        if (s == Style_Original && !originalTexture) { continue; }
        if ((s == Style_Day || s == Style_Night) && heights.empty()) { continue; }
        break;
    }
    setStyle(s);
}

std::wstring ChartView::styleName() const
{
    switch (style) {
    case Style_Day: return L"Carte marine (jour)";
    case Style_Night: return L"Carte marine (nuit)";
    case Style_HD: return L"Image haute r\u00E9solution";
    default: return L"Carte d'origine";
    }
}

void ChartView::rememberStyle() const
{
    std::ofstream f(stylePath().c_str());
    if (f) { f << style << std::endl; }
}

int ChartView::recalledStyle()
{
    std::ifstream f(stylePath().c_str());
    int s = Style_Day;
    if (f) { f >> s; }
    if (s < 0 || s > 3) { s = Style_Day; }
    return s;
}

double ChartView::lonToX(double lon) const { return (lon - west) * widthM / longExtent; }
double ChartView::latToZ(double lat) const { return (lat - south) * heightM / latExtent; }
double ChartView::xToLon(double x) const { return west + x * longExtent / widthM; }
double ChartView::zToLat(double z) const { return south + z * latExtent / heightM; }

void ChartView::setViewport(const irr::core::recti& r) { viewport = r; }

void ChartView::centreOn(const IncidentPoint& p) { cx = lonToX(p.lon); cz = latToZ(p.lat); }
void ChartView::centreOnXZ(double x, double z) { cx = x; cz = z; }
IncidentPoint ChartView::centre() const { return IncidentPoint(zToLat(cz), xToLon(cx)); }

void ChartView::fitWorld()
{
    cx = widthM / 2.0;
    cz = heightM / 2.0;
    double w = viewport.getWidth() > 0 ? viewport.getWidth() : 1;
    double h = viewport.getHeight() > 0 ? viewport.getHeight() : 1;
    mpp = std::max(widthM / w, heightM / h) * 1.05;
}

void ChartView::setMetresPerPixel(double metres)
{
    double maxMpp = std::max(widthM / std::max(1, viewport.getWidth()), heightM / std::max(1, viewport.getHeight())) * 2.0;
    mpp = std::min(std::max(metres, 0.05), maxMpp);
}

void ChartView::zoomAt(irr::core::position2di screen, irr::f32 factor)
{
    double x0, z0;
    toXZ(screen, x0, z0);
    setMetresPerPixel(mpp * factor);
    irr::core::position2di c = viewport.getCenter();
    cx = x0 - (screen.X - c.X) * mpp;   // keep the point under the cursor where it was
    cz = z0 + (screen.Y - c.Y) * mpp;
}

void ChartView::panPixels(irr::s32 dx, irr::s32 dy)
{
    cx -= dx * mpp;
    cz += dy * mpp;
}

irr::core::position2di ChartView::toScreenXZ(double x, double z) const
{
    irr::core::position2di c = viewport.getCenter();
    double sx = c.X + (x - cx) / mpp;
    double sy = c.Y - (z - cz) / mpp;
    // keep far-off points representable without overflowing
    sx = std::max(-1.0e7, std::min(1.0e7, sx));
    sy = std::max(-1.0e7, std::min(1.0e7, sy));
    return irr::core::position2di((irr::s32)std::floor(sx + 0.5), (irr::s32)std::floor(sy + 0.5));
}

irr::core::position2di ChartView::toScreen(const IncidentPoint& p) const
{
    return toScreenXZ(lonToX(p.lon), latToZ(p.lat));
}

void ChartView::toXZ(irr::core::position2di screen, double& x, double& z) const
{
    irr::core::position2di c = viewport.getCenter();
    x = cx + (screen.X - c.X) * mpp;
    z = cz - (screen.Y - c.Y) * mpp;
}

IncidentPoint ChartView::toLatLong(irr::core::position2di screen) const
{
    double x, z;
    toXZ(screen, x, z);
    return IncidentPoint(zToLat(z), xToLon(x));
}

double ChartView::metresBetween(const IncidentPoint& a, const IncidentPoint& b) const
{
    double dx = lonToX(b.lon) - lonToX(a.lon);
    double dz = latToZ(b.lat) - latToZ(a.lat);
    return std::sqrt(dx * dx + dz * dz);
}

void ChartView::draw(irr::video::IVideoDriver* drv)
{
    const Palette& pal = (style == Style_Day) ? kDay : kNight;
    drv->draw2DRectangle(irr::video::SColor(255, (irr::u32)pal.outside.r, (irr::u32)pal.outside.g, (irr::u32)pal.outside.b), viewport);

    irr::video::ITexture* tex = originalTexture;
    if (style == Style_Day && dayTexture) { tex = dayTexture; }
    if (style == Style_Night && nightTexture) { tex = nightTexture; }
    if (style == Style_HD && hdTexture) { tex = hdTexture; }
    if (!tex) { return; }

    irr::core::position2di tl = toScreenXZ(0.0, heightM);
    irr::core::position2di br = toScreenXZ(widthM, 0.0);
    irr::core::dimension2du size = tex->getOriginalSize();
    irr::core::recti src(0, 0, (irr::s32)size.Width, (irr::s32)size.Height);

    drv->getMaterial2D().TextureLayer[0].BilinearFilter = true;
    drv->enableMaterial2D(true);
    drv->draw2DImage(tex, irr::core::recti(tl, br), src, &viewport);

    updateDetail();
    if (detailTexture && detailStyle == style && baseW > 0 && mpp <= (widthM / baseW) * 0.8) {
        irr::core::position2di dtl = toScreenXZ(detailX0, detailZ1);
        irr::core::position2di dbr = toScreenXZ(detailX1, detailZ0);
        irr::core::dimension2du ds = detailTexture->getOriginalSize();
        drv->draw2DImage(detailTexture, irr::core::recti(dtl, dbr), irr::core::recti(0, 0, (irr::s32)ds.Width, (irr::s32)ds.Height), &viewport);
    }
    drv->enableMaterial2D(false);
}

void ChartView::drawGraticule(irr::video::IVideoDriver* drv, irr::gui::IGUIFont* font, irr::s32 topMargin, irr::s32 bottomMargin)
{
    // Grid step in minutes of arc: the smallest that keeps lines at least ~110 px apart.
    static const double steps[] = { 0.1, 0.2, 0.5, 1, 2, 5, 10, 15, 30, 60, 120 };
    double pxPerMinute = 1852.0 / mpp;
    double step = steps[10];
    for (int i = 0; i < 11; i++) { if (steps[i] * pxPerMinute >= 110.0) { step = steps[i]; break; } }
    double stepDeg = step / 60.0;

    bool light = (style == Style_Day);
    irr::video::SColor lineCol = light ? irr::video::SColor(110, 60, 90, 120) : irr::video::SColor(70, 190, 210, 230);
    irr::video::SColor textCol = light ? irr::video::SColor(255, 40, 62, 86) : irr::video::SColor(255, 190, 205, 220);
    irr::video::SColor textBg = light ? irr::video::SColor(170, 245, 249, 252) : irr::video::SColor(150, 10, 20, 32);

    IncidentPoint a = toLatLong(viewport.UpperLeftCorner);
    IncidentPoint b = toLatLong(viewport.LowerRightCorner);
    double latMin = std::min(a.lat, b.lat), latMax = std::max(a.lat, b.lat);
    double lonMin = std::min(a.lon, b.lon), lonMax = std::max(a.lon, b.lon);

    wchar_t buf[48];
    for (double lat = std::ceil(latMin / stepDeg) * stepDeg; lat <= latMax; lat += stepDeg) {
        irr::s32 y = toScreen(IncidentPoint(lat, lonMin)).Y;
        drv->draw2DLine(irr::core::position2di(viewport.UpperLeftCorner.X, y), irr::core::position2di(viewport.LowerRightCorner.X, y), lineCol);
        if (!font || y < viewport.UpperLeftCorner.Y + topMargin + 8 || y > viewport.LowerRightCorner.Y - bottomMargin - 8) { continue; }
        double v = std::fabs(lat);
        int deg = (int)(v + 1e-9);
        double min = (v - deg) * 60.0;
        if (min > 59.995) { deg++; min = 0; }
        swprintf(buf, 48, step < 1 ? L"%d\u00B0%04.1f'%lc" : L"%d\u00B0%02.0f'%lc", deg, min, lat >= 0 ? L'N' : L'S');
        irr::core::dimension2du d = font->getDimension(buf);
        irr::core::recti r(viewport.UpperLeftCorner.X + 4, y - (irr::s32)d.Height - 1, viewport.UpperLeftCorner.X + 8 + (irr::s32)d.Width, y - 1);
        drv->draw2DRectangle(textBg, r);
        font->draw(buf, r + irr::core::position2di(2, 0), textCol, false, false, &viewport);
    }
    for (double lon = std::ceil(lonMin / stepDeg) * stepDeg; lon <= lonMax; lon += stepDeg) {
        irr::s32 x = toScreen(IncidentPoint(latMin, lon)).X;
        drv->draw2DLine(irr::core::position2di(x, viewport.UpperLeftCorner.Y), irr::core::position2di(x, viewport.LowerRightCorner.Y), lineCol);
        if (!font || x < viewport.UpperLeftCorner.X + 60 || x > viewport.LowerRightCorner.X - 80) { continue; }
        double v = std::fabs(lon);
        int deg = (int)(v + 1e-9);
        double min = (v - deg) * 60.0;
        if (min > 59.995) { deg++; min = 0; }
        swprintf(buf, 48, step < 1 ? L"%03d\u00B0%04.1f'%lc" : L"%03d\u00B0%02.0f'%lc", deg, min, lon >= 0 ? L'E' : L'W');
        irr::core::dimension2du d = font->getDimension(buf);
        irr::s32 top = viewport.UpperLeftCorner.Y + topMargin + 4;
        irr::core::recti r(x + 2, top, x + 6 + (irr::s32)d.Width, top + (irr::s32)d.Height);
        drv->draw2DRectangle(textBg, r);
        font->draw(buf, r + irr::core::position2di(2, 0), textCol, false, false, &viewport);
    }
}
