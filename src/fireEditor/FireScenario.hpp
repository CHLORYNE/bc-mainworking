/* SCENARIO INCENDIE - fire scenario editor (Simulator-fe).
   The whole exercise as the editor sees it: environment, own ship, the burning ship, the SAR
   boats, any other traffic, and the fire / SAR settings (incident.ini). Loads from and saves to
   a normal scenario folder, so the simulator and the standard editor can still open it. */
#ifndef __FIRESCENARIO_HPP_INCLUDED__
#define __FIRESCENARIO_HPP_INCLUDED__

#include <string>
#include <vector>

#include "../IncidentConfig.hpp"

struct EdLeg {
    float bearing, speed, distance;
    EdLeg() : bearing(0), speed(0), distance(0) {}
};

struct EdShip {
    std::string type;           // model folder name
    IncidentPoint pos;          // initial position
    float heading;              // initial heading (own ship: InitialBearing, others: first leg bearing)
    float speed;                // own ship only: initial speed, kts
    unsigned int mmsi;
    bool drifting;
    std::vector<EdLeg> legs;    // other traffic only: kept exactly as loaded
    EdShip() : heading(0), speed(0), mmsi(0), drifting(false) {}
};

class FireScenario {
public:
    FireScenario();

    // A brand-new exercise on worldName, laid out around the given position.
    void makeNew(const std::string& worldName, const IncidentPoint& centre,
        const std::string& ownShipType, const std::string& otherShipType, const std::string& rescueShipType,
        const std::string& heloModel);

    // Load an existing scenario folder. Without an incident.ini the burning ship, the SAR boats and
    // the survivors are set up the way the simulator would run that scenario today.
    bool load(const std::string& scenarioDir, const std::string& scenarioName, std::string& error);

    // Write environment.ini, ownship.ini, othership.ini, description.ini and incident.ini.
    bool save(const std::string& scenarioDir, std::string& error);

    // A model may be a SAR boat only if it is an other-ship whose boat.ini says FireFighting=1
    // (Models/Othership/<type>/boat.ini, user folder first).
    static bool isRescueModel(const std::string& type);

    // True if pt lies on the world's chart (used to decide whether the built-in preset applies).
    static bool inBounds(const IncidentPoint& pt, double south, double west, double latExtent, double longExtent);
    void applyBuiltInPresetIfInside(double south, double west, double latExtent, double longExtent);

    std::string name;
    std::string worldName;
    std::string description;
    float startTimeHours;
    unsigned int day, month, year;
    float sunRise, sunSet, weather, rain, visibility, windDirection, windSpeed;

    EdShip ownShip;
    bool hasCasualty;
    EdShip casualty;
    std::vector<EdShip> sarBoats;   // parallel to incident.sarBoats
    std::vector<EdShip> traffic;

    // Timings, survivors, SAR routes and helicopters. On save, casualtyShip and sarBoats[i].ship
    // are rewritten to match the order ships are written to othership.ini.
    IncidentConfig incident;
    bool hadIncidentFile;           // false: scenario opened without incident.ini (preset values shown)

    unsigned int nextMmsi() const;

    // Default survivor layout around the casualty: 5 MOB in a ring, radeau to port, liferaft to starboard
    // (the simulator's built-in layout, for a 40 m casualty).
    static std::vector<IncidentSurvivor> defaultSurvivors(const IncidentPoint& casualty);
};

// Metres <-> degrees around a reference latitude (small distances).
IncidentPoint offsetMetres(const IncidentPoint& from, double eastM, double northM);

#endif
