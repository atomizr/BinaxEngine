#version 460 core
#extension GL_ARB_bindless_texture : require

in vec3 v_WorldPos;
in vec3 v_Normal;
in vec2 v_UV;
flat in uint v_InstanceIndex;

out vec4 FragColor;

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
    vec4  SkyReflParams;
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

uniform vec3  u_CameraPosition;

uniform samplerCube u_EnvironmentMap;
uniform int   u_HasEnvironment;
uniform float u_SkyboxRotation;
uniform float u_SkyboxBrightness;

const int MAX_LIGHTS = 32;
struct Light {
    vec3  Position;
    float Range;
    vec3  Direction;
    float Intensity;
    vec3  Color;
    int   LightType;
    float InnerCutoff;
    float OuterCutoff;
    int   ShadowMode;
    float ShadowStrength;
    float BiasMultiplier;
    int   ShadowSlot;
};
uniform Light u_Lights[MAX_LIGHTS];
uniform int   u_LightCount;

const float PI = 3.14159265359;
const uvec2 kNoHandle = uvec2(0u, 0u);

vec3 sRGBToLinear(vec3 c) { return pow(c, vec3(2.2)); }

vec3 SampleSkybox(vec3 dir)
{
    float c = cos(u_SkyboxRotation);
    float s = sin(u_SkyboxRotation);
    vec3 rotated = vec3(
        dir.x * c - dir.z * s,
        dir.y,
        dir.x * s + dir.z * c);
    return texture(u_EnvironmentMap, rotated).rgb * u_SkyboxBrightness;
}

float D_GGX(float NdotH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, 1e-6);
}
float G_SchlickGGX(float NdotX, float k) { return NdotX / (NdotX * (1.0 - k) + k); }
float G_Smith(float NdotV, float NdotL, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return G_SchlickGGX(NdotV, k) * G_SchlickGGX(NdotL, k);
}
vec3 F_Schlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
vec3 EvaluateBRDF(vec3 N, vec3 V, vec3 L, vec3 radiance,
                  vec3 albedo, float metallic, float roughness)
{
    vec3 Hv = V + L;
    float hLen = length(Hv);
    if (hLen < 1e-5) return vec3(0.0);
    vec3 H = Hv / hLen;
    float NdotV = max(dot(N, V), 1e-4);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);
    if (NdotL <= 0.0) return vec3(0.0);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    float D = D_GGX(NdotH, roughness);
    float G = G_Smith(NdotV, NdotL, roughness);
    vec3  F = F_Schlick(HdotV, F0);
    vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 1e-4);
    vec3 kD = (1.0 - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PI;
    return (diffuse + specular) * radiance * NdotL;
}
float PointAttenuation(float dist, float range)
{
    float d2 = dist * dist;
    float t  = dist / max(range, 1e-3);
    float cutoff = pow(clamp(1.0 - pow(t, 4.0), 0.0, 1.0), 2.0);
    return cutoff / max(d2, 1e-3);
}

void main()
{
    InstanceGPU inst = instances[v_InstanceIndex];
    MaterialGPU m    = materials[inst.MaterialId];

    vec2 uv = v_UV * m.MetallicAOEms.w;

    
    vec3 albedo = sRGBToLinear(m.AlbedoRoughness.rgb);
    float alpha = m.SkyReflParams.w;
    if (m.AlbedoHandle != kNoHandle)
    {
        vec4 tex = texture(sampler2D(m.AlbedoHandle), uv);
        albedo *= tex.rgb * m.StrengthB.x;
        alpha  *= tex.a;
    }

    
    vec3 N = normalize(v_Normal);
    

    vec3  V = normalize(u_CameraPosition - v_WorldPos);
    float metallic  = m.MetallicAOEms.x;
    float roughness = clamp(m.AlbedoRoughness.w, 0.04, 1.0);
    float emissive  = m.MetallicAOEms.z;

    
    vec3 Lo = vec3(0.0);
    for (int i = 0; i < u_LightCount; ++i)
    {
        vec3 L, radiance;
        if (u_Lights[i].LightType == 0)
        {
            L = normalize(-u_Lights[i].Direction);
            radiance = u_Lights[i].Color * u_Lights[i].Intensity;
        }
        else if (u_Lights[i].LightType == 1)
        {
            vec3 toLight = u_Lights[i].Position - v_WorldPos;
            float dist   = length(toLight);
            L = toLight / max(dist, 1e-4);
            radiance = u_Lights[i].Color * u_Lights[i].Intensity
                     * PointAttenuation(dist, u_Lights[i].Range);
        }
        else
        {
            vec3 toLight = u_Lights[i].Position - v_WorldPos;
            float dist   = length(toLight);
            L = toLight / max(dist, 1e-4);
            float theta   = dot(L, normalize(-u_Lights[i].Direction));
            float epsilon = u_Lights[i].InnerCutoff - u_Lights[i].OuterCutoff;
            float spot    = clamp((theta - u_Lights[i].OuterCutoff)
                                  / max(epsilon, 1e-4), 0.0, 1.0);
            spot *= spot;
            radiance = u_Lights[i].Color * u_Lights[i].Intensity
                     * PointAttenuation(dist, u_Lights[i].Range) * spot;
        }
        Lo += EvaluateBRDF(N, V, L, radiance, albedo, metallic, roughness);
    }

    
    vec3 ambient = vec3(0.35, 0.42, 0.55) * albedo * 0.3;
    if (u_HasEnvironment == 1)
    {
        vec3 skyN = SampleSkybox(N);
        ambient += skyN * albedo * 0.15;

        if (m.SkyReflParams.x > 0.5)
        {
            vec3 R = reflect(-V, N);
            vec3 skyR = SampleSkybox(R);
            vec3 reflTint = mix(vec3(1.0), albedo, metallic);
            ambient += skyR * reflTint * max(m.SkyReflParams.y, 0.0) * 0.5;
        }
    }

    vec3 color = ambient + Lo + albedo * emissive;
    FragColor = vec4(color, alpha);
}