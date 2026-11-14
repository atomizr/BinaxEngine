#version 460 core

in  vec2 v_UV;
out vec4 FragColor;

uniform sampler2D      u_DepthTexture;
uniform sampler2DArray u_ShadowMap;
uniform mat4           u_InvViewProj;
uniform mat4           u_ViewMatrix;
uniform vec3           u_CameraPosition;

uniform mat4  u_LightSpaceMatrices[4];
uniform float u_CascadeSplits[4];
uniform int   u_CascadeCount;
uniform int   u_ShadowMode;
uniform float u_MaxShadowDistance;

uniform vec3  u_SunDirection;
uniform vec3  u_SunColor;
uniform float u_SunIntensity;

uniform vec3  u_FogColor;
uniform float u_FogIntensity;
uniform float u_FogDensity;
uniform float u_FogMinHeight;
uniform float u_FogMaxHeight;
uniform float u_FogDistance;
uniform int   u_FogSteps;
uniform float u_Scattering;

// ---- Noise ---------------------------------------------------------------
uniform int   u_NoiseEnabled;
uniform float u_NoiseSpeed;
uniform float u_NoiseScale;
uniform float u_NoiseIntensity;
uniform float u_Time;

const float PI = 3.14159265359;

vec3 ReconstructWorldPos(vec2 uv, float depth)
{
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 world = u_InvViewProj * clip;
    return world.xyz / world.w;
}

float SampleShadowCSM(vec3 worldPos, float viewZ)
{
    if (u_ShadowMode == 0 || u_CascadeCount == 0) return 1.0;

    int cascadeIdx = u_CascadeCount - 1;
    for (int i = 0; i < u_CascadeCount; ++i)
    {
        if (viewZ < u_CascadeSplits[i]) { cascadeIdx = i; break; }
    }

    vec4 lsPos = u_LightSpaceMatrices[cascadeIdx] * vec4(worldPos, 1.0);
    vec3 projCoords = lsPos.xyz / lsPos.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0 ||
        projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0)
        return 1.0;

    float storedDepth = texture(u_ShadowMap, vec3(projCoords.xy, float(cascadeIdx))).r;
    return (projCoords.z > storedDepth + 0.005) ? 0.0 : 1.0;
}

float HG(float cosTheta, float g)
{
    float g2 = g * g;
    float denom = max(1.0 + g2 - 2.0 * g * cosTheta, 1e-4);
    return (1.0 - g2) / (4.0 * PI * pow(denom, 1.5));
}

// ============================================================================
// 3D value noise + 2 octave FBM
// ============================================================================
float hash31(vec3 p)
{
    p = fract(p * 0.3183099 + vec3(0.1, 0.2, 0.3));
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float noise3D(vec3 p)
{
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    float n000 = hash31(i + vec3(0.0, 0.0, 0.0));
    float n100 = hash31(i + vec3(1.0, 0.0, 0.0));
    float n010 = hash31(i + vec3(0.0, 1.0, 0.0));
    float n110 = hash31(i + vec3(1.0, 1.0, 0.0));
    float n001 = hash31(i + vec3(0.0, 0.0, 1.0));
    float n101 = hash31(i + vec3(1.0, 0.0, 1.0));
    float n011 = hash31(i + vec3(0.0, 1.0, 1.0));
    float n111 = hash31(i + vec3(1.0, 1.0, 1.0));

    float nx00 = mix(n000, n100, f.x);
    float nx10 = mix(n010, n110, f.x);
    float nx01 = mix(n001, n101, f.x);
    float nx11 = mix(n011, n111, f.x);

    float nxy0 = mix(nx00, nx10, f.y);
    float nxy1 = mix(nx01, nx11, f.y);

    return mix(nxy0, nxy1, f.z);
}

float fbm2(vec3 p)
{
    float sum  = 0.5  * noise3D(p);
    sum       += 0.25 * noise3D(p * 2.0);
    return sum / 0.75;   // нормируем к [0, 1]
}

void main()
{
    vec4 clip = vec4(v_UV * 2.0 - 1.0, 1.0, 1.0);
    vec4 farWorld = u_InvViewProj * clip;
    vec3 rayDir = normalize(farWorld.xyz / farWorld.w - u_CameraPosition);

    float sceneDepth = texture(u_DepthTexture, v_UV).r;
    float sceneDist = u_FogDistance;
    if (sceneDepth < 0.99999)
    {
        vec3 scenePos = ReconstructWorldPos(v_UV, sceneDepth);
        sceneDist = min(length(scenePos - u_CameraPosition), u_FogDistance);
    }

    float cosTheta = dot(rayDir, -normalize(u_SunDirection));
    float phase    = HG(cosTheta, u_Scattering);

    int   steps    = max(u_FogSteps, 4);
    float stepSize = sceneDist / float(steps);

    vec3 accum = vec3(0.0);

    // Scroll по XZ — горизонтальный ветер
    vec3 scrollOffset = vec3(u_Time * u_NoiseSpeed, 0.0, u_Time * u_NoiseSpeed * 0.7);

    for (int i = 0; i < steps; ++i)
    {
        float t = (float(i) + 0.5) * stepSize;
        vec3  p = u_CameraPosition + rayDir * t;

        float heightMask =
            smoothstep(u_FogMinHeight, u_FogMinHeight + 1.0, p.y) *
            (1.0 - smoothstep(u_FogMaxHeight - 1.0, u_FogMaxHeight, p.y));

        if (heightMask <= 0.0)
            continue;

        float shadow = SampleShadowCSM(p, t);

        // ---- Noise (движущийся "дым") ---------------------------------
        float noiseFactor = 1.0;
        if (u_NoiseEnabled == 1)
        {
            vec3  noisePos = p * u_NoiseScale + scrollOffset;
            float n        = fbm2(noisePos);                       // [0, 1]
            noiseFactor    = mix(1.0, n * 2.0, u_NoiseIntensity);  // [0, 2]
        }

        float density = u_FogDensity * heightMask * noiseFactor;
        accum += u_SunColor * u_SunIntensity * shadow * density * phase * stepSize;
    }

    accum *= u_FogColor * u_FogIntensity;

    FragColor = vec4(accum, 1.0);
}