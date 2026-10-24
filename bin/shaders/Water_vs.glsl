#version 130

    /*Shader for Open GL*/
    /*Based on shader for realisticWaterSceneNode:
    Copyright (c) 2007, elvman  (see original BSD notice in repo)
    Normal-map + Jacobian-foam additions layered on top.

    KYARA - two things in here that Water_ps.glsl depends on.

    (a) ViewDirection used to be normalize(gl_Position.xyz), i.e. the CLIP-space
        position, while Normal was transformed into WORLD space on the line above it.
        The pixel shader then took dot(N, V) between those two - a world vector
        against a clip vector. The result was not the viewing angle at all, it varied
        mostly with where the pixel sat on the screen. That is why Fresnel never
        tracked the waves and the specular sat in roughly the same place regardless of
        the sea. It is now built the same way Water_vs_noReflection.glsl already built
        it: vertex to eye space, then back out to world through matViewInverse.

        The ripple shimmer in the pixel shader uses reflect(-V, N), so it depends on
        this being a correct world-space vector. With the old clip-space version the
        shimmer would key off screen position instead of the waves.

    (b) vWorldXZ. The wind-streak and foam noise fields used to run on bumpTexCoord,
        which resets at every tile boundary in the water mesh - value noise then jumps
        discontinuously there, which is part of what produced the vertical seam. World
        XZ is continuous across the whole sea surface however it is tiled.

        This stays continuous even if matViewInverse carries no translation, because
        viewPos itself is continuous across the mesh. Worst case the noise pattern
        drifts with the camera, which is barely noticeable; it cannot reintroduce the
        seam.

    This file and Water_ps.glsl must be updated together.                          */

    uniform mat4 matViewInverse;
    uniform mat4 WorldReflectionViewProj;

    varying vec3  reflectionMapTexCoord;
    varying vec3  Normal;
    varying vec3  ViewDirection;
    varying vec2  bumpTexCoord;
    varying vec2  vWorldXZ;      // world-space horizontal position, for seam-free noise
    varying float vWorldY;       // world-space height of the surface (hull mask)
    varying float foamAmount;    // per-vertex Jacobian foam (vertex colour alpha)
    varying float vWaveHeight;   // vertical displacement of the wave


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
       // KYARA HOULE: world position of the undisplaced vertex - the swell is a world-space field,
       // and the water node only translates (no rotation, no scale), so object Y = world Y.
       vec3 viewPos0  = (gl_ModelViewMatrix * gl_Vertex).xyz;
       vec4 worldPos0 = matViewInverse * vec4(viewPos0, 1.0);
       vec3 sw        = swellAt(worldPos0.xz);

       vec4 swelledVertex = gl_Vertex;
       swelledVertex.y += sw.x;

       gl_Position = gl_ModelViewProjectionMatrix * swelledVertex; // was ftransform()

       // World-space normal, with the swell slope folded in. A normal (nx, ny, nz) is the slope
       // form (-dh/dx, 1, -dh/dz) once divided by ny, so the two slopes simply add.
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
       Normal = gl_NormalMatrix * nObj;
       Normal = normalize((matViewInverse * vec4(Normal, 0.0)).xyz);

       // World-space view direction (camera -> this vertex), for Fresnel + specular.
       vec3 viewPos  = (gl_ModelViewMatrix * swelledVertex).xyz;
       ViewDirection = normalize((matViewInverse * vec4(viewPos, 0.0)).xyz);
       // OLD (clip-space, mismatched with Normal - kept only for A/B comparison):
       // ViewDirection = normalize(gl_Position.xyz);

       // World-space position, used only for the noise fields in the pixel shader.
       vec4 worldPos = matViewInverse * vec4(viewPos, 1.0);
       vWorldXZ = worldPos.xz;
       vWorldY = worldPos.y;

       // Eye-space distance, so fixed-function fog distance is available to any pass
       // that wants it. Harmless if unused.
       gl_FogFragCoord = length(viewPos);

       bumpTexCoord = gl_MultiTexCoord0.xy;

       // The CPU (OnAnimate) packs Jacobian foam into the vertex colour's alpha.
       foamAmount  = gl_Color.a;
       // KYARA HOULE: the crest/trough shading in Water_ps.glsl is tuned for the FFT chop
       // (thresholds around +/- 1.5 m). Only a fraction of the swell height is added so a 3 m
       // houle does not saturate that shading; raise 0.35 if you want swell crests to light up more.
       vWaveHeight = gl_Vertex.y + 0.35 * sw.x;

       // Reflection texcoords. x and y are scaled by 1.5 to compensate for the
       // larger render texture.
       vec4 pos = WorldReflectionViewProj * swelledVertex; // KYARA HOULE: displaced vertex
       reflectionMapTexCoord.x = 0.5 * (pos.w + pos.x / 1.5);
       reflectionMapTexCoord.y = 0.5 * (pos.w + pos.y / 1.5);
       reflectionMapTexCoord.z = pos.w;
    }
