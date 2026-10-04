//Based on CWaterSurfaceSceneNode, with the original copyright notice:
// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

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

#include "MovingWater.hpp"
     //#include "Utilities.hpp"

#include <iostream>
#include <cstdint>
//#include <cmath>

namespace irr
{
    namespace scene
    {

        //! constructor
        MovingWaterSceneNode::MovingWaterSceneNode(ISceneNode* parent, ISceneManager* mgr, ISceneNode* ownShip, irr::s32 id, irr::u32 disableShaders, bool withReflection, irr::u32 segments, irr::u32 reflectionEveryN,
            const core::vector3df& position, const core::vector3df& rotation)
            : IMeshSceneNode(parent, mgr, id, position, rotation, irr::core::vector3df(1.0f, 1.0f, 1.0f)), lightLevel(0.75), seaState(0.5), wavesEvaluated(false), shaderTime(0.0f), disableShaders(disableShaders), withReflection(withReflection), segments(segments), reflectionEveryN(reflectionEveryN)
        {
#ifdef _DEBUG
            setDebugName("MovingWaterSceneNode");
#endif

            //scaleFactorVertical = 1.0;

            driver = mgr->getVideoDriver();
            ownShipSceneNode = ownShip;
            // KYARA: default to neutral white until OnAnimate runs
            lightColour[0] = 1.0f; lightColour[1] = 1.0f; lightColour[2] = 1.0f;

            //From Mel demo (http://irrlicht.sourceforge.net/forum/viewtopic.php?f=9&t=51130&start=15#p296723) START
            irr::video::E_DRIVER_TYPE driverType = mgr->getVideoDriver()->getDriverType();

            IsOpenGL = (driverType == irr::video::EDT_OPENGL);
            firstRun = true;

            // KYARA HOULE: flat until the model sends the first swell
            for (int i = 0; i < 20; i++) { swellData[i] = 0.0f; }
            swellFade[0] = 0.0f; swellFade[1] = 0.0f; swellFade[2] = 750.0f; swellFade[3] = 1000.0f;
            for (int i = 0; i < 5; i++) { idSwell[i] = -1; }
            idSwellFade = -1;

            //cubemapConstants* cns = new cubemapConstants(driverType==irr::video::EDT_OPENGL);
            //So far there are no materials ready to use a cubemap, so we provide our own.
            irr::s32 shader = 0;

            if (!disableShaders) {
                irr::io::path vertexShader;
                irr::io::path pixelShader;
                if (driverType == irr::video::EDT_DIRECT3D9) {
                    //DirectX, not currently used
                    vertexShader = "shaders/Water_vs.hlsl";
                    pixelShader = "shaders/Water_ps.hlsl";
                }
                else {
                    //OpenGL
                    if (withReflection) {
                        vertexShader = "shaders/Water_vs.glsl";
                        pixelShader = "shaders/Water_ps.glsl";
                    }
                    else {
                        vertexShader = "shaders/Water_vs_noReflection.glsl";
                        pixelShader = "shaders/Water_ps_noReflection.glsl";
                    }

                }

                shader = driver->getGPUProgrammingServices()->addHighLevelShaderMaterialFromFiles(
                    vertexShader,
                    "main",
                    irr::video::EVST_VS_2_0,
                    pixelShader,
                    "main",
                    irr::video::EPST_PS_2_0,
                    this, //For callbacks
                    irr::video::EMT_SOLID
                );
            }
            shader = shader == -1 ? 0 : shader; //Just in case something goes horribly wrong...

            //FIXME: Hardcoded or defined in multiple places
            //SHADERS WATER WIDTH
            tileWidth = 100; //Width in metres - Note this is used in Simulation model normalisation as 100, so visible jumps in water are minimised
            irr::f32 segmentSize = tileWidth / segments;

            ocean = new cOcean(segments, 0.00005f, vector2(32.0f, 32.0f), tileWidth); //Note that the A and w parameters will get overwritten by ocean->resetParameters() dependent on the model's weather

            mesh = mgr->addHillPlaneMesh("myHill",
                irr::core::dimension2d<irr::f32>(segmentSize, segmentSize),
                irr::core::dimension2d<irr::u32>(segments, segments),
                0,
                0.0f,
                irr::core::dimension2d<irr::f32>(0, 0),
                irr::core::dimension2d<irr::f32>(tileWidth / (irr::f32)(segments), tileWidth / (irr::f32)(segments)));


            flatMesh = mgr->getMesh("media/flatsea.x");
            if (!flatMesh) {
                std::cerr << "Could not load flat sea mesh from media/flatsea.x" << std::endl;
                exit(EXIT_FAILURE);
            }


            //For testing, make wireframe
            /*
            for (irr::u32 i=0; i<mesh->getMeshBufferCount(); ++i)
            {
                scene::IMeshBuffer* mb = mesh->getMeshBuffer(i);
                if (mb)
                {
                    mb->getMaterial().setFlag(video::EMF_WIREFRAME, true);
                }
            }
            */

            //Create local camera for reflections
            _camera = 0;
            _reflectionMap = 0;

            if (!disableShaders) {
                if (withReflection) {
                    _camera = mgr->addCameraSceneNode(0, irr::core::vector3df(0, 0, 0), irr::core::vector3df(0, 0, 0), -1, false);
                    //KYARA: 256 is plenty for a reflection on a rippled sea - the normal map perturbation
                    //destroys any detail finer than this anyway. On the Eyefinity canvas the water covers a
                    //huge fraction of the screen, so this saves real fill on every frame.
                    _reflectionMap = driver->addRenderTargetTexture(irr::core::dimension2d<irr::u32>(256, 256));
                }

                irr::video::ITexture* bumpTexture = driver->getTexture("media/waterbump.png");

                for (irr::u32 i = 0; i < mesh->getMeshBufferCount(); ++i)
                {
                    scene::IMeshBuffer* mb = mesh->getMeshBuffer(i);
                    if (mb)
                    {
                        mb->getMaterial().setTexture(0, bumpTexture);
                        //KYARA EAU: without anisotropic filtering the ripple normal map is blurred flat when
                        //seen at a low angle - which is every piece of water more than ~100 m away.
                        mb->getMaterial().TextureLayer[0].AnisotropicFilter = 16;
                        mb->getMaterial().TextureLayer[0].TrilinearFilter = true;
                        if (withReflection) {
                            mb->getMaterial().setTexture(1, _reflectionMap);
                        }
                        mb->getMaterial().MaterialType = (irr::video::E_MATERIAL_TYPE)shader;
                        mb->getMaterial().FogEnable = true;
                    }
                }


                for (irr::u32 i = 0; i < flatMesh->getMeshBufferCount(); ++i)
                {
                    scene::IMeshBuffer* mb = flatMesh->getMeshBuffer(i);
                    if (mb)
                    {
                        mb->getMaterial().setTexture(0, bumpTexture);
                        //KYARA EAU: without anisotropic filtering the ripple normal map is blurred flat when
                        //seen at a low angle - which is every piece of water more than ~100 m away.
                        mb->getMaterial().TextureLayer[0].AnisotropicFilter = 16;
                        mb->getMaterial().TextureLayer[0].TrilinearFilter = true;
                        if (withReflection) {
                            mb->getMaterial().setTexture(1, _reflectionMap);
                        }
                        mb->getMaterial().MaterialType = (irr::video::E_MATERIAL_TYPE)shader;
                        mb->getMaterial().FogEnable = true;

                    }
                }
            }


            if (disableShaders) {
                for (irr::u32 i = 0; i < mesh->getMeshBufferCount(); ++i)
                {
                    scene::IMeshBuffer* mb = mesh->getMeshBuffer(i);
                    if (mb)
                    {
                        mb->getMaterial().FogEnable = true;
                    }
                }


                for (irr::u32 i = 0; i < flatMesh->getMeshBufferCount(); ++i)
                {
                    scene::IMeshBuffer* mb = flatMesh->getMeshBuffer(i);
                    if (mb)
                    {
                        mb->getMaterial().FogEnable = true;
                    }
                }
            }

            //Hard code bounding box to be large - we always want to render water, and we actually render multiple displaced copies of the mesh, so just getting the mesh bounding box isn't correct.
            //TODO: Look here if there's a problem with the water disappearing or if we implement collision with water.
            boundingBox = irr::core::aabbox3d<irr::f32>(-10000, -100, -10000, 10000, 100, 10000);

        }


        //! destructor
        MovingWaterSceneNode::~MovingWaterSceneNode()
        {
            // Mesh is dropped in IMeshSceneNode destructor (??? FIXME: Probably not true!)
            delete ocean;

            if (_camera)
            {
                _camera->drop();
                _camera = NULL;
            }

            if (_reflectionMap)
            {
                _reflectionMap->drop();
                _reflectionMap = NULL;
            }

        }

        //From OpenCV via http://stackoverflow.com/a/20723890
        int MovingWaterSceneNode::localisinf(double x) const
        {
            union { uint64_t u; double f; } ieee754;
            ieee754.f = x;
            return ((unsigned)(ieee754.u >> 32) & 0x7fffffff) == 0x7ff00000 &&
                ((unsigned)ieee754.u == 0);
        }

        int MovingWaterSceneNode::localisnan(double x) const
        {
            union { uint64_t u; double f; } ieee754;
            ieee754.f = x;
            return ((unsigned)(ieee754.u >> 32) & 0x7fffffff) +
                ((unsigned)ieee754.u != 0) > 0x7ff00000;
        }
        //End From OpenCV via http://stackoverflow.com/a/20723890


        void MovingWaterSceneNode::setSwellShaderData(const irr::f32* comp20, const irr::f32* fade4)
        {
            // KYARA HOULE: stored here, sent to the GPU in OnSetConstants
            for (int i = 0; i < 20; i++) { swellData[i] = comp20[i]; }
            for (int i = 0; i < 4; i++) { swellFade[i] = fade4[i]; }
        }

        void MovingWaterSceneNode::resetParameters(float A, vector2 w, float seaState)
        {
            ocean->resetParameters(A, w);
            this->seaState = seaState;
        }

        void MovingWaterSceneNode::OnSetConstants(video::IMaterialRendererServices* services, irr::s32 userData)
        {
            //From Mel's cubemap demo
            if (!disableShaders) {
                if (firstRun) {
                    firstRun = false;

                    driver = services->getVideoDriver();
                    //Looking for our constants IDs...
                    matViewInverse = services->getVertexShaderConstantID("matViewInverse");
                    if (withReflection) {
                        matWorldReflectionViewProj = services->getVertexShaderConstantID("WorldReflectionViewProj");
                    }
                    idLightLevel = services->getVertexShaderConstantID("lightLevel");
                    idSeaState = services->getVertexShaderConstantID("seaState");
                    //kyara fix
                    idTime = services->getVertexShaderConstantID("time");   // NEW
                    idLightColour = services->getVertexShaderConstantID("lightColour");  // KYARA
                    // KYARA HOULE: -1 if the shader doesn't declare them yet (then nothing is sent)
                    idSwell[0] = services->getVertexShaderConstantID("swell0");
                    idSwell[1] = services->getVertexShaderConstantID("swell1");
                    idSwell[2] = services->getVertexShaderConstantID("swell2");
                    idSwell[3] = services->getVertexShaderConstantID("swell3");
                    idSwell[4] = services->getVertexShaderConstantID("swell4");
                    idSwellFade = services->getVertexShaderConstantID("swellFade");
                    if (IsOpenGL)
                    {
                        if (withReflection) {
                            baseMap = services->getPixelShaderConstantID("baseMap");
                            reflectionMap = services->getPixelShaderConstantID("reflectionMap");
                        }
                    }
                    else
                    {
                        matWorldViewProjection = services->getVertexShaderConstantID("matWorldViewProjection");
                        matWorld = services->getVertexShaderConstantID("matWorld");
                    }
                }

                //Setting up our constants...
                irr::core::matrix4 mat;

                mat = driver->getTransform(irr::video::ETS_VIEW);
                mat.makeInverse();
                services->setVertexShaderConstant(matViewInverse, mat.pointer(), 16);

                if (withReflection) {
                    irr::core::matrix4 worldReflectionViewProj = driver->getTransform(video::ETS_PROJECTION);
                    worldReflectionViewProj *= _camera->getViewMatrix();;
                    worldReflectionViewProj *= driver->getTransform(video::ETS_WORLD);
                    services->setVertexShaderConstant(matWorldReflectionViewProj, worldReflectionViewProj.pointer(), 16);
                }

                if (IsOpenGL)
                {
                    int sampler = 0;
                    if (withReflection) {
                        services->setPixelShaderConstant(baseMap, &sampler, 1);
                    }
                    sampler = 1;
                    if (withReflection) {
                        services->setPixelShaderConstant(reflectionMap, &sampler, 1);
                    }
                    services->setPixelShaderConstant(idLightLevel, &lightLevel, 1);
                    services->setPixelShaderConstant(idSeaState, &seaState, 1);
                    services->setPixelShaderConstant(idLightColour, lightColour, 3);  // KYARA
                    //kyara fix 
                    services->setVertexShaderConstant(idTime, &shaderTime, 1);    // NEW
                    // KYARA HOULE
                    for (int i = 0; i < 5; i++) {
                        if (idSwell[i] >= 0) { services->setVertexShaderConstant(idSwell[i], &swellData[i * 4], 4); }
                    }
                    if (idSwellFade >= 0) { services->setVertexShaderConstant(idSwellFade, swellFade, 4); }

                }
                else
                {
                    mat = driver->getTransform(irr::video::ETS_PROJECTION);
                    mat *= driver->getTransform(irr::video::ETS_VIEW);
                    mat *= driver->getTransform(irr::video::ETS_WORLD);
                    services->setVertexShaderConstant(matWorldViewProjection, mat.pointer(), 16);

                    mat = driver->getTransform(irr::video::ETS_WORLD);
                    services->setVertexShaderConstant(matWorld, mat.pointer(), 16);
                }
                //End from Mel's cubemap demo
            }

        }


        //! frame
        void MovingWaterSceneNode::OnRegisterSceneNode()
        {
            //std::cout << "In OnRegisterSceneNode()" << std::endl;

            if (IsVisible) {
                SceneManager->registerNodeForRendering(this);
            }

            ISceneNode::OnRegisterSceneNode();
        }

        /*
        void MovingWaterSceneNode::setVerticalScale(irr::f32 scale)
        {
            scaleFactorVertical = scale;
        }
        */

        void MovingWaterSceneNode::OnAnimate(irr::u32 timeMs)
        {
            //std::cout << "In OnAnimate()" << std::endl;
            if (mesh && IsVisible)
            {
                shaderTime = timeMs / 1000.f;   // NEW: expose time to the water shader

                //Set light level
                video::SColorf ambientLight = this->getSceneManager()->getAmbientLight();
                lightLevel = (ambientLight.r + ambientLight.g + ambientLight.b) / 3.0; //Average
                // KYARA: keep the *chroma* of the ambient light, normalised so the brightest
                // channel is 1.0. Brightness is already carried by lightLevel, so this is pure hue.
                // Grey ambient -> (1,1,1) -> shader behaves exactly as before.
                irr::f32 maxC = ambientLight.r;
                if (ambientLight.g > maxC) { maxC = ambientLight.g; }
                if (ambientLight.b > maxC) { maxC = ambientLight.b; }
                if (maxC < 0.0001f) { maxC = 0.0001f; }
                lightColour[0] = ambientLight.r / maxC;
                lightColour[1] = ambientLight.g / maxC;
                lightColour[2] = ambientLight.b / maxC;
                const irr::f32 time = timeMs / 1000.f;

                //Update the FFT Calculation
                ocean->evaluateWavesFFT(time);
                wavesEvaluated = true;
                vertex_ocean* vertices = ocean->getVertices();

                const irr::u32 meshBufferCount = mesh->getMeshBufferCount();

                //KYARA: grid constants hoisted out - these were being recomputed inside the loop.
                const int   gridW = (int)segments + 1;
                const float spacing = tileWidth / (float)segments;
                const float invTwoSpacing = 1.0f / (2.0f * spacing);
                const irr::u32 gridCnt = (irr::u32)(gridW * gridW);

                // --- TUNABLES ---
                const float foamThreshold = 0.87f; // J below this => foam begins (raise => more foam)
                const float foamSharpness = 5.0f;  // how hard foam ramps once below threshold

                for (irr::u32 b = 0; b < meshBufferCount; ++b)
                {
                    irr::scene::IMeshBuffer* mb = mesh->getMeshBuffer(b);   //KYARA: hoisted out of the loop
                    if (!mb) { continue; }

                    const irr::u32 vtxCnt = mb->getVertexCount();

                    //KYARA: ONE fused pass. This used to be two full walks of the grid, and the first
                    //one went through getMeshBuffer()/getPosition()/getNormal() - all virtual - SIX
                    //times per vertex. At 128 segments that is ~100k virtual calls a frame, every frame,
                    //for work that is a straight array write. Grab the vertex array once and index it.
                    if (mb->getVertexType() == irr::video::EVT_STANDARD)
                    {
                        irr::video::S3DVertex* mv = (irr::video::S3DVertex*)mb->getVertices();

                        for (irr::u32 i = 0; i < vtxCnt; ++i)
                        {
                            mv[i].Pos.X = -1 * vertices[i].x;
                            mv[i].Pos.Y = vertices[i].y;
                            mv[i].Pos.Z = vertices[i].z;

                            // FFT solver's own analytic normals - correct AND cheaper than
                            // recalculateNormals(), which is O(triangles) and comes out faceted.
                            mv[i].Normal.X = -1 * vertices[i].nx;
                            mv[i].Normal.Y = vertices[i].ny;
                            mv[i].Normal.Z = vertices[i].nz;

                            // ---- Jacobian foam (Tessendorf), same pass ----
                            // Foam forms where the choppy horizontal displacement folds the surface
                            // onto itself - i.e. at pinching crests. Packed into vertex colour alpha.
                            if (i >= gridCnt) { continue; }

                            const int r = (int)i / gridW;
                            const int c = (int)i % gridW;
                            const int rU = (r > 0) ? r - 1 : r;
                            const int rD = (r < gridW - 1) ? r + 1 : r;
                            const int cL = (c > 0) ? c - 1 : c;
                            const int cR = (c < gridW - 1) ? c + 1 : c;

                            const float DxR = vertices[r * gridW + cR].x - vertices[r * gridW + cR].ox;
                            const float DxL = vertices[r * gridW + cL].x - vertices[r * gridW + cL].ox;
                            const float DxD = vertices[rD * gridW + c].x - vertices[rD * gridW + c].ox;
                            const float DxU = vertices[rU * gridW + c].x - vertices[rU * gridW + c].ox;
                            const float DzR = vertices[r * gridW + cR].z - vertices[r * gridW + cR].oz;
                            const float DzL = vertices[r * gridW + cL].z - vertices[r * gridW + cL].oz;
                            const float DzD = vertices[rD * gridW + c].z - vertices[rD * gridW + c].oz;
                            const float DzU = vertices[rU * gridW + c].z - vertices[rU * gridW + c].oz;

                            const float dDx_dx = (DxR - DxL) * invTwoSpacing;
                            const float dDx_dz = (DxD - DxU) * invTwoSpacing;
                            const float dDz_dx = (DzR - DzL) * invTwoSpacing;
                            const float dDz_dz = (DzD - DzU) * invTwoSpacing;

                            const float J = (1.0f + dDx_dx) * (1.0f + dDz_dz) - dDx_dz * dDz_dx;

                            float foam = (foamThreshold - J) * foamSharpness;
                            if (foam < 0.0f) { foam = 0.0f; }
                            if (foam > 1.0f) { foam = 1.0f; }

                            mv[i].Color = irr::video::SColor((irr::u32)(foam * 255.0f), 255, 255, 255);
                        }
                    }
                    else
                    {
                        //Fallback for any non-standard vertex format: no foam, positions only.
                        for (irr::u32 i = 0; i < vtxCnt; ++i) {
                            mb->getPosition(i).X = -1 * vertices[i].x;
                            mb->getPosition(i).Y = vertices[i].y;
                            mb->getPosition(i).Z = vertices[i].z;
                            mb->getNormal(i).X = -1 * vertices[i].nx;
                            mb->getNormal(i).Y = vertices[i].ny;
                            mb->getNormal(i).Z = vertices[i].nz;
                        }
                    }
                }


                mesh->setDirty(scene::EBT_VERTEX);
            }

            IMeshSceneNode::OnAnimate(timeMs);
            //Fixme: Need to store timeMs in something accessible to the shader for ripples

            //Render reflection to texture
            //Render reflection to texture
            if (IsVisible && !disableShaders)
            {
                //KYARA: this block is a FULL second drawAll() of the whole scene, and it was running every
                //frame - it is over half the frame time (Render 16.1ms vs 1.5ms for the same drawAll with
                //the water hidden). The RTT keeps its contents between frames, so refreshing it every 2nd
                //frame costs nothing visually. Raise REFLECTION_EVERY_N to 3 if you need more back.
                //static is safe: there is exactly one water node. If that ever changes, move these two to
                //members in MovingWater.hpp.
        // KYARA: cadence now driven by the ini tri-state (1 = full, 2 = half). When reflection is
                // OFF, withReflection is false and the whole pass below is skipped regardless of N.
                const irr::u32 reflectionEveryNsafe = (reflectionEveryN < 1) ? 1 : reflectionEveryN;
                static irr::u32 reflectionFrameCounter = 0;
                bool updateReflectionThisFrame = ((reflectionFrameCounter++ % reflectionEveryNsafe) == 0);

                if (withReflection && updateReflectionThisFrame) {
                    //fixes glitches with incomplete refraction
                    const irr::f32 CLIP_PLANE_OFFSET_Y = 0.0f;

                    irr::core::rect<irr::s32> currentViewPort = driver->getViewPort(); //Get the previous viewPort

                    setVisible(false); //hide the water

                    bool reShowOwnShip = false;

                    if (ownShipSceneNode->isVisible()) {
                        ownShipSceneNode->setVisible(false);
                        reShowOwnShip = true;
                    }

                    //reflection
                    driver->setRenderTarget(_reflectionMap, irr::video::ECBF_COLOR | irr::video::ECBF_DEPTH); //render to reflection

                    //get current camera
                    scene::ICameraSceneNode* currentCamera = SceneManager->getActiveCamera();
                    irr::f32 currentAspect = currentCamera->getAspectRatio();

                    //use this aspect ratio
                    _camera->setAspectRatio(currentAspect);

                    //KYARA: the reflection pass is a FULL second drawAll() of the whole scene. Inheriting the
                     //main camera's far value means it frustum-culls and submits every piece of distant terrain
                     //and every land object - none of which you can make out in a reflection on a moving sea.
                     //Clamping the far value prunes most of that before it ever reaches a draw call.
                     //Raise REFLECTION_FAR if you lose a reflection you actually wanted (a nearby jetty, say).
                    const irr::f32 REFLECTION_FAR = 2000.0f; //metres
                    irr::f32 reflectionFar = currentCamera->getFarValue();
                    if (reflectionFar > REFLECTION_FAR) { reflectionFar = REFLECTION_FAR; }
                    _camera->setFarValue(reflectionFar);
                    irr::f32 renderScale = 1.5; //This matches the scaling in the shader, to avoid artefacts near the edge of the screen
                    irr::f32 renderFOV = 2 * atan(renderScale * tan(currentCamera->getFOV() / 2));
                    _camera->setFOV(renderFOV);

                    irr::core::vector3df position = currentCamera->getAbsolutePosition();
                    position.Y = -position.Y + 2 * RelativeTranslation.Y; //position of the water
                    _camera->setPosition(position);

                    irr::core::vector3df target = currentCamera->getTarget();

                    //invert Y position of current camera
                    target.Y = -target.Y + 2 * RelativeTranslation.Y;
                    _camera->setTarget(target);

                    //set the reflection camera
                    SceneManager->setActiveCamera(_camera);

                    //reflection clipping plane
                    irr::core::plane3d<irr::f32> reflectionClipPlane(0, RelativeTranslation.Y - CLIP_PLANE_OFFSET_Y, 0, 0, 1, 0);
                    driver->setClipPlane(0, reflectionClipPlane, true);

                    SceneManager->drawAll(); //draw the scene

                    //disable clip plane
                    driver->enableClipPlane(0, false);

                    //set back old render target
                    driver->setRenderTarget(0, 0);

                    //set back the active camera
                    SceneManager->setActiveCamera(currentCamera);

                    setVisible(true); //show it again

                    if (reShowOwnShip) {
                        ownShipSceneNode->setVisible(true);
                    }

                    //Reset :: Fixme: Doesn't seem to be working on old PC
                    driver->setViewPort(irr::core::rect<irr::s32>(0, 0, 10, 10));//Set to a dummy value first to force the next call to make the change
                    driver->setViewPort(currentViewPort);
                    currentCamera->setAspectRatio(currentAspect);
                }
            }
        }

        irr::f32 MovingWaterSceneNode::getWaveHeight(irr::f32 relPosX, irr::f32 relPosZ) const
        {
            //Before the first frame is drawn the grid holds no sea yet (not the scenario's sea state):
            //read as flat, else the ship's first physics step feels steep slopes and is pushed off.
            if (!wavesEvaluated) { return 0; }

            //Adjust relative position by 1/2 tile width


            //Get the wave height (not including tide height) at this position relative to the origin of the water
            irr::f32 relPosXInternal = fmod(relPosX + tileWidth / 2, tileWidth);
            irr::f32 relPosZInternal = fmod(relPosZ + tileWidth / 2, tileWidth);

            //TODO: Probably not needed?
            while (relPosXInternal < 0)
                relPosXInternal += tileWidth;
            while (relPosZInternal < 0)
                relPosZInternal += tileWidth;

            irr::f32 xIndexFloat = (irr::f32)(segments + 1) * relPosXInternal / tileWidth;
            irr::f32 zIndexFloat = (irr::f32)(segments + 1) * relPosZInternal / tileWidth;
            xIndexFloat = (segments + 1) - xIndexFloat; //Sign of x is flipped when heights are applied!

            //std::cout << "xIndexF:" << xIndexFloat << " zIndexF:" << zIndexFloat << " segments+1:" << segments+1 << std::endl;

            //Bilinear interpolation
            unsigned int xIndex0 = floor(xIndexFloat);
            unsigned int zIndex0 = floor(zIndexFloat);
            unsigned int xIndex1 = ceil(xIndexFloat);
            unsigned int zIndex1 = ceil(zIndexFloat);

            //If any indexes are equal to segments+1, set to 0 (as sea tiles)
            if (xIndex0 == (segments + 1)) { xIndex0 = 0; }
            if (zIndex0 == (segments + 1)) { zIndex0 = 0; }
            if (xIndex1 == (segments + 1)) { xIndex1 = 0; }
            if (zIndex1 == (segments + 1)) { zIndex1 = 0; }

            irr::f32 interpX = xIndexFloat - xIndex0;
            irr::f32 interpZ = zIndexFloat - zIndex0;

            unsigned int index00 = (segments + 1) * zIndex0 + xIndex0;
            unsigned int index01 = (segments + 1) * zIndex1 + xIndex0;
            unsigned int index10 = (segments + 1) * zIndex0 + xIndex1;
            unsigned int index11 = (segments + 1) * zIndex1 + xIndex1;

            vertex_ocean* vertices = ocean->getVertices();

            //Error checking here?
            irr::f32 height00 = vertices[index00].y;
            irr::f32 height01 = vertices[index01].y;
            irr::f32 height10 = vertices[index10].y;
            irr::f32 height11 = vertices[index11].y;


            irr::f32 localHeight = height00 * (1 - interpX) * (1 - interpZ) + height10 * interpX * (1 - interpZ) + height01 * (1 - interpX) * interpZ + height11 * interpX * interpZ;

            if (localisnan(localHeight) || localisinf(localHeight)) {
                return 0;
            }
            else {
                return localHeight;
            }

        }

        irr::core::vector2df MovingWaterSceneNode::getLocalNormals(irr::f32 relPosX, irr::f32 relPosZ) const
        {
            if (!wavesEvaluated) { return irr::core::vector2df(0, 0); } //flat until the first FFT evaluation

            //Adjust relative position by 1/2 tile width

            //Get the wave normal
            irr::f32 relPosXInternal = fmod(relPosX + tileWidth / 2, tileWidth);
            irr::f32 relPosZInternal = fmod(relPosZ + tileWidth / 2, tileWidth);

            //TODO: Probably not needed?
            while (relPosXInternal < 0)
                relPosXInternal += tileWidth;
            while (relPosZInternal < 0)
                relPosZInternal += tileWidth;

            irr::f32 xIndexFloat = (irr::f32)(segments + 1) * relPosXInternal / tileWidth;
            irr::f32 zIndexFloat = (irr::f32)(segments + 1) * relPosZInternal / tileWidth;
            xIndexFloat = (segments + 1) - xIndexFloat; //Sign of x is flipped when heights are applied!

            //std::cout << "xIndexF:" << xIndexFloat << " zIndexF:" << zIndexFloat << " segments+1:" << segments+1 << std::endl;

            //Bilinear interpolation
            unsigned int xIndex0 = floor(xIndexFloat);
            unsigned int zIndex0 = floor(zIndexFloat);
            unsigned int xIndex1 = ceil(xIndexFloat);
            unsigned int zIndex1 = ceil(zIndexFloat);

            //If any indexes are equal to segments+1, set to 0 (as sea tiles)
            if (xIndex0 == (segments + 1)) { xIndex0 = 0; }
            if (zIndex0 == (segments + 1)) { zIndex0 = 0; }
            if (xIndex1 == (segments + 1)) { xIndex1 = 0; }
            if (zIndex1 == (segments + 1)) { zIndex1 = 0; }

            irr::f32 interpX = xIndexFloat - xIndex0;
            irr::f32 interpZ = zIndexFloat - zIndex0;

            unsigned int index00 = (segments + 1) * zIndex0 + xIndex0;
            unsigned int index01 = (segments + 1) * zIndex1 + xIndex0;
            unsigned int index10 = (segments + 1) * zIndex0 + xIndex1;
            unsigned int index11 = (segments + 1) * zIndex1 + xIndex1;

            vertex_ocean* vertices = ocean->getVertices();

            //Error checking here?
            irr::f32 nx00 = vertices[index00].nx;
            irr::f32 nx01 = vertices[index01].nx;
            irr::f32 nx10 = vertices[index10].nx;
            irr::f32 nx11 = vertices[index11].nx;

            irr::f32 nz00 = vertices[index00].nz;
            irr::f32 nz01 = vertices[index01].nz;
            irr::f32 nz10 = vertices[index10].nz;
            irr::f32 nz11 = vertices[index11].nz;

            irr::f32 localNx = nx00 * (1 - interpX) * (1 - interpZ) + nx10 * interpX * (1 - interpZ) + nx01 * (1 - interpX) * interpZ + nx11 * interpX * interpZ;
            irr::f32 localNz = nz00 * (1 - interpX) * (1 - interpZ) + nz10 * interpX * (1 - interpZ) + nz01 * (1 - interpX) * interpZ + nz11 * interpX * interpZ;

            if (localisnan(localNx) || localisinf(localNx) || localisnan(localNz) || localisinf(localNz)) {
                return irr::core::vector2df(0, 0);
            }
            else {
                return irr::core::vector2df(localNx, localNz);
            }

        }

        void MovingWaterSceneNode::setMesh(IMesh* mesh)
        {
            //std::cout << "In setMesh()" << std::endl;
        }


        void MovingWaterSceneNode::render()
        {

            //std::cout << "In render()" << std::endl;

            if (!mesh || !driver) {
                std::cerr << "Could not render" << std::endl;
                return;
            }

            //driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);

            //Draw main water
            for (irr::u32 i = 0; i < mesh->getMeshBufferCount(); ++i)
            {
                scene::IMeshBuffer* mb = mesh->getMeshBuffer(i);
                if (mb)
                {
                    const video::SMaterial& material = mb->getMaterial();

                    // only render transparent buffer if this is the transparent render pass
                    // and solid only in solid pass: TODO: Does this need implementing?
                    driver->setMaterial(material);

                    irr::core::vector3df basicPosition = AbsoluteTransformation.getTranslation();

                    //Draw multiple copies of tileable water
                    for (int j = -10; j <= 10; j++) {
                        for (int k = -10; k <= 10; k++) {
                            AbsoluteTransformation.setTranslation(basicPosition + irr::core::vector3df(j * tileWidth, 0, k * tileWidth));
                            driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);
                            driver->drawMeshBuffer(mb);
                        }
                    }

                    AbsoluteTransformation.setTranslation(basicPosition);

                }
                else {
                    std::cerr << "No meshbuffer to render" << std::endl;
                }
            }

            //Draw flat sea beyond the animated sea
            for (irr::u32 i = 0; i < flatMesh->getMeshBufferCount(); ++i)
            {
                scene::IMeshBuffer* mb = flatMesh->getMeshBuffer(i);
                if (mb)
                {
                    const video::SMaterial& material = mb->getMaterial();

                    // only render transparent buffer if this is the transparent render pass
                    // and solid only in solid pass: TODO: Does this need implementing?
                    driver->setMaterial(material);

                    driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);
                    driver->drawMeshBuffer(mb);


                }
                else {
                    std::cerr << "No meshbuffer to render" << std::endl;
                }
            }

        }

        const irr::core::aabbox3d<irr::f32>& MovingWaterSceneNode::getBoundingBox() const
        {
            return boundingBox;
        }

        IMesh* MovingWaterSceneNode::getMesh(void)
        {
            //std::cerr << "In getMesh()" << std::endl;
            return mesh;
        }

        IShadowVolumeSceneNode* MovingWaterSceneNode::addShadowVolumeSceneNode(const IMesh* shadowMesh, irr::s32 id, bool zfailmethod, irr::f32 infinity)
        {
            //std::cerr << "In addShadowVolumeSceneNode()" << std::endl;
            return 0;
        }

        void MovingWaterSceneNode::setReadOnlyMaterials(bool readonly)
        {
            //Ignored
            //std::cerr << "In setReadOnlyMaterials()" << std::endl;
        }

        bool MovingWaterSceneNode::isReadOnlyMaterials() const
        {
            //std::cout << "In isReadOnlyMaterials()" << std::endl;
            return true; //Fixme: Check!
        }

        void MovingWaterSceneNode::setMaterialTexture(irr::u32 textureLayer, video::ITexture* texture)
        {
            if (textureLayer >= video::MATERIAL_MAX_TEXTURES)
                return;

            for (irr::u32 i = 0; i < mesh->getMeshBufferCount(); i++) {
                mesh->getMeshBuffer(i)->getMaterial().setTexture(textureLayer, texture);
            }

            //also set for far mesh
            for (irr::u32 i = 0; i < flatMesh->getMeshBufferCount(); i++) {
                flatMesh->getMeshBuffer(i)->getMaterial().setTexture(textureLayer, texture);
            }
            //for (irr::u32 i=0; i<getMaterialCount(); ++i)
            //    getMaterial(i).setTexture(textureLayer, texture);
        }


    }
}