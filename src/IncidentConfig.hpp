/* SCENARIO INCENDIE (Kyara): everything the fire / abandon-ship / SAR sequence needs, stored per
   scenario in incident.ini. Shared by the simulator (SimulationModel) and the fire scenario
   editor (Simulator-fe). Positions are kept as lat/long so the file does not depend on the
   simulator's floating scene origin.

   All times are seconds after ignition (instructor Ctrl+F) unless stated otherwise. */
#ifndef __INCIDENTCONFIG_HPP_INCLUDED__
#define __INCIDENTCONFIG_HPP_INCLUDED__

#include <map>
#include <string>
#include <vector>

struct IncidentPoint {
    double lat, lon;
    IncidentPoint() : lat(0), lon(0) {}
    IncidentPoint(double latitude, double longitude) : lat(latitude), lon(longitude) {}
};

enum IncidentSurvivorKind { Survivor_MOB = 0, Survivor_Liferaft = 1, Survivor_Radeau = 2 };

struct IncidentSurvivor {
    int kind;              // IncidentSurvivorKind
    IncidentPoint pos;     // where it goes into the water
    int boat;              // rafts only: SAR boat number (1-based) that recovers it, 0 = nearest boat
    IncidentSurvivor() : kind(Survivor_MOB), boat(0) {}
};

struct IncidentSarBoat {
    int ship;                               // othership.ini number (1-based) of the rescue craft
    float speedKts;                         // transit speed
    float launchDelay;                      // s after the last survivor is in the water before she slips
    float moorHeading;                      // heading once back alongside, < 0 = her starting heading
    std::vector<IncidentPoint> outbound;    // route to the scene, walked in order before the pickups
    std::vector<IncidentPoint> inbound;     // route home, last point = berth. Empty = outbound reversed, back to her start
    IncidentSarBoat() : ship(0), speedKts(35.0f), launchDelay(0.0f), moorHeading(-1.0f) {}
};

struct IncidentHelo {
    std::string model;     // model folder (Models/Othership/<model>)
    bool hasBase;          // where she takes off from to reach the scene
    IncidentPoint base;
    float baseHeight;      // m above the casualty's waterline
    bool hasPad;           // where she lands once the operation is over
    IncidentPoint pad;
    float padHeight;
    IncidentHelo() : model("MAC SAR Eurocopter"), hasBase(false), baseHeight(12.0f), hasPad(false), padHeight(12.0f) {}
};

class IncidentConfig {
public:
    IncidentConfig();                        // the simulator's built-in behaviour (no incident.ini)
    static IncidentConfig trainingPreset();  // sensible starting values for a brand-new exercise

    bool load(const std::string& fileName);  // false if the file is missing; *this is left at defaults
    bool save(const std::string& fileName) const;
    std::string toText() const;                       // the incident.ini contents
    void loadFromText(const std::string& text);       // from incident.ini contents (e.g. an imported scenario)
    void loadFromMap(const std::map<std::string, std::string>& keys);   // keys lower-cased, as IncidentIni reads them

    float sinkStartTime() const;             // fireDuration - sinkLeadTime, never negative

    static const char* survivorKey(int kind);     // value written to SurvivorType(n)
    static const char* survivorModel(int kind);   // model folder spawned in the simulator

    int casualtyShip;          // othership.ini number (1-based) that burns on Ctrl+F, 0 = nearest to own ship
    float fireDuration;        // ignition -> fully sunk, if the fire is not put out
    float sinkLeadTime;        // she starts going down this long before fireDuration
    float fireSpreadTime;      // ignition -> fully involved
    float abandonTime;         // abandon-ship alarm, survivors start going into the water
    float survivorInterval;    // s between each survivor / raft entering the water
    float permanentListTime;   // put out after this -> saved, but the list is permanent
    float heloDelay;           // s from the trainee's call (3rd Ctrl+A, OSC) to the helicopters on scene
    float heloSpeedKts;        // helicopter transit speed (inbound and back to the pad)
    std::string coordinationCentre;   // local SAR centre named in the radio log, e.g. "MRSC DAKHLA"

    std::vector<IncidentSurvivor> survivors;
    std::vector<IncidentSarBoat> sarBoats;
    std::vector<IncidentHelo> helos;
};

// The DAKHLA exercise's SAR set-up, hard-coded in the simulator before incident.ini existed. Still
// used by the simulator for a scenario with no incident.ini, and by the editor to start from the
// same thing when it opens such a scenario.
struct IncidentBuiltInPreset {
    std::string rescueBoatName;              // matched against the ship names in the scenario
    float boatSpeedKts;
    float boatMoorHeading;
    std::vector<IncidentPoint> boatReturn;   // walked in order, last point = where she moors
    IncidentPoint heloPad[2];
    float heloPadHeight[2];
};
const IncidentBuiltInPreset& incidentBuiltInPreset();

// Small ini helpers: the file is read fresh every time (no cache), keys are case-insensitive and
// surrounding quotes are stripped, as with IniFile.
namespace IncidentIni {
    typedef std::map<std::string, std::string> Map;
    bool read(const std::string& fileName, Map& out);
    void readText(const std::string& text, Map& out);   // same, from text already in memory
    bool has(const Map& m, const std::string& key);
    std::string str(const Map& m, const std::string& key, const std::string& def = "");
    double num(const Map& m, const std::string& key, double def = 0.0);
    std::string key(const char* name, int a);
    std::string key(const char* name, int a, int b);
}

#endif // __INCIDENTCONFIG_HPP_INCLUDED__
