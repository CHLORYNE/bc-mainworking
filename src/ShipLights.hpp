/*   NAUTITECH - Simulateur de Navigation (Bridge Command fork)
     KYARA FEUX: navigation lights driven by the vessel's COLREG situation.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

     // WHAT THIS IS
     // A vessel does not have "lights on" or "lights off": it shows the set of lights that matches
     // what it is doing, and a watchkeeper reads that set to know what he is looking at. So the
     // instructor picks a SITUATION here, and the correct lights follow from the rules - which means
     // the display is compliant by construction and nobody has to remember whether a vessel
     // restricted in her ability to manoeuvre is red-white-red or red-red-white.
     //
     // Manual per-lamp overrides exist on top of that, precisely so an instructor CAN show a wrong or
     // incomplete set on purpose and ask the trainee what is wrong with it.
     //
     // LAMPS
     // Lamps come from the vessel's own boat.ini (the existing NumberOfLights / LightX(n) ... block).
     // Each one is given a role, either from LightRole(n) or inferred from its colour and position, so
     // existing ini files keep working untouched. The signal lights a situation needs but the 3D model
     // does not have (the two reds of a vessel not under command, an anchor light, and so on) are
     // created automatically in a vertical line above the masthead, 2 m apart as the rules require.

#ifndef __SHIPLIGHTS_HPP_INCLUDED__
#define __SHIPLIGHTS_HPP_INCLUDED__

//KYARA FEUX: MASTER SWITCH for the COLREG situation feature. 0 = off (current state): other ships
//keep only the lamps their own boat.ini declares, exactly like stock Bridge Command, the "Feux"
//instructor tab is not built, and Ctrl+Shift+L does nothing. Set to 1 to bring it all back - no
//other edit is needed anywhere. Everything still compiles either way, so nothing is lost.
//Off because generated lamp positions were landing wrong on some other-ship models.
#define KYARA_COLREG_ENABLED 1

#include "irrlicht.h"
#include <string>
#include <vector>

class NavLight;

class ShipLights
{
public:
    // The common half-dozen, plus "all lights out" for casualties and blackout drills.
    enum Situation
    {
        SIT_UNDERWAY = 0,   // Feux de route - power-driven vessel under way (Rule 23)
        SIT_ANCHORED,       // Au mouillage (Rule 30)
        SIT_AGROUND,        // Echoue (Rule 30 d)
        SIT_NUC,            // Non maitre de sa manoeuvre (Rule 27 a)
        SIT_RAM,            // Capacite de manoeuvre restreinte (Rule 27 b)
        SIT_FISHING,        // En peche / chalutage (Rule 26)
        SIT_LIGHTS_OUT,     // tous feux eteints
        SIT_COUNT
    };

    // What a single lamp is for.
    enum Role
    {
        ROLE_UNUSED = 0,
        ROLE_MASTHEAD,      // 225 deg white, forward
        ROLE_MASTHEAD_AFT,  // second masthead light, vessels 50 m and over
        ROLE_PORT,          // 112.5 deg red
        ROLE_STARBOARD,     // 112.5 deg green
        ROLE_STERN,         // 135 deg white
        ROLE_ANCHOR,        // all-round white
        ROLE_SIGNAL,        // all-round light in the vertical signal line (created by us)
        ROLE_DECK,          // working / deck light (not a navigation light)
        ROLE_ACCOMMODATION, // interior spill
        ROLE_SEARCHLIGHT
    };

    ShipLights();
    ~ShipLights();

    // shipNode: the vessel's scene node. lengthMetres decides the 50 m rules. modelUnitsPerMetre
    // is 1/scaleFactor, used to space the signal lights correctly.
    // modelBox: the vessel's own bounding box in MODEL units (unscaled), used to place any lamp
    // the ini does not provide. Many boat.ini files have no NumberOfLights block at all - the own
    // ship's usually doesn't - and a vessel with no lamps can show no lights, so the standard set
    // is built from the hull's own dimensions instead.
    void load(irr::scene::ISceneManager* smgr, irr::scene::ISceneNode* shipNode,
        const std::string& iniFilename, irr::f32 lengthMetres, irr::f32 modelUnitsPerMetre,
        irr::core::aabbox3df modelBox, bool allowDynamicLights,
        irr::f32 waterlineModelY = 0.0f, bool generateMissing = false);
    //KYARA FEUX: generateMissing is now OFF by default, and that is deliberate. Guessed lamp
    //positions cannot be right: a mast is where the modeller put it, and no rule drawn from a
    //bounding box finds it - the guesses came out beside the hull or halfway up the air. A lamp
    //only ever comes from LightX/Y/Z(n) in the vessel's own boat.ini now, which is data you can
    //see and correct. A vessel with no light block simply shows nothing, which is honest.
    //To opt one vessel back into the old guesswork, put GenerateLights=1 in her boat.ini.

    // makingWay: she is moving through the water. It matters: a vessel not under command shows
    // her sidelights and sternlight only when making way, and never a masthead light.
    void update(irr::f32 scenarioTime, irr::u32 lightLevel, bool makingWay);

    void setSituation(Situation s);
    Situation getSituation() const;
    static const char* getSituationName(Situation s);      // English, for logs
    static const wchar_t* getSituationNameFr(Situation s); // French, for the panel

    // Working lights, independent of the COLREG set
    void setDeckLights(bool on);
    bool getDeckLights() const;

    // Deliberate errors: force one role off (or back to automatic) so a trainee can be asked
    // what is wrong with what he sees.
    void setRoleOverride(Role role, bool forceOff);
    bool isRoleOverridden(Role role) const;
    void clearOverrides();
    bool hasOverrides() const;

    void moveNode(irr::f32 deltaX, irr::f32 deltaY, irr::f32 deltaZ);

    //What was actually built, for the log: "lamps: 9 (masthead 1, sidelights 2, ...)"
    std::string describe() const;
    int countRole(Role role) const;

    //KYARA FEUX TAB ---------------------------------------------------------------------------
    std::wstring describeExpectedFr() const;
    bool isMakingWay() const;
    irr::f32 getLengthMetres() const;
    static const wchar_t* getSituationShortFr(Situation s);
    static const int OVERRIDE_SLOTS = 7;
    static Role overrideRole(int slot);
    static const wchar_t* overrideLabelFr(int slot);
    static const wchar_t* overrideTipFr(int slot);

private:
    struct Lamp
    {
        NavLight* light;
        Role role;
        int signalIndex;      // position in the vertical signal line, 0 = lowest
        irr::video::SColor colour;
        //KYARA FEUX: a working light actually lights things. This is a real point light on the
        //ship's geometry, not a painted patch - a fake pool is a flat sheet that hangs in the air
        //the moment it is not exactly on a surface, which is what the first attempt looked like.
        irr::scene::ILightSceneNode* lightSource;
        Lamp() : light(0), role(ROLE_UNUSED), signalIndex(-1), lightSource(0) {}
    };

    Role roleFromName(const std::string& name) const;
    Role inferRole(irr::video::SColor colour, irr::core::vector3df position,
        irr::f32 minZ, irr::f32 maxZ, irr::f32 maxY) const;
    void addLamp(irr::scene::ISceneManager* smgr, irr::scene::ISceneNode* shipNode, Role role,
        irr::core::vector3df position, irr::video::SColor colour,
        irr::f32 a0, irr::f32 a1, irr::f32 rangeNm);
    bool hasRole(Role role) const;
    void addDeckLamp(irr::scene::ISceneManager* smgr, irr::scene::ISceneNode* shipNode,
        irr::core::vector3df position, irr::f32 radiusMetres);
    void attachLightSource(irr::scene::ISceneManager* smgr, irr::scene::ISceneNode* shipNode,
        Lamp& lamp, irr::core::vector3df position, irr::f32 radiusMetres);

    NavLight* makeSignalLight(irr::scene::ISceneManager* smgr, irr::scene::ISceneNode* shipNode,
        irr::core::vector3df position, irr::video::SColor colour, irr::f32 rangeNm);
    bool lampShouldBeLit(const Lamp& lamp, bool makingWay) const;

    std::vector<Lamp> lamps;
    // The signal lights we created: indices into lamps, lowest first, one set per colour pattern.
    std::vector<int> signalLamps;   // three all-round lamps in a vertical line
    int anchorFwd, anchorAft;       // indices into lamps, -1 if none

    Situation situation;
    bool deckLights;
    bool overrideOff[16];
    irr::f32 lengthMetres;
    //Fixed-function rendering can only handle a handful of lights at once (the sun is one of
    //them), so only the own ship gets real light sources. Other vessels get the lamps only -
    //you are looking at them from far enough away that it makes no difference.
    bool allowDynamicLights;
    int dynamicLightsUsed;
    bool loaded;
    bool lastMakingWay; //KYARA FEUX TAB
};

#endif