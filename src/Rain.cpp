

#include "Rain.hpp"
#include "Utilities.hpp"
#include "Constants.hpp"
#include <iostream>
#include <cmath>

//KYARA RAIN TUNING ----------------------------------------------------------------------
//Fall speed, in shell units per second. This is now a real speed, not a texture scroll rate:
//it is divided back out by the tile count in update(), so changing the shell size or the drop
//size below will NOT change how fast the rain appears to fall. One knob, one job.
//Turn UP for a squall, DOWN for drizzle.
static const irr::f32 FALL_SPEED_BASE = 0.5f;
//How much heavier rain speeds up (heavier drops fall faster).
static const irr::f32 FALL_SPEED_PER_INTENSITY = 0.35f;
//Maximum slant of the streaks, in degrees, at high wind.
static const irr::f32 MAX_SLANT_DEG = 35.0f;
//Assumed terminal velocity of a raindrop, m/s. Used to work out the slant angle from wind.
static const irr::f32 DROP_FALL_SPEED = 9.0f;

//KYARA SHELL GEOMETRY -------------------------------------------------------------------
//THE DOME OF NO RAIN. rain.x is a cylinder with open ends and we stand inside it, so looking
//straight down there is simply no geometry between you and the sea. The clear disc you see
//from a bird's eye view is that opening, and its half-angle is exactly:
//
//      atan(radius / half-height)
//
//The stock shells were radius 3.5 and half-height 2.5, which is a 54 degree cone of nothing -
//an enormous hole, which is what you were looking at. Making the cylinder TALLER relative to
//its radius shrinks that cone. At an aspect of 6 the hole is down to about 9 degrees, which
//only shows if you point the camera dead vertical.
//
//The rain below the waterline is not a problem: depth testing is still on, so the sea occludes
//it. Only ZWRITE is off.
static const irr::f32 SHELL_ASPECT = 18.0f;   //height scale = radius * this. Raise to shrink the hole.

static const irr::f32 RADIUS_NEAR = 1.5f;
static const irr::f32 RADIUS_MID = 4.0f;
static const irr::f32 RADIUS_FAR = 5.0f;

static const irr::f32 HEIGHT_NEAR = RADIUS_NEAR * SHELL_ASPECT;
static const irr::f32 HEIGHT_MID = RADIUS_MID * SHELL_ASPECT;
static const irr::f32 HEIGHT_FAR = RADIUS_FAR * SHELL_ASPECT;

//KYARA DROP SIZE ------------------------------------------------------------------------
//Tiling shrinks the drop: width divides by U_REPEAT, length divides by V_REPEAT.
//
//U is per-shell because the circumference grows with the radius. V is now expressed PER UNIT
//OF SHELL HEIGHT, so that the drops stay the same physical length no matter how tall we make
//the cylinders above. That decoupling is the whole point - otherwise every time you adjust
//SHELL_ASPECT to close up the hole, the streaks stretch and you are back where you started.
//
//Raise these to make the drops smaller and finer. The far shell is deliberately finer than the
//near one: that size gradient is most of what reads as depth.
static const irr::f32 U_REPEAT_NEAR = 6.0f;
static const irr::f32 U_REPEAT_MID = 9.0f;
static const irr::f32 U_REPEAT_FAR = 13.0f;

static const irr::f32 V_PER_UNIT_NEAR = 2.8f;
static const irr::f32 V_PER_UNIT_MID = 4.4f;
static const irr::f32 V_PER_UNIT_FAR = 6.4f;

static const irr::f32 V_REPEAT_NEAR = HEIGHT_NEAR * V_PER_UNIT_NEAR;
static const irr::f32 V_REPEAT_MID = HEIGHT_MID * V_PER_UNIT_MID;
static const irr::f32 V_REPEAT_FAR = HEIGHT_FAR * V_PER_UNIT_FAR;

//Mip bias, 0 = sharp. The drops are sampled far below the texture's native resolution, so they
//alias into hard-edged scratches. Nudging the sampler onto a blurrier mip is what turns a
//drawn-on line into something that looks like water. Raise for softer, 0 for crisp.
static const irr::s8 RAIN_LOD_BIAS = 4;
//----------------------------------------------------------------------------------------

//KYARA: build a texture matrix that does scale then translate.
//
//The original code called setTextureRotationCenter() and then setTextureTranslate(). Those are
//not composable: setTextureRotationCenter writes M[8] and M[9] (the offsets that keep the
//rotation pivoting about the middle of the tile), and setTextureTranslate then overwrites both.
//So the rotation pivoted about the tile corner, and there was no way to add a scale at all.
//
//The rotation is GONE from here entirely now, and lives on the scene node instead - see
//update(). A texture rotation slants the streaks the same way whatever direction you are
//looking, which means it has no direction in the world: it cannot respond to the wind, and it
//looks wrong the moment you turn the ship. Tilting the actual cylinder does have a world
//direction, and the projection then takes care of itself.
//
//Irrlicht applies the texture matrix as:
//    u' = u*M[0] + v*M[4] + M[8]
//    v' = u*M[1] + v*M[5] + M[9]
static void setRainTextureMatrix(irr::scene::ISceneNode* node,
    irr::f32 uRepeat, irr::f32 vRepeat,
    irr::f32 uOffset, irr::f32 vOffset)
{
    if (!node) { return; }

    irr::core::matrix4& m = node->getMaterial(0).getTextureMatrix(0);
    m.makeIdentity();

    m[0] = uRepeat;
    m[5] = vRepeat;

    //Scroll happens in tile space, so one unit of vOffset is exactly one tile - which is why
    //keeping the offsets wrapped to 0..1 in update() gives a seamless loop.
    m[8] = uOffset;
    m[9] = vOffset;
}

Rain::Rain() {
    rainIntensity = 0.0f;
    windSpeedKts = 0.0f;
    windDirDeg = 0.0f;
    smgr = 0;
    parent = 0;
    rainNode1 = 0;
    rainNode2 = 0;
    rainNode3 = 0;
}

Rain::~Rain() {
}

//KYARA: shared material setup for every rain layer.
void Rain::setupNode(irr::scene::ISceneNode* node)
{
    if (!node) { return; }

    node->setMaterialType(irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL);

    //Lighting OFF. addMeshSceneNode leaves it on, so the stock rain was modulated by the
    //ambient light and went almost black at night. Real rain scatters light and reads as
    //bright streaks against a dark background, not dark streaks against it.
    node->setMaterialFlag(irr::video::EMF_LIGHTING, false);

    //Don't write depth: the rain shells surround the camera and would otherwise clip other
    //transparent geometry (spray, other rain layers) drawn after them.
    node->setMaterialFlag(irr::video::EMF_ZWRITE_ENABLE, false);

    //We are inside the shells, so we need the inward faces.
    node->setMaterialFlag(irr::video::EMF_BACK_FACE_CULLING, false);

    //Fog on: rain further out should fade into the murk with everything else.
    node->setMaterialFlag(irr::video::EMF_FOG_ENABLE, true);

    node->setMaterialFlag(irr::video::EMF_BILINEAR_FILTER, true);

    //KYARA: bilinear alone is not enough now that the texture is tiled 14-32 times. At that
    //density each drop covers only a few pixels, the sampler undersamples it, and you get the
    //hard aliased scratches rather than rain. Trilinear picks a smaller mip and blends between
    //levels; anisotropic keeps it from smearing to mush on the shells seen at a glancing angle
    //(which is most of them, since we are stood inside the cylinder).
    node->setMaterialFlag(irr::video::EMF_TRILINEAR_FILTER, true);
    node->setMaterialFlag(irr::video::EMF_ANISOTROPIC_FILTER, true);

    //Push the sampler onto a blurrier mip than it would pick on its own. This is the softness
    //knob - it is what makes the drops read as fine and wet instead of drawn on with a pencil.
    node->getMaterial(0).TextureLayer[0].LODBias = RAIN_LOD_BIAS;

    //KYARA: the tiling in setRainTextureMatrix() only works if the sampler wraps. If this is
    //left clamped, a V repeat of 22 gives you ONE drop and twenty-one copies of the last row of
    //pixels smeared down the shell - which looks exactly like the long scratches we are trying
    //to kill.
    node->getMaterial(0).TextureLayer[0].TextureWrapU = irr::video::ETC_REPEAT;
    node->getMaterial(0).TextureLayer[0].TextureWrapV = irr::video::ETC_REPEAT;
}

void Rain::load(irr::scene::ISceneManager* smgr, irr::scene::ISceneNode* parent, irr::IrrlichtDevice* dev)
{
    this->parent = parent;
    this->smgr = smgr; //KYARA: needed in update() to find the active camera
    irr::video::IVideoDriver* driver = smgr->getVideoDriver();

    rainIntensity = 0.0f;
    windSpeedKts = 0.0f;
    windDirDeg = 0.0f;

    irr::scene::IMesh* rainMesh = smgr->getMesh("media/rain.x");
    irr::scene::IMeshManipulator* meshManipulator = smgr->getMeshManipulator();

    if (rainMesh != 0) {
        meshManipulator->flipSurfaces(rainMesh);
        rainNode1 = smgr->addMeshSceneNode(rainMesh);
        rainNode2 = smgr->addMeshSceneNode(rainMesh);
        rainNode3 = smgr->addMeshSceneNode(rainMesh);

        //THREE shells at clearly different radii. The stock pair were at 5.0 and 6.0 and
        //scrolled at almost the same rate (/2.0 and /2.2), so they moved as one sheet with no
        //sense of depth. Spreading the radii gives real parallax: the near layer streaks past
        //fast, the far layer drifts.
        //
        //KYARA: the HEIGHT is now derived from the radius via SHELL_ASPECT rather than being a
        //flat 5.0. Tall thin cylinders. This is what closes the open-ended hole you were seeing
        //from above - see the note on SHELL_ASPECT.
        rainNode1->setScale(irr::core::vector3df(RADIUS_NEAR, HEIGHT_NEAR, RADIUS_NEAR)); //near
        rainNode2->setScale(irr::core::vector3df(RADIUS_MID, HEIGHT_MID, RADIUS_MID));   //mid
        rainNode3->setScale(irr::core::vector3df(RADIUS_FAR, HEIGHT_FAR, RADIUS_FAR));   //far
    }
    else {
        dev->getLogger()->log("Failed to load rain mesh (rain.x)");
        rainNode1 = smgr->addEmptySceneNode();
        rainNode2 = smgr->addEmptySceneNode();
        rainNode3 = smgr->addEmptySceneNode();
    }

    //set textures
    irr::video::ITexture* texture;
    std::vector<irr::io::path> textureNames;
    textureNames.push_back("./media/rain_0.png");
    textureNames.push_back("./media/rain_1.png");
    textureNames.push_back("./media/rain_2.png");
    textureNames.push_back("./media/rain_3.png");
    textureNames.push_back("./media/rain_4.png");
    textureNames.push_back("./media/rain_5.png");
    textureNames.push_back("./media/rain_6.png");
    textureNames.push_back("./media/rain_7.png");
    textureNames.push_back("./media/rain_8.png");
    textureNames.push_back("./media/rain_9.png");
    textureNames.push_back("./media/rain_10.png");

    for (std::vector<irr::io::path>::iterator it = textureNames.begin(); it != textureNames.end(); ++it) {
        texture = driver->getTexture(*it);
        if (texture != 0) {
            rainTextures.push_back(texture);
        }
    }

    setupNode(rainNode1);
    setupNode(rainNode2);
    setupNode(rainNode3);

    applyTextures();
}
// RAIN RENDER 0
void Rain::setIntensity(irr::f32 intensity) {

    if (intensity != rainIntensity && intensity <= 10 && intensity >= 0) {
        rainIntensity = intensity;
        applyTextures();
    }
    //KYARA: at zero intensity the shells still cost a full alpha-blended, double-sided draw each,
    //per viewport, per reflection pass - for a texture that is entirely transparent. Take them out
    //of the render. The far shell is also empty below intensity 2.5 (see farIntensity in
    //applyTextures), so drop that one early too.
    bool rainVisible = (rainIntensity > 0.01f);
    if (rainNode1) { rainNode1->setVisible(rainVisible); }
    if (rainNode2) { rainNode2->setVisible(rainVisible); }
    if (rainNode3) { rainNode3->setVisible(rainVisible && rainIntensity > 2.5f); }
}

void Rain::setWind(irr::f32 windSpeedKts) {
    this->windSpeedKts = windSpeedKts;
}

//KYARA: METEOROLOGICAL convention - the direction the wind blows FROM.
void Rain::setWindDirection(irr::f32 windDirDeg) {
    this->windDirDeg = windDirDeg;
}

void Rain::applyTextures() {
    if (rainTextures.size() == 11) { //Check all textures 0-10 are loaded

        //Round one up and one down so we can get half step rain intensity level.
        irr::u8 texture1 = Utilities::round(rainIntensity + 0.25);
        irr::u8 texture2 = Utilities::round(rainIntensity - 0.25);

        //KYARA: the far layer is deliberately lighter. Distant rain is thinner and hazier than
        //the sheet right in front of your face, and mixing densities across the three shells
        //stops the whole thing reading as one flat curtain. Pulled further back (was -1.5) now
        //that the higher tile counts put a lot more drops on screen per layer - without this the
        //three shells stack into a solid wall of water.
        irr::f32 farIntensity = rainIntensity - 2.5f;
        if (farIntensity < 0.0f) { farIntensity = 0.0f; }
        irr::u8 texture3 = Utilities::round(farIntensity);

        if (rainNode1) { rainNode1->setMaterialTexture(0, rainTextures.at(texture1)); }
        if (rainNode2) { rainNode2->setMaterialTexture(0, rainTextures.at(texture2)); }
        if (rainNode3) { rainNode3->setMaterialTexture(0, rainTextures.at(texture3)); }
    }
}

void Rain::update(irr::f32 scenarioTime) {

    if (!rainNode1 || !rainNode2 || !rainNode3) { return; }

    if (rainIntensity <= 0.01f) { return; } //KYARA: nothing to scroll, nothing to position

    //KYARA: slant the streaks into the wind. Real rain does not fall vertically in a breeze -
    //the drop reaches terminal velocity (~9 m/s) and is carried sideways at the wind speed, so
    //the streak leans at atan(wind / fallSpeed). At 20 kn that is about 45 degrees; we cap it
    //at MAX_SLANT_DEG so it stays believable rather than horizontal, and so the tilted cylinder
    //does not lean far enough to swing its open end into view.
    irr::f32 windMps = windSpeedKts * KTS_TO_MPS;
    irr::f32 slantRad = atan2(windMps, DROP_FALL_SPEED);
    irr::f32 maxSlantRad = MAX_SLANT_DEG * RAD_IN_DEG;
    if (slantRad > maxSlantRad) { slantRad = maxSlantRad; }
    if (slantRad < -maxSlantRad) { slantRad = -maxSlantRad; }
    irr::f32 slantDeg = slantRad / RAD_IN_DEG;

    //KYARA: WIND DIRECTION. Tilt the whole cylinder instead of rotating the texture.
    //
    //Irrlicht's Euler rotation applies X first, then Y. A tilt of +slant about X leans the
    //cylinder's axis towards +Z; the subsequent yaw about Y then swings that lean round to any
    //azimuth we like. So (slant, azimuth, 0) gives "lean by slant degrees, towards azimuth".
    //
    //We want the TOP of the streak to lean UPWIND: a falling drop is blown downwind as it
    //descends, so the top of its trail is where it was a moment ago - further upwind and higher.
    //windDirDeg is the direction the wind blows FROM, i.e. upwind, so it goes straight in.
    //
    //If it ever looks backwards, the fix is a single "+ 180.0f" here and nowhere else.
    irr::core::vector3df tilt(slantDeg, windDirDeg, 0.0f);
    rainNode1->setRotation(tilt);
    rainNode2->setRotation(tilt);
    rainNode3->setRotation(tilt);

    //Fall speed in shell units per second. Heavier rain means bigger drops, which fall faster.
    irr::f32 fallSpeed = FALL_SPEED_BASE + FALL_SPEED_PER_INTENSITY * rainIntensity;

    //KYARA: convert that speed into a texture scroll, in TILES per second.
    //
    //A tile is 1/V_PER_UNIT of a shell unit tall - and V_PER_UNIT is the same regardless of how
    //tall we make the shells. So multiplying by V_PER_UNIT converts units/sec into tiles/sec and
    //the apparent fall speed becomes completely independent of both SHELL_ASPECT and the drop
    //size. Change either of those and the rain still falls at FALL_SPEED_BASE.
    //
    //The small per-layer differences stop the three shells drifting into a visible moire beat.
    //They are NOT the source of the parallax - that now comes from the shells being at genuinely
    //different distances, which is where it should come from.
    irr::f32 y1 = scenarioTime * fallSpeed * 1.00f * V_PER_UNIT_NEAR;
    irr::f32 y2 = scenarioTime * fallSpeed * 0.93f * V_PER_UNIT_MID;
    irr::f32 y3 = scenarioTime * fallSpeed * 0.86f * V_PER_UNIT_FAR;

    //Sideways drift, so the sheet visibly moves across the wind rather than just leaning.
    irr::f32 drift = scenarioTime * (windMps / 60.0f);

    //Keep the offsets in 0..1 - the texture matrix wraps, but letting these grow without bound
    //eventually costs float precision and the rain starts to judder.
    //
    //KYARA: this used to be "y1 - (int)y1". scenarioTime is absolute, so once it is multiplied
    //by the fall rate AND the tile count the value can pass 2^31 and the cast to int overflows -
    //undefined behaviour, and in practice the rain locks solid or stutters. fmod has no ceiling.
    y1 = fmod(y1, 1.0f);
    y2 = fmod(y2, 1.0f);
    y3 = fmod(y3, 1.0f);
    drift = fmod(drift, 1.0f);

    //Slightly different phase on each layer, so they never line up into a visible pattern.
    setRainTextureMatrix(rainNode1, U_REPEAT_NEAR, V_REPEAT_NEAR, 0.50f + drift, y1);
    setRainTextureMatrix(rainNode2, U_REPEAT_MID, V_REPEAT_MID, 0.17f + drift * 0.7f, y2);
    setRainTextureMatrix(rainNode3, U_REPEAT_FAR, V_REPEAT_FAR, 0.71f + drift * 0.4f, y3);

    //KYARA: centre the shells on the CAMERA, not the ship.
    //
    //This is the other half of the missing-rain bug. The shells used to sit on the own ship, so
    //the moment you switched to any view that is not standing on the bridge - bird's eye, an
    //external camera, another vessel - you were looking at a small cylinder of rain hanging
    //around the hull from the OUTSIDE, with clear air everywhere else. Rain is supposed to be
    //everywhere; what it actually has to do is surround the eye.
    //
    //Fall back to the ship if there is no camera yet (first frame, or a headless run).
    irr::core::vector3df centre;
    irr::scene::ICameraSceneNode* camera = smgr ? smgr->getActiveCamera() : 0;
    if (camera) {
        centre = camera->getAbsolutePosition();
    }
    else if (parent) {
        centre = parent->getPosition();
    }
    else {
        return;
    }

    //Position only, never rotation - the rain must not spin with the hull or the camera.
    rainNode1->setPosition(centre);
    rainNode2->setPosition(centre);
    rainNode3->setPosition(centre);
}
