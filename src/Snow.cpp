/*   NAUTITECH - Simulateur de Navigation
     Falling snow round the bridge.

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation. */

#include "Snow.hpp"
#include "Constants.hpp"
#include <cmath>
#include <cstdlib>
#include <string>

namespace
{
    //Shells round the camera (same mesh as the rain, a tall open cylinder)
    const irr::f32 RADIUS[3] = { 2.0f, 4.5f, 8.0f };
    const irr::f32 HEIGHT_PER_RADIUS = 18.0f;
    const irr::f32 U_REPEAT[3] = { 7.0f, 13.0f, 19.0f };     //pictures round each shell (more on the far ones: smaller flakes)
    const irr::f32 FALL_SPEED = 1.0f;                        //metres a second

    //Soft round flakes scattered over a tile that repeats seamlessly
    irr::video::ITexture* makePicture(irr::video::IVideoDriver* driver, int level)
    {
        const irr::u32 size = 256;
        irr::video::IImage* image = driver->createImage(irr::video::ECF_A8R8G8B8, irr::core::dimension2du(size, size));
        if (!image) { return 0; }
        image->fill(irr::video::SColor(0, 255, 255, 255));
        srand(1234 + level);
        const int flakes = 40 + level * 70;
        for (int f = 0; f < flakes; f++) {
            const irr::s32 cx = rand() % size, cy = rand() % size;
            const irr::f32 r = 1.4f + (rand() % 100) / 100.0f * 2.4f;
            const irr::u32 brightness = 225 + rand() % 30;
            for (int dy = -4; dy <= 4; dy++) {
                for (int dx = -4; dx <= 4; dx++) {
                    const irr::f32 d = sqrtf((irr::f32)(dx * dx + dy * dy)) / r;
                    if (d >= 1.0f) { continue; }
                    const irr::u32 x = (irr::u32)((cx + dx + (irr::s32)size) % (irr::s32)size);
                    const irr::u32 y = (irr::u32)((cy + dy + (irr::s32)size) % (irr::s32)size);
                    irr::f32 a = 1.0f - d;
                    a = a * a * (3.0f - 2.0f * a);
                    const irr::video::SColor old = image->getPixel(x, y);
                    const irr::u32 alpha = irr::core::max_(old.getAlpha(), (irr::u32)(a * 245.0f));
                    image->setPixel(x, y, irr::video::SColor(alpha, brightness, brightness, irr::core::min_(255u, brightness + 6)));
                }
            }
        }
        const std::string name = "snow-flakes-" + std::to_string(level);
        irr::video::ITexture* texture = driver->addTexture(name.c_str(), image);
        image->drop();
        return texture;
    }

    void setTextureMatrix(irr::scene::ISceneNode* node, irr::f32 uRepeat, irr::f32 vRepeat, irr::f32 uOffset, irr::f32 vOffset)
    {
        irr::core::matrix4& m = node->getMaterial(0).getTextureMatrix(0);
        m.makeIdentity();
        m[0] = uRepeat;
        m[5] = vRepeat;
        m[8] = uOffset;
        m[9] = vOffset;
    }
}

Snow::Snow() : smgr(0), shownLevel(-1)
{
    for (int i = 0; i < 3; i++) { layer[i] = 0; }
    for (int i = 0; i < LEVELS; i++) { picture[i] = 0; }
}

void Snow::load(irr::scene::ISceneManager* sceneManager)
{
    smgr = sceneManager;
    if (!smgr) { return; }
    irr::video::IVideoDriver* driver = smgr->getVideoDriver();
    for (int i = 0; i < LEVELS; i++) { picture[i] = makePicture(driver, i); }

    //The rain's mesh (already turned inside out by the rain), copied: its vertex colour is a beige
    //that the rain's bluish pictures make up for, white flakes need white
    irr::scene::IMesh* rainMesh = smgr->getMesh("media/rain.x");
    irr::scene::IMesh* mesh = rainMesh ? smgr->getMeshManipulator()->createMeshCopy(rainMesh) : 0;
    if (mesh) { smgr->getMeshManipulator()->setVertexColors(mesh, irr::video::SColor(255, 255, 255, 255)); }
    for (int i = 0; i < 3; i++) {
        layer[i] = mesh ? (irr::scene::ISceneNode*)smgr->addMeshSceneNode(mesh) : smgr->addEmptySceneNode();
        if (!layer[i]) { continue; }
        layer[i]->setScale(irr::core::vector3df(RADIUS[i], RADIUS[i] * HEIGHT_PER_RADIUS, RADIUS[i]));
        layer[i]->setMaterialType(irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL);
        layer[i]->setMaterialFlag(irr::video::EMF_LIGHTING, false);
        layer[i]->setMaterialFlag(irr::video::EMF_ZWRITE_ENABLE, false);
        layer[i]->setMaterialFlag(irr::video::EMF_BACK_FACE_CULLING, false);
        layer[i]->setMaterialFlag(irr::video::EMF_FOG_ENABLE, true);
        layer[i]->setMaterialFlag(irr::video::EMF_BILINEAR_FILTER, true);
        layer[i]->getMaterial(0).TextureLayer[0].TextureWrapU = irr::video::ETC_REPEAT;
        layer[i]->getMaterial(0).TextureLayer[0].TextureWrapV = irr::video::ETC_REPEAT;
        layer[i]->setVisible(false);
    }
    if (mesh) { mesh->drop(); }
}

void Snow::update(irr::f32 intensity, irr::f32 windSpeedKts, irr::f32 windDirDeg, irr::f32 scenarioTime)
{
    if (!layer[0] || !layer[1] || !layer[2]) { return; }
    intensity = irr::core::clamp(intensity, 0.0f, 1.0f);
    const bool snowing = intensity > 0.02f;
    //Light snow: only the far shells; heavy snow: all three, with denser pictures
    layer[0]->setVisible(snowing && intensity > 0.45f);
    layer[1]->setVisible(snowing && intensity > 0.2f);
    layer[2]->setVisible(snowing);
    if (!snowing) { return; }

    const int level = irr::core::clamp((int)(intensity * LEVELS), 0, LEVELS - 1);
    if (level != shownLevel) {
        shownLevel = level;
        for (int i = 0; i < 3; i++) {
            irr::video::ITexture* t = picture[irr::core::clamp(level - (2 - i) / 2, 0, LEVELS - 1)];
            if (t) { layer[i]->setMaterialTexture(0, t); }
        }
    }

    //Leaning with the wind (up to 30 degrees: more stretches the flakes), drifting with it, and
    //swaying a little as they fall
    const irr::f32 windMps = windSpeedKts * KTS_TO_MPS;
    irr::f32 slant = atan2f(windMps * 0.25f, FALL_SPEED * 3.0f) * irr::core::RADTODEG;
    if (slant > 30.0f) { slant = 30.0f; }
    for (int i = 0; i < 3; i++) {
        //Pictures up the shell so that the flakes come out round (found by eye: the mesh's texture
        //runs round the cylinder unevenly, so the geometry alone does not give it)
        const irr::f32 vRepeat = 3.0f * U_REPEAT[i] * 0.68f * (6.25f * HEIGHT_PER_RADIUS) / (2.0f * irr::core::PI * 3.0f);
        const irr::f32 heightMetres = 6.25f * RADIUS[i] * HEIGHT_PER_RADIUS;
        const irr::f32 fall = fmodf(scenarioTime * FALL_SPEED * vRepeat / heightMetres * (1.0f - 0.08f * i), 1.0f);
        const irr::f32 sway = 0.004f * sinf(scenarioTime * (0.6f + 0.2f * i) + i);
        const irr::f32 drift = fmodf(scenarioTime * windMps * 0.002f * (1.0f - 0.25f * i), 1.0f);
        setTextureMatrix(layer[i], U_REPEAT[i], vRepeat, 0.3f * i + drift + sway, fall);
        layer[i]->setRotation(irr::core::vector3df(slant, windDirDeg, 0.0f));
    }

    irr::scene::ICameraSceneNode* camera = smgr->getActiveCamera();
    if (camera) {
        for (int i = 0; i < 3; i++) { layer[i]->setPosition(camera->getAbsolutePosition()); }
    }
}
