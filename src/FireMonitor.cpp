/* FIRE FEATURE */
#include "FireMonitor.hpp"

using namespace irr;

FireMonitor::FireMonitor()
    : waterPS(0), waterEmitter(0), mistPS(0), mistEmitter(0), nozzleNode(0),
    monitorBase(0), monitorSwivel(0), barrelPivot(0),
    barrelLen(2.4f), pedestalH(1.2f),
    firing(false), mounted(false), aimPoint(0, 0, 0),
    baseMinPPS(1100), baseMaxPPS(1500),   // denser, more solid-looking column
    //tweak helicopter hose
    jetSpeedScale(0.0038f)                 // faster = taut stream that carries to the target
{
}

FireMonitor::~FireMonitor()
{
    if (waterPS)       waterPS->remove();
    if (mistPS)        mistPS->remove();
    if (nozzleNode)    nozzleNode->remove();
    if (monitorBase)   monitorBase->remove();
    if (monitorSwivel) monitorSwivel->remove();
    if (barrelPivot)   barrelPivot->remove(); // removes barrel + cone children too
}

void FireMonitor::mount(scene::ISceneManager* smgr, video::IVideoDriver* driver,
    scene::ISceneNode* fireBoatNode, core::vector3df nozzleOffset)
{
    // Invisible anchor that rides the boat - its world position is the pedestal base.
    nozzleNode = smgr->addEmptySceneNode(fireBoatNode);
    nozzleNode->setPosition(nozzleOffset);

    // ---- Water jet (world space; repositioned to the nozzle tip each frame) ----
    // A real monitor jet is a tight, fast column that arcs under gravity and only frays into
    // spray downrange.
    // kyara: Irrlicht emits a whole frame's worth of particles at a SINGLE point, so a fast jet
    // comes out as separate blobs spaced (speed * frame time) apart - about 2 m per bead at 60 fps
    // with the old settings, against a 0.28 m particle. Three changes make it read as one stream:
    //   1) a longer emission box, so each frame's batch is spread out instead of stacked at a point;
    //   2) bigger droplets, so consecutive batches overlap rather than leaving gaps;
    //   3) a slightly lower top speed, which shortens the gap between batches.
    waterPS = smgr->addParticleSystemSceneNode(false, 0, -1, nozzleOffset);
    waterEmitter = waterPS->createBoxEmitter(
        core::aabbox3df(-0.60f, -0.60f, -0.60f, 0.60f, 0.60f, 0.60f),  // spread the per-frame batch
        core::vector3df(0.0f, 0.0f, 0.02f),
        baseMinPPS, baseMaxPPS,
        video::SColor(210, 225, 240, 255),          // denser, whiter core
        video::SColor(255, 255, 255, 255),
        900, 1700,                                  // enough to reach the target, without overshooting far
        7,                                          // NARROW cone -> a stream, not a fan
        core::dimension2df(0.90f, 0.90f),           // large enough that consecutive batches merge
        core::dimension2df(2.40f, 2.40f));          // grows as it atomises downrange
    waterPS->setEmitter(waterEmitter);
    waterEmitter->drop();

    scene::IParticleFadeOutAffector* fade =
        waterPS->createFadeOutParticleAffector(video::SColor(0, 0, 0, 0), 350);
    waterPS->addAffector(fade);
    fade->drop();

    // Stronger downward pull so the jet visibly arcs (ballistic), instead of firing dead straight.
    scene::IParticleGravityAffector* grav =
        waterPS->createGravityAffector(core::vector3df(0.0f, -0.020f, 0.0f), 1400);
    waterPS->addAffector(grav);
    grav->drop();

    waterPS->setMaterialFlag(video::EMF_LIGHTING, false);
    waterPS->setMaterialFlag(video::EMF_ZWRITE_ENABLE, false);
    waterPS->setMaterialFlag(video::EMF_FOG_ENABLE, true);
    waterPS->setMaterialTexture(0, driver->getTexture("media/waterspray.png"));
    waterPS->setMaterialType(video::EMT_TRANSPARENT_ALPHA_CHANNEL);
    waterEmitter->setMinParticlesPerSecond(0);
    waterEmitter->setMaxParticlesPerSecond(0);

    // ---- Impact mist (world space; parked at the aim point each frame) ----
    // Soft, slow, expanding cloud where the jet lands - sells the water breaking up on the target.
    mistPS = smgr->addParticleSystemSceneNode(false, 0, -1, nozzleOffset);
    mistEmitter = mistPS->createBoxEmitter(
        core::aabbox3df(-0.6f, -0.6f, -0.6f, 0.6f, 0.6f, 0.6f),
        core::vector3df(0.0f, 0.006f, 0.0f),        // drifts gently upward like spray/steam
        260, 380,
        video::SColor(70, 230, 240, 250),           // faint, cool-white haze
        video::SColor(150, 255, 255, 255),
        500, 1000,
        40,                                         // wide, soft puff
        core::dimension2df(1.4f, 1.4f),
        core::dimension2df(3.6f, 3.6f));
    mistPS->setEmitter(mistEmitter);
    mistEmitter->drop();

    scene::IParticleFadeOutAffector* mistFade =
        mistPS->createFadeOutParticleAffector(video::SColor(0, 0, 0, 0), 500);
    mistPS->addAffector(mistFade);
    mistFade->drop();

    mistPS->setMaterialFlag(video::EMF_LIGHTING, false);
    mistPS->setMaterialFlag(video::EMF_ZWRITE_ENABLE, false);
    mistPS->setMaterialFlag(video::EMF_FOG_ENABLE, true);
    mistPS->setMaterialTexture(0, driver->getTexture("media/waterspray.png"));
    mistPS->setMaterialType(video::EMT_TRANSPARENT_ALPHA_CHANNEL);
    mistEmitter->setMinParticlesPerSecond(0);
    mistEmitter->setMaxParticlesPerSecond(0);

    // ---- Visible monitor (round primitives; world-space, scale-immune) ----
    const scene::IGeometryCreator* gc = smgr->getGeometryCreator();

    // Pedestal: a vertical cylinder (created along +Y = already vertical).
    scene::IMesh* pedMesh = gc->createCylinderMesh(0.22f, pedestalH, 16, video::SColor(255, 78, 80, 86));
    monitorBase = smgr->addMeshSceneNode(pedMesh);
    pedMesh->drop();
    monitorBase->setMaterialFlag(video::EMF_LIGHTING, true);
    monitorBase->getMaterial(0).EmissiveColor = video::SColor(255, 26, 27, 30);
    monitorBase->setVisible(false);

    // Swivel ball on top of the pedestal.
    monitorSwivel = smgr->addSphereSceneNode(0.34f, 16);
    monitorSwivel->setMaterialFlag(video::EMF_LIGHTING, true);
    monitorSwivel->getMaterial(0).EmissiveColor = video::SColor(255, 20, 21, 24);
    monitorSwivel->setVisible(false);

    // Barrel + nozzle live under a pivot that swivels to aim. Cylinders/cones are made
    // along +Y, so each child is turned (90,0,0) to lie along the pivot's +Z (= aim dir).
    barrelPivot = smgr->addEmptySceneNode(0);

    scene::IMesh* barMesh = gc->createCylinderMesh(0.14f, barrelLen, 16, video::SColor(255, 205, 45, 45));
    scene::ISceneNode* barrel = smgr->addMeshSceneNode(barMesh, barrelPivot);
    barMesh->drop();
    barrel->setRotation(core::vector3df(90.0f, 0.0f, 0.0f)); // +Y -> +Z  (flip to -90 if it points backwards)
    barrel->setMaterialFlag(video::EMF_LIGHTING, true);
    barrel->getMaterial(0).EmissiveColor = video::SColor(255, 70, 16, 16);

    scene::IMesh* coneMesh = gc->createConeMesh(0.20f, 0.55f, 16,
        video::SColor(255, 230, 205, 130), video::SColor(255, 180, 150, 90), 0.0f);
    scene::ISceneNode* cone = smgr->addMeshSceneNode(coneMesh, barrelPivot);
    coneMesh->drop();
    cone->setRotation(core::vector3df(90.0f, 0.0f, 0.0f));
    cone->setPosition(core::vector3df(0.0f, 0.0f, barrelLen)); // sit at the barrel end
    cone->setMaterialFlag(video::EMF_LIGHTING, true);
    cone->getMaterial(0).EmissiveColor = video::SColor(255, 90, 78, 40);

    barrelPivot->setVisible(false); // hides its children too

    mounted = true;
}

void FireMonitor::setFiring(bool f) { firing = f; }
bool FireMonitor::isFiring() const { return firing && mounted; }
void FireMonitor::setAimPoint(core::vector3df a) { aimPoint = a; }
core::vector3df FireMonitor::getAimPoint() const { return aimPoint; }
bool FireMonitor::isMounted() const { return mounted; }

core::vector3df FireMonitor::getNozzleWorldPos() const
{
    if (nozzleNode) { nozzleNode->updateAbsolutePosition(); return nozzleNode->getAbsolutePosition(); }
    return core::vector3df(0, 0, 0);
}

void FireMonitor::update(f32 /*deltaTime*/)
{
    if (!mounted || !waterEmitter) return;

    if (firing) {
        core::vector3df base = getNozzleWorldPos();                 // pedestal foot, rides the boat
        core::vector3df swivel = base + core::vector3df(0, pedestalH, 0);
        core::vector3df dir = aimPoint - swivel;
        f32 distance = dir.getLength();
        if (distance > 0.001f) dir /= distance;

        if (monitorBase) { monitorBase->setPosition(base);     monitorBase->setVisible(true); }
        if (monitorSwivel) { monitorSwivel->setPosition(swivel); monitorSwivel->setVisible(true); }
        if (barrelPivot) {
            barrelPivot->setPosition(swivel);
            barrelPivot->setRotation(dir.getHorizontalAngle());      // aim the barrel at the fire
            barrelPivot->setVisible(true);
        }

        // Water leaves the nozzle TIP (barrel + cone), not the pedestal.
        core::vector3df tip = swivel + dir * (barrelLen + 0.55f);
        waterPS->setPosition(tip);

        f32 speedMag = distance * jetSpeedScale;
        if (speedMag < 0.020f) speedMag = 0.020f;
        if (speedMag > 0.075f) speedMag = 0.075f;   // kyara: capped so per-frame batches stay close enough to merge
        waterEmitter->setDirection(dir * speedMag);
        waterEmitter->setMinParticlesPerSecond(baseMinPPS);
        waterEmitter->setMaxParticlesPerSecond(baseMaxPPS);

        // Impact mist sits on the aim point (where the jet lands) and breaks into soft spray.
        if (mistPS && mistEmitter) {
            mistPS->setPosition(aimPoint);
            mistEmitter->setMinParticlesPerSecond(260);
            mistEmitter->setMaxParticlesPerSecond(380);
        }
    }
    else {
        if (monitorBase)   monitorBase->setVisible(false);
        if (monitorSwivel) monitorSwivel->setVisible(false);
        if (barrelPivot)   barrelPivot->setVisible(false);
        waterEmitter->setMinParticlesPerSecond(0);
        waterEmitter->setMaxParticlesPerSecond(0);
        if (mistEmitter) {
            mistEmitter->setMinParticlesPerSecond(0);
            mistEmitter->setMaxParticlesPerSecond(0);
        }
    }
}