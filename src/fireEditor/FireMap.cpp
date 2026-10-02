/* SCENARIO INCENDIE - fire scenario editor: the chart. See FireMap.hpp. */
#include "FireMap.hpp"

#include <algorithm>
#include <cmath>

#include "../Utilities.hpp"

namespace {
const double kEarthRadiusM = 6.371e6;
const double kPi = 3.14159265358979;
}

FireMap::FireMap()
    : south(0), west(0), latExtent(1), longExtent(1),
    texture(0), driver(0), widthM(1), heightM(1), cx(0), cz(0), mpp(10)
{
}

FireMap::~FireMap()
{
    if (texture && driver) { driver->removeTexture(texture); }
}

bool FireMap::load(irr::IrrlichtDevice* device, const std::string& worldName, std::string& error)
{
    driver = device->getVideoDriver();

    // Same lookup as the standard editor (ControllerModel): user folder first, terrain.ini or a
    // lone .hdr height map, the chart image from MapImage (or RadarImage).
    std::string worldPath = "World/" + worldName;
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
    if (heightMap.size() > 4) {
        std::string ext = heightMap.substr(heightMap.size() - 4);
        Utilities::to_lower(ext);
        IncidentIni::Map hdr;
        if (ext == ".hdr" && IncidentIni::read(worldPath + "/" + heightMap, hdr)) {
            west = IncidentIni::num(hdr, "left_map_x");
            south = IncidentIni::num(hdr, "lower_map_y");
            longExtent = IncidentIni::num(hdr, "right_map_x") - west;
            latExtent = IncidentIni::num(hdr, "upper_map_y") - south;
        }
    }

    if (mapName.empty()) {
        error = "Pas d'image de carte (MapImage) dans " + terrainFile;
        return false;
    }
    if (longExtent <= 0 || latExtent <= 0) {
        error = "Dimensions de carte invalides dans " + terrainFile;
        return false;
    }

    driver->setTextureCreationFlag(irr::video::ETCF_CREATE_MIP_MAPS, false);
    texture = driver->getTexture((worldPath + "/" + mapName).c_str());
    driver->setTextureCreationFlag(irr::video::ETCF_CREATE_MIP_MAPS, true);
    if (!texture) {
        error = "Impossible de charger l'image " + worldPath + "/" + mapName;
        return false;
    }

    widthM = longExtent * 2.0 * kPi * kEarthRadiusM * std::cos((south + latExtent / 2.0) * kPi / 180.0) / 360.0;
    heightM = latExtent * 2.0 * kPi * kEarthRadiusM / 360.0;
    cx = widthM / 2.0;
    cz = heightM / 2.0;
    return true;
}

double FireMap::lonToX(double lon) const { return (lon - west) * widthM / longExtent; }
double FireMap::latToZ(double lat) const { return (lat - south) * heightM / latExtent; }
double FireMap::xToLon(double x) const { return west + x * longExtent / widthM; }
double FireMap::zToLat(double z) const { return south + z * latExtent / heightM; }

void FireMap::setViewport(const irr::core::recti& r)
{
    viewport = r;
}

void FireMap::centreOn(const IncidentPoint& p)
{
    cx = lonToX(p.lon);
    cz = latToZ(p.lat);
}

IncidentPoint FireMap::centre() const
{
    return IncidentPoint(zToLat(cz), xToLon(cx));
}

void FireMap::fitWorld()
{
    cx = widthM / 2.0;
    cz = heightM / 2.0;
    double w = viewport.getWidth() > 0 ? viewport.getWidth() : 1;
    double h = viewport.getHeight() > 0 ? viewport.getHeight() : 1;
    mpp = std::max(widthM / w, heightM / h) * 1.05;
}

void FireMap::zoomAt(irr::core::position2di screen, irr::f32 factor)
{
    irr::core::position2di c = viewport.getCenter();
    double x0 = cx + (screen.X - c.X) * mpp;
    double z0 = cz - (screen.Y - c.Y) * mpp;

    double maxMpp = std::max(widthM / std::max(1, viewport.getWidth()), heightM / std::max(1, viewport.getHeight())) * 2.0;
    mpp *= factor;
    if (mpp < 0.05) { mpp = 0.05; }
    if (mpp > maxMpp) { mpp = maxMpp; }

    // keep the point under the cursor where it was
    cx = x0 - (screen.X - c.X) * mpp;
    cz = z0 + (screen.Y - c.Y) * mpp;
}

void FireMap::setMetresPerPixel(double metres)
{
    double maxMpp = std::max(widthM / std::max(1, viewport.getWidth()), heightM / std::max(1, viewport.getHeight())) * 2.0;
    mpp = std::min(std::max(metres, 0.05), maxMpp);
}

void FireMap::panPixels(irr::s32 dx, irr::s32 dy)
{
    cx -= dx * mpp;
    cz += dy * mpp;
}

irr::core::position2di FireMap::toScreen(const IncidentPoint& p) const
{
    irr::core::position2di c = viewport.getCenter();
    double sx = c.X + (lonToX(p.lon) - cx) / mpp;
    double sy = c.Y - (latToZ(p.lat) - cz) / mpp;
    // keep far-off points representable without overflowing
    if (sx > 1.0e7) { sx = 1.0e7; }
    if (sx < -1.0e7) { sx = -1.0e7; }
    if (sy > 1.0e7) { sy = 1.0e7; }
    if (sy < -1.0e7) { sy = -1.0e7; }
    return irr::core::position2di((irr::s32)std::floor(sx + 0.5), (irr::s32)std::floor(sy + 0.5));
}

IncidentPoint FireMap::toLatLong(irr::core::position2di screen) const
{
    irr::core::position2di c = viewport.getCenter();
    double x = cx + (screen.X - c.X) * mpp;
    double z = cz - (screen.Y - c.Y) * mpp;
    return IncidentPoint(zToLat(z), xToLon(x));
}

double FireMap::metresBetween(const IncidentPoint& a, const IncidentPoint& b) const
{
    double dx = lonToX(b.lon) - lonToX(a.lon);
    double dz = latToZ(b.lat) - latToZ(a.lat);
    return std::sqrt(dx * dx + dz * dz);
}

void FireMap::draw(irr::video::IVideoDriver* drv)
{
    drv->draw2DRectangle(irr::video::SColor(255, 12, 30, 52), viewport);   // open sea beyond the chart
    if (!texture) { return; }

    irr::core::position2di tl = toScreen(IncidentPoint(south + latExtent, west));
    irr::core::position2di br = toScreen(IncidentPoint(south, west + longExtent));
    irr::core::dimension2du size = texture->getOriginalSize();
    irr::core::recti src(0, 0, (irr::s32)size.Width, (irr::s32)size.Height);

    drv->getMaterial2D().TextureLayer[0].BilinearFilter = true;
    drv->enableMaterial2D(true);
    drv->draw2DImage(texture, irr::core::recti(tl, br), src, &viewport);
    drv->enableMaterial2D(false);
}
