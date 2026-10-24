/*   Bridge Command 5.0 Ship Simulator
     Copyright (C) 2014 James Packer

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

#include "Sky.hpp"
#include "Constants.hpp"
#include <string>

     //KYARA SKY TUNING -----------------------------------------------------------------------
     //Visibility (nautical miles) at or above which the sky carries no haze at all.
static const irr::f32 HAZE_CLEAR_VIS_NM = 28.0f;   //start hazing the sky sooner

//Peak strength of the haze layer.
static const irr::f32 HAZE_MAX = 0.0f;

//----------------------------------------------------------------------------------------

Sky::Sky()
{
    dayDome = 0;
    cloudDome = 0;
    glowDome = 0;
    hazeDome = 0;
    heroBolt = 0;
    heroBoltTex = 0;
    heroBoltActive = false;
    heroBoltAge = 0.0f;
    heroBoltFullHeight = 0.0f;
    heroBoltWidth = 0.0f;

    // NAUTITECH tenders: spread evenly around the full circle of the sky (360/N apart),
    // so lightning appears all around the ship over time. tender1 dead ahead, then clockwise.
    for (int i = 0; i < NUM_TENDERS; i++) {
        tender[i] = 0;
        tenderTex[i] = 0;
        tenderActive[i] = false;
        tenderAge[i] = 0.0f;
        tenderBearingDeg[i] = (360.0f / (irr::f32)NUM_TENDERS) * (irr::f32)i;
    }
}

Sky::~Sky()
{
    //dtor
}

//KYARA: shared setup for the ADDITIVE domes.
//Additive blending means black texels contribute nothing, so a black-background texture just
//lays its bright pixels over whatever sky is already there.
//Irrlicht exposes no global colour on a skydome, so brightness is driven through
//EmissiveColor: with Lighting on and Ambient/Diffuse/Specular zeroed, emission is the only
//contributing term, which makes it a clean 0..255 fade knob.
//ColorMaterial MUST be ECM_NONE: the default (ECM_DIFFUSE) enables GL_COLOR_MATERIAL, which
//lets the mesh's white vertex colour override our material colours.
//Fog MUST be off: with an additive material the fog COLOUR gets added to the scene rather
//than blended into it, which washes the whole sky out.
static void setupAdditiveDome(irr::scene::ISceneNode* dome)
{
    if (!dome) { return; }

    dome->setMaterialFlag(irr::video::EMF_FOG_ENABLE, false);
    dome->setMaterialFlag(irr::video::EMF_LIGHTING, true);

    irr::video::SMaterial& m = dome->getMaterial(0);
    m.MaterialType = irr::video::EMT_TRANSPARENT_ADD_COLOR;
    m.ColorMaterial = irr::video::ECM_NONE;
    m.AmbientColor = irr::video::SColor(255, 0, 0, 0);
    m.DiffuseColor = irr::video::SColor(255, 0, 0, 0);
    m.SpecularColor = irr::video::SColor(255, 0, 0, 0);
    m.EmissiveColor = irr::video::SColor(255, 0, 0, 0);
}

void Sky::load(irr::scene::ISceneManager* smgr)
{
    irr::video::IVideoDriver* driver = smgr->getVideoDriver();
    driver->setTextureCreationFlag(irr::video::ETCF_CREATE_MIP_MAPS, false);

    //Sky-pass nodes are drawn in creation order, so create back to front.

    //1) BASE SKY. Solid and lit, as stock Bridge Command. Do NOT make this additive - an
    //   additive base sky adds the fog colour and the background clear colour into the result.
    skyTexFair = driver->getTexture("media/Sky_partlycloudy_4096.jpg");
    skyTexStorm = driver->getTexture("media/Sky_overcast_storm_4096.jpg");
    skyIsStorm = false;
    dayDome = smgr->addSkyDomeSceneNode(skyTexFair,
        64, 32, 1.0f, 1.05f, 3.5 * M_IN_NM);
    dayDome->setMaterialFlag(irr::video::EMF_FOG_ENABLE, true);
    dayDome->setMaterialFlag(irr::video::EMF_LIGHTING, true);

    //1b) CLOUD COVER. The overcast picture over the fair one, lit the same way, its opacity the
    //    cloud cover: lit material, ColorMaterial off, so the diffuse alpha is the layer's alpha.
    cloudDome = smgr->addSkyDomeSceneNode(skyTexStorm, 64, 32, 1.0f, 1.05f, 3.5 * M_IN_NM);
    if (cloudDome) {
        cloudDome->setMaterialFlag(irr::video::EMF_FOG_ENABLE, true);
        cloudDome->setMaterialFlag(irr::video::EMF_LIGHTING, true);
        irr::video::SMaterial& cm = cloudDome->getMaterial(0);
        cm.MaterialType = irr::video::EMT_TRANSPARENT_VERTEX_ALPHA;
        cm.ColorMaterial = irr::video::ECM_NONE;
        cm.AmbientColor = irr::video::SColor(255, 255, 255, 255);
        cm.DiffuseColor = irr::video::SColor(0, 255, 255, 255);
        cm.ZWriteEnable = irr::video::EZW_OFF;
        cloudDome->setVisible(false);
    }

    //2) HORIZON GLOW. A uniform ambient tints the whole dome equally, which turned even the
    //   zenith red at sunset. This layer puts the warm colour only where it belongs, low down.
    glowDome = smgr->addSkyDomeSceneNode(driver->getTexture("media/Sky_glow.jpg"),
        64, 32, 1.0f, 1.05f, 3.5 * M_IN_NM);
    setupAdditiveDome(glowDome);

    //3) HAZE. The stock sky dome sits at a FIXED 3.5 NM while the fog runs out to the full
    //   visibility range, so on a 10 NM day the sky is only ~35% fogged while the sea and
    //   distant land are 100% fogged - the sky stays stubbornly clear. (James Packer's own
    //   comment in the original Sky.cpp: "Fixme: Range should probably be dependent on fog
    //   & camera range.") Rather than fight the fixed-function fog, we drive an explicit haze
    //   layer from the visibility range, tinted to the fog colour. Denser toward the horizon,
    //   as real haze is. Drawn LAST, so it sits over everything else.
    hazeDome = smgr->addSkyDomeSceneNode(driver->getTexture("media/Sky_haze.png"),
        64, 32, 1.0f, 1.05f, 3.5 * M_IN_NM);
    setupAdditiveDome(hazeDome);

    //4) NAUTITECH HERO BOLT. A big camera-facing billboard placed far out in the sky. Because
    //   it's a real 3D node at sky distance it is occluded by terrain, never draws over the
    //   radar, and only shows where sky is visible (through the wheelhouse windows, not over the
    //   ship). Additive so it glows against the storm cloud; the texture should be a bright bolt
    //   on a BLACK/transparent background (additive treats black as nothing). Hidden until fired.
    heroBoltTex = driver->getTexture("media/lightning_bolt.png");
    heroBolt = smgr->addBillboardSceneNode(0, irr::core::dimension2d<irr::f32>(1.0f, 1.0f));
    if (heroBolt) {
        if (heroBoltTex) { heroBolt->setMaterialTexture(0, heroBoltTex); }
        heroBolt->setMaterialType(irr::video::EMT_TRANSPARENT_ADD_COLOR);
        heroBolt->setMaterialFlag(irr::video::EMF_LIGHTING, false);
        heroBolt->setMaterialFlag(irr::video::EMF_FOG_ENABLE, false);
        heroBolt->setMaterialFlag(irr::video::EMF_ZWRITE_ENABLE, false);
        heroBolt->setVisible(false);
    }

    //5) NAUTITECH TENDER LIGHTNING. One billboard per picture (media/tender1.png ..). Same
    //   additive sky-billboard setup as the hero bolt; each is parked at its own bearing and
    //   flashed in turn by the SimulationModel timer. Hidden until fired. A missing texture just
    //   means that one shows nothing - it never crashes.
    for (int i = 0; i < NUM_TENDERS; i++) {
        std::string texPath = "media/tender" + std::to_string(i + 1) + ".png";
        tenderTex[i] = driver->getTexture(texPath.c_str());
        tender[i] = smgr->addBillboardSceneNode(0, irr::core::dimension2d<irr::f32>(1.0f, 1.0f));
        if (tender[i]) {
            if (tenderTex[i]) { tender[i]->setMaterialTexture(0, tenderTex[i]); }
            tender[i]->setMaterialType(irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL); // respect the PNG's transparency (no white-haze rectangle)
            tender[i]->getMaterial(0).MaterialTypeParam = 0.05f; // alpha-ref: cut the faint haze, keep the bolt
            tender[i]->setMaterialFlag(irr::video::EMF_LIGHTING, false);
            tender[i]->setMaterialFlag(irr::video::EMF_FOG_ENABLE, false);
            tender[i]->setMaterialFlag(irr::video::EMF_ZWRITE_ENABLE, false);
            tender[i]->setVisible(false);
        }
    }

    driver->setTextureCreationFlag(irr::video::ETCF_CREATE_MIP_MAPS, true);
}

//NAUTITECH: flash tender[index] at its fixed bearing around the sky, high up and far out.
//Position is computed relative to the current camera so it stays "in the sky" as the ship moves.
void Sky::triggerTender(int index, irr::core::vector3df cameraPos)
{
    if (index < 0 || index >= NUM_TENDERS) { return; }
    if (!tender[index]) { return; }

    const irr::f32 TENDER_DIST = 1.2f * M_IN_NM;   // distance out
    const irr::f32 TENDER_TOP_Y = 0.85f * M_IN_NM;  // top of the bolt, above the horizon
    const irr::f32 TENDER_HEIGHT = 1.0f * M_IN_NM;  // taller (bigger picture)
    const irr::f32 TENDER_WIDTH = 0.55f * M_IN_NM;  // proportional width (~0.55 x height)

    irr::f32 bearing = irr::core::DEGTORAD * tenderBearingDeg[index];
    irr::core::vector3df pos(cameraPos.X + TENDER_DIST * sin(bearing),
        cameraPos.Y + TENDER_TOP_Y - TENDER_HEIGHT * 0.5f, // centre so the top sits high
        cameraPos.Z + TENDER_DIST * cos(bearing));

    tender[index]->setSize(irr::core::dimension2d<irr::f32>(TENDER_WIDTH, TENDER_HEIGHT));
    tender[index]->setPosition(pos);
    tender[index]->setColor(irr::video::SColor(255, 255, 255, 255));
    tender[index]->setVisible(true);
    tenderActive[index] = true;
    tenderAge[index] = 0.0f;
}

//NAUTITECH: advance every active tender - instant flash on, brief hold, then fade out.
void Sky::updateTenders(irr::f32 deltaTime)
{
    const irr::f32 HOLD = 0.16f; // fully lit
    const irr::f32 FADE = 0.32f; // fade out
    const irr::f32 TOTAL = HOLD + FADE;

    for (int i = 0; i < NUM_TENDERS; i++) {
        if (!tender[i] || !tenderActive[i]) { continue; }
        tenderAge[i] += deltaTime;

        if (tenderAge[i] >= TOTAL) {
            tenderActive[i] = false;
            tender[i]->setVisible(false);
            continue;
        }

        // Fade via ALPHA (alpha-blended material): dimming RGB would leave a dark shape,
        // so we fade the vertex alpha instead and keep the bolt white.
        irr::f32 bright = 1.0f;
        if (tenderAge[i] > HOLD) {
            bright = 1.0f - (tenderAge[i] - HOLD) / FADE;
        }
        if (bright < 0.0f) { bright = 0.0f; }
        if (bright > 1.0f) { bright = 1.0f; }
        irr::u32 a = (irr::u32)(bright * 255.0f);
        tender[i]->setColor(irr::video::SColor(a, 255, 255, 255));
    }
}

//NAUTITECH: fire the hero bolt. worldTop is where the TOP of the bolt sits (high in the sky);
//it then descends over ~0.55 s from that fixed top anchor.
void Sky::triggerHeroBolt(irr::core::vector3df worldTop, irr::f32 fullHeight, irr::f32 width)
{
    if (!heroBolt) { return; }
    heroBoltTop = worldTop;
    heroBoltFullHeight = fullHeight;
    heroBoltWidth = width;
    heroBoltAge = 0.0f;
    heroBoltActive = true;
    heroBolt->setVisible(true);
    heroBolt->setColor(irr::video::SColor(255, 255, 255, 255));
}

//NAUTITECH: advance the reveal. Grows downward from the fixed top (reads as a bolt reaching
//down), holds briefly, then fades. Slower than the base clapTimes flashes.
void Sky::updateHeroBolt(irr::f32 deltaTime)
{
    if (!heroBolt || !heroBoltActive) { return; }
    heroBoltAge += deltaTime;

    const irr::f32 REVEAL = 0.0f;  // no descend - whole bolt at once
    const irr::f32 HOLD = 0.18f; // fully lit
    const irr::f32 FADE = 0.30f; // fade out
    const irr::f32 TOTAL = REVEAL + HOLD + FADE;

    if (heroBoltAge >= TOTAL) {
        heroBoltActive = false;
        heroBolt->setVisible(false);
        return;
    }

    // Whole bolt appears at once, full size, top anchored. No descend.
    heroBolt->setSize(irr::core::dimension2d<irr::f32>(heroBoltWidth, heroBoltFullHeight));
    irr::core::vector3df centre = heroBoltTop;
    centre.Y = heroBoltTop.Y - heroBoltFullHeight * 0.5f;
    heroBolt->setPosition(centre);

    // Brightness: full during reveal + hold, then fade (additive: black = invisible)
    irr::f32 bright = 0.0f;
    if (heroBoltAge > REVEAL + HOLD) {
        bright = 1.0f - (heroBoltAge - REVEAL - HOLD) / FADE;
    }
    if (bright < 0.0f) { bright = 0.0f; }
    if (bright > 1.0f) { bright = 1.0f; }
    irr::u32 v = (irr::u32)(bright * 255.0f);
    heroBolt->setColor(irr::video::SColor(255, v, v, v));
}

void Sky::update(irr::u32 lightLevel, //unused - kept so the SimulationModel call site is unchanged
    irr::f32 warmth,
    bool isDawn,
    irr::f32 visibilityRangeNm,
    irr::video::SColor fogColour,
    bool stormMode,
    irr::f32 cloudCover)
{
    if (cloudCover >= 0.0f && cloudDome) {
        //Cloud cover given: the fair sky underneath, the overcast faded in over it
        if (stormMode && cloudCover < 0.9f) { cloudCover = 0.9f; }
        if (cloudCover > 1.0f) { cloudCover = 1.0f; }
        if (dayDome && skyIsStorm) {
            skyIsStorm = false;
            dayDome->getMaterial(0).setTexture(0, skyTexFair);
        }
        cloudDome->setVisible(cloudCover > 0.01f);
        cloudDome->getMaterial(0).DiffuseColor = irr::video::SColor((irr::u32)(cloudCover * 255.0f), 255, 255, 255);
        stormMode = cloudCover > 0.6f; //no warm horizon glow under a cloud deck
    }
    // KYARA: overcast dome during "mauvais temps". Swap the base-dome texture only when the
    // state changes (setMaterialTexture every frame is wasteful).
    else if (dayDome && stormMode != skyIsStorm) {
        skyIsStorm = stormMode;
        dayDome->getMaterial(0).setTexture(0, skyIsStorm ? skyTexStorm : skyTexFair);
    }
    // ---- HAZE ---------------------------------------------------------------------------
    irr::f32 haze = 1.0f - (visibilityRangeNm / HAZE_CLEAR_VIS_NM);
    if (haze < 0.0f) { haze = 0.0f; }
    if (haze > 1.0f) { haze = 1.0f; }

    haze *= HAZE_MAX;

    if (hazeDome) {
        hazeDome->setVisible(haze > 0.01f);
        hazeDome->getMaterial(0).EmissiveColor = irr::video::SColor(255,
            (irr::u32)(fogColour.getRed() * haze),
            (irr::u32)(fogColour.getGreen() * haze),
            (irr::u32)(fogColour.getBlue() * haze));
    }

    // ---- HORIZON GLOW -------------------------------------------------------------------
    //Sunset is the fiery one - warm orange, strong, because the afternoon atmosphere is full
    //of dust and haze. Dawn is a cool rose/violet under a still-blue sky, and gentler.
    if (glowDome) {
        irr::f32 w = warmth;
        if (stormMode) { w = 0.0f; }   // KYARA: sun hidden behind storm cloud - no warm horizon glow
        if (w < 0.0f) { w = 0.0f; }
        if (w > 1.0f) { w = 1.0f; }

        irr::f32 glowR, glowG, glowB;
        if (isDawn) {
            glowR = 185.0f; glowG = 130.0f; glowB = 200.0f;   //rose/violet, bright enough to actually SEE
            w *= 0.85f;                                       //was 0.60 - dawn was being crushed twice
        }
        else {
            glowR = 255.0f; glowG = 115.0f; glowB = 45.0f;    //warm golden orange
            w *= 0.90f;
        }

        glowDome->setVisible(w > 0.01f);
        glowDome->getMaterial(0).EmissiveColor = irr::video::SColor(255,
            (irr::u32)(glowR * w),
            (irr::u32)(glowG * w),
            (irr::u32)(glowB * w));

    }

}