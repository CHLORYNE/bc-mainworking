/* SCENARIO INCENDIE (Kyara): incident.ini reader / writer. See IncidentConfig.hpp. */
#include "IncidentConfig.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <locale>
#include <map>
#include <sstream>

namespace {

std::string trim(const std::string& s, const char* chars = " \t\r\n\"")
{
    size_t b = s.find_first_not_of(chars);
    if (b == std::string::npos) { return ""; }
    size_t e = s.find_last_not_of(chars);
    return s.substr(b, e - b + 1);
}

std::string lower(std::string s)
{
    for (size_t i = 0; i < s.size(); i++) { s[i] = (char)std::tolower((unsigned char)s[i]); }
    return s;
}

int survivorKindFromKey(const std::string& v)
{
    std::string l = lower(v);
    if (l == "liferaft") { return Survivor_Liferaft; }
    if (l == "radeau") { return Survivor_Radeau; }
    return Survivor_MOB;
}

} // namespace

namespace IncidentIni {

bool read(const std::string& fileName, Map& out)
{
    std::ifstream file(fileName.c_str());
    if (!file.is_open()) { return false; }
    std::string line;
    while (std::getline(file, line)) {
        std::string t = trim(line, " \t\r\n");
        if (t.empty() || t[0] == '#' || t[0] == ';') { continue; }
        size_t eq = t.find('=');
        if (eq == std::string::npos) { continue; }
        out[lower(trim(t.substr(0, eq)))] = trim(t.substr(eq + 1));
    }
    return true;
}

bool has(const Map& m, const std::string& k) { return m.find(lower(k)) != m.end(); }

std::string str(const Map& m, const std::string& k, const std::string& def)
{
    Map::const_iterator it = m.find(lower(k));
    return (it == m.end()) ? def : it->second;
}

double num(const Map& m, const std::string& k, double def)
{
    Map::const_iterator it = m.find(lower(k));
    if (it == m.end() || it->second.empty()) { return def; }
    std::istringstream in(it->second);
    in.imbue(std::locale::classic());   // '.' decimal point whatever the user's locale
    double v = def;
    if (!(in >> v)) { return def; }
    return v;
}

std::string key(const char* name, int a)
{
    std::ostringstream o; o << name << "(" << a << ")"; return o.str();
}

std::string key(const char* name, int a, int b)
{
    std::ostringstream o; o << name << "(" << a << "," << b << ")"; return o.str();
}

} // namespace IncidentIni

// BUILT-IN DAKHLA PRESET (moved here from SimulationModel.cpp so the editor can start from it).
// The boat and the helicopters both recover into Dakhla harbour: the boat follows boatReturn
// and moors, the helicopters fly to heloPad and land on the quay.
const IncidentBuiltInPreset& incidentBuiltInPreset()
{
    static IncidentBuiltInPreset p;
    if (p.rescueBoatName.empty()) {
        //CHANGE BOAT NAME FOR sar
        p.rescueBoatName = "CQPM DAKHLA SAR-1";
        //SAR BOAT SPEED
        p.boatSpeedKts = 35.0f;                 // constant transit speed
        p.boatMoorHeading = 47.0f;              // matches Rotation(1)=47 in LandObject.ini
        // Boat path, walked strictly in order. Last entry = where she moors. (lat, long)
        p.boatReturn.push_back(IncidentPoint(23.6608, -15.9400));   // P1  clear of the wreck
        p.boatReturn.push_back(IncidentPoint(23.6553, -15.9467));   // P2  swing OUT into open water
        p.boatReturn.push_back(IncidentPoint(23.6568, -15.9499));   // P3  come back in past the land finger
        p.boatReturn.push_back(IncidentPoint(23.6593, -15.9468));   // ...as many as the bend needs
        // Helo pads: one per helo - lat, long, height above sea level.
        // Height: too low = inside the quay model, too high = floating.
        p.heloPad[0] = IncidentPoint(23.6582, -15.9460); p.heloPadHeight[0] = 12.0f;
        p.heloPad[1] = IncidentPoint(23.6585, -15.9455); p.heloPadHeight[1] = 12.0f;
    }
    return p;
}

IncidentConfig::IncidentConfig()
    // Values the simulator has always used (SimulationModel.cpp constants), so a scenario with
    // no incident.ini behaves exactly as before.
    : casualtyShip(0),
    fireDuration(63.0f), sinkLeadTime(18.0f), fireSpreadTime(30.0f), abandonTime(20.0f),
    survivorInterval(4.0f), permanentListTime(180.0f),
    heloDelay(0.0f), heloSpeedKts(79.7f), coordinationCentre("MRSC DAKHLA")
{
    helos.push_back(IncidentHelo());
    helos.push_back(IncidentHelo());
}

IncidentConfig IncidentConfig::trainingPreset()
{
    IncidentConfig c;
    c.fireDuration = 360.0f;     // 6 min to lose her
    c.sinkLeadTime = 60.0f;      // going down for the last minute
    c.fireSpreadTime = 150.0f;   // fully involved at 2 min 30
    c.abandonTime = 120.0f;      // abandon ship at 2 min
    c.heloDelay = 120.0f;        // on scene 2 min after the call
    return c;
}

float IncidentConfig::sinkStartTime() const
{
    float t = fireDuration - sinkLeadTime;
    return t < 0.0f ? 0.0f : t;
}

const char* IncidentConfig::survivorKey(int kind)
{
    if (kind == Survivor_Liferaft) { return "Liferaft"; }
    if (kind == Survivor_Radeau) { return "Radeau"; }
    return "MOB";
}

const char* IncidentConfig::survivorModel(int kind)
{
    if (kind == Survivor_Liferaft) { return "Liferaft"; }
    if (kind == Survivor_Radeau) { return "Radeau_Sauvetage"; }
    return "ManOverboard";
}

bool IncidentConfig::load(const std::string& fileName)
{
    using namespace IncidentIni;
    Map m;
    if (!read(fileName, m)) { return false; }

    *this = IncidentConfig();
    casualtyShip = (int)num(m, "CasualtyShip", casualtyShip);
    fireDuration = (float)num(m, "FireDuration", fireDuration);
    sinkLeadTime = (float)num(m, "SinkLeadTime", sinkLeadTime);
    fireSpreadTime = (float)num(m, "FireSpreadTime", fireSpreadTime);
    abandonTime = (float)num(m, "AbandonTime", abandonTime);
    survivorInterval = (float)num(m, "SurvivorInterval", survivorInterval);
    permanentListTime = (float)num(m, "PermanentListTime", permanentListTime);
    heloDelay = (float)num(m, "HeloDelay", heloDelay);
    heloSpeedKts = (float)num(m, "HeloSpeed", heloSpeedKts);
    coordinationCentre = str(m, "CoordinationCentre", coordinationCentre);
    if (fireSpreadTime < 1.0f) { fireSpreadTime = 1.0f; }
    if (sinkLeadTime < 1.0f) { sinkLeadTime = 1.0f; }
    if (heloSpeedKts < 1.0f) { heloSpeedKts = 1.0f; }

    survivors.clear();
    int n = (int)num(m, "Survivors", 0);
    for (int i = 1; i <= n; i++) {
        IncidentSurvivor s;
        s.kind = survivorKindFromKey(str(m, key("SurvivorType", i), "MOB"));
        s.pos.lat = num(m, key("SurvivorLat", i), 0.0);
        s.pos.lon = num(m, key("SurvivorLong", i), 0.0);
        s.boat = (int)num(m, key("SurvivorBoat", i), 0);
        survivors.push_back(s);
    }

    sarBoats.clear();
    n = (int)num(m, "SarBoats", 0);
    for (int i = 1; i <= n; i++) {
        IncidentSarBoat b;
        b.ship = (int)num(m, key("SarBoatShip", i), 0);
        b.speedKts = (float)num(m, key("SarBoatSpeed", i), b.speedKts);
        b.launchDelay = (float)num(m, key("SarBoatDelay", i), b.launchDelay);
        b.moorHeading = (float)num(m, key("SarBoatMoorHeading", i), b.moorHeading);
        if (b.speedKts < 1.0f) { b.speedKts = 1.0f; }
        int nOut = (int)num(m, key("SarBoatOutbound", i), 0);
        for (int j = 1; j <= nOut; j++) {
            b.outbound.push_back(IncidentPoint(num(m, key("SarBoatOutLat", i, j), 0.0),
                num(m, key("SarBoatOutLong", i, j), 0.0)));
        }
        int nRet = (int)num(m, key("SarBoatReturn", i), 0);
        for (int j = 1; j <= nRet; j++) {
            b.inbound.push_back(IncidentPoint(num(m, key("SarBoatRetLat", i, j), 0.0),
                num(m, key("SarBoatRetLong", i, j), 0.0)));
        }
        sarBoats.push_back(b);
    }

    if (has(m, "Helicopters")) {
        helos.clear();
        n = (int)num(m, "Helicopters", 0);
        for (int i = 1; i <= n; i++) {
            IncidentHelo h;
            h.model = str(m, key("HeloModel", i), h.model);
            h.hasBase = (int)num(m, key("HeloHasBase", i), 0) != 0;
            h.base.lat = num(m, key("HeloBaseLat", i), 0.0);
            h.base.lon = num(m, key("HeloBaseLong", i), 0.0);
            h.baseHeight = (float)num(m, key("HeloBaseHeight", i), h.baseHeight);
            h.hasPad = (int)num(m, key("HeloHasPad", i), 0) != 0;
            h.pad.lat = num(m, key("HeloPadLat", i), 0.0);
            h.pad.lon = num(m, key("HeloPadLong", i), 0.0);
            h.padHeight = (float)num(m, key("HeloPadHeight", i), h.padHeight);
            helos.push_back(h);
        }
    }
    return true;
}

bool IncidentConfig::save(const std::string& fileName) const
{
    std::ofstream f(fileName.c_str());
    if (!f.is_open()) { return false; }
    f.imbue(std::locale::classic());

    // Comment lines must not contain an equals sign: the simulator's ini reader would take them as keys.
    f << "# Scenario incendie / SAR - genere par l'editeur incendie (Simulator-fe)" << std::endl;
    f << "# Temps en secondes apres la mise a feu (Ctrl+F). HeloDelay compte depuis l'appel OSC (3e Ctrl+A)." << std::endl;
    f << std::fixed << std::setprecision(1);
    f << "CasualtyShip=" << casualtyShip << std::endl;
    f << "FireDuration=" << fireDuration << std::endl;
    f << "SinkLeadTime=" << sinkLeadTime << std::endl;
    f << "FireSpreadTime=" << fireSpreadTime << std::endl;
    f << "AbandonTime=" << abandonTime << std::endl;
    f << "SurvivorInterval=" << survivorInterval << std::endl;
    f << "PermanentListTime=" << permanentListTime << std::endl;
    f << "HeloDelay=" << heloDelay << std::endl;
    f << "HeloSpeed=" << heloSpeedKts << std::endl;
    f << "CoordinationCentre=\"" << coordinationCentre << "\"" << std::endl;

    f << std::endl << "# Naufrages et radeaux, dans l'ordre de mise a l'eau" << std::endl;
    f << "Survivors=" << survivors.size() << std::endl;
    for (size_t k = 0; k < survivors.size(); k++) {
        int i = (int)k + 1;
        const IncidentSurvivor& s = survivors[k];
        f << "SurvivorType(" << i << ")=\"" << survivorKey(s.kind) << "\"" << std::endl;
        f << std::setprecision(7);
        f << "SurvivorLat(" << i << ")=" << s.pos.lat << std::endl;
        f << "SurvivorLong(" << i << ")=" << s.pos.lon << std::endl;
        f << "SurvivorBoat(" << i << ")=" << s.boat << std::endl;
    }

    f << std::endl << "# Vedettes SAR" << std::endl;
    f << "SarBoats=" << sarBoats.size() << std::endl;
    for (size_t k = 0; k < sarBoats.size(); k++) {
        int i = (int)k + 1;
        const IncidentSarBoat& b = sarBoats[k];
        f << std::setprecision(1);
        f << "SarBoatShip(" << i << ")=" << b.ship << std::endl;
        f << "SarBoatSpeed(" << i << ")=" << b.speedKts << std::endl;
        f << "SarBoatDelay(" << i << ")=" << b.launchDelay << std::endl;
        f << "SarBoatMoorHeading(" << i << ")=" << b.moorHeading << std::endl;
        f << "SarBoatOutbound(" << i << ")=" << b.outbound.size() << std::endl;
        f << std::setprecision(7);
        for (size_t w = 0; w < b.outbound.size(); w++) {
            f << "SarBoatOutLat(" << i << "," << (w + 1) << ")=" << b.outbound[w].lat << std::endl;
            f << "SarBoatOutLong(" << i << "," << (w + 1) << ")=" << b.outbound[w].lon << std::endl;
        }
        f << "SarBoatReturn(" << i << ")=" << b.inbound.size() << std::endl;
        for (size_t w = 0; w < b.inbound.size(); w++) {
            f << "SarBoatRetLat(" << i << "," << (w + 1) << ")=" << b.inbound[w].lat << std::endl;
            f << "SarBoatRetLong(" << i << "," << (w + 1) << ")=" << b.inbound[w].lon << std::endl;
        }
    }

    f << std::endl << "# Helicopteres" << std::endl;
    f << "Helicopters=" << helos.size() << std::endl;
    for (size_t k = 0; k < helos.size(); k++) {
        int i = (int)k + 1;
        const IncidentHelo& h = helos[k];
        f << "HeloModel(" << i << ")=\"" << h.model << "\"" << std::endl;
        f << "HeloHasBase(" << i << ")=" << (h.hasBase ? 1 : 0) << std::endl;
        f << std::setprecision(7);
        f << "HeloBaseLat(" << i << ")=" << h.base.lat << std::endl;
        f << "HeloBaseLong(" << i << ")=" << h.base.lon << std::endl;
        f << std::setprecision(1);
        f << "HeloBaseHeight(" << i << ")=" << h.baseHeight << std::endl;
        f << "HeloHasPad(" << i << ")=" << (h.hasPad ? 1 : 0) << std::endl;
        f << std::setprecision(7);
        f << "HeloPadLat(" << i << ")=" << h.pad.lat << std::endl;
        f << "HeloPadLong(" << i << ")=" << h.pad.lon << std::endl;
        f << std::setprecision(1);
        f << "HeloPadHeight(" << i << ")=" << h.padHeight << std::endl;
    }
    f.close();
    return !f.fail();
}
