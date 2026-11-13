#version 460 core
#extension GL_ARB_bindless_texture : require

layout(location = 0) out vec3  g_Position;
layout(location = 1) out vec4  g_NormalA;   // rgb=normal, a=unlitFlag
layout(location = 2) out vec4  g_Albedo_AO;
layout(location = 3) out vec4  g_Material;
layout(location = 4) out vec2  g_Velocity;

in vec3 v_WorldPos;
in vec3 v_Normal;
in vec2 v_UV;
in vec2 v_Velocity;
flat in uint v_InstanceIndex;

struct InstanceGPU {
    mat4 Model;
    mat4 PrevModel;
    mat4 NormalMatrix;
    uint MaterialId;
    uint MeshId;
    uint BoneOffset;
    uint Flags;
};

layout(std430, binding = 1) readonly buffer Instances {
    InstanceGPU instances[];
};

struct MaterialGPU {
    vec4  AlbedoRoughness;
    vec4  MetallicAOEms;
    vec4  SkyReflParams;    // z = ShaderType (0=Lit,1=SimpleLit,2=Unlit)
    vec4  StrengthA;
    uvec2 AlbedoHandle;
    uvec2 NormalHandle;
    uvec2 MetallicHandle;
    uvec2 RoughnessHandle;
    uvec2 AOHandle;
    uvec2 EmissiveHandle;
    vec4  StrengthB;
};

layout(std430, binding = 2) readonly buffer Materials {
    MaterialGPU materials[];
};

vec3 sRGBToLinear(vec3 c) { return pow(c, vec3(2.2)); }

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

    float len2 = max(dot(T, T), dot(B, B));
    if (len2 < 1e-12)
        return mat3(1.0);

    float invmax = inversesqrt(len2);
    return mat3(T * invmax, B * invmax, N);
}

const uvec2 kNoHandle = uvec2(0u, 0u);

void main()
{
    InstanceGPU inst = instances[v_InstanceIndex];
    MaterialGPU m    = materials[inst.MaterialId];

    int shaderType = int(m.SkyReflParams.z + 0.5);

    vec2 uv = v_UV * m.MetallicAOEms.w;

    vec3 albedoTint = sRGBToLinear(m.AlbedoRoughness.rgb);
    vec3 baseColor  = albedoTint;

    if (m.AlbedoHandle != kNoHandle)
        baseColor *= texture(sampler2D(m.AlbedoHandle), uv).rgb * m.StrengthB.x;

    vec3 N = normalize(v_Normal);

    // ===== UNLIT ==========================================================
    if (shaderType == 2)
    {
        float emissive = m.MetallicAOEms.z;
        if (m.EmissiveHandle != kNoHandle)
            emissive = texture(sampler2D(m.EmissiveHandle), uv).r * m.StrengthA.w;

        g_Position  = v_WorldPos;
        g_NormalA   = vec4(N, 1.0);                    // a=1 → unlit
        g_Albedo_AO = vec4(baseColor, 1.0);
        g_Material  = vec4(0.0, 1.0, emissive, -1.0);  // metallic=0, rough=1, no IBL
        g_Velocity  = v_Velocity;
        return;
    }

    // ===== LIT / SIMPLE LIT ==============================================
    // Simple Lit отключает PBR-карты и IBL, но оставляет normal-map.
    const bool simpleLit = (shaderType == 1);

    if (m.NormalHandle != kNoHandle)
    {
        mat3 TBN = ComputeTBN(N, v_WorldPos, uv);
        vec3 s   = texture(sampler2D(m.NormalHandle), uv).rgb * 2.0 - 1.0;
        s.xy    *= m.StrengthB.y;
        N        = normalize(TBN * normalize(s));
    }

    float metallic  = 0.0;
    float roughness = 1.0;
    float ao        = 1.0;
    float emissive  = m.MetallicAOEms.z;

    if (!simpleLit)
    {
        metallic  = m.MetallicAOEms.x;
        roughness = m.AlbedoRoughness.w;
        ao        = m.MetallicAOEms.y;

        if (m.MetallicHandle != kNoHandle)
            metallic = clamp(texture(sampler2D(m.MetallicHandle), uv).r * m.StrengthA.x, 0.0, 1.0);

        if (m.RoughnessHandle != kNoHandle)
            roughness = clamp(texture(sampler2D(m.RoughnessHandle), uv).r * m.StrengthA.y, 0.04, 1.0);

        if (m.AOHandle != kNoHandle)
            ao = clamp(texture(sampler2D(m.AOHandle), uv).r * m.StrengthA.z, 0.0, 1.0);
    }

    if (m.EmissiveHandle != kNoHandle)
        emissive = texture(sampler2D(m.EmissiveHandle), uv).r * m.StrengthA.w;

    float skyReflPacked = (!simpleLit && m.SkyReflParams.x > 0.5)
        ? max(m.SkyReflParams.y, 0.0)
        : -1.0;

    g_Position  = v_WorldPos;
    g_NormalA   = vec4(N, 0.0);                        // a=0 → lit
    g_Albedo_AO = vec4(baseColor, ao);
    g_Material  = vec4(metallic, roughness, emissive, skyReflPacked);
    g_Velocity  = v_Velocity;
}