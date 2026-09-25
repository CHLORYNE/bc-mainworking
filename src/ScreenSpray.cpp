/*   NAUTITECH - Simulateur de Navigation (Bridge Command fork)
     KYARA SLAM: water on the wheelhouse glass. See ScreenSpray.hpp.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#include "ScreenSpray.hpp"

#include <cmath>
#include <cstdio>

namespace
{
    // TUNABLES
    const irr::f32 MIN_SEVERITY = 1.0f;     // below this, nothing reaches the glass
    const irr::u32 MAX_DROPS = 240;
    const irr::f32 DROPS_PER_SEVERITY = 26.0f;

    const irr::f32 LIFE_MIN = 9.0f;         // s before a drop has dried / blown off
    const irr::f32 LIFE_MAX = 22.0f;

    //NB: not SIZE_MIN / SIZE_MAX - SIZE_MAX is a standard macro from <cstdint>, which MSVC pulls
    //in through the Windows headers, so those names expand to constants and break the build.
    const irr::f32 DROP_SIZE_MIN = 0.020f;  // as a fraction of the pane height
    const irr::f32 DROP_SIZE_MAX = 0.090f;

    const irr::f32 RUN_SPEED = 0.030f;      // pane fractions per second for the biggest drops
    const irr::f32 RUN_ACCEL = 1.35f;       // runlets speed up as they gather water

    const irr::f32 GLASS_ALPHA = 0.70f;     // overall strength of the effect

    const irr::u32 TEX_W = 48, TEX_H = 96;  // bead in the bottom half, tail in the top half
}

// -------------------------------------------------------------------------------------------
// The pane itself: a scene node that draws the drops as real geometry, sitting just inside the
// windscreen. Everything in front of it (window frames, deckhead, console) hides it through the
// ordinary depth test, which is what keeps the water inside the window openings.
// -------------------------------------------------------------------------------------------
class SprayPaneSceneNode : public irr::scene::ISceneNode
{
public:
    SprayPaneSceneNode(irr::scene::ISceneNode* parent, irr::scene::ISceneManager* mgr,
        irr::core::vector3df position, irr::f32 w, irr::f32 h, irr::f32 tiltDeg)
        : irr::scene::ISceneNode(parent, mgr, -1, position,
            irr::core::vector3df(tiltDeg, 0, 0), irr::core::vector3df(1, 1, 1)),
        paneWidth(w), paneHeight(h)
    {
        material.Wireframe = false;
        material.Lighting = false;
        material.BackfaceCulling = false;
        material.FogEnable = false;
        material.ZWriteEnable = irr::video::EZW_OFF; // drops must not occlude each other
        material.MaterialType = irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL;

        const irr::f32 halfW = 0.5f * paneWidth, halfH = 0.5f * paneHeight;
        box = irr::core::aabbox3df(-halfW, -halfH, -0.05f, halfW, halfH, 0.05f);
    }

    virtual void OnRegisterSceneNode()
    {
        if (IsVisible) {
            SceneManager->registerNodeForRendering(this, irr::scene::ESNRP_TRANSPARENT);
        }
        irr::scene::ISceneNode::OnRegisterSceneNode();
    }

    virtual void render()
    {
        if (vertices.empty()) { return; }
        irr::video::IVideoDriver* driver = SceneManager->getVideoDriver();
        driver->setMaterial(material);
        driver->setTransform(irr::video::ETS_WORLD, AbsoluteTransformation);
        driver->drawVertexPrimitiveList(&vertices[0], vertices.size(),
            &indices[0], indices.size() / 3,
            irr::video::EVT_STANDARD, irr::scene::EPT_TRIANGLES,
            irr::video::EIT_16BIT);
    }

    virtual const irr::core::aabbox3df& getBoundingBox() const { return box; }
    virtual irr::u32 getMaterialCount() const { return 1; }
    virtual irr::video::SMaterial& getMaterial(irr::u32 i) { return material; }

    void setTexture(irr::video::ITexture* t) { material.setTexture(0, t); }

    void beginGeometry() { vertices.clear(); indices.clear(); }

    // x, y are 0..1 across the pane (y from the top); size is a fraction of the pane height.
    void addDrop(irr::f32 x, irr::f32 y, irr::f32 size, irr::f32 alpha, bool withTail)
    {
        if (vertices.size() + 4 > 65000) { return; } // 16-bit indices
        const irr::f32 s = size * paneHeight;
        const irr::f32 halfW = 0.5f * s * ((irr::f32)TEX_W / (irr::f32)(TEX_H / 2));
        // With a tail the quad is twice as tall, and the bead stays at the bottom of it.
        const irr::f32 height = withTail ? s * 2.0f : s;

        const irr::f32 cx = (x - 0.5f) * paneWidth;
        const irr::f32 cyBead = (0.5f - y) * paneHeight;
        const irr::f32 top = cyBead + (withTail ? height - 0.5f * s : 0.5f * s);
        const irr::f32 bottom = cyBead - 0.5f * s;

        // Bead-only drops use the bottom half of the texture; running ones use all of it.
        const irr::f32 v0 = withTail ? 0.0f : 0.5f;
        const irr::u32 a = (irr::u32)(alpha * 255.0f);
        const irr::video::SColor col(a, 255, 255, 255);
        const irr::core::vector3df n(0, 0, -1);

        const irr::u16 base = (irr::u16)vertices.size();
        vertices.push_back(irr::video::S3DVertex(cx - halfW, top, 0, n.X, n.Y, n.Z, col, 0.0f, v0));
        vertices.push_back(irr::video::S3DVertex(cx + halfW, top, 0, n.X, n.Y, n.Z, col, 1.0f, v0));
        vertices.push_back(irr::video::S3DVertex(cx + halfW, bottom, 0, n.X, n.Y, n.Z, col, 1.0f, 1.0f));
        vertices.push_back(irr::video::S3DVertex(cx - halfW, bottom, 0, n.X, n.Y, n.Z, col, 0.0f, 1.0f));
        indices.push_back(base); indices.push_back(base + 1); indices.push_back(base + 2);
        indices.push_back(base); indices.push_back(base + 2); indices.push_back(base + 3);
    }

private:
    irr::core::aabbox3df box;
    irr::video::SMaterial material;
    std::vector<irr::video::S3DVertex> vertices;
    std::vector<irr::u16> indices;
    irr::f32 paneWidth, paneHeight;
};

// -------------------------------------------------------------------------------------------

ScreenSpray::ScreenSpray()
    : pane(0), mode(1), minSeverity(MIN_SEVERITY), loaded(false), seed(12345)
{
}

irr::f32 ScreenSpray::randf()
{
    //Own generator: this runs every frame and must not disturb std::rand(), which the weather,
    //the nav lights and the slam detune all pull from.
    seed = seed * 1103515245u + 12345u;
    return (irr::f32)((seed >> 8) & 0xFFFF) / 65535.0f;
}

void ScreenSpray::load(irr::scene::ISceneManager* smgr, irr::IrrlichtDevice* dev,
    irr::scene::ISceneNode* parent,
    irr::core::vector3df position, irr::f32 width, irr::f32 height, irr::f32 tiltDeg)
{
    if (!smgr || !dev || !parent) { return; }
    if (width <= 0.0f || height <= 0.0f) { return; }

    irr::video::IVideoDriver* driver = dev->getVideoDriver();
    if (!driver) { return; }

    //A drop on glass is mostly invisible: what you see is the bright rim where it bends the
    //light, plus a highlight. The top half of the texture is the trail a running drop leaves.
    const bool mip = driver->getTextureCreationFlag(irr::video::ETCF_CREATE_MIP_MAPS);
    driver->setTextureCreationFlag(irr::video::ETCF_CREATE_MIP_MAPS, false);

    irr::video::ITexture* texture = 0;
    irr::video::IImage* image = driver->createImage(irr::video::ECF_A8R8G8B8,
        irr::core::dimension2d<irr::u32>(TEX_W, TEX_H));
    if (image) {
        const irr::f32 cx = (irr::f32)TEX_W * 0.5f;
        const irr::f32 cy = (irr::f32)TEX_H * 0.75f;   // bead centred in the bottom half
        const irr::f32 rad = (irr::f32)TEX_W * 0.46f;
        for (irr::u32 y = 0; y < TEX_H; y++) {
            for (irr::u32 x = 0; x < TEX_W; x++) {
                const irr::f32 dx = ((irr::f32)x + 0.5f - cx) / rad;
                const irr::f32 dy = ((irr::f32)y + 0.5f - cy) / rad;
                irr::f32 a = 0.0f;

                const irr::f32 r = sqrtf(dx * dx + dy * dy);
                if (r < 1.0f) {
                    const irr::f32 rim = expf(-((r - 0.80f) * (r - 0.80f)) / 0.020f);
                    const irr::f32 body = (1.0f - r * r) * 0.20f;
                    a = body + rim * 0.80f;
                    const irr::f32 hx = dx + 0.35f, hy = dy + 0.35f;
                    a += 0.55f * expf(-(hx * hx + hy * hy) / 0.045f);
                }
                if (dy < 0.0f) {
                    //The trail above the bead, narrowing and fading as it goes up
                    const irr::f32 t = -dy;
                    const irr::f32 wide = 0.40f * (1.0f - 0.30f * t);
                    if (wide > 0.02f && fabsf(dx) < wide) {
                        const irr::f32 across = 1.0f - fabsf(dx) / wide;
                        a += 0.38f * across * across * expf(-t * 0.40f);
                    }
                }
                if (a > 1.0f) { a = 1.0f; }
                if (a < 0.0f) { a = 0.0f; }
                image->setPixel(x, y, irr::video::SColor((irr::u32)(a * 255.0f), 240, 248, 255));
            }
        }
        texture = driver->addTexture("nautitech_glass_drop", image);
        image->drop();
    }
    driver->setTextureCreationFlag(irr::video::ETCF_CREATE_MIP_MAPS, mip);

    pane = new SprayPaneSceneNode(parent, smgr, position, width, height, tiltDeg);
    if (texture) { pane->setTexture(texture); }
    pane->drop(); //the scene manager owns it now

    drops.clear();
    loaded = true;
}

void ScreenSpray::setMode(int m)
{
    mode = m;
    minSeverity = (mode == 2) ? 0.2f : MIN_SEVERITY;
    if (mode == 0) { clear(); }
    if (pane) { pane->setVisible(mode != 0); }
}

int ScreenSpray::getMode() const
{
    return mode;
}

void ScreenSpray::trigger(irr::f32 severity)
{
    if (!loaded || mode == 0) { return; }
    if (severity < minSeverity) { return; }
    if (severity > 4.0f) { severity = 4.0f; }

    const irr::f32 strength = severity - minSeverity + 0.4f;
    int count = (int)(DROPS_PER_SEVERITY * strength * strength);
    if (count < 4) { count = 4; }

    for (int i = 0; i < count; i++) {
        if (drops.size() >= MAX_DROPS) { break; }

        Drop d;
        //Water arrives over the bow, so it lands mostly on the upper half of the glass.
        d.x = 0.5f + (randf() - 0.5f) * (0.80f + 0.20f * randf());
        d.y = randf() * 0.55f;
        const irr::f32 sizeMix = randf() * randf();  // many fine drops, a few fat ones
        d.size = DROP_SIZE_MIN + (DROP_SIZE_MAX - DROP_SIZE_MIN) * sizeMix * (0.55f + 0.45f * strength);
        d.alpha = 0.55f + 0.45f * randf();
        //Only the big ones run; fine spray sits there and dries.
        const irr::f32 sizeFrac = (d.size - DROP_SIZE_MIN) / (DROP_SIZE_MAX - DROP_SIZE_MIN);
        d.speed = RUN_SPEED * sizeFrac * sizeFrac * (0.5f + randf());
        d.drift = (randf() - 0.5f) * 0.004f;
        d.age = 0.0f;
        d.life = LIFE_MIN + (LIFE_MAX - LIFE_MIN) * randf();
        d.running = (d.speed > RUN_SPEED * 0.25f);
        drops.push_back(d);
    }
}

void ScreenSpray::update(irr::f32 deltaTime)
{
    if (!loaded || !pane) { return; }

    if (deltaTime > 0.0f) {
        for (size_t i = 0; i < drops.size(); ) {
            Drop& d = drops[i];
            d.age += deltaTime;

            //Running water gathers more as it goes, so it speeds up and wanders.
            if (d.speed > 0.0f) {
                d.speed *= (1.0f + RUN_ACCEL * deltaTime * 0.1f);
                d.y += d.speed * deltaTime;
                d.x += d.drift * deltaTime * (0.5f + d.speed * 40.0f);
            }

            if (d.age >= d.life || d.y > 1.02f || d.x < -0.05f || d.x > 1.05f) {
                drops[i] = drops.back();
                drops.pop_back();
            }
            else {
                i++;
            }
        }
    }

    //Rebuild the geometry for this frame.
    pane->beginGeometry();
    if (mode != 0) {
        for (size_t i = 0; i < drops.size(); i++) {
            const Drop& d = drops[i];
            const irr::f32 t = d.age / d.life;
            irr::f32 fade = 1.0f;
            if (t > 0.65f) { fade = 1.0f - (t - 0.65f) / 0.35f; }  // drying, or blown off
            if (d.age < 0.15f) { fade *= d.age / 0.15f; }          // no hard pop on arrival
            const irr::f32 alpha = d.alpha * fade * GLASS_ALPHA;
            if (alpha <= 0.01f) { continue; }
            pane->addDrop(d.x, d.y, d.size, alpha, d.running);
        }
    }
}

void ScreenSpray::clear()
{
    drops.clear();
    if (pane) { pane->beginGeometry(); }
}

bool ScreenSpray::isWet() const
{
    return !drops.empty();
}