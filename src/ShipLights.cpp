/*   NAUTITECH - Simulateur de Navigation (Bridge Command fork)
     KYARA FEUX: navigation lights driven by the vessel's COLREG situation. See ShipLights.hpp.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#include "ShipLights.hpp"
#include "NavLight.hpp"
#include "IniFile.hpp"
#include "Constants.hpp"

#include <cmath>
#include <string>

namespace
{
    // Arcs, from Rule 21. Angles are measured from right ahead, the way NavLight wants them.
    const irr::f32 ARC_MASTHEAD_START = -112.5f, ARC_MASTHEAD_END = 112.5f;   // 225 deg
    const irr::f32 ARC_SIDE_PORT_START = -112.5f, ARC_SIDE_PORT_END = 0.0f;   // 112.5 deg
    const irr::f32 ARC_SIDE_STBD_START = 0.0f, ARC_SIDE_STBD_END = 112.5f;
    const irr::f32 ARC_STERN_START = 112.5f, ARC_STERN_END = 247.5f;          // 135 deg
    const irr::f32 ARC_ALL_START = -360.0f, ARC_ALL_END = 360.0f;

    const irr::f32 SIGNAL_SPACING_M = 2.0f;    // Rule: at least 2 m apart vertically
    const irr::f32 SIGNAL_RANGE_NM = 3.0f;     // all-round signal lights, nominal
    const irr::f32 ANCHOR_RANGE_NM = 3.0f;
    //Rule 22 ranges for a vessel of 12 m or more but less than 50 m
    const irr::f32 MASTHEAD_RANGE_NM = 5.0f;
    const irr::f32 SIDE_RANGE_NM = 2.0f;
    const irr::f32 STERN_RANGE_NM = 2.0f;

    const irr::video::SColor COL_RED(255, 255, 40, 25);
    const irr::video::SColor COL_WHITE(255, 255, 250, 235);
    const irr::video::SColor COL_GREEN(255, 30, 255, 60);
    //Working lights are warm, not white: sodium and filament fittings, not navigation lamps.
    const irr::video::SColor COL_DECK(255, 255, 226, 170);
    const irr::f32 DECK_RANGE_NM = 1.0f;
    const irr::f32 FLOOD_RADIUS_M = 22.0f;     // how far a deck flood reaches
    const irr::u32 DECK_DAYLIGHT_CUTOFF = 150; // above this ambient level a flood adds nothing
    const int MAX_DYNAMIC_LIGHTS = 3;          // per vessel; the driver has very few to give

    inline bool isRedish(irr::video::SColor c) { return c.getRed() > 120 && c.getGreen() < 100 && c.getBlue() < 100; }
    inline bool isGreenish(irr::video::SColor c) { return c.getGreen() > 120 && c.getRed() < 120; }
    inline bool isYellowish(irr::video::SColor c) { return c.getRed() > 180 && c.getGreen() > 150 && c.getBlue() < 140; }
}

ShipLights::ShipLights()
    : anchorFwd(-1), anchorAft(-1), situation(SIT_UNDERWAY), deckLights(false),
    lengthMetres(20.0f), allowDynamicLights(false), dynamicLightsUsed(0), loaded(false)
{
    for (int i = 0; i < 16; i++) { overrideOff[i] = false; }
}

ShipLights::~ShipLights()
{
    //NavLight nodes belong to the scene manager; it cleans them up with the ship.
}

ShipLights::Role ShipLights::roleFromName(const std::string& name) const
{
    if (name == "masthead") { return ROLE_MASTHEAD; }
    if (name == "masthead_aft" || name == "masthead2") { return ROLE_MASTHEAD_AFT; }
    if (name == "port") { return ROLE_PORT; }
    if (name == "starboard" || name == "stbd") { return ROLE_STARBOARD; }
    if (name == "stern") { return ROLE_STERN; }
    if (name == "anchor") { return ROLE_ANCHOR; }
    if (name == "deck") { return ROLE_DECK; }
    if (name == "accommodation") { return ROLE_ACCOMMODATION; }
    if (name == "searchlight") { return ROLE_SEARCHLIGHT; }
    return ROLE_UNUSED;
}

//No LightRole given? Work it out, so existing boat.ini files keep working untouched.
ShipLights::Role ShipLights::inferRole(irr::video::SColor colour, irr::core::vector3df position,
    irr::f32 minZ, irr::f32 maxZ, irr::f32 maxY) const
{
    if (isRedish(colour)) { return ROLE_PORT; }
    if (isGreenish(colour)) { return ROLE_STARBOARD; }
    //Yellow lamps are working lights, not navigation lights - nobody puts a yellow sidelight on.
    if (isYellowish(colour)) { return ROLE_DECK; }

    //White: the highest one is the masthead, the furthest aft is the sternlight, and anything
    //else white and high is a second masthead light.
    const irr::f32 span = (maxZ - minZ);
    if (span > 0.01f) {
        if (position.Y > 0.80f * maxY) { return ROLE_MASTHEAD; }
        if (position.Z < minZ + 0.25f * span) { return ROLE_STERN; }
        if (position.Z > minZ + 0.70f * span) { return ROLE_MASTHEAD_AFT; }
    }
    return ROLE_ANCHOR;
}

NavLight* ShipLights::makeSignalLight(irr::scene::ISceneManager* smgr, irr::scene::ISceneNode* shipNode,
    irr::core::vector3df position, irr::video::SColor colour, irr::f32 rangeNm)
{
    return new NavLight(shipNode, smgr, position, colour, ARC_ALL_START, ARC_ALL_END,
        rangeNm * M_IN_NM, "", 0);
}

void ShipLights::addLamp(irr::scene::ISceneManager* smgr, irr::scene::ISceneNode* shipNode, Role role,
    irr::core::vector3df position, irr::video::SColor colour,
    irr::f32 a0, irr::f32 a1, irr::f32 rangeNm)
{
    Lamp lamp;
    lamp.role = role;
    lamp.colour = colour;
    lamp.light = new NavLight(shipNode, smgr, position, colour, a0, a1, rangeNm * M_IN_NM, "", 0);
    lamps.push_back(lamp);
}

bool ShipLights::hasRole(Role role) const
{
    for (size_t i = 0; i < lamps.size(); i++) {
        if (lamps[i].role == role) { return true; }
    }
    return false;
}

//A real light source on the ship's geometry: the deck, the rails and the superstructure under it
//are genuinely lit, and the light moves and heels with her because it is a child of her node.
void ShipLights::attachLightSource(irr::scene::ISceneManager* smgr, irr::scene::ISceneNode* shipNode,
    Lamp& lamp, irr::core::vector3df position, irr::f32 radiusMetres)
{
    if (!allowDynamicLights || dynamicLightsUsed >= MAX_DYNAMIC_LIGHTS) { return; }

    irr::scene::ILightSceneNode* node = smgr->addLightSceneNode(shipNode, position);
    if (!node) { return; }
    node->setLightType(irr::video::ELT_POINT);

    irr::video::SLight data = node->getLightData();
    //Warm, and not too strong: a deck flood should pick out the deck, not turn night into day.
    data.DiffuseColor = irr::video::SColorf(0.85f, 0.74f, 0.52f);
    data.AmbientColor = irr::video::SColorf(0.06f, 0.05f, 0.04f);
    data.SpecularColor = irr::video::SColorf(0.20f, 0.18f, 0.14f);
    data.Radius = radiusMetres;
    data.Attenuation = irr::core::vector3df(0.0f, 1.0f / radiusMetres, 0.0f);
    node->setLightData(data);
    node->setVisible(false);

    lamp.lightSource = node;
    dynamicLightsUsed++;
}

//A working light: the fitting you see from outside, and the light it casts.
void ShipLights::addDeckLamp(irr::scene::ISceneManager* smgr, irr::scene::ISceneNode* shipNode,
    irr::core::vector3df position, irr::f32 radiusMetres)
{
    Lamp lamp;
    lamp.role = ROLE_DECK;
    lamp.colour = COL_DECK;
    lamp.light = new NavLight(shipNode, smgr, position, COL_DECK, ARC_ALL_START, ARC_ALL_END,
        DECK_RANGE_NM * M_IN_NM, "", 0);
    attachLightSource(smgr, shipNode, lamp, position, radiusMetres);
    lamps.push_back(lamp);
}

void ShipLights::load(irr::scene::ISceneManager* smgr, irr::scene::ISceneNode* shipNode,
    const std::string& iniFilename, irr::f32 shipLengthMetres, irr::f32 modelUnitsPerMetre,
    irr::core::aabbox3df modelBox, bool dynamicLights)
{
    allowDynamicLights = dynamicLights;
    dynamicLightsUsed = 0;
    if (!smgr || !shipNode) { return; }
    lengthMetres = shipLengthMetres;
    if (modelUnitsPerMetre <= 0.0f) { modelUnitsPerMetre = 1.0f; }

    lamps.clear();
    signalLamps.clear();
    anchorFwd = anchorAft = -1;

    //---- 1. the lamps the model actually has -------------------------------------------------
    const irr::u32 numberOfLights = IniFile::iniFileTou32(iniFilename, "NumberOfLights");

    std::vector<irr::core::vector3df> positions;
    std::vector<irr::video::SColor> colours;
    std::vector<irr::f32> ranges;
    std::vector<irr::f32> startAngles, endAngles;
    std::vector<std::string> sequences;
    std::vector<irr::u32> phases;
    std::vector<Role> roles;

    irr::f32 minZ = 1e9f, maxZ = -1e9f, maxY = -1e9f;

    for (irr::u32 i = 1; i <= numberOfLights; i++) {
        irr::core::vector3df p(IniFile::iniFileTof32(iniFilename, IniFile::enumerate1("LightX", i)),
            IniFile::iniFileTof32(iniFilename, IniFile::enumerate1("LightY", i)),
            IniFile::iniFileTof32(iniFilename, IniFile::enumerate1("LightZ", i)));
        irr::video::SColor c(255,
            (irr::u32)IniFile::iniFileTof32(iniFilename, IniFile::enumerate1("LightRed", i)),
            (irr::u32)IniFile::iniFileTof32(iniFilename, IniFile::enumerate1("LightGreen", i)),
            (irr::u32)IniFile::iniFileTof32(iniFilename, IniFile::enumerate1("LightBlue", i)));
        //A black entry is a light that has been commented out with '/' in these ini files.
        if (c.getRed() == 0 && c.getGreen() == 0 && c.getBlue() == 0) { continue; }

        positions.push_back(p);
        colours.push_back(c);
        ranges.push_back(IniFile::iniFileTof32(iniFilename, IniFile::enumerate1("LightRange", i)) * M_IN_NM);
        startAngles.push_back(IniFile::iniFileTof32(iniFilename, IniFile::enumerate1("LightStartAngle", i)));
        endAngles.push_back(IniFile::iniFileTof32(iniFilename, IniFile::enumerate1("LightEndAngle", i)));
        sequences.push_back(IniFile::iniFileToString(iniFilename, IniFile::enumerate1("Sequence", i)));
        phases.push_back(IniFile::iniFileTou32(iniFilename, IniFile::enumerate1("PhaseStart", i)));
        roles.push_back(roleFromName(IniFile::iniFileToString(iniFilename, IniFile::enumerate1("LightRole", i))));

        if (p.Z < minZ) { minZ = p.Z; }
        if (p.Z > maxZ) { maxZ = p.Z; }
        if (p.Y > maxY) { maxY = p.Y; }
    }

    for (size_t i = 0; i < positions.size(); i++) {
        Lamp lamp;
        lamp.role = (roles[i] != ROLE_UNUSED) ? roles[i]
            : inferRole(colours[i], positions[i], minZ, maxZ, maxY);
        lamp.colour = colours[i];

        //An arc of -360..360 on a lamp that the rules say is sectored means "not specified", so
        //give it the correct COLREG arc rather than letting a sidelight shine astern.
        irr::f32 a0 = startAngles[i], a1 = endAngles[i];
        const bool unspecified = (a1 - a0) >= 700.0f;
        if (unspecified) {
            switch (lamp.role) {
            case ROLE_MASTHEAD: case ROLE_MASTHEAD_AFT: a0 = ARC_MASTHEAD_START; a1 = ARC_MASTHEAD_END; break;
            case ROLE_PORT:     a0 = ARC_SIDE_PORT_START; a1 = ARC_SIDE_PORT_END; break;
            case ROLE_STARBOARD:a0 = ARC_SIDE_STBD_START; a1 = ARC_SIDE_STBD_END; break;
            case ROLE_STERN:    a0 = ARC_STERN_START; a1 = ARC_STERN_END; break;
            default: break; //all-round lamps and working lights keep what they were given
            }
        }

        lamp.light = new NavLight(shipNode, smgr, positions[i], colours[i], a0, a1,
            ranges[i], sequences[i], phases[i]);
        lamps.push_back(lamp);
        if (lamp.role == ROLE_ANCHOR && anchorFwd < 0) { anchorFwd = (int)lamps.size() - 1; }
    }

    //Deck lamps that came from the ini get a real light source too, so the five yellow fittings
    //on the other-ship version of your boat actually light her decks.
    for (size_t i = 0; i < lamps.size(); i++) {
        if (lamps[i].lightSource) { continue; }
        if (lamps[i].role != ROLE_DECK && lamps[i].role != ROLE_ACCOMMODATION) { continue; }
        attachLightSource(smgr, shipNode, lamps[i], positions[i], FLOOD_RADIUS_M);
    }

    //---- 2. whatever navigation lamps this vessel does not have ------------------------------
    //Plenty of boat.ini files carry no NumberOfLights block at all (the own ship's usually
    //doesn't), and a few carry only some of the set. A vessel with no lamps can show no lights,
    //so the missing ones are built from the hull's own dimensions. Anything the ini DOES provide
    //is left exactly as it is.
    {
        const irr::f32 fwd = modelBox.MaxEdge.Z, aft = modelBox.MinEdge.Z;
        const irr::f32 top = modelBox.MaxEdge.Y, bottom = modelBox.MinEdge.Y;
        const irr::f32 lenU = fwd - aft;
        const irr::f32 halfBeam = 0.5f * (modelBox.MaxEdge.X - modelBox.MinEdge.X);
        const irr::f32 heightU = top - bottom;
        const irr::f32 deck = bottom + 0.45f * heightU;   // roughly bridge-deck level

        if (!hasRole(ROLE_MASTHEAD)) {
            addLamp(smgr, shipNode, ROLE_MASTHEAD,
                irr::core::vector3df(0.0f, bottom + 0.80f * heightU, aft + 0.62f * lenU),
                COL_WHITE, ARC_MASTHEAD_START, ARC_MASTHEAD_END, MASTHEAD_RANGE_NM);
        }
        if (!hasRole(ROLE_MASTHEAD_AFT) && lengthMetres >= 50.0f) {
            addLamp(smgr, shipNode, ROLE_MASTHEAD_AFT,
                irr::core::vector3df(0.0f, bottom + 0.95f * heightU, aft + 0.35f * lenU),
                COL_WHITE, ARC_MASTHEAD_START, ARC_MASTHEAD_END, MASTHEAD_RANGE_NM);
        }
        if (!hasRole(ROLE_PORT)) {
            addLamp(smgr, shipNode, ROLE_PORT,
                irr::core::vector3df(-0.92f * halfBeam, deck, aft + 0.60f * lenU),
                COL_RED, ARC_SIDE_PORT_START, ARC_SIDE_PORT_END, SIDE_RANGE_NM);
        }
        if (!hasRole(ROLE_STARBOARD)) {
            addLamp(smgr, shipNode, ROLE_STARBOARD,
                irr::core::vector3df(0.92f * halfBeam, deck, aft + 0.60f * lenU),
                COL_GREEN, ARC_SIDE_STBD_START, ARC_SIDE_STBD_END, SIDE_RANGE_NM);
        }
        if (!hasRole(ROLE_DECK) && !hasRole(ROLE_ACCOMMODATION)) {
            //No working lights in the ini either (the own ship's has none at all), so give her a
            //foredeck flood, an afterdeck flood and a light over the side door.
            //Just above the working deck, not up in the rigging: a flood mounted too high reads
            //as a glow hanging in the air.
            const irr::f32 fittingY = bottom + 0.56f * heightU;
            addDeckLamp(smgr, shipNode,
                irr::core::vector3df(0.0f, fittingY, aft + 0.78f * lenU), FLOOD_RADIUS_M);
            addDeckLamp(smgr, shipNode,
                irr::core::vector3df(0.0f, fittingY, aft + 0.20f * lenU), FLOOD_RADIUS_M);
            addDeckLamp(smgr, shipNode,
                irr::core::vector3df(0.70f * halfBeam, fittingY, aft + 0.45f * lenU),
                FLOOD_RADIUS_M * 0.7f);
        }
        if (!hasRole(ROLE_STERN)) {
            addLamp(smgr, shipNode, ROLE_STERN,
                irr::core::vector3df(0.0f, deck, aft + 0.03f * lenU),
                COL_WHITE, ARC_STERN_START, ARC_STERN_END, STERN_RANGE_NM);
        }
    }

    //---- 3. the signal lights the model does NOT have ----------------------------------------
    //Three all-round lamps in a vertical line above the masthead: enough for every situation in
    //the list (red-red, red-white-red, green-white). They are created dark and only lit when the
    //situation calls for them.
    //Over the highest white light if there is one, otherwise over the top of the hull itself.
    irr::core::vector3df mastTop(0.0f, modelBox.MaxEdge.Y,
        modelBox.MinEdge.Z + 0.60f * (modelBox.MaxEdge.Z - modelBox.MinEdge.Z));
    {
        irr::f32 bestY = -1e9f;
        for (size_t i = 0; i < positions.size(); i++) {
            if (positions[i].Y > bestY && !isRedish(colours[i]) && !isGreenish(colours[i])) {
                bestY = positions[i].Y;
                mastTop = positions[i];
            }
        }
    }

    const irr::f32 spacing = SIGNAL_SPACING_M * modelUnitsPerMetre;
    for (int i = 0; i < 3; i++) {
        Lamp lamp;
        lamp.role = ROLE_SIGNAL;
        lamp.signalIndex = i;
        lamp.colour = COL_WHITE;
        irr::core::vector3df p = mastTop;
        p.Y += spacing * (irr::f32)(i + 1);
        lamp.light = makeSignalLight(smgr, shipNode, p, COL_WHITE, SIGNAL_RANGE_NM);
        lamps.push_back(lamp);
        signalLamps.push_back((int)lamps.size() - 1);
    }

    //An anchor light if the vessel has none of its own: forward and low, as Rule 30 wants.
    if (anchorFwd < 0) {
        Lamp lamp;
        lamp.role = ROLE_ANCHOR;
        lamp.colour = COL_WHITE;
        irr::core::vector3df p = mastTop;
        p.Y += 0.5f * spacing;
        p.Z += 0.0f;
        lamp.light = makeSignalLight(smgr, shipNode, p, COL_WHITE, ANCHOR_RANGE_NM);
        lamps.push_back(lamp);
        anchorFwd = (int)lamps.size() - 1;
    }

    loaded = true;
}

int ShipLights::countRole(Role role) const
{
    int n = 0;
    for (size_t i = 0; i < lamps.size(); i++) { if (lamps[i].role == role) { n++; } }
    return n;
}

std::string ShipLights::describe() const
{
    //Printed at load, so a vessel that shows no lights can be diagnosed from the log alone.
    std::string out = "lamps: ";
    out.append(std::to_string((int)lamps.size()));
    out.append(" (masthead ").append(std::to_string(countRole(ROLE_MASTHEAD) + countRole(ROLE_MASTHEAD_AFT)));
    out.append(", sidelights ").append(std::to_string(countRole(ROLE_PORT) + countRole(ROLE_STARBOARD)));
    out.append(", stern ").append(std::to_string(countRole(ROLE_STERN)));
    out.append(", anchor ").append(std::to_string(countRole(ROLE_ANCHOR)));
    out.append(", signal ").append(std::to_string(countRole(ROLE_SIGNAL)));
    out.append(", deck ").append(std::to_string(countRole(ROLE_DECK) + countRole(ROLE_ACCOMMODATION)));
    out.append(")");
    return out;
}

//The rules themselves, in one place.
bool ShipLights::lampShouldBeLit(const Lamp& lamp, bool makingWay) const
{
    if (situation == SIT_LIGHTS_OUT) { return false; }

    const bool big = (lengthMetres >= 50.0f);
    //Under way for lights purposes = not at anchor and not aground.
    const bool underWay = (situation != SIT_ANCHORED && situation != SIT_AGROUND);

    //IMPORTANT: "under way" is not the same as "making way". A power-driven vessel under way
    //shows her masthead lights, sidelights and sternlight whether she is moving through the
    //water or lying stopped - Rule 23 does not care. Making way only matters for the vessels of
    //Rules 26 and 27: not under command, restricted in her ability to manoeuvre and fishing show
    //their sidelights and sternlight ONLY when making way, and a vessel not under command never
    //shows a masthead light at all.
    const bool special = (situation == SIT_NUC || situation == SIT_RAM || situation == SIT_FISHING);
    const bool showMasthead = underWay && (situation == SIT_UNDERWAY ||
        (situation == SIT_RAM && makingWay));
    const bool showSidesAndStern = underWay && (!special || makingWay);

    switch (lamp.role) {
    case ROLE_MASTHEAD:      return showMasthead;
    case ROLE_MASTHEAD_AFT:  return showMasthead && big;
    case ROLE_PORT:
    case ROLE_STARBOARD:
    case ROLE_STERN:         return showSidesAndStern;

    case ROLE_ANCHOR:        return (situation == SIT_ANCHORED || situation == SIT_AGROUND);

    case ROLE_SIGNAL:
        //The vertical line, lowest lamp first.
        switch (situation) {
        case SIT_NUC:     return lamp.signalIndex < 2;              // red over red
        case SIT_RAM:     return lamp.signalIndex < 3;              // red, white, red
        case SIT_AGROUND: return lamp.signalIndex < 2;              // red over red
        case SIT_FISHING: return lamp.signalIndex < 2;              // green over white (trawling)
        default:          return false;
        }

    case ROLE_DECK:
    case ROLE_ACCOMMODATION: return deckLights;
    case ROLE_SEARCHLIGHT:   return false; // handled separately
    default:                 return false;
    }
}

void ShipLights::update(irr::f32 scenarioTime, irr::u32 lightLevel, bool makingWay)
{
    if (!loaded) { return; }

    //Colour the signal line for the situation before deciding what is lit: the same three lamps
    //serve every pattern.
    for (size_t i = 0; i < signalLamps.size(); i++) {
        Lamp& lamp = lamps[signalLamps[i]];
        irr::video::SColor want = COL_RED;
        if (situation == SIT_RAM) {
            want = (lamp.signalIndex == 1) ? COL_WHITE : COL_RED;   // red - white - red
        }
        else if (situation == SIT_FISHING) {
            want = (lamp.signalIndex == 1) ? COL_GREEN : COL_WHITE; // white below, green above
        }
        if (want != lamp.colour) {
            lamp.colour = want;
            if (lamp.light) { lamp.light->setColour(want); }
        }
    }

    for (size_t i = 0; i < lamps.size(); i++) {
        Lamp& lamp = lamps[i];
        if (!lamp.light) { continue; }
        bool lit = lampShouldBeLit(lamp, makingWay);
        if (lamp.role < 16 && overrideOff[lamp.role]) { lit = false; }
        lamp.light->setEnabled(lit);
        lamp.light->update(scenarioTime, lightLevel);
        //A working light only casts light when it is on, and only once it is dark enough for it
        //to make any difference.
        if (lamp.lightSource) {
            lamp.lightSource->setVisible(lit && lightLevel < DECK_DAYLIGHT_CUTOFF);
        }
    }
}

void ShipLights::setSituation(Situation s)
{
    if (s >= 0 && s < SIT_COUNT) { situation = s; }
}

ShipLights::Situation ShipLights::getSituation() const { return situation; }

const char* ShipLights::getSituationName(Situation s)
{
    switch (s) {
    case SIT_UNDERWAY:   return "Under way";
    case SIT_ANCHORED:   return "At anchor";
    case SIT_AGROUND:    return "Aground";
    case SIT_NUC:        return "Not under command";
    case SIT_RAM:        return "Restricted in ability to manoeuvre";
    case SIT_FISHING:    return "Fishing / trawling";
    case SIT_LIGHTS_OUT: return "All lights out";
    default:             return "?";
    }
}

const wchar_t* ShipLights::getSituationNameFr(Situation s)
{
    switch (s) {
    case SIT_UNDERWAY:   return L"Feux de route";
    case SIT_ANCHORED:   return L"Au mouillage";
    case SIT_AGROUND:    return L"\u00C9chou\u00E9";
    case SIT_NUC:        return L"Non ma\u00EEtre de sa manoeuvre";
    case SIT_RAM:        return L"Capacit\u00E9 de manoeuvre restreinte";
    case SIT_FISHING:    return L"En p\u00EAche / chalutage";
    case SIT_LIGHTS_OUT: return L"Tous feux \u00E9teints";
    default:             return L"?";
    }
}

void ShipLights::setDeckLights(bool on) { deckLights = on; }
bool ShipLights::getDeckLights() const { return deckLights; }

void ShipLights::setRoleOverride(Role role, bool forceOff)
{
    if (role >= 0 && role < 16) { overrideOff[role] = forceOff; }
}

bool ShipLights::isRoleOverridden(Role role) const
{
    return (role >= 0 && role < 16) ? overrideOff[role] : false;
}

void ShipLights::clearOverrides()
{
    for (int i = 0; i < 16; i++) { overrideOff[i] = false; }
}

bool ShipLights::hasOverrides() const
{
    for (int i = 0; i < 16; i++) { if (overrideOff[i]) { return true; } }
    return false;
}

void ShipLights::moveNode(irr::f32 deltaX, irr::f32 deltaY, irr::f32 deltaZ)
{
    for (size_t i = 0; i < lamps.size(); i++) {
        if (lamps[i].light) { lamps[i].light->moveNode(deltaX, deltaY, deltaZ); }
    }
}