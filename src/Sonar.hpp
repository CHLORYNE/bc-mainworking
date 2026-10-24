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
//360 degree search sonar, as an omnidirectional (full circle) scanning sonar works: each
//transmission sends sound all round the ship at once, in a beam tilted down by the tilt angle
//(-5 to 90 degrees), and the picture is renewed from the ship outwards as the echoes come back
//(at the speed of sound: 0.67 s for 500 m); past the front, the last transmission's picture stays.
//One beam every 2 degrees. The beam reaches the bottom at a range set by the depth and the tilt:
//the bottom makes a ring, and past it the ground hides the rest.
//Fish finder (echo sounder): a cone straight down, one column per ping, at 50 kHz (wide cone,
//deep reach, more noise, the sound goes further into soft ground) and 200 kHz (narrow cone, finer
//detail, weaker with depth), both worked out at each ping so they can be shown side by side;
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
    irr::u32 picture360Serial() const { return serial360; }
    //How far the current transmission's echoes have come back, 0..1 of the range
    irr::f32 front360() const { return (irr::f32)frontBin / BINS; }

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
    enum { F50 = 0, F200 = 1 };
    struct Column {
        irr::u8 echo[2][ROWS];   //[F50 / F200] 0..255, row r: depth (r + 0.5) / ROWS of the range below the transducer
        irr::s16 bottomRow[2];   //row of the bottom's first edge (-1: none in the range)
        irr::f32 range;          //metres
        irr::f32 bottom;         //depth below the surface (0: no bottom found in the range)
        bool rangeChanged;       //first column at a new range
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

    int scrollSpeed() const { return scrollDiv; }   //pings per column: 1 (fast), 2 or 4 (slow)
    void setScrollSpeed(int pingsPerColumn);
    int clutter() const { return clutterLevel; }
    void setClutter(int c);
    //Depth below the surface under the ship, from the last ping (0: none)
    irr::f32 lastDepth() const { return depthNow; }
    //Heave compensation (and beam stabilisation for the 360): the transducer's rise and fall on the
    //waves taken out, as with a motion sensor. Without it the bottom trace waves with the sea.
    bool heaveCompensation() const { return heaveComp; }
    void setHeaveCompensation(bool on) { heaveComp = on; }

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

    void startTransmission();
    void computeCell(int b, int i);
    void ping();
    void computeColumn(int f, irr::f32 R, std::vector<irr::f32>& col, irr::s16& bottomRow);
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
    bool heaveComp;
    irr::f32 heave;               //the transducer's rise on the waves, metres
    irr::f32 stillDepth;          //depth under the ship in still water (the bridge echo sounder's)
    irr::f32 autoRangeTimer;      //how long the bottom has been out of the picture's band
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
    irr::u32 serial360;
    //The transmission under way
    bool txStarted;
    irr::f32 txTime, txRange, txTilt;
    int frontBin;
    std::vector<irr::f32> targetEcho;   //[beam * BINS + bin] fish, hulls, wakes, wrecks
    std::vector<irr::u8> shadowMask;    //[beam * BINS + bin] 1: behind a wreck
    int beamBlocked[BEAMS];             //first bin hidden by the ground
    irr::f32 beamPrevDepth[BEAMS];
    irr::f32 beamTail[BEAMS];           //echo where the ground took the whole beam: it fades out behind

    //Fish finder
    std::vector<Column> echogram;  //ring
    irr::u32 head;                 //next column written
    irr::u32 columnsFilled;
    irr::u32 pingSerial;
    bool autoRange;
    int sounderRangeIdx;
    int gainSounder;
    int scrollDiv;
    int clutterLevel;
    irr::f32 pingTimer;
    int pingsInColumn;
    std::vector<irr::f32> accumulated[2];  //pings into the column being made, per frequency
    irr::f32 depthNow;
    bool pendingRangeChange;

    static const irr::f32 TRANSDUCER_DEPTH;
};

#endif
