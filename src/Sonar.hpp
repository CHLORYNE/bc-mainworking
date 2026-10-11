/*   NAUTITECH - Simulateur de Navigation
     The sonars: a 360 degree search sonar and a fish finder (vertical echo sounder), worked out
     from the simulation's world.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#ifndef __SONAR_HPP_INCLUDED__
#define __SONAR_HPP_INCLUDED__

#include "irrlicht.h"
#include <vector>
#include <deque>
#include <string>
#include <random>

class SimulationModel;

//What sends an echo back:
//  - the seabed: the terrain, with a fine relief and harder (rock) or softer (mud) patches added,
//    so the bottom reads as it would; the coast; ground rising in front of the beam
//  - fish: schools and single fish, made up around the ship as it goes, drifting slowly
//  - the hulls of the other ships, and the bubbles of their wakes (and of the own ship's)
//  - objects of the scenario (its sonar.ini: wrecks, lost containers...)
//  - the sea surface: reverberation that grows with the sea state
//
//360 degree search sonar: a beam tilted down by the tilt angle, turned all round the ship, one
//beam every 2 degrees; each beam gives the echoes along its slant range. The beam reaches the
//bottom at a range that depends on the depth and the tilt: past it the ground hides the rest.
//Fish finder: a cone straight down (wide at 50 kHz, narrow at 200 kHz), one column per ping;
//the bottom, its thickness from the slope under the cone and the hardness, the second echo of a
//hard bottom, fish (single fish make arches as the boat goes over them), the heave of the boat.
class Sonar
{
public:
    Sonar();

    void setModel(SimulationModel* m);
    //Advances everything: fish, wakes, the 360 sweep, the pings. dt: real seconds since the last call.
    void update(irr::f32 dt);

    //--- 360 degree search sonar ---
    enum { BEAMS = 180, BINS = 256 };
    //Echo strength 0..255, [beam * BINS + bin]; beam b is the true bearing 2b+1 degrees, bin i the
    //slant range (i + 0.5) / BINS of the range.
    const std::vector<irr::u8>& picture360() const { return beams; }
    irr::f32 sweepBearing() const { return (lastBeam + 0.5f) * (360.0f / BEAMS); } //true, degrees
    irr::u32 picture360Serial() const { return serial360; }

    static const int RANGE_COUNT = 8;
    int rangeIndex360() const { return range360; }
    void setRangeIndex360(int i);
    irr::f32 range360m() const;
    static irr::f32 range360Value(int i);
    irr::f32 tilt() const { return tiltDeg; }
    void setTilt(irr::f32 deg);
    int gain360() const { return gain360v; }
    void setGain360(int g);
    void clear360();
    irr::f32 transducerDepth() const { return TRANSDUCER_DEPTH; }

    //--- fish finder ---
    enum { COLS = 512, ROWS = 512 };
    struct Column {
        irr::u8 echo[ROWS];   //0..255, row r: depth (r + 0.5) / ROWS of the range below the transducer
        irr::f32 range;       //metres
        irr::f32 bottom;      //depth below the surface (0: no bottom found in the range)
        bool rangeChanged;    //first column at a new range
    };
    //Oldest first: column(0) .. column(columnCount() - 1)
    int columnCount() const { return (int)columnsFilled; }
    const Column& column(int i) const;
    irr::u32 echogramSerial() const { return pingSerial; }

    static const int SOUNDER_RANGE_COUNT = 13;
    static irr::f32 sounderRangeValue(int i);
    bool sounderAuto() const { return autoRange; }
    void setSounderAuto(bool on) { autoRange = on; }
    int sounderRangeIndex() const { return sounderRangeIdx; }
    void setSounderRangeIndex(int i);
    irr::f32 sounderRange() const { return sounderRangeValue(sounderRangeIdx); }
    int sounderGain() const { return gainSounder; }
    void setSounderGain(int g);
    bool lowFrequency() const { return freq50; }
    void setLowFrequency(bool on) { freq50 = on; }
    int scrollSpeed() const { return scrollDiv; }   //pings per column: 1 (fast), 2 or 4 (slow)
    void setScrollSpeed(int pingsPerColumn);
    int clutter() const { return clutterLevel; }
    void setClutter(int c);
    //Depth below the surface under the ship, from the last ping (0: none)
    irr::f32 lastDepth() const { return depthNow; }

private:
    struct Fish {
        irr::f32 x, z;        //absolute world metres
        irr::f32 depth;       //below the mean sea surface, metres
        irr::f32 radius;      //horizontal (single fish: 0)
        irr::f32 thick;       //vertical half thickness
        irr::f32 strength;    //0..1
        irr::f32 heading, speed;
        bool school;
    };
    struct WakePoint { irr::f32 x, z, t, strength; };
    struct Wreck { irr::f32 x, z, length, height, heading; std::wstring name; };
    struct Ship { irr::f32 x, z, y, heading, length, breadth, draft; };

    //The world, absolute coordinates (those of the scene plus the re-centring offset)
    void readWorld();
    irr::f32 seabedY(irr::f32 ax, irr::f32 az) const;     //height of the seabed (scene Y)
    irr::f32 hardness(irr::f32 ax, irr::f32 az) const;    //0 soft mud .. 1 rock
    irr::f32 waterDepth(irr::f32 ax, irr::f32 az) const;  //below the mean sea level
    irr::f32 layerDepth(irr::f32 ax, irr::f32 az) const;  //depth of the scattering layer
    void manageFish(irr::f32 simDt);
    void spawnFish(bool school, bool anywhere);
    void recordWakes();
    void loadScenarioObjects();

    void computeBeam(int b);
    void ping();
    irr::f32 random01();

    SimulationModel* model;
    std::mt19937 rng;
    bool loaded;

    //This frame's world
    irr::f32 offX, offZ;          //scene to absolute
    irr::f32 ownX, ownZ;          //absolute
    irr::f32 transducerY;         //scene Y of the transducer (moves with the heave)
    irr::f32 tide;
    irr::f32 seaState;            //0..12
    irr::f32 ownHeading, ownSpeed;
    std::vector<Ship> ships;
    irr::f32 simTime, lastSimTime;

    std::vector<Fish> fish;
    std::vector<std::deque<WakePoint> > wakes;   //[0]: own ship, [1 + n]: other ship n
    irr::f32 lastWakeTime;
    std::vector<Wreck> wrecks;
    bool fishOn;
    irr::f32 fishDensity;
    irr::f32 fishCheckTime;

    //360
    std::vector<irr::u8> beams;
    int range360;
    irr::f32 tiltDeg;
    int gain360v;
    irr::f32 beamAccumulator;
    int lastBeam;
    irr::u32 serial360;

    //Fish finder
    std::vector<Column> echogram;  //ring
    irr::u32 head;                 //next column written
    irr::u32 columnsFilled;
    irr::u32 pingSerial;
    bool autoRange;
    int sounderRangeIdx;
    int gainSounder;
    bool freq50;
    int scrollDiv;
    int clutterLevel;
    irr::f32 pingTimer;
    int pingsInColumn;
    std::vector<irr::f32> accumulated;  //pings averaged into the column being made
    irr::f32 depthNow;
    bool pendingRangeChange;

    static const irr::f32 TRANSDUCER_DEPTH;
};

#endif
