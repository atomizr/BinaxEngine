#version 460 core

layout(location = 0) out vec3 g_Position;
layout(location = 1) out vec3 g_Normal;
layout(location = 2) out vec4 g_Albedo_AO;
layout(location = 3) out vec4 g_Material;

in vec3 v_WorldPos;
in vec3 v_Normal;
in vec2 v_UV;

// ---- Scalar params ----
uniform vec3  u_Albedo;
uniform float u_Metallic;
uniform float u_Roughness;
uniform float u_AO;
uniform float u_Emissive;

// ---- Texture samplers ----
uniform sampler2D u_AlbedoMap;
uniform sampler2D u_NormalMap;
uniform sampler2D u_MetallicMap;
uniform sampler2D u_RoughnessMap;
uniform sampler2D u_AOMap;
uniform sampler2D u_EmissiveMap;

// ---- Presence flags (0 = no texture bound) ----
uniform int u_HasAlbedoMap;
uniform int u_HasNormalMap;
uniform int u_HasMetallicMap;
uniform int u_HasRoughnessMap;
uniform int u_HasAOMap;
uniform int u_HasEmissiveMap;

// ---- Per-map strengths (0..10) ----
uniform float u_AlbedoStrength;
uniform float u_NormalStrength;
uniform float u_MetallicStrength;
uniform float u_RoughnessStrength;
uniform float u_AOStrength;
uniform float u_EmissiveStrength;

// ---- UV multiplier ----
uniform float u_Tiling;

// ---- Skybox reflections (direct sample, no baking) ----
uniform int   u_SkyReflectionEnabled;
uniform float u_SkyReflectionStrength;

vec3 sRGBToLinear(vec3 c) { return pow(c, vec3(2.2)); }

// Cotangent-frame TBN — works without per-vertex tangents.
mat3 ComputeTBN(vec3 N, vec3 worldPos, vec2 uv)
{
    vec3 dp1  = dFdx(worldPos);
    vec3 dp2  = dFdy(worldPos);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);

    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);

    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    float invmax = inversesqrt(max(dot(T, T), dot(B, B)));
    return mat3(T * invmax, B * invmax, N);
}

void main()
{
    vec2 uv = v_UV * u_Tiling;

    // ---------- Albedo (scalar × texture × strength) ----------
    vec3 baseColor = u_Albedo;
    if (u_HasAlbedoMap == 1)
        baseColor *= texture(u_AlbedoMap, uv).rgb * u_AlbedoStrength;

    // ---------- Normal ----------
    vec3 N = normalize(v_Normal);
    if (u_HasNormalMap == 1)
    {
        mat3 TBN = ComputeTBN(N, v_WorldPos, uv);
        vec3 sampled = texture(u_NormalMap, uv).rgb * 2.0 - 1.0;
        sampled.xy *= u_NormalStrength;
        sampled = normalize(sampled);
        N = normalize(TBN * sampled);
    }

    // ---------- Metallic (texture replaces scalar) ----------
    float metallic = u_Metallic;
    if (u_HasMetallicMap == 1)
        metallic = clamp(texture(u_MetallicMap, uv).r * u_MetallicStrength, 0.0, 1.0);

    // ---------- Roughness ----------
    float roughness = u_Roughness;
    if (u_HasRoughnessMap == 1)
        roughness = clamp(texture(u_RoughnessMap, uv).r * u_RoughnessStrength, 0.04, 1.0);

    // ---------- AO ----------
    float ao = u_AO;
    if (u_HasAOMap == 1)
        ao = clamp(texture(u_AOMap, uv).r * u_AOStrength, 0.0, 1.0);

    // ---------- Emissive (grayscale from R channel) ----------
    float emissive = u_Emissive;
    if (u_HasEmissiveMap == 1)
        emissive = texture(u_EmissiveMap, uv).r * u_EmissiveStrength;

    // ---------- Sky reflection pack ----------
    // .a = enabled ? strength : -1.0  (negative = off; RGBA16F keeps sign losslessly)
    float skyReflPacked = (u_SkyReflectionEnabled == 1)
                            ? max(u_SkyReflectionStrength, 0.0)
                            : -1.0;

    // ---------- Write to G-buffer ----------
    g_Position  = v_WorldPos;
    g_Normal    = N;
    g_Albedo_AO = vec4(sRGBToLinear(baseColor), ao);
    g_Material  = vec4(metallic, roughness, emissive, skyReflPacked);
}