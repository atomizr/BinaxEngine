#version 460 core
#extension GL_ARB_bindless_texture : require

layout(location = 0) out vec3 g_Position;
layout(location = 1) out vec3 g_Normal;
layout(location = 2) out vec4 g_Albedo_AO;
layout(location = 3) out vec4 g_Material;
layout(location = 4) out vec2 g_Velocity;

in vec3 v_WorldPos;
in vec3 v_Normal;
in vec2 v_UV;
in vec2 v_Velocity;
flat in uint v_InstanceIndex;

struct InstanceGPU {
    mat4 Model;
    mat4 PrevModel;
    uint MaterialId;
    uint MeshId;
    uint BoneOffset;
    uint Flags;
};
layout(std430, binding = 1) readonly buffer Instances {
    InstanceGPU instances[];
};

// [FIX] AMD Adrenalin не поддерживает uint64_t в 4.60 core.
//       Поэтому handle'ы хранятся как uvec2 (lo, hi).
//       CPU-структура MaterialGPU остаётся 128 байт: uvec2 == uint64_t
//       по размеру и выравниванию в std430.
struct MaterialGPU {
    vec4 AlbedoRoughness;
    vec4 MetallicAOEms;
    vec4 SkyReflParams;
    vec4 StrengthA;
    uvec2 AlbedoHandle;
    uvec2 NormalHandle;
    uvec2 MetallicHandle;
    uvec2 RoughnessHandle;
    uvec2 AOHandle;
    uvec2 EmissiveHandle;
    vec4 StrengthB;
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

    // [FIX] На вырожденных UV (нулевые производные) прежний inversesqrt(0)
    //       давал inf/NaN, и нормаль уезжала в мусор. Оставляем базис
    //       единичным — normal map в таком случае просто не применяется.
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
    MaterialGPU m = materials[inst.MaterialId];

    vec2 uv = v_UV * m.MetallicAOEms.w;

    // ---------- Albedo ----------
    vec3 albedoTint = sRGBToLinear(m.AlbedoRoughness.rgb);
    vec3 baseColor  = albedoTint;
    if (m.AlbedoHandle != kNoHandle)
        baseColor *= texture(sampler2D(m.AlbedoHandle), uv).rgb * m.StrengthB.x;

    // ---------- Normal ----------
    vec3 N = normalize(v_Normal);
    if (m.NormalHandle != kNoHandle)
    {
        mat3 TBN = ComputeTBN(N, v_WorldPos, uv);
        vec3 s = texture(sampler2D(m.NormalHandle), uv).rgb * 2.0 - 1.0;
        s.xy *= m.StrengthB.y;
        N = normalize(TBN * normalize(s));
    }

    // ---------- Material scalars ----------
    float metallic  = m.MetallicAOEms.x;
    float roughness = m.AlbedoRoughness.w;
    float ao        = m.MetallicAOEms.y;
    float emissive  = m.MetallicAOEms.z;

    if (m.MetallicHandle != kNoHandle)
        metallic = clamp(texture(sampler2D(m.MetallicHandle), uv).r * m.StrengthA.x, 0.0, 1.0);

    if (m.RoughnessHandle != kNoHandle)
        roughness = clamp(texture(sampler2D(m.RoughnessHandle), uv).r * m.StrengthA.y, 0.04, 1.0);

    if (m.AOHandle != kNoHandle)
        ao = clamp(texture(sampler2D(m.AOHandle), uv).r * m.StrengthA.z, 0.0, 1.0);

    if (m.EmissiveHandle != kNoHandle)
        emissive = texture(sampler2D(m.EmissiveHandle), uv).r * m.StrengthA.w;

    // ---------- Sky reflection pack ----------
    float skyReflPacked = (m.SkyReflParams.x > 0.5)
                            ? max(m.SkyReflParams.y, 0.0)
                            : -1.0;

    g_Position  = v_WorldPos;
    g_Normal    = N;
    g_Albedo_AO = vec4(baseColor, ao);
    g_Material  = vec4(metallic, roughness, emissive, skyReflPacked);
    g_Velocity  = v_Velocity;
}