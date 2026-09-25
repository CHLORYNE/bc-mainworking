/* FIRE FEATURE */
#include "Fire.hpp"
#include <cmath>
#include <cstdlib>

using namespace irr;

Fire::Fire()
    : firePS(0), smokePS(0), fireEmitter(0), smokeEmitter(0),
    smokeWindAffector(0), fireLight(0), lightFlickerPhase(0.0f),
    smokeColMin(70, 30, 30, 30), smokeColMax(140, 70, 70, 70),
    steamColMin(180, 210, 210, 215), steamColMax(240, 245, 245, 250),
    intensity(0.0f), extinguishRate(0.025f), reigniteRate(0.0f),
    beingHitThisFrame(false),
    fireBaseMinPPS(420), fireBaseMaxPPS(620),      // denser blaze
    smokeBaseMinPPS(120), smokeBaseMaxPPS(180),
    radius(3.0f), fireBox(0), smokeBox(0), escalation(0.0f), vis(3.0f),
    seatHalfX(1.0f), seatHalfZ(1.0f), hullHalfX(3.0f), hullHalfZ(3.0f), baseOffset(0, 0, 0)
{
}

Fire::~Fire() { remove(); }

void Fire::ignite(scene::ISceneManager* smgr, video::IVideoDriver* driver,
    scene::ISceneNode* parent, core::vector3df localOffset, f32 radiusIn, f32 hullHalfLen, f32 hullHalfBreadth)
{
    remove();
    baseOffset = localOffset;
    intensity = 1.0f;
    radius = radiusIn;

    // Visual size, decoupled from the hit radius. Clamp to 2.5 .. 6.5 m.
    vis = radiusIn;
    if (vis < 2.5f) vis = 2.5f;
    if (vis > 6.5f) vis = 6.5f;

    // Kyara FIRE: keep the blaze ON the casualty instead of spilling onto the sea.
    // Particles are drawn up to vis*1.5 across, so a particle emitted at the very edge of the
    // hull box overhangs the hull by about vis*0.75. Two corrections:
    //  1) cap the particle size against the hull's narrow half-extent, so a small boat doesn't
    //     get flames wider than itself;
    //  2) INSET the emission box by that overhang, so the drawn flames - not just the emission
    //     points - land inside the hull footprint.
    f32 hullMinHalf = (hullHalfBreadth > 0.0f && hullHalfLen > 0.0f)
        ? ((hullHalfBreadth < hullHalfLen) ? hullHalfBreadth : hullHalfLen) : 0.0f;
    if (hullMinHalf > 0.0f) {
        f32 visCap = 1.5f * hullMinHalf;      // flame no wider than ~1.5x the narrow half-beam
        if (vis > visCap) vis = visCap;
    }
    if (vis < 1.2f) vis = 1.2f;               // floor, so tiny craft still show something

    const f32 particleHalf = vis * 0.75f;     // half of the max particle size (vis * 1.5)

    // Full-spread (escalation = 1) half-extents, inset by the overhang above.
    f32 fullHalfX = (hullHalfBreadth > 0.0f) ? (hullHalfBreadth - particleHalf) : (vis * 0.9f);
    f32 fullHalfZ = (hullHalfLen > 0.0f) ? (hullHalfLen - particleHalf) : (vis * 0.9f);
    if (fullHalfX < 0.20f) fullHalfX = 0.20f;
    if (fullHalfZ < 0.20f) fullHalfZ = 0.20f;

    // Fresh fire starts as a small central seat and spreads outward via setEscalation().
    f32 baseHalfX = fullHalfX * 0.25f;   // port-stbd
    f32 baseHalfZ = fullHalfZ * 0.25f;   // fore-aft
    f32 bandH = vis * 0.15f;             // thin base band

    // ---- FIRE (additive) ----
    firePS = smgr->addParticleSystemSceneNode(false, parent, -1, localOffset);
    fireEmitter = firePS->createBoxEmitter(
        core::aabbox3df(-baseHalfX, 0.0f, -baseHalfZ, baseHalfX, bandH, baseHalfZ),
        core::vector3df(0.0f, 0.012f, 0.0f),           // was 0.006f — more lift, licks rise into a plume
        fireBaseMinPPS, fireBaseMaxPPS,
        video::SColor(0, 255, 170, 60), video::SColor(0, 255, 240, 150),
        650, 1400, 28,
        core::dimension2df(vis * 0.60f, vis * 0.60f),
        core::dimension2df(vis * 1.50f, vis * 1.50f));

    firePS->setEmitter(fireEmitter);
    fireEmitter->drop();
    fireBox = (scene::IParticleBoxEmitter*)fireEmitter;   // same object, box-typed view
    seatHalfX = baseHalfX; seatHalfZ = baseHalfZ;   // small central seat at ignition
    hullHalfX = fullHalfX; hullHalfZ = fullHalfZ;   // inset whole-hull spread at escalation 1
    escalation = 0.0f;
    scene::IParticleFadeOutAffector* fade =
        firePS->createFadeOutParticleAffector(video::SColor(0, 0, 0, 0), 300);
    firePS->addAffector(fade);
    fade->drop();

    firePS->setMaterialFlag(video::EMF_LIGHTING, false);
    firePS->setMaterialFlag(video::EMF_ZWRITE_ENABLE, false);
    firePS->setMaterialFlag(video::EMF_FOG_ENABLE, true);
    firePS->setMaterialTexture(0, driver->getTexture("media/fire.png"));
    firePS->setMaterialType(video::EMT_TRANSPARENT_ADD_COLOR);

    // ---- SMOKE (alpha-blended) ----
    smokePS = smgr->addParticleSystemSceneNode(false, parent, -1,
        localOffset + core::vector3df(0, vis * 0.9f, 0));
    smokeEmitter = smokePS->createBoxEmitter(
        core::aabbox3df(-vis * 0.28f, 0.0f, -vis * 0.28f,
            vis * 0.28f, vis * 0.18f, vis * 0.28f),
        core::vector3df(0.0f, 0.007f, 0.0f),
        smokeBaseMinPPS, smokeBaseMaxPPS,
        smokeColMin, smokeColMax,
        1900, 3600,
        28,
        core::dimension2df(vis * 1.3f, vis * 1.3f),
        core::dimension2df(vis * 3.2f, vis * 3.2f));
    smokePS->setEmitter(smokeEmitter);
    smokeEmitter->drop();
    smokeBox = (scene::IParticleBoxEmitter*)smokeEmitter;   // FIX: was re-assigning fireBox
    scene::IParticleFadeOutAffector* smokeFade =
        smokePS->createFadeOutParticleAffector(video::SColor(0, 0, 0, 0), 1000);
    smokePS->addAffector(smokeFade);
    smokeFade->drop();

    smokeWindAffector =
        smokePS->createGravityAffector(core::vector3df(0.0f, 0.0f, 0.0f), 3200);
    smokePS->addAffector(smokeWindAffector);
    smokeWindAffector->drop();

    smokePS->setMaterialFlag(video::EMF_LIGHTING, false);
    smokePS->setMaterialFlag(video::EMF_ZWRITE_ENABLE, false);
    smokePS->setMaterialFlag(video::EMF_FOG_ENABLE, true);
    smokePS->setMaterialTexture(0, driver->getTexture("media/smoke.png"));
    smokePS->setMaterialType(video::EMT_TRANSPARENT_ALPHA_CHANNEL);

    // ---- FIRE GLOW ----
    fireLight = smgr->addLightSceneNode(parent,
        localOffset + core::vector3df(0, vis * 0.5f, 0),
        video::SColorf(1.0f, 0.55f, 0.18f), radius * 9.0f);
    lightFlickerPhase = 0.0f;
}

void Fire::setEscalation(f32 e)
{
    if (e < 0.0f) e = 0.0f; if (e > 1.0f) e = 1.0f;
    escalation = e;
    // Grow the burning footprint from the central seat out to the whole hull.
    f32 hx = seatHalfX + (hullHalfX - seatHalfX) * e;
    f32 hz = seatHalfZ + (hullHalfZ - seatHalfZ) * e;
    //FIRE SIZE
    f32 hy = vis * 0.15f;   // was vis*(0.35f+0.45f*e) — thin band; height comes from lift, not box size
    if (fireBox)  fireBox->setBox(core::aabbox3df(-hx, 0.0f, -hz, hx, hy, hz));
    if (smokeBox) smokeBox->setBox(core::aabbox3df(-hx * 1.05f, 0.0f, -hz * 1.05f,
        hx * 1.05f, vis * 0.18f, hz * 1.05f));
    if (smokePS) smokePS->setPosition(baseOffset + core::vector3df(0.0f, hy, 0.0f)); // smoke rides the flame top
}
f32 Fire::getEscalation() const { return escalation; }
void Fire::applyWater(f32 /*deltaTime*/) { beingHitThisFrame = true; }

void Fire::update(f32 deltaTime, f32 windDirectionDeg, f32 windSpeedKts)
{
    if (intensity <= 0.0f) {
        if (fireLight) { fireLight->setVisible(false); }
        return;
    }

    if (beingHitThisFrame) {
        intensity -= extinguishRate * deltaTime;
        if (intensity < 0.0f) intensity = 0.0f;
    }
    else if (reigniteRate > 0.0f) {
        intensity += reigniteRate * deltaTime;
        if (intensity > 1.0f) intensity = 1.0f;
    }

    if (fireEmitter) {
        f32 escBoost = 1.0f + escalation * 1.6f;   // denser as the whole deck lights up
        fireEmitter->setMinParticlesPerSecond((u32)(fireBaseMinPPS * intensity * escBoost));
        fireEmitter->setMaxParticlesPerSecond((u32)(fireBaseMaxPPS * intensity * escBoost) + 1);
    }

    if (smokeEmitter) {
        f32 smokeScale = (intensity > 0.15f) ? intensity : intensity * 0.5f;
        if (beingHitThisFrame) {
            smokeEmitter->setMinStartColor(steamColMin);
            smokeEmitter->setMaxStartColor(steamColMax);
            smokeEmitter->setMinParticlesPerSecond((u32)(smokeBaseMinPPS * 2.2f));
            smokeEmitter->setMaxParticlesPerSecond((u32)(smokeBaseMaxPPS * 2.2f) + 1);
        }
        else {
            smokeEmitter->setMinStartColor(smokeColMin);
            smokeEmitter->setMaxStartColor(smokeColMax);
            smokeEmitter->setMinParticlesPerSecond((u32)(smokeBaseMinPPS * smokeScale));
            smokeEmitter->setMaxParticlesPerSecond((u32)(smokeBaseMaxPPS * smokeScale) + 1);
        }
    }

    if (smokeWindAffector) {
        f32 windFlowDir = windDirectionDeg + 180.0f;
        f32 mag = windSpeedKts * 0.00045f;
        if (mag > 0.020f) mag = 0.020f;
        f32 wx = mag * std::sin(windFlowDir * core::DEGTORAD);
        f32 wz = mag * std::cos(windFlowDir * core::DEGTORAD);
        smokeWindAffector->setGravity(core::vector3df(wx, 0.0f, wz));
    }

    if (fireLight) {
        lightFlickerPhase += deltaTime * 11.0f;
        f32 flicker = 0.80f + 0.15f * std::sin(lightFlickerPhase)
            + 0.05f * ((f32)std::rand() / RAND_MAX);
        fireLight->setVisible(true);
        //FIRE LIGHT RADIUS
        fireLight->setRadius(radius * 9.0f * (1.0f + escalation) * intensity * flicker);
        fireLight->getLightData().DiffuseColor =
            video::SColorf(1.0f * intensity, 0.55f * intensity, 0.18f * intensity);
    }

    beingHitThisFrame = false;

    if (intensity <= 0.0f) {
        if (fireEmitter) { fireEmitter->setMinParticlesPerSecond(0);  fireEmitter->setMaxParticlesPerSecond(0); }
        if (smokeEmitter) { smokeEmitter->setMinParticlesPerSecond(0); smokeEmitter->setMaxParticlesPerSecond(0); }
        if (fireLight) { fireLight->setVisible(false); }
    }
}

bool Fire::isActive() const { return intensity > 0.0f; }
bool Fire::isExtinguished() const { return intensity <= 0.0f; }

core::vector3df Fire::getWorldPosition() const
{
    if (firePS) { firePS->updateAbsolutePosition(); return firePS->getAbsolutePosition(); }
    return core::vector3df(0, 0, 0);
}

f32 Fire::getRadius() const { return radius; }

void Fire::remove()
{
    intensity = 0.0f;   // FIX: mark inactive so post-sink audio/logic doesn't re-fire
    if (fireLight) { fireLight->remove(); fireLight = 0; }
    if (fireLight) { fireLight->remove(); fireLight = 0; }
    if (firePS) { firePS->remove();  firePS = 0; }
    if (smokePS) { smokePS->remove(); smokePS = 0; }
    fireEmitter = 0; smokeEmitter = 0; smokeWindAffector = 0;
}