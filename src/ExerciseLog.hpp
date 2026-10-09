/*   NAUTITECH - Simulateur de Navigation
     Exercise record for the debrief: own ship's track, the traffic's tracks, closest approaches,
     and the events of the exercise (collisions, groundings, radar alarms, sound signals, failures
     given by the instructor...). Written as an HTML report (track plot, summary, event log) and a
     CSV of the track, in the user folder under Bilans/.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#ifndef __EXERCISELOG_HPP_INCLUDED__
#define __EXERCISELOG_HPP_INCLUDED__

#include <string>
#include <vector>
#include <stdint.h>

class ExerciseLog
{
public:
    //Own ship at one moment
    struct OwnState {
        float lat = 0, lon = 0;        //degrees
        float heading = 0, cog = 0;    //degrees
        float sogKts = 0, stwKts = 0;
        float rudder = 0;              //degrees
        float portEngine = 0, stbdEngine = 0; //-1..1
        float depth = 0;               //metres under the keel
        float visibilityNm = 10;
    };
    //Another ship at one moment
    struct OtherState {
        std::string name;
        float lat = 0, lon = 0;
        float rangeNm = 0;             //true distance from own ship
        bool present = true;           //false: not in the exercise (multiplayer, nobody on her) or sunk
    };

    //Event categories (colour and counts in the report)
    enum Category { EV_INFO, EV_COLLISION, EV_GROUNDING, EV_CONTACT, EV_ALARM, EV_SIGNAL, EV_FAILURE, EV_INSTRUCTOR };

    void begin(const std::string& scenario, const std::string& world, const std::string& ownShip, uint64_t startTimestamp);
    bool isStarted() const { return started; }

    //Every frame: closest approaches are kept from every frame, the track every 10 s of exercise time
    void update(float exerciseSeconds, const OwnState& own, const std::vector<OtherState>& others);
    void event(float exerciseSeconds, Category category, const std::wstring& text);

    //Writes the report (HTML) and the track (CSV) into folder (made if needed). Returns false on failure;
    //path: the HTML file written.
    bool write(const std::string& folder, std::string& path) const;

    //For the instructor window
    int count(Category category) const;
    float closestApproachNm(std::string* shipName = 0) const; //over all ships, -1 if none
    float elapsedSeconds() const { return lastSeconds; }

private:
    struct TrackPoint { float t; OwnState s; };
    struct OtherTrack {
        std::string name;
        std::vector<float> lat, lon;   //every 10 s
        float minRangeNm = -1;
        float minRangeTime = 0;
        float minRangeLat = 0, minRangeLon = 0;
    };
    struct Event { float t; Category category; std::wstring text; float lat, lon; };

    bool started = false;
    std::string scenarioName, worldName, ownShipName;
    uint64_t startTime = 0;
    float lastSeconds = 0;
    float lastSample = -1000;
    OwnState lastOwn;
    std::vector<TrackPoint> track;
    std::vector<OtherTrack> otherTracks;
    std::vector<Event> events;
    double distanceNm = 0;
    float maxSogKts = 0;
};

#endif
