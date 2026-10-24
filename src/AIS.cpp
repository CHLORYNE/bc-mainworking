/*   BridgeCommand 5.7 Copyright (C) James Packer
     This file is Copyright (C) 2022 Fraunhofer FKIE

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation

     This program is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY Or FITNESS For A PARTICULAR PURPOSE.  See the
     GNU General Public License For more details.

     You should have received a copy of the GNU General Public License along
     with this program; if not, write to the Free Software Foundation, Inc.,
     51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. */

#include "AIS.hpp"
#include <cmath>
#include "Constants.hpp"
#include "SimulationModel.hpp"
#include "libs/Irrlicht/irrlicht-svn/include/irrTypes.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <tuple>
#include <vector>
#include <cstdlib>

int AIS::currentShip = 0;
bool AIS::initialized = false;
std::vector<irr::u32> AIS::lastUpdates;
std::vector<bool> AIS::classAReport(168, 0);
std::vector<irr::u32> AIS::lastStaticUpdates;
bool AIS::staticInitialized = false;
// arbitrary MMSIs from European countries to assign to otherShips
constexpr const int AIS::mmsis[] = {211032189, 226155323, 232984311, 224513921, 245193002, 247829914};

std::vector<irr::u32> AIS::getReadyShips(SimulationModel* model, irr::u32 now) {
    if (!initialized) {
        for (irr::u32 i=0; i < model->getNumberOfOtherShips(); i++) {
            lastUpdates.push_back(i * 600); // offset ship reports in 600 ms increments
        }
        initialized = true;
    }

    std::vector<irr::u32> readyShips;
    readyShips.clear();

    for (irr::u32 ship=0; ship < lastUpdates.size(); ship++) {
        irr::u32 elapsed_time;
        if (now > lastUpdates[ship]) {
            elapsed_time = now - lastUpdates[ship];
        } else {
            elapsed_time = 0;
        }
        irr::u32 reportingInterval;
        irr::f32 shipSpeed = model->getOtherShipSpeed(ship);

        // TODO: take into account course changes
        // TODO: take into account transmission range in the case of huge maps
        if (shipSpeed <= 0) {
            reportingInterval = 180000; // 3 mins when moored
        } else if (shipSpeed <= 14 * KTS_TO_MPS) {
            reportingInterval = 10000; // 10 seconds under 14 knots
        } else if (shipSpeed <= 23 * KTS_TO_MPS) {
            reportingInterval = 6000; // 6 seconds under 23 knots
        } else {
            reportingInterval = 2000; // 2 seconds over 23 knots
        }

        // random delay to reporting to avoid coalescence of reports after a while
        if (elapsed_time >= reportingInterval + (rand() % 500)) {
            lastUpdates[ship] = now;
            if (!model->isOtherShipAbsent((int)ship)) { readyShips.push_back(ship); } //multiplayer: no student on her
        }
    }
    return readyShips;
}

std::tuple<std::string, int> AIS::generateClassAReport(SimulationModel* model, irr::u32 ship) {

    bool done = false;

    //0..359: a heading below 0 or of 360 and more would spill into the next fields of the message
    irr::f32 headingDeg = fmod(model->getOtherShipHeading(ship), 360.0f);
    if (headingDeg < 0) { headingDeg += 360.0f; }
    irr::u32 heading = (irr::u32)headingDeg % 360;
    irr::u32 mmsi = model->getOtherShipMMSI(ship);

    if (mmsi == 0) {
        // mmsi is not set, give the ship a vanity mmsi
        mmsi = mmsis[ship % (sizeof(mmsis) / sizeof(mmsis[0]))] + (ship % 10000);
        // keep incrementing mmsi until it reaches a value that has not been allocated
        // e.g. via scenario config
        bool collision = true;
        irr::u32 ships = model->getNumberOfOtherShips();
        while (collision) {
            collision = false;
            for (int i=0; i < ships; i++) {
                if (model->getOtherShipMMSI(i) == mmsi) {
                    collision = true;
                    break;
                }
            }
            if (collision) mmsi++;
        }
        model->setOtherShipMMSI(ship, mmsi);
    }

    // AIS speed over ground is in 0.1-knot increments, capped to 102.2 knots
    // getOtherShipSpeed returns speed in m/s, multiply by 1.9438445 to get knots
    //(Speed over ground has no sign: a ship going astern used to give a negative number, sent as ~97 kn)
    irr::u32 speed = std::min<int>((int)(10.0f * MPS_TO_KTS * fabs(model->getOtherShipSpeed(ship))), 1022);

    // BC internal coordinate system
    irr::f32 shipLong = model->getOtherShipLong(ship);
    irr::f32 shipLat  = model->getOtherShipLat(ship);

    std::uint32_t timestamp = model->getTimestamp() % 60;


    // fill class A report fields
    
    // 0-5: message type, set to 0b000001 for normal class A position report
    classAReport[5] = 1;
    
    // 6-7 repeat indicator, set to 0b11 to signify do not repeat
    classAReport[6] = 1;
    classAReport[7] = 1;

    // 8-37 MMSI, 9-decimal digit in 30 bit field
    for (int i=0; i < 30; i++) {
        classAReport[8 + 29 - i] = mmsi % 2;
        mmsi >>= 1;
    }

    // 38-41 navigation status
    // set to 0b0000 for underway using engine
    classAReport[38] = 0;
    classAReport[39] = 0;
    classAReport[40] = 0;
    classAReport[41] = 0;
    if (speed == 0) {
        // if not moving, set to 0b0001 for anchored
        classAReport[41] = 1;
    }

    // 42-49 rate of turn, set to 0x80 for no turn information available
    // TODO: add rate of turn of other ships 
    classAReport[42] = 1;
    for (int i=43; i <= 49; i++) {
        classAReport[i] = 0;
    }

    // 50-59 speed over ground, 10 bit field
    for (int i=0; i < 10; i++) {
        classAReport[50 + 9 - i] = speed % 2;
        speed >>= 1;
    }

    // 60 position accuracy, set to 0b1 to indicate DGPS-quality fix, since
    // shipLong and shipLat have 5 decimals giving a 1m resolution.
    classAReport[60] = 1;

    // 61-88 longitude in a 28-bit field encoding a signed integer representing a float with a
    // resolution of 0.0001 corresponding to the longitude in minutes
    std::int32_t longitude = (int) 600000.0f * shipLong;
    bool longIsNeg = longitude < 0;
    for (int i=0; i < 28; i++) {
        classAReport[61 + 27 - i] = longitude % 2;
        longitude >>= 1;
    }
    if (longIsNeg) classAReport[61] = 1; // set the sign bit
    
    // 89-115 latitude in a 27-bit field encoding a signed integer representing a float with a
    // resolution of 0.0001 corresponding to the latitude in minutes
    std::int32_t latitude = (int) 600000.0f * shipLat;
    bool latIsNeg = latitude < 0;
    for (int i=0; i < 27; i++) {
        classAReport[89 + 26 - i] = latitude % 2;
        latitude >>= 1;
    }
    if (latIsNeg) classAReport[89] = 1; // set the sign bit

    // 116-127 course over ground, 12 bit field, unsigned int representing a float with
    // a resolution of 0.1 corresponding to the course over ground in degrees relative to true north
    irr::u32 cog = 10 * heading;
    for (int i=0; i < 12; i++) {
        classAReport[116 + 11 - i] = cog % 2;
        cog >>= 1;
    }

    // 128-136 true heading, 9 bit field, unsigned int
    for (int i=0; i < 9; i++) {
        classAReport[128 + 8 - i] = heading % 2;
        heading >>= 1;
    }

    // 137-142 timestamp, 6 bit field, unsigned int corresponding to the seconds of current UTC time
    for (int i=0; i < 6; i++) {
        classAReport[137 + 5 - i] = timestamp % 2;
        timestamp >>= 1;
    }

    // 143-144 maneuver indicator, set to 0b00 for no special maneuver
    classAReport[143] = 0;
    classAReport[144] = 1;

    // 145-147 not used
    
    // 148 RAIM flag, set to 0b0 for unset
    classAReport[148] = 0;

    // 149-167 radio status, 19 bit field, unsigned integer for radio diagnostic, leave as 0 for now
    
    // convert bit sequence to armored ASCII
    std::string payload = bitsToArmoredASCII(classAReport);

    // number of bits we need to append to get the payload length to a multiple of 6
    // always 0 since we always generate a class A Report of length 168
    // int fillBits = (6 - (168 % 6)) % 6;

    return std::make_tuple(payload, 0);
}

std::string AIS::bitsToArmoredASCII(std::vector<bool> bits) {
    // must be called with padded payload!
    assert(bits.size() % 6 == 0);

    int counter = 0;

    std::string payload(bits.size() / 6, 0);
    int index = 0;

    for (int i=0; i < bits.size(); i++) {
        payload[index] <<= 1;
        payload[index] |= bits[i];
        counter += 1;
        
        if (counter % 6 == 0) {
            counter = 0;
            payload[index] += 48;

            if (payload[index] >= 88) {
                payload[index] += 8;
            }
            index += 1;
        }
    }
    return payload;
}
// --- Kyara: AIS 6-bit field helpers for Type 5 static reports ---
static void aisAppendUInt(std::vector<bool>& bits, irr::u32 value, int nbits) {
    for (int b = nbits - 1; b >= 0; b--) { bits.push_back((value >> b) & 1); }
}
static void aisAppendText(std::vector<bool>& bits, const std::string& s, int nChars) {
    for (int i = 0; i < nChars; i++) {
        int val = 0;
        if (i < (int)s.size()) {
            int c = toupper((unsigned char)s[i]);
            if (c >= 64 && c <= 95) { val = c - 64; } // @A-Z[\]^_
            else if (c >= 32 && c <= 63) { val = c; } // space..?
            else { val = 0; } // unmapped -> '@'
        }
        for (int b = 5; b >= 0; b--) { bits.push_back((val >> b) & 1); }
    }
}

std::vector<irr::u32> AIS::getReadyShipsStatic(SimulationModel* model, irr::u32 now) {
    if (!staticInitialized) {
        for (irr::u32 i = 0; i < model->getNumberOfOtherShips(); i++) {
            lastStaticUpdates.push_back(i * 1000); // stagger initial static reports
        }
        staticInitialized = true;
    }
    const irr::u32 staticInterval = 30000; // 30 s (real AIS is ~6 min; faster is fine in sim)
    std::vector<irr::u32> ready;
    for (irr::u32 ship = 0; ship < lastStaticUpdates.size(); ship++) {
        irr::u32 elapsed = (now > lastStaticUpdates[ship]) ? now - lastStaticUpdates[ship] : 0;
        if (elapsed >= staticInterval) {
            lastStaticUpdates[ship] = now;
            ready.push_back(ship);
        }
    }
    return ready;
}

std::tuple<std::string, int> AIS::generateStaticReport(SimulationModel* model, irr::u32 ship) {
    irr::u32 mmsi = model->getOtherShipMMSI(ship); // Type 1 has already assigned a vanity MMSI

    irr::f32 length = model->getOtherShipLength(ship);
    irr::f32 breadth = model->getOtherShipBreadth(ship);
    irr::u32 dimBow = (irr::u32)(length * 0.5f); if (dimBow > 511) dimBow = 511;
    irr::u32 dimStern = (irr::u32)(length * 0.5f); if (dimStern > 511) dimStern = 511;
    irr::u32 dimPort = (irr::u32)(breadth * 0.5f); if (dimPort > 63)  dimPort = 63;
    irr::u32 dimStbd = (irr::u32)(breadth * 0.5f); if (dimStbd > 63)  dimStbd = 63;

    std::string name = model->getOtherShipName(ship); // shown on the chart

    std::vector<bool> b;
    aisAppendUInt(b, 5, 6);          // message type 5
    aisAppendUInt(b, 0, 2);          // repeat
    aisAppendUInt(b, mmsi, 30);      // MMSI
    aisAppendUInt(b, 0, 2);          // AIS version
    aisAppendUInt(b, 0, 30);         // IMO number (none)
    aisAppendText(b, "", 7);         // call sign (7 chars, blank)
    aisAppendText(b, name, 20);      // vessel name (20 chars)
    aisAppendUInt(b, 0, 8);          // ship type (0 = not available)
    aisAppendUInt(b, dimBow, 9);
    aisAppendUInt(b, dimStern, 9);
    aisAppendUInt(b, dimPort, 6);
    aisAppendUInt(b, dimStbd, 6);
    aisAppendUInt(b, 1, 4);          // EPFD type = GPS
    aisAppendUInt(b, 0, 4);          // ETA month
    aisAppendUInt(b, 0, 5);          // ETA day
    aisAppendUInt(b, 24, 5);         // ETA hour (24 = n/a)
    aisAppendUInt(b, 60, 6);         // ETA minute (60 = n/a)
    aisAppendUInt(b, 0, 8);          // draught
    aisAppendText(b, "", 20);        // destination (blank)
    aisAppendUInt(b, 0, 1);          // DTE
    aisAppendUInt(b, 0, 1);          // spare
    // 424 bits -> pad to 426 for 6-bit alignment, 2 fill bits
    b.push_back(0); b.push_back(0);
    return std::make_tuple(bitsToArmoredASCII(b), 2);
}