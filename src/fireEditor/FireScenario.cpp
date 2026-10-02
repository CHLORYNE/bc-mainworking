/* SCENARIO INCENDIE - fire scenario editor: scenario load / save. See FireScenario.hpp. */
#define _CRT_SECURE_NO_WARNINGS   // std::localtime, as in the standard editor
#include "FireScenario.hpp"

#include <cctype>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>

#include "../Utilities.hpp"

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

namespace {

const double kMetresPerDegLat = 111320.0;

std::string lowerCopy(std::string s)
{
    for (size_t i = 0; i < s.size(); i++) { s[i] = (char)std::tolower((unsigned char)s[i]); }
    return s;
}

double distanceM(const IncidentPoint& a, const IncidentPoint& b)
{
    double north = (b.lat - a.lat) * kMetresPerDegLat;
    double east = (b.lon - a.lon) * kMetresPerDegLat * std::cos(a.lat * 3.14159265358979 / 180.0);
    return std::sqrt(north * north + east * east);
}

// boat.ini of an other-ship model, user folder first, as the simulator looks for it.
std::string boatIniFor(const std::string& type)
{
    std::string userFolder = Utilities::getUserDir();
    std::string tries[2] = { "Models/Othership/" + type + "/boat.ini", "Models/Ownship/" + type + "/boat.ini" };
    for (int i = 0; i < 2; i++) {
        if (Utilities::pathExists(userFolder + tries[i])) { return userFolder + tries[i]; }
        if (Utilities::pathExists(tries[i])) { return tries[i]; }
    }
    return "";
}

bool isFireFightingType(const std::string& type)
{
    std::string ini = boatIniFor(type);
    if (ini.empty()) { return false; }
    IncidentIni::Map m;
    IncidentIni::read(ini, m);
    return (int)IncidentIni::num(m, "FireFighting", 0) == 1;
}

} // namespace

bool FireScenario::isRescueModel(const std::string& type)
{
    if (type.empty()) { return false; }
    std::string ini = "Models/Othership/" + type + "/boat.ini";
    std::string userIni = Utilities::getUserDir() + ini;
    if (Utilities::pathExists(userIni)) { ini = userIni; }
    IncidentIni::Map m;
    if (!IncidentIni::read(ini, m)) { return false; }
    return (int)IncidentIni::num(m, "FireFighting", 0) == 1;
}

namespace {

// OtherShips::findByName: exact match first, then case-insensitive containment.
int findByName(const std::vector<EdShip>& ships, const std::string& shipName)
{
    for (size_t i = 0; i < ships.size(); i++) {
        if (ships[i].type == shipName) { return (int)i; }
    }
    std::string needle = lowerCopy(shipName);
    for (size_t i = 0; i < ships.size(); i++) {
        if (lowerCopy(ships[i].type).find(needle) != std::string::npos) { return (int)i; }
    }
    return -1;
}

void writeShip(std::ofstream& f, int i, const EdShip& s, bool stationary)
{
    f << "Type(" << i << ")=\"" << s.type << "\"" << std::endl;
    f << std::setprecision(7);
    f << "InitLong(" << i << ")=" << s.pos.lon << std::endl;
    f << "InitLat(" << i << ")=" << s.pos.lat << std::endl;
    f << "mmsi(" << i << ")=" << s.mmsi << std::endl;
    if (s.drifting) { f << "Drifting(" << i << ")=1" << std::endl; }
    f << std::setprecision(2);
    if (stationary) {
        // Casualty and SAR boats lie stopped on their heading until the fire / rescue run takes over.
        f << "Legs(" << i << ")=1" << std::endl;
        f << "Bearing(" << i << ",1)=" << s.heading << std::endl;
        f << "Speed(" << i << ",1)=0" << std::endl;
        f << "Distance(" << i << ",1)=0" << std::endl;
        return;
    }
    f << "Legs(" << i << ")=" << s.legs.size() << std::endl;
    for (size_t j = 0; j < s.legs.size(); j++) {
        int n = (int)j + 1;
        f << std::setprecision(4);
        f << "Bearing(" << i << "," << n << ")=" << s.legs[j].bearing << std::endl;
        f << "Speed(" << i << "," << n << ")=" << s.legs[j].speed << std::endl;
        f << "Distance(" << i << "," << n << ")=" << s.legs[j].distance << std::endl;
    }
}

bool makeDir(const std::string& path)
{
    if (Utilities::pathExists(path)) { return true; }
#ifdef _WIN32
    return _mkdir(path.c_str()) == 0;
#else
    return mkdir(path.c_str(), 0755) == 0;
#endif
}

} // namespace

IncidentPoint offsetMetres(const IncidentPoint& from, double eastM, double northM)
{
    double lat = from.lat + northM / kMetresPerDegLat;
    double lon = from.lon + eastM / (kMetresPerDegLat * std::cos(from.lat * 3.14159265358979 / 180.0));
    return IncidentPoint(lat, lon);
}

FireScenario::FireScenario()
    : startTimeHours(12.0f), day(1), month(1), year(2026),
    sunRise(6.0f), sunSet(18.0f), weather(1.5f), rain(0.0f), visibility(10.0f), windDirection(0.0f), windSpeed(0.0f),
    hasCasualty(false), hadIncidentFile(false)
{
}

std::vector<IncidentSurvivor> FireScenario::defaultSurvivors(const IncidentPoint& c)
{
    // SimulationModel::spawnAbandonStep's built-in layout, for a 40 m casualty.
    std::vector<IncidentSurvivor> out;
    const double ringR = 0.75 * 40.0 + 14.0;
    for (int k = 0; k < 5; k++) {
        double a = (double)k / 5.0 * 6.2832;
        double r = ringR + (double)(k % 2) * 4.0;
        IncidentSurvivor s;
        s.kind = Survivor_MOB;
        s.pos = offsetMetres(c, r * std::sin(a), r * std::cos(a));
        out.push_back(s);
    }
    IncidentSurvivor radeau;
    radeau.kind = Survivor_Radeau;
    radeau.pos = offsetMetres(c, -ringR * 1.25, -8.0);
    out.push_back(radeau);
    IncidentSurvivor liferaft;
    liferaft.kind = Survivor_Liferaft;
    liferaft.pos = offsetMetres(c, ringR * 1.25, 8.0);
    out.push_back(liferaft);
    return out;
}

unsigned int FireScenario::nextMmsi() const
{
    unsigned int highest = 242000100;
    if (ownShip.mmsi > highest) { highest = ownShip.mmsi; }
    if (hasCasualty && casualty.mmsi > highest) { highest = casualty.mmsi; }
    for (size_t i = 0; i < sarBoats.size(); i++) { if (sarBoats[i].mmsi > highest) { highest = sarBoats[i].mmsi; } }
    for (size_t i = 0; i < traffic.size(); i++) { if (traffic[i].mmsi > highest) { highest = traffic[i].mmsi; } }
    return highest + 1;
}

void FireScenario::makeNew(const std::string& world, const IncidentPoint& centre,
    const std::string& ownShipType, const std::string& otherShipType, const std::string& rescueShipType,
    const std::string& heloModel)
{
    *this = FireScenario();
    name = "Nouvel exercice incendie";
    worldName = world;
    description = "Exercice incendie / SAR";

    std::time_t t = std::time(0);
    std::tm* now = std::localtime(&t);
    startTimeHours = (float)now->tm_hour + (float)now->tm_min / 60.0f;
    day = now->tm_mday;
    month = now->tm_mon + 1;
    year = now->tm_year + 1900;

    ownShip.type = ownShipType;
    ownShip.pos = offsetMetres(centre, 0.0, -500.0);
    ownShip.heading = 0.0f;
    ownShip.mmsi = 242000100;

    hasCasualty = true;
    casualty.type = otherShipType;
    casualty.pos = centre;
    casualty.heading = 90.0f;
    casualty.mmsi = 242000101;

    incident = IncidentConfig::trainingPreset();
    std::string upperWorld = world;
    for (size_t i = 0; i < upperWorld.size(); i++) { upperWorld[i] = (char)std::toupper((unsigned char)upperWorld[i]); }
    incident.coordinationCentre = "MRSC " + upperWorld;

    if (!rescueShipType.empty()) {   // only when a FireFighting=1 model is installed
        EdShip boat;
        boat.type = rescueShipType;
        boat.pos = offsetMetres(centre, -800.0, -300.0);
        boat.mmsi = 242000102;
        sarBoats.push_back(boat);
        incident.sarBoats.push_back(IncidentSarBoat());
    }
    incident.survivors = defaultSurvivors(casualty.pos);
    for (size_t k = 0; k < incident.helos.size(); k++) {
        if (!heloModel.empty()) { incident.helos[k].model = heloModel; }
    }
    hadIncidentFile = true;
}

bool FireScenario::load(const std::string& dir, const std::string& scenarioName, std::string& error)
{
    using namespace IncidentIni;
    *this = FireScenario();
    Map env, own, other;
    if (!read(dir + "/environment.ini", env)) {
        error = "environment.ini introuvable dans " + dir;
        return false;
    }
    read(dir + "/ownship.ini", own);
    read(dir + "/othership.ini", other);

    name = scenarioName;
    worldName = str(env, "Setting");
    startTimeHours = (float)num(env, "StartTime", 12.0);
    day = (unsigned int)num(env, "StartDay", 1);
    month = (unsigned int)num(env, "StartMonth", 1);
    year = (unsigned int)num(env, "StartYear", 2026);
    sunRise = (float)num(env, "SunRise", 6.0);
    sunSet = (float)num(env, "SunSet", 18.0);
    if (sunRise == 0.0f) { sunRise = 6.0f; }
    if (sunSet == 0.0f) { sunSet = 18.0f; }
    weather = (float)num(env, "Weather", 0.0);
    visibility = (float)num(env, "VisibilityRange", 10.0);
    windDirection = (float)num(env, "WindDirection", 0.0);
    windSpeed = (float)num(env, "WindSpeed", 0.0);
    rain = (float)num(env, "Rain", 0.0);

    ownShip.type = str(own, "ShipName");
    ownShip.pos = IncidentPoint(num(own, "InitialLat"), num(own, "InitialLong"));
    ownShip.heading = (float)num(own, "InitialBearing");
    ownShip.speed = (float)num(own, "InitialSpeed");
    ownShip.mmsi = 242000100;

    std::vector<EdShip> ships;
    int n = (int)num(other, "Number", 0);
    for (int i = 1; i <= n; i++) {
        EdShip s;
        s.type = str(other, key("Type", i));
        s.pos = IncidentPoint(num(other, key("InitLat", i)), num(other, key("InitLong", i)));
        s.mmsi = (unsigned int)num(other, key("mmsi", i), 0);
        s.drifting = (int)num(other, key("Drifting", i), 0) == 1;
        int legs = (int)num(other, key("Legs", i), 0);
        for (int j = 1; j <= legs; j++) {
            EdLeg l;
            l.bearing = (float)num(other, key("Bearing", i, j));
            l.speed = (float)num(other, key("Speed", i, j));
            l.distance = (float)num(other, key("Distance", i, j));
            s.legs.push_back(l);
        }
        s.heading = s.legs.empty() ? 0.0f : s.legs[0].bearing;
        ships.push_back(s);
    }
    unsigned int mmsi = 242000101;
    for (size_t i = 0; i < ships.size(); i++) {
        if (ships[i].mmsi == 0) { ships[i].mmsi = mmsi; }
        mmsi = ships[i].mmsi + 1;
    }

    std::ifstream descriptionFile((dir + "/description.ini").c_str());
    if (descriptionFile.is_open()) {
        std::stringstream buffer;
        buffer << descriptionFile.rdbuf();
        description = buffer.str();
        while (!description.empty() && (description[description.size() - 1] == '\n' || description[description.size() - 1] == '\r')) {
            description.erase(description.size() - 1);
        }
    }

    std::vector<bool> used(ships.size(), false);
    hadIncidentFile = incident.load(dir + "/incident.ini");
    int casualtyIndex = -1;
    std::vector<int> boatIndex;

    if (hadIncidentFile) {
        if (incident.casualtyShip >= 1 && incident.casualtyShip <= (int)ships.size()) {
            casualtyIndex = incident.casualtyShip - 1;
            used[casualtyIndex] = true;
        }
        // Keep the boats that still match a ship; renumber survivors' boat assignments to suit.
        std::vector<IncidentSarBoat> keptBoats;
        std::vector<int> newNumber(incident.sarBoats.size() + 1, 0);
        for (size_t b = 0; b < incident.sarBoats.size(); b++) {
            int idx = incident.sarBoats[b].ship - 1;
            if (idx < 0 || idx >= (int)ships.size() || used[idx]) { continue; }
            used[idx] = true;
            boatIndex.push_back(idx);
            keptBoats.push_back(incident.sarBoats[b]);
            newNumber[b + 1] = (int)keptBoats.size();
        }
        incident.sarBoats = keptBoats;
        for (size_t s = 0; s < incident.survivors.size(); s++) {
            int bn = incident.survivors[s].boat;
            incident.survivors[s].boat = (bn >= 1 && bn < (int)newNumber.size()) ? newNumber[bn] : 0;
        }
    }
    else {
        // What the simulator does with this scenario today: the rescue boat found by name...
        const IncidentBuiltInPreset& preset = incidentBuiltInPreset();
        int idx = findByName(ships, preset.rescueBoatName);
        if (idx < 0) {
            // ...or, to give the instructor a starting point, the first ship certified as a
            // rescue / fire-fighting craft (FireFighting=1).
            for (size_t i = 0; i < ships.size() && idx < 0; i++) {
                if (isRescueModel(ships[i].type)) { idx = (int)i; }
            }
        }
        if (idx >= 0) {
            used[idx] = true;
            boatIndex.push_back(idx);
            IncidentSarBoat cfg;
            cfg.speedKts = preset.boatSpeedKts;
            incident.sarBoats.push_back(cfg);
        }
    }

    if (casualtyIndex < 0) {
        // Ctrl+F's choice: the ship nearest own ship that is neither a fire-fighter nor a rescuer.
        double best = 1.0e30;
        for (size_t i = 0; i < ships.size(); i++) {
            if (used[i] || isFireFightingType(ships[i].type)) { continue; }
            double d = distanceM(ownShip.pos, ships[i].pos);
            if (d < best) { best = d; casualtyIndex = (int)i; }
        }
        if (casualtyIndex >= 0) { used[casualtyIndex] = true; }
    }

    hasCasualty = (casualtyIndex >= 0);
    if (hasCasualty) { casualty = ships[casualtyIndex]; }
    for (size_t b = 0; b < boatIndex.size(); b++) { sarBoats.push_back(ships[boatIndex[b]]); }
    for (size_t i = 0; i < ships.size(); i++) { if (!used[i]) { traffic.push_back(ships[i]); } }

    if (!hadIncidentFile && hasCasualty) {
        incident.survivors = defaultSurvivors(casualty.pos);
    }
    return true;
}

bool FireScenario::inBounds(const IncidentPoint& pt, double south, double west, double latExtent, double longExtent)
{
    return pt.lat >= south && pt.lat <= south + latExtent && pt.lon >= west && pt.lon <= west + longExtent;
}

void FireScenario::applyBuiltInPresetIfInside(double south, double west, double latExtent, double longExtent)
{
    if (hadIncidentFile) { return; }
    const IncidentBuiltInPreset& preset = incidentBuiltInPreset();
    bool inside = true;
    for (size_t i = 0; i < preset.boatReturn.size(); i++) {
        if (!inBounds(preset.boatReturn[i], south, west, latExtent, longExtent)) { inside = false; }
    }
    for (int k = 0; k < 2; k++) {
        if (!inBounds(preset.heloPad[k], south, west, latExtent, longExtent)) { inside = false; }
    }
    if (!inside) { return; }
    if (!incident.sarBoats.empty()) {
        incident.sarBoats[0].inbound = preset.boatReturn;
        incident.sarBoats[0].moorHeading = preset.boatMoorHeading;
    }
    for (size_t k = 0; k < incident.helos.size() && k < 2; k++) {
        incident.helos[k].hasPad = true;
        incident.helos[k].pad = preset.heloPad[k];
        incident.helos[k].padHeight = preset.heloPadHeight[k];
    }
}

bool FireScenario::save(const std::string& dir, std::string& error)
{
    if (!makeDir(dir)) {
        error = "Impossible de cr\xE9""er le dossier " + dir;
        return false;
    }

    std::ofstream env((dir + "/environment.ini").c_str());
    env.imbue(std::locale::classic());
    env << "Setting=\"" << worldName << "\"" << std::endl;
    env << std::fixed << std::setprecision(4);
    env << "StartTime=" << startTimeHours << std::endl;
    env << "StartDay=" << day << std::endl;
    env << "StartMonth=" << month << std::endl;
    env << "StartYear=" << year << std::endl;
    env << std::setprecision(2);
    env << "SunRise=" << sunRise << std::endl;
    env << "SunSet=" << sunSet << std::endl;
    env << "VisibilityRange=" << visibility << std::endl;
    env << "Weather=" << weather << std::endl;
    env << "WindDirection=" << windDirection << std::endl;
    env << "WindSpeed=" << windSpeed << std::endl;
    env << "Rain=" << rain << std::endl;
    env.close();

    std::ofstream own((dir + "/ownship.ini").c_str());
    own.imbue(std::locale::classic());
    own << "ShipName=\"" << ownShip.type << "\"" << std::endl;
    own << std::fixed << std::setprecision(7);
    own << "InitialLong=" << ownShip.pos.lon << std::endl;
    own << "InitialLat=" << ownShip.pos.lat << std::endl;
    own << std::setprecision(2);
    own << "InitialBearing=" << ownShip.heading << std::endl;
    own << "InitialSpeed=" << ownShip.speed << std::endl;
    own.close();

    // Order in othership.ini: burning ship, SAR boats, then the other traffic.
    std::ofstream other((dir + "/othership.ini").c_str());
    other.imbue(std::locale::classic());
    other << std::fixed;
    int count = (hasCasualty ? 1 : 0) + (int)sarBoats.size() + (int)traffic.size();
    other << "Number=" << count << std::endl;
    int i = 1;
    if (hasCasualty) { writeShip(other, i++, casualty, true); }
    int firstBoat = i;
    for (size_t b = 0; b < sarBoats.size(); b++) { writeShip(other, i++, sarBoats[b], true); }
    for (size_t t = 0; t < traffic.size(); t++) { writeShip(other, i++, traffic[t], false); }
    other.close();

    std::ofstream desc((dir + "/description.ini").c_str());
    desc << description;
    desc.close();

    incident.casualtyShip = hasCasualty ? 1 : 0;
    for (size_t b = 0; b < incident.sarBoats.size() && b < sarBoats.size(); b++) {
        incident.sarBoats[b].ship = firstBoat + (int)b;
    }
    bool ok = incident.save(dir + "/incident.ini");

    if (!ok || env.fail() || own.fail() || other.fail() || desc.fail()) {
        error = "Erreur d'\xE9""criture dans " + dir;
        return false;
    }
    hadIncidentFile = true;
    return true;
}
