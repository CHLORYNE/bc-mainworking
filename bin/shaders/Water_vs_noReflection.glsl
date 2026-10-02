#version 130

    /*Shader for Open GL*/
    /*Based on shader for realisticWaterSceneNode:
    Copyright (c) 2007, elvman

    All rights reserved.

    Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
    Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
    Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
    CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
    EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
    PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
    LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
    NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
    */

    /* KYARA: no-reflection fallback path. It has no RTT to mirror, so all of its
       surface interest has to come from shading.

       vWorldXZ drives PROCEDURAL ripple normals in the pixel shader. This path
       deliberately does not sample a normal map: the bump texture is not guaranteed
       to be bound on this material, and an unbound sampler returns black, which would
       flatten the surface completely. World space rather than gl_Vertex.xz so the
       ripples cannot break at a tile boundary, same fix as the main path.

       vWaveHeight lets troughs be shaded darker than crests, matching the main
       shader.                                                                     */

    uniform mat4 matViewInverse; //We need to move the normal into world space so the reflections are accurate
    uniform mat4 WorldReflectionViewProj; // World * Reflection View * Projection transformation

    //varying vec3 reflectionMapTexCoord;
    varying vec3  Normal;
    varying vec3  ViewDirection;
    varying vec2  vWorldXZ;     // world-space horizontal position, drives the ripples
    varying float vWaveHeight;  // vertical displacement of the wave


    // ---- KYARA HOULE: long-wave swell -------------------------------------------------------
    // The FFT sea in the mesh is a 100 m tile that repeats, so it cannot carry a real houle
    // (60-300 m wavelength). These five long waves are added here, in WORLD space, so they cross
    // the tile boundaries seamlessly. The numbers come from Swell.cpp via MovingWater.cpp, and the
    // SAME numbers are used by the hull physics - what you see is what the ships ride on.
    //   swellN   = (kx, kz, amplitude, phase)      eta = A * cos(kx*x + kz*z - phase)
    //   swellFade= (centreX, centreZ, fadeStart, fadeEnd)  fades out at the edge of the animated
    //              patch so it meets the flat far-sea mesh without a step.
    uniform vec4 swell0;
    uniform vec4 swell1;
    uniform vec4 swell2;
    uniform vec4 swell3;
    uniform vec4 swell4;
    uniform vec4 swellFade;

    // .x = height, .y = dh/dx, .z = dh/dz
    vec3 swellWave(vec4 s, vec2 p)
    {
       float th = s.x * p.x + s.y * p.y - s.w;
       return vec3(s.z * cos(th), -s.z * sin(th) * s.x, -s.z * sin(th) * s.y);
    }

    vec3 swellAt(vec2 p)
    {
       vec3 h = swellWave(swell0, p) + swellWave(swell1, p) + swellWave(swell2, p)
              + swellWave(swell3, p) + swellWave(swell4, p);
       float r = length(p - swellFade.xy);
       float t = clamp((r - swellFade.z) / max(swellFade.w - swellFade.z, 1.0), 0.0, 1.0);
       return h * (1.0 - t * t * (3.0 - 2.0 * t));   // smoothstep, same curve as Swell.cpp
    }
    // ---- end KYARA HOULE ---------------------------------------------------------------------

    void main()
    {
       // KYARA HOULE: see Water_vs.glsl - same displacement, same uniforms.
       vec3 viewPos0  = (gl_ModelViewMatrix * gl_Vertex).xyz;
       vec4 worldPos0 = matViewInverse * vec4(viewPos0, 1.0);
       vec3 sw        = swellAt(worldPos0.xz);

       vec4 swelledVertex = gl_Vertex;
       swelledVertex.y += sw.x;

       gl_Position = gl_ModelViewProjectionMatrix * swelledVertex; // was ftransform()

       // World-space normal (for lighting), with the swell slope folded in
       vec3 nObj = gl_Normal;
       float ny  = max(nObj.y, 0.05);
       // KYARA EAU - TRIANGLES: the shortest FFT waves are as small as the ~3 m mesh grid, so
       // their per-vertex normal flips from one vertex to the next and the sun sheen outlines
       // each triangle. Only the chop SLOPE is scaled; swell slope and all heights unchanged.
       //   1.00 = full wave shading - shows triangle-shaped shading in rough seas
       //   0.40 = default (the photo ripples do NOT hide the triangles once the sea is rough)
       //   0.25 = smoother still, if any triangle shapes remain
       const float CHOP_NORMAL_WEIGHT = 0.40;
       nObj = vec3(CHOP_NORMAL_WEIGHT * nObj.x / ny - sw.y, 1.0, CHOP_NORMAL_WEIGHT * nObj.z / ny - sw.z);
       Normal      = gl_NormalMatrix * nObj;
       Normal      = normalize((matViewInverse*vec4(Normal,0)).xyz);

       // World-space view direction (camera -> this vertex), for fresnel + specular.
       vec3 viewPos  = (gl_ModelViewMatrix * swelledVertex).xyz;
       ViewDirection = normalize((matViewInverse*vec4(viewPos,0.0)).xyz);

       // World-space position, for the procedural ripple field.
       vec4 worldPos = matViewInverse * vec4(viewPos, 1.0);
       vWorldXZ      = worldPos.xz;

       vWaveHeight = gl_Vertex.y + 0.35 * sw.x; // KYARA HOULE

       // Eye-space distance for scene-matched fog (read back as gl_FogFragCoord in the
       // pixel shader). This is the same quantity the fixed-function fog uses for ships.
       gl_FogFragCoord = length(viewPos);

       // reflection texcoords
       //vec4 pos = WorldReflectionViewProj * gl_Vertex;
       //Scale pos.x and .y by 1.5 to compensate for larger render texture
       //reflectionMapTexCoord.x = 0.5 * (pos.w + pos.x/1.5);// + 0.0*cnoise(Normal);
       //reflectionMapTexCoord.y = 0.5 * (pos.w + pos.y/1.5);// + 0.0*cnoise(Normal);
       //reflectionMapTexCoord.z = pos.w;

    }
