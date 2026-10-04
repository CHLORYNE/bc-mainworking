#version 130

/* Bridge Command - realistic water fragment shader (Tier 8)
   Drop-in replacement for bin/shaders/Water_ps.glsl.

   KYARA - this round is about getting the shimmer onto the WHOLE sea.

   What you were seeing: the bright rippled patch was the SUN GLINT, not the
   reflection map. Measured off your screenshot it peaks at (211, 251, 255) with
   saturation dropping to 0.51 against 0.76 for the water around it - that is an
   additive white highlight. A sun glitter path is directional by nature: it only
   exists where the sun's mirror image lands, so it can never appear in every
   direction at once. Meanwhile the water outside it had a pixel range topping out at
   (32, 78, 105) on one side and (26, 75, 105) on the other. Flat paint. The glint was
   the only thing revealing the wave structure at all.

   Three fixes:

   1. ANALYTIC SKY ON THE RIPPLES (the main one). reflect(-V, N).y changes with every
      wavelet no matter which way the camera faces, so it gives shimmer in all
      directions rather than only towards the sun.

      Applied as a DEVIATION, not a mix. If it simply blended towards sky colour it
      would lighten the whole sea and undo the palette you approved. Instead it
      computes what R.y would be on flat water (upFlat) and modulates by the
      difference, so flat water comes out EXACTLY unchanged and ripples go brighter
      and darker around it. Verified: mean stays at the reference (24, 80, 116) while
      the spread opens from pinned-flat to roughly (19, 67, 96) - (27, 90, 130).

   2. THE NORMAL WAS BEING FLATTENED AT 500m. 'N = mix(vec3(0,1,0), N, distanceFade)'
      with distanceFade = 1 - z/500 meant everything past 500 metres was a
      mirror-smooth plane by construction - which is why the mid and far water in your
      bow-view shot showed nothing at all. There is now a separate range with a floor
      so distant water keeps some structure. See NORMAL_FLATTEN_* below; if you get
      shimmer crawl on the horizon, that floor is the knob.

   3. SPECULAR REACHES FURTHER and the broad lobe is wider (exponent 22 -> 10), so the
      sun path spreads across more of the surface instead of being one tight blob.

   Requires: matching Water_vs.glsl (this set - it supplies vWorldXZ), the 'time'
   uniform, and the 'lightColour' uniform set from MovingWater.cpp's OnAnimate.
   Based on the elvman realisticWaterSceneNode shader.                            */

/* KYARA EAU (far water / grey water) - see the "KYARA EAU" tags below:
   1. Reflection hazed to the analytic SKY (sky hue, light level) instead of gl_Fog.color.
      The fog colour is a near-grey (188,201,212) and it took over the reflection from
      260 m out - that was the pale grey band. Real atmospheric haze is still applied at
      the very end by fogFactor, which follows the visibility, so fog/storm still grey out.
   2. REFL_WATER_TINT keeps reflections in the water's own hue, so the sea stays blue when
      there is no sun on it (the grey sky/cloud it reflects no longer turns it grey).
   3. Far water keeps ripples: world-space ripple UVs (the far flat sea mesh gets the same
      ripples as the animated sea, and the seam every 100 m is gone), two large-scale
      "far" ripple layers, and a floor on the ripple fade instead of fading to a mirror.
   4. The animated sea ends ~1 km out; beyond it is a flat mesh. The wave normal and
      crest shading now fade out before that edge, so there is no hard line.
   5. FAR_DARKEN, FRESNEL_MAX, REFLECTION_STRENGTH back to your approved values
      (0.60/0.50/0.55), FAR_DARKEN set to 1.0 so far water is the same colour as near. */

// ============================================================================
//  KYARA METEO - BAD-WEATHER SEA ("mauvais temps")
// ============================================================================
// 'gloom' (0..1) comes from the C++ side (Water::update): it rises with the sea state and
// with rain, and eases in over ~20 s. Under a storm sky the sea loses its blue: it reflects
// a grey overcast sky, there is no sun to light the body of the water, and it turns a dark
// slate grey-green with more, whiter foam. 0 = exactly the fair-weather look you have now.
uniform float gloom;
const vec3  STORM_BODY        = vec3(0.105, 0.165, 0.175); // looking down: dark slate grey-green
const vec3  STORM_GRAZE       = vec3(0.085, 0.135, 0.150); // towards the horizon
const vec3  STORM_SKY_HORIZON = vec3(0.50, 0.53, 0.55);    // overcast sky seen in the water
const vec3  STORM_SKY_ZENITH  = vec3(0.36, 0.39, 0.42);
const float STORM_DARKEN      = 0.80;  // overall level of the water at full storm (1 = no change)
const float STORM_DESATURATE  = 0.45;  // 0 = keep colour, 1 = fully grey
const float STORM_SUN         = 0.10;  // fraction of sun glint/glitter left under full overcast
const float STORM_FOAM_BOOST  = 1.70;  // more / whiter foam in a storm
float gStorm = 0.0;                    // set at the top of main()

uniform sampler2D baseMap;         // tangent-space normal map (unit 0, waterbump.png)
uniform sampler2D reflectionMap;   // reflection RTT (unit 1)
uniform float     lightLevel;
uniform float     seaState;
uniform float     time;
uniform vec3      lightColour;     // normalised ambient/sun chroma (white = neutral)

varying vec3  Normal;
varying vec3  ViewDirection;
varying vec3  reflectionMapTexCoord;
varying vec2  bumpTexCoord;
varying vec2  vWorldXZ;            // world-space horizontal position (seam-free noise)
varying float foamAmount;          // Jacobian foam from CPU (vertex colour alpha)
varying float vWaveHeight;         // vertical height from the vertex shader

// ============================================================================
//  SEA COLOUR - measured off your reference screenshot
// ============================================================================
// WATER_BODY  = looking straight DOWN into the water (NdotV -> 1)
// WATER_GRAZE = looking out towards the horizon (NdotV -> 0)
//
// Presets - swap the two lines below:
//   Reference blue (default) : (0.094, 0.325, 0.467) / (0.078, 0.280, 0.405)
//   A little deeper overall  : (0.078, 0.290, 0.430) / (0.062, 0.245, 0.365)
//   Brighter / tropical      : (0.115, 0.365, 0.510) / (0.098, 0.320, 0.450)
const vec3 WATER_BODY  = vec3(0.094, 0.325, 0.467);
const vec3 WATER_GRAZE = vec3(0.078, 0.280, 0.405);

const float WATER_BRIGHTNESS = 1.00; // hue is above; this is the level knob

// ---- Distance darkening - the "far water darker" control ----
//   1.00 = off, 0.60 = default, 0.45 = strong deep-water look offshore
const float FAR_DARKEN       = 1.00; // KYARA EAU: 1.0 = far water same colour as near water
const float FAR_DARKEN_RANGE = 900.0;

const float SUN_SCATTER  = 0.30;                    // how strongly the sea takes the sun's colour
const vec3  SSS_COLOUR   = vec3(0.08, 0.46, 0.44);  // glow through thin wave crests
const float SSS_STRENGTH = 0.30;
const float TROUGH_SHADE = 0.92;  // brightness at the bottom of a trough (1.0 = off)
const float CREST_LIFT   = 1.05;  // brightness at the top of a crest

// ============================================================================
//  RIPPLE SHIMMER - the all-directions replacement for relying on the sun glint
// ============================================================================
// SHIMMER_GAIN is the main knob. It is a symmetric brightness swing around the flat
// water value, so raising it adds contrast WITHOUT making the sea lighter or darker
// on average.
//   0.00 = off (Tier 7 behaviour, flat outside the sun path)
//   0.60 = default
//   0.90 = strong, very lively surface
// SKY_GLASS additionally tints the skyward-tilted ripples towards sky colour, which
// is what gives them that glassy look. It only acts on the bright side, so its effect
// on average colour is small.
const float SHIMMER_GAIN = 0.60;
const float SKY_GLASS    = 0.25;

const vec3 SKY_HORIZON = vec3(0.60, 0.72, 0.86); // sky colour near the horizon
const vec3 SKY_ZENITH  = vec3(0.28, 0.48, 0.80); // sky colour overhead

// ============================================================================
//  REFLECTION
// ============================================================================
const float REFLECTION_STRENGTH = 0.55; // overall mirror amount
const float FRESNEL_F0          = 0.02; // water's true reflectance looking straight down
const float FRESNEL_MAX         = 0.50; // HARD CAP. Stops the RTT taking over the horizon.
const float REFL_DISTORT        = 0.09; // how far ripples bend the mirrored image
const float REFL_CREST_BIAS     = 0.80; // extra distortion on crests vs troughs
const float REFL_ROUGHNESS      = 0.55; // 2-tap blur width, scaled by sea state
const float REFL_HAZE_RANGE     = 260.0;// metres over which the RTT fades to the analytic sky
// KYARA EAU: how much of the reflection takes the WATER's hue (brightness kept).
//   0.0 = reflection is the true grey/white of the sky (sea goes grey under cloud)
//   0.35 = default, 0.6 = sea stays strongly blue whatever the sky
const float REFL_WATER_TINT     = 0.35;
const float SKY_RTT_MIX         = 0.45; // how much of the reflection is the analytic sky
                                        //   rather than the render target. Higher = less
                                        //   dependent on the RTT, more even across the sea.

// ============================================================================
//  SURFACE TEXTURE
// ============================================================================
const float TILING          = 12.0;  // base ripple size. Lower = bigger ripples.
                                     // KYARA EAU: keep RIPPLE_UV_PER_METRE*TILING*1000 a whole number (now 375)
const float SCROLL_SPEED    = 0.04;
const float BUMP_STRENGTH   = 1.10;  // master ripple depth
const float DETAIL_STRENGTH = 0.85;  // weight of the 3rd (fine crest-detail) layer
const float MICRO_STRENGTH  = 0.45;  // weight of the 4th (very fine, near-field only) layer
const float MICRO_RANGE     = 120.0; // metres; beyond this the 4th layer would only alias
const float RIPPLE_RANGE    = 1200.0;// metres over which ripple depth tapers off
// KYARA EAU: ripple depth tapers to this instead of to 0 - distant water keeps texture.
const float RIPPLE_FAR_FLOOR = 0.45;

// KYARA EAU: ripple UVs now come from WORLD position. 0.03125 per metre = exactly the old
// ripple size with water_segments = 32. The layer scales below are fractions of 375 so the
// pattern repeats exactly every 1000 m: the scene is re-centred in 1000 m steps, and this
// keeps the ripples from jumping when that happens. Keep the /375.0 form if you retune.
const float RIPPLE_UV_PER_METRE = 0.03125;
const float SCALE_B   = 649.0  / 375.0;  // was 1.73
const float SCALE_C   = 1613.0 / 375.0;  // was 4.30
const float SCALE_D   = 3413.0 / 375.0;  // was 9.10
const float SCALE_F1  = 45.0   / 375.0;  // far layer, ~22 m ripples
const float SCALE_F2  = 26.0   / 375.0;  // far layer, ~38 m ripples
const float FAR_LAYER_STRENGTH = 0.00;   // 0 = no extra detail on distant water. KYARA EAU: 0 with the
                                         // photo-ripple look (sampling the photo large tilts far water); 1.0 with the normal map
const float FAR_LAYER_START    = 150.0;  // metres: far layers fade in from here...
const float FAR_LAYER_END      = 900.0;  // ...to full strength here

// KYARA EAU: the animated (FFT) sea stops ~1000 m from the camera and a FLAT mesh takes
// over. Its wave normal and crest shading fade out over this range so the join is invisible.
const float GEOM_FADE_START = 700.0;
const float GEOM_FADE_END   = 950.0;

// How far out the surface normal is allowed to keep its shape. Used to be hard-tied to
// 500m, which flattened everything beyond that into a perfect plane.
//   NORMAL_FLATTEN_FLOOR is how much perturbation survives at maximum range.
//   0.0 = fully flat (old behaviour), 0.25 = default, higher = more distant detail.
// If the horizon starts to crawl or sparkle between frames, LOWER the floor.
const float NORMAL_FLATTEN_RANGE = 1400.0;
const float NORMAL_FLATTEN_FLOOR = 0.40; // KYARA EAU: was 0.25

// Wind streaks: large stretched areas of calm and ruffle drifting downwind.
const float PATCH_SCALE    = 0.020;  // world units; lower = bigger patches
const float PATCH_STRETCH  = 3.0;    // elongation along the wind
const float PATCH_CONTRAST = 0.30;   // 0.0 = perfectly uniform surface

// ============================================================================
//  HIGHLIGHTS AND FOAM
// ============================================================================
const float GLINT_STRENGTH = 0.90;   // tight sparkle on individual ripples
const float SHEEN_STRENGTH = 0.18;   // broad sun path across the swell
const float SHEEN_POWER    = 10.0;   // LOWER = wider sun path. Was 22, which was tight.
const float SPEC_RANGE     = 1200.0; // metres; specular used to die at 500

// KYARA EAU - SUN GLITTER PATH. Far away, each pixel covers many ripples whose slopes the
// texture can no longer show (the mipmaps average them flat), so the sun only hit one spot
// under the sun and the path stopped short. This lobe stands in for those unseen slopes: it
// widens with distance, so the path stretches out toward the horizon, and the visible
// ripples break it into flecks where they are still resolved.
// With the sun ~45 deg up (SUN_DIR), the far path needs a slope spread of ~0.25-0.30 to show.
//   GLITTER_STRENGTH   0 = off, 0.60 = default, 0.9 = very bright path
//   GLITTER_SIGMA_FAR  how far/wide the path reaches. 0.22 short, 0.30 default, 0.36 long
//   GLITTER_RANGE      metres over which the spread grows from NEAR to FAR
const float GLITTER_STRENGTH   = 0.00;   // KYARA EAU: 0 with the photo-ripple look (the photo already
                                         // carries the sparkle far out); 0.60 with the normal map
const float GLITTER_SIGMA_NEAR = 0.20;
const float GLITTER_SIGMA_FAR  = 0.30;
const float GLITTER_RANGE      = 300.0;
const float GLITTER_SPARKLE    = 8.0;   // how strongly ripples break the path into flecks
const float FOAM_STRENGTH  = 0.45;
const float FOAM_NOISE_SCALE = 0.35;
const float FOAM_DRIFT       = 0.06;

// Sun direction, world space. Must match the scene's sun or the glints land in the
// wrong place. Kept const because a uniform the C++ never sets would silently zero.
const vec3 SUN_DIR = vec3(0.45, 0.62, 0.35);

// Applied to the tangent-space NORMAL of layers B and D, never to their UVs - rotating
// a UV breaks the texture's tiling and puts a hard seam at every mesh tile boundary.
const mat2 ROT = mat2(0.80, -0.60, 0.60, 0.80);

// sin-free hash - the old valueNoise cost 8 sin() per fragment.
float hash(vec2 p){
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}
float valueNoise(vec2 p){
    vec2 i=floor(p), f=fract(p);
    float a=hash(i), b=hash(i+vec2(1,0)), c=hash(i+vec2(0,1)), d=hash(i+vec2(1,1));
    vec2 u=f*f*(3.0-2.0*f);
    return mix(mix(a,b,u.x), mix(c,d,u.x), u.y);
}

// KYARA EAU - FOAM EDGES. The foam inputs (Jacobian foam, wave height, wave normal) only
// exist at the mesh vertices, ~3 m apart, and are blended in straight lines across each
// triangle - so the foam/crest bands had saw-tooth triangle edges. The edge is now decided
// per pixel against a small noise pattern, so it follows a ragged, organic line instead.
//   FOAM_EDGE_BREAKUP  0 = old triangle edges, 1 = fully noise-shaped edges
//   FOAM_EDGE_SOFT     width of the edge blend (higher = softer, less defined foam)
//   FOAM_EDGE_SCALE    noise frequency per metre (higher = finer, lacier edge)
const float FOAM_EDGE_BREAKUP = 1.0;
const float FOAM_EDGE_SOFT    = 0.22;
const float FOAM_EDGE_SCALE   = 0.8;

// Per-pixel edge noise (0..1). Faded out with distance, where it would only shimmer.
float foamEdgeNoise(vec2 xz, float t, float dist)
{
    float en = valueNoise(xz * FOAM_EDGE_SCALE + vec2(t * 0.15, t * 0.10)) * 0.6
             + valueNoise(xz * (FOAM_EDGE_SCALE * 2.6) - vec2(t * 0.12) + 7.0) * 0.4;
    float amt = (1.0 - smoothstep(150.0, 500.0, dist)) * FOAM_EDGE_BREAKUP;
    return mix(0.05, 0.05 + 0.55 * en, amt);    // the threshold the foam field must reach
}

// Analytic sky colour looking along direction d. Used both as a reflection source and
// as the shimmer reference, so the two always agree.
vec3 skyAlong(vec3 d, vec3 tint, float level)
{
    float upness = clamp(d.y, 0.0, 1.0);
    vec3 skyH = mix(SKY_HORIZON, STORM_SKY_HORIZON, gStorm);   // KYARA METEO: grey overcast sky
    vec3 skyZ = mix(SKY_ZENITH,  STORM_SKY_ZENITH,  gStorm);
    return mix(skyH, skyZ, pow(upness, 0.7)) * tint * max(level, 0.15);
}

// KYARA EAU: sun glitter path. nMean = the surface without ripples, n = with ripples.
float glitterPath(vec3 nMean, vec3 n, vec3 h, float dist, float sea)
{
    float c     = clamp(dot(nMean, h), 0.05, 1.0);
    float tan2  = (1.0 - c * c) / (c * c);
    float sigma = mix(GLITTER_SIGMA_NEAR, GLITTER_SIGMA_FAR, smoothstep(0.0, GLITTER_RANGE, dist))
                * clamp(0.8 + sea / 8.0, 0.8, 1.3);          // rougher sea = wider path
    float lobe  = exp(-tan2 / (2.0 * sigma * sigma));
    float fleck = clamp(0.5 + (dot(n, h) - dot(nMean, h)) * GLITTER_SPARKLE, 0.0, 1.0);
    return lobe * (0.35 + 0.65 * fleck);
}

// KYARA EAU: give a reflected colour the water's hue, keeping its brightness.
vec3 waterTinted(vec3 c)
{
    const vec3 LUMA = vec3(0.299, 0.587, 0.114);
    vec3 body = mix(WATER_BODY, STORM_BODY, gStorm);            // KYARA METEO
    vec3 hue = body / dot(body, LUMA);
    return mix(c, dot(c, LUMA) * hue, REFL_WATER_TINT * (1.0 - gStorm)); // storm: reflect the grey sky as it is
}

void main()
{
    gStorm = smoothstep(0.0, 1.0, clamp(gloom, 0.0, 1.0)); // KYARA METEO
    float z = gl_FragCoord.z / gl_FragCoord.w;

    // exp2 fog, reading the SAME density the engine set via setFog so the sea hazes
    // at the same rate as the sky and the ships.
    float fd = gl_Fog.density * z;
    float fogFactor    = clamp(exp(-(fd * fd)), 0.0, 1.0);
    float distanceFade = clamp(1.0 - z / 500.0,           0.0, 1.0);
    float rippleFade   = mix(1.0, RIPPLE_FAR_FLOOR, clamp(z / RIPPLE_RANGE, 0.0, 1.0)); // KYARA EAU: floor
    float farMix       = smoothstep(FAR_LAYER_START, FAR_LAYER_END, z);                  // KYARA EAU
    float geomFade     = smoothstep(GEOM_FADE_START, GEOM_FADE_END, gl_FogFragCoord);    // KYARA EAU (radial distance)
    float waveH        = vWaveHeight * (1.0 - geomFade);                                // KYARA EAU
    float microFade    = clamp(1.0 - z / MICRO_RANGE,     0.0, 1.0);
    float reflFade     = clamp(1.0 - z / 600.0,           0.0, 1.0);
    float specFade     = clamp(1.0 - z / SPEC_RANGE,      0.0, 1.0);

    // ---------- Surface normal: four de-correlated ripple layers ----------
    // KYARA EAU: world-space UVs (was bumpTexCoord, which restarted at every 100 m tile
    // and has unknown/huge UVs on the far flat mesh - so distant water got no ripples).
    vec2 uv = vWorldXZ * (RIPPLE_UV_PER_METRE * TILING);

    vec3 nA = texture2D(baseMap, uv           + vec2( 0.80, 1.00) * time * SCROLL_SPEED      ).rgb * 2.0 - 1.0;
    vec3 nB = texture2D(baseMap, uv * SCALE_B + vec2(-1.10, 0.60) * time * SCROLL_SPEED      ).rgb * 2.0 - 1.0;
    vec3 nC = texture2D(baseMap, uv * SCALE_C + vec2( 0.50,-0.90) * time * SCROLL_SPEED * 2.0).rgb * 2.0 - 1.0;
    vec3 nD = texture2D(baseMap, uv * SCALE_D + vec2(-0.30, 0.70) * time * SCROLL_SPEED * 3.5).rgb * 2.0 - 1.0;
    // KYARA EAU: large, slow layers that stay visible where the fine ones have blurred out
    vec3 nF1 = texture2D(baseMap, uv * SCALE_F1 + vec2( 0.30, 0.50) * time * SCROLL_SPEED * 0.25).rgb * 2.0 - 1.0;
    vec3 nF2 = texture2D(baseMap, uv * SCALE_F2 + vec2(-0.45, 0.20) * time * SCROLL_SPEED * 0.20).rgb * 2.0 - 1.0;

    // De-correlate by turning the slope vectors, NOT the sample coordinates.
    nB.xy  = ROT * nB.xy;
    nD.xy  = ROT * nD.xy;
    nF2.xy = ROT * nF2.xy;

    vec3 nT = normalize(nA + nB * 0.90
                           + nC * DETAIL_STRENGTH
                           + nD * MICRO_STRENGTH * microFade
                           + (nF1 + nF2 * 0.80) * FAR_LAYER_STRENGTH * farMix);
    vec3 rippleWorld = vec3(nT.x, nT.z, nT.y);

    // Wind streaks, driven from WORLD position so they never break at a tile edge.
    vec2  patchUV = vWorldXZ * PATCH_SCALE * vec2(1.0, PATCH_STRETCH)
                  + vec2(time * 0.020, time * 0.012);
    float pn    = valueNoise(patchUV) * 0.6 + valueNoise(patchUV * 2.7 + 5.0) * 0.4;
    float patch = mix(1.0 - PATCH_CONTRAST, 1.0 + PATCH_CONTRAST * 0.6,
                      smoothstep(0.25, 0.75, pn));

    // the wave normal without the ripple detail - KYARA EAU: faded to flat before the flat mesh starts
    vec3 Nflat = normalize(mix(normalize(Normal), vec3(0.0, 1.0, 0.0), geomFade));
    float bump = BUMP_STRENGTH * clamp(0.4 + seaState / 8.0, 0.0, 1.4) * rippleFade * patch;
    vec3 N = normalize(Nflat + vec3(rippleWorld.x, 0.0, rippleWorld.z) * bump);

    vec3 V = normalize(-ViewDirection);

    // Taper the detail with distance, but never all the way to a perfect plane.
    float normalKeep = mix(1.0, NORMAL_FLATTEN_FLOOR, clamp(z / NORMAL_FLATTEN_RANGE, 0.0, 1.0));
    N = normalize(mix(Nflat, N, normalKeep));
    float NdotV = clamp(dot(N, V), 0.0, 1.0);

    // ---------- Body colour ----------
    // 'warmth' is how far the light has gone red: 0 under neutral daylight, rising
    // towards sunset. The scatter term matters because the base colour is nearly
    // red-free, so a pure multiply could never make the sea look warm.
    float warmth  = clamp(lightColour.r - lightColour.b, 0.0, 1.0);
    vec3  scatter = lightColour * SUN_SCATTER * warmth * (1.0 - gStorm); // KYARA METEO: no sun, no warm glow

    // KYARA METEO: fair-weather blue -> storm slate grey-green
    vec3 waterGraze = mix(WATER_GRAZE, STORM_GRAZE, gStorm);
    vec3 waterBody  = mix(WATER_BODY,  STORM_BODY,  gStorm);
    vec3 deepColor    = (waterGraze * lightColour + scatter * 0.6) * lightLevel * WATER_BRIGHTNESS;
    vec3 shallowColor = (waterBody  * lightColour + scatter      ) * lightLevel * WATER_BRIGHTNESS;
    vec3 bodyColor    = mix(deepColor, shallowColor, NdotV);

    // ---------- Reflection ----------
    // Schlick, then HARD CAPPED. Without the cap, grazing angles hand the pixel over
    // entirely to the RTT and the distant sea goes dark and blotchy.
    float f5      = pow(1.0 - NdotV, 5.0);
    float fresnel = min(FRESNEL_F0 + (1.0 - FRESNEL_F0) * f5, FRESNEL_MAX) * REFLECTION_STRENGTH;

    // Where the ripple actually points, and where flat water would have pointed.
    vec3 R      = reflect(-V, N);
    vec3 Rflat  = reflect(-V, Nflat);
    vec3 skyRefl = waterTinted(skyAlong(R, lightColour, lightLevel)); // KYARA EAU: tinted

    // Crests bend the mirrored sky further than troughs, so the reflection rides the swell.
    float crest   = smoothstep(-0.5, 2.0, waveH);
    vec2  distort = N.xz * REFL_DISTORT * reflFade * (1.0 - REFL_CREST_BIAS * 0.5 + REFL_CREST_BIAS * crest);

    vec2 baseUV = reflectionMapTexCoord.xy / reflectionMapTexCoord.z;

    // Two taps, separated more as the sea roughens - a cheap blur. Rough water
    // scatters its reflection; it does not lose it.
    float rough = REFL_ROUGHNESS * clamp(seaState / 8.0, 0.0, 1.0) * reflFade;
    vec2  perp  = vec2(-distort.y, distort.x);
    vec2  uv1   = clamp(baseUV + distort, 0.0, 1.0);
    vec2  uv2   = clamp(baseUV + distort * (1.0 + 2.0 * rough) + perp * rough * 1.5, 0.0, 1.0);

    vec3 reflectionColour = mix(texture2D(reflectionMap, uv1).rgb,
                                texture2D(reflectionMap, uv2).rgb, 0.5);

    // Blend in the analytic sky so the reflection does not depend entirely on the RTT,
    // which only covers part of the scene and degenerates towards its clamped edges.
    reflectionColour = mix(reflectionColour, skyRefl, SKY_RTT_MIX);
    float skyBlend = clamp(z / REFL_HAZE_RANGE, 0.0, 1.0);
    // KYARA EAU: was gl_Fog.color - a near-grey that turned all far water pale grey
    reflectionColour = mix(reflectionColour, skyRefl, skyBlend);
    reflectionColour = waterTinted(reflectionColour);
    // Only a light tint towards water colour in very heavy seas.
    reflectionColour = mix(reflectionColour, deepColor, clamp(seaState / 30.0, 0.0, 1.0));

    vec3 color = mix(bodyColor, reflectionColour, fresnel);

    // ---------- Ripple shimmer, in every direction ----------
    // How much more (or less) sky this ripple is facing than flat water would be.
    // Zero on flat water by construction, so this adds contrast without shifting the
    // average colour of the sea.
    float shimmer = clamp(R.y, 0.0, 1.0) - clamp(Rflat.y, 0.0, 1.0);
    shimmer = clamp(shimmer, -1.0, 1.0) * rippleFade;

    color *= 1.0 + shimmer * SHIMMER_GAIN;
    // Skyward-tilted ripples also pick up sky colour - the glassy look.
    color = mix(color, skyRefl, max(shimmer, 0.0) * SKY_GLASS);

    // ---------- Wave volume ----------
    color *= mix(TROUGH_SHADE, CREST_LIFT, smoothstep(-1.5, 1.5, waveH));

    // Subsurface scattering: light through the thin part of a crest, strongest when
    // looking edge-on at a raised wave.
    float sss = smoothstep(0.2, 2.5, waveH) * pow(1.0 - NdotV, 2.0);
    color += SSS_COLOUR * sss * lightColour * lightLevel * SSS_STRENGTH * distanceFade
           * (1.0 - 0.7 * gStorm); // KYARA METEO: little light through the crests under cloud

    // ---------- Distance darkening ----------
    // Applied to the finished water colour, reflection included, so offshore water
    // goes evenly deeper instead of patchily. Before fog, so the horizon still hazes
    // into the sky normally.
    color *= mix(1.0, FAR_DARKEN, clamp(z / FAR_DARKEN_RANGE, 0.0, 1.0));

    // KYARA METEO: storm - duller and darker (before foam and sun, which stay as they are)
    {
        float lum = dot(color, vec3(0.299, 0.587, 0.114));
        color = mix(color, vec3(lum), STORM_DESATURATE * gStorm) * mix(1.0, STORM_DARKEN, gStorm);
    }

    // ---------- Specular: two lobes ----------
    vec3  sunDir = normalize(SUN_DIR);
    vec3  H      = normalize(sunDir + V);
    float NdotH  = max(dot(N, H), 0.0);
    float glint  = pow(NdotH, 220.0);         // tight sparkle on individual ripples
    float sheen  = pow(NdotH, SHEEN_POWER);   // broad sun path across the swell

    // KYARA EAU: + glitter path. No specFade on it - it is meant to reach the horizon;
    // the fog at the end still fades it in poor visibility.
    float glitter = glitterPath(Nflat, N, H, z, seaState) * GLITTER_STRENGTH;

    vec3 specular = vec3(1.0, 0.96, 0.86) * lightColour * lightLevel
                  * ((glint * GLINT_STRENGTH + sheen * SHEEN_STRENGTH) * specFade + glitter)
                  * mix(1.0, STORM_SUN, gStorm); // KYARA METEO: overcast hides the sun

    // ---------- Foam ----------
    float seaStateGate = clamp(seaState / 5.0, 0.0, 1.0); // no foam in calm seas

    // World-space, so the foam breakup does not break at tile edges either.
    vec2  noiseUV = vWorldXZ * (0.02 / FOAM_NOISE_SCALE)
                  + vec2(time * FOAM_DRIFT, time * FOAM_DRIFT * 0.7);
    float fn = clamp(valueNoise(noiseUV) * 0.65 + valueNoise(noiseUV * 2.3 + 11.0) * 0.35, 0.0, 1.0);
    float patchiness = smoothstep(0.30, 0.85, fn);

    float jacobianFoam = foamAmount;                        // physics foam from the CPU
    float heightMask   = smoothstep(0.5, 2.0, waveH);       // erase foam in the troughs
    float slopeFoam    = smoothstep(0.35, 0.60, 1.0 - N.y) * heightMask;

    float foamField = max(jacobianFoam, slopeFoam) * heightMask * seaStateGate;
    // KYARA EAU: per-pixel ragged edge instead of the triangle-shaped one
    float edgeT     = foamEdgeNoise(vWorldXZ, time, z);
    float foam = smoothstep(edgeT, edgeT + FOAM_EDGE_SOFT, foamField)
               * mix(0.55, 1.0, patchiness);
    foam = clamp(foam * mix(1.0, STORM_FOAM_BOOST, gStorm), 0.0, 1.0) * FOAM_STRENGTH; // KYARA METEO: + storm foam

    // Foam is white, but it is lit by the sun, so it warms up at dusk too.
    vec3 foamColor = mix(vec3(1.0), lightColour, 0.7) * (0.9 + 0.5 * lightLevel);
    color = mix(color, foamColor, foam * distanceFade);

    color += specular * (1.0 - foam * 0.6); // foam is matte

    vec3 final = mix(gl_Fog.color.rgb, color, fogFactor);
    gl_FragColor = vec4(final, 1.0);
}
