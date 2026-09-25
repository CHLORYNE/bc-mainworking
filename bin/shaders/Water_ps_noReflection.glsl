#version 130

/* Bridge Command - water fragment shader, NO-REFLECTION fallback path.

   KYARA: this runs when the reflection RTT is off. It has no mirror to draw from, so
   it fakes one: a two-tone analytic sky sampled through the reflected view vector,
   mixed in by a capped Schlick Fresnel, plus the same all-directions ripple shimmer
   as the main shader.

   Ripple normals are PROCEDURAL - two octaves of value noise, differentiated to get a
   slope, driven from world XZ so they cannot break at a tile boundary. It does not
   sample the bump texture on purpose: this material is not guaranteed to have a
   texture bound on unit 0, and an unbound sampler returns black, which would flatten
   the whole surface. Procedural costs a little more ALU but it cannot silently fail.

   'time' is declared as a uniform. If the C++ does not set it on this material the
   value is 0 and the ripples simply sit still - the surface still has texture, it
   just does not animate. Nothing breaks.

   Colours, FAR_DARKEN, SHIMMER_GAIN and the Fresnel cap are kept in step with the
   main Water_ps.glsl so switching paths does not change the look of the sea. If you
   retune one file, retune the other.                                              */

uniform float lightLevel;
uniform float seaState;
uniform float time;

varying vec3  Normal;
varying vec3  ViewDirection;
varying vec2  vWorldXZ;
varying float vWaveHeight;

const vec3 SUN_DIR = vec3(0.45, 0.62, 0.35);

// ----- Sea colour: keep these matching Water_ps.glsl -----
const vec3  WATER_BODY  = vec3(0.094, 0.325, 0.467);
const vec3  WATER_GRAZE = vec3(0.078, 0.280, 0.405);
const float WATER_BRIGHTNESS = 1.00;

// ----- Distance darkening: keep matching Water_ps.glsl -----
const float FAR_DARKEN       = 0.60;
const float FAR_DARKEN_RANGE = 900.0;

// ----- Ripple shimmer: keep matching Water_ps.glsl -----
const float SHIMMER_GAIN = 0.60;
const float SKY_GLASS    = 0.25;

// ----- Analytic sky, used as the reflection source -----
const vec3 SKY_HORIZON = vec3(0.60, 0.72, 0.86);
const vec3 SKY_ZENITH  = vec3(0.28, 0.48, 0.80);

// ----- Surface texture -----
const float RIPPLE_SCALE  = 0.55;   // higher = smaller ripples
const float RIPPLE_SPEED  = 0.35;
const float BUMP_STRENGTH = 0.35;   // master ripple depth for this path
const float RIPPLE_RANGE  = 1200.0; // metres; ripple depth tapers out to here

const float NORMAL_FLATTEN_RANGE = 1400.0;
const float NORMAL_FLATTEN_FLOOR = 0.25; // lower this if the horizon crawls

const float REFLECTION_STRENGTH = 0.55;
const float FRESNEL_F0          = 0.02;
const float FRESNEL_MAX         = 0.50;
const float GLINT_STRENGTH      = 0.80;
const float SHEEN_STRENGTH      = 0.18;
const float SHEEN_POWER         = 10.0;  // lower = wider sun path
const float SPEC_RANGE          = 1200.0;
const float TROUGH_SHADE        = 0.92;
const float CREST_LIFT          = 1.05;

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

// Two octaves drifting in different directions, so they never lock into a pattern.
float rippleHeight(vec2 p)
{
    float h  = valueNoise(p              + vec2( 0.80, 1.00) * time * RIPPLE_SPEED) * 0.65;
          h += valueNoise(p * 2.30 + 7.0 + vec2(-1.10, 0.60) * time * RIPPLE_SPEED) * 0.35;
    return h;
}

vec3 skyAlong(vec3 d, float level)
{
    float upness = clamp(d.y, 0.0, 1.0);
    return mix(SKY_HORIZON, SKY_ZENITH, pow(upness, 0.7)) * max(level, 0.15);
}

void main()
{
    float z = gl_FragCoord.z / gl_FragCoord.w;

    float distanceSmoothing = clamp(1.0 - z / 300.0,        0.0, 1.0);
    float rippleFade        = clamp(1.0 - z / RIPPLE_RANGE, 0.0, 1.0);
    float specFade          = clamp(1.0 - z / SPEC_RANGE,   0.0, 1.0);
    float lv = clamp(lightLevel, 0.55, 1.0);

    // ---------- Procedural ripple normal ----------
    // Forward difference on the noise field gives the surface slope.
    vec2  p   = vWorldXZ * RIPPLE_SCALE;
    float eps = 0.35;
    float h0  = rippleHeight(p);
    float hx  = rippleHeight(p + vec2(eps, 0.0));
    float hz  = rippleHeight(p + vec2(0.0, eps));
    vec2  slope = vec2(hx - h0, hz - h0) / eps;

    vec3 Nflat = normalize(Normal);
    if (Nflat.y < 0.0) Nflat = -Nflat;

    float bump = BUMP_STRENGTH * clamp(0.4 + seaState / 8.0, 0.0, 1.4) * rippleFade;
    vec3  N = normalize(Nflat + vec3(slope.x, 0.0, slope.y) * bump);

    // Taper the detail with distance, but never all the way to a perfect plane.
    float normalKeep = mix(1.0, NORMAL_FLATTEN_FLOOR, clamp(z / NORMAL_FLATTEN_RANGE, 0.0, 1.0));
    N = normalize(mix(Nflat, N, normalKeep));

    vec3 V = normalize(-ViewDirection);
    vec3 L = normalize(SUN_DIR);
    vec3 H = normalize(L + V);

    float NdotV = clamp(dot(N, V), 0.0, 1.0);

    // ---------- Body colour ----------
    float ndl   = max(dot(N, L), 0.0);
    float shade = mix(0.80, 1.08, ndl);
    shade = mix(1.0, shade, distanceSmoothing);

    vec3 deepColor = WATER_GRAZE * WATER_BRIGHTNESS;   // towards the horizon
    vec3 bodyColor = mix(deepColor, WATER_BODY * WATER_BRIGHTNESS, NdotV) * shade * lv;

    // ---------- Fake reflection ----------
    vec3 R     = reflect(-V, N);
    vec3 Rflat = reflect(-V, Nflat);
    vec3 skyRefl = skyAlong(R, lv) * lv;

    // Rough seas scatter the reflection towards the water's own colour.
    vec3 skyColor = mix(skyRefl, deepColor * lv, clamp(seaState / 24.0, 0.0, 1.0));

    float f5      = pow(1.0 - NdotV, 5.0);
    float fresnel = min(FRESNEL_F0 + (1.0 - FRESNEL_F0) * f5, FRESNEL_MAX) * REFLECTION_STRENGTH;

    vec3 color = mix(bodyColor, skyColor, fresnel);

    // ---------- Ripple shimmer, in every direction ----------
    // Zero on flat water by construction, so it adds contrast without shifting the
    // average colour of the sea.
    float shimmer = clamp(R.y, 0.0, 1.0) - clamp(Rflat.y, 0.0, 1.0);
    shimmer = clamp(shimmer, -1.0, 1.0) * rippleFade;

    color *= 1.0 + shimmer * SHIMMER_GAIN;
    color = mix(color, skyRefl, max(shimmer, 0.0) * SKY_GLASS);

    // ---------- Wave volume ----------
    color *= mix(TROUGH_SHADE, CREST_LIFT, smoothstep(-1.5, 1.5, vWaveHeight));

    // ---------- Distance darkening (matches the main shader) ----------
    color *= mix(1.0, FAR_DARKEN, clamp(z / FAR_DARKEN_RANGE, 0.0, 1.0));

    // ---------- Specular: two lobes ----------
    float NdotH = max(dot(N, H), 0.0);
    float glint = pow(NdotH, 180.0);
    float sheen = pow(NdotH, SHEEN_POWER);
    color += vec3(1.0, 0.97, 0.90) * lv * specFade
           * (glint * GLINT_STRENGTH + sheen * SHEEN_STRENGTH);

    // ---------- Foam ----------
    float heightMask = smoothstep(0.5, 2.0, vWaveHeight);
    float steepness  = 1.0 - clamp(N.y, 0.0, 1.0);
    float foamAmount = smoothstep(0.18, 0.45, steepness)
                     * heightMask * clamp(seaState / 8.0, 0.0, 1.0);
    color = mix(color, vec3(0.92, 0.95, 0.98) * lv, foamAmount * 0.55);

    vec4 outputColour = vec4(color, 1.0);

    // ---------- Scene-matched linear fog ----------
    // Same fog start/end/colour that driver->setFog() pushes into the GL fog state, so
    // the sea hazes at exactly the same distance and tint as the ships, terrain and
    // sky. gl_FogFragCoord (eye-space distance) is written by the vertex shader.
    float fogFactor = clamp((gl_Fog.end - gl_FogFragCoord) / (gl_Fog.end - gl_Fog.start), 0.0, 1.0);
    gl_FragColor = mix(vec4(gl_Fog.color.rgb, 1.0), outputColour, fogFactor);
}
