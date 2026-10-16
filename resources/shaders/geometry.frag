#version 460 core
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_gpu_shader_int64  : require

layout(location = 0) out vec3 g_Position;
layout(location = 1) out vec3 g_Normal;
layout(location = 2) out vec4 g_Albedo_AO;
layout(location = 3) out vec4 g_Material;

in vec3 v_WorldPos;
in vec3 v_Normal;
in vec2 v_UV;
flat in uint v_InstanceIndex;

struct InstanceGPU {
    mat4 Model;
    uint MaterialId;
    uint MeshId;
    uint BoneOffset;
    uint Flags;
};
layout(std430, binding = 1) readonly buffer Instances {
    InstanceGPU instances[];
};

struct MaterialGPU {
    vec4 AlbedoRoughness;
    vec4 MetallicAOEms;
    vec4 SkyReflParams;
    vec4 StrengthA;
    uint64_t AlbedoHandle;
    uint64_t NormalHandle;
    uint64_t MetallicHandle;
    uint64_t RoughnessHandle;
    uint64_t AOHandle;
    uint64_t EmissiveHandle;
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

    float invmax = inversesqrt(max(dot(T, T), dot(B, B)));
    return mat3(T * invmax, B * invmax, N);
}

// AMD-friendly bindless sampler. Contract: empty handle -> 0ul.
sampler2D MakeSampler(uint64_t handle)
{
    return sampler2D(unpackUint2x32(handle));
}

void main()
{
    InstanceGPU inst = instances[v_InstanceIndex];
    MaterialGPU m = materials[inst.MaterialId];

    vec2 uv = v_UV * m.MetallicAOEms.w;   // tiling

    // ---------- Albedo ----------
    // [FIX] Раньше константа m.AlbedoRoughness.rgb (sRGB из ColorEdit3)
    //       перемножалась с уже линейной sRGB-текстурой, а в конце
    //       ко всему результату применялся sRGBToLinear — итог был
    //       дважды тонирован и слишком тёмный.
    //       Правильно: константу один раз перевести в linear,
    //       использовать её как tint для линейной текстуры.
    vec3 albedoTint = sRGBToLinear(m.AlbedoRoughness.rgb);
    vec3 baseColor  = albedoTint;
    if (m.AlbedoHandle != 0ul)
        baseColor *= texture(MakeSampler(m.AlbedoHandle), uv).rgb * m.StrengthB.x;

    // ---------- Normal ----------
    vec3 N = normalize(v_Normal);
    if (m.NormalHandle != 0ul)
    {
        mat3 TBN = ComputeTBN(N, v_WorldPos, uv);
        vec3 s = texture(MakeSampler(m.NormalHandle), uv).rgb * 2.0 - 1.0;
        s.xy *= m.StrengthB.y;
        N = normalize(TBN * normalize(s));
    }

    // ---------- Material scalars ----------
    float metallic  = m.MetallicAOEms.x;
    float roughness = m.AlbedoRoughness.w;   // .w = roughness (в CPU-структуре)
    float ao        = m.MetallicAOEms.y;
    float emissive  = m.MetallicAOEms.z;

    if (m.MetallicHandle != 0ul)
        metallic = clamp(texture(MakeSampler(m.MetallicHandle), uv).r * m.StrengthA.x, 0.0, 1.0);

    if (m.RoughnessHandle != 0ul)
        roughness = clamp(texture(MakeSampler(m.RoughnessHandle), uv).r * m.StrengthA.y, 0.04, 1.0);

    if (m.AOHandle != 0ul)
        ao = clamp(texture(MakeSampler(m.AOHandle), uv).r * m.StrengthA.z, 0.0, 1.0);

    if (m.EmissiveHandle != 0ul)
        emissive = texture(MakeSampler(m.EmissiveHandle), uv).r * m.StrengthA.w;

    // ---------- Sky reflection pack ----------
    float skyReflPacked = (m.SkyReflParams.x > 0.5)
                            ? max(m.SkyReflParams.y, 0.0)
                            : -1.0;

    g_Position  = v_WorldPos;
    g_Normal    = N;
    g_Albedo_AO = vec4(baseColor, ao);          // [FIX] без повторного sRGBToLinear
    g_Material  = vec4(metallic, roughness, emissive, skyReflPacked);
}
