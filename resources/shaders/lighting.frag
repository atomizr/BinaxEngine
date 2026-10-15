#version 460 core

in vec2 v_UV;
out vec4 FragColor;

uniform sampler2D u_PositionTexture;
uniform sampler2D u_NormalTexture;
uniform sampler2D u_AlbedoTexture;
uniform sampler2D u_MaterialTexture;

uniform vec3  u_CameraPosition;
uniform mat4  u_InvViewProj;

// ---- Skybox ----
uniform samplerCube u_EnvironmentMap;
uniform int   u_HasEnvironment;
uniform float u_SkyboxRotation;    // radians around Y
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
};
uniform Light u_Lights[MAX_LIGHTS];
uniform int   u_LightCount;

const float PI = 3.14159265359;

// ---------------- BRDF ----------------

float D_GGX(float NdotH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, 1e-6);
}

float G_SchlickGGX(float NdotX, float k)
{
    return NdotX / (NdotX * (1.0 - k) + k);
}

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
    vec3 H = normalize(V + L);

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

// ---------------- Skybox ----------------

vec3 ComputeRayDirection()
{
    // Fullscreen-quad UV → NDC → world ray
    vec4 clip  = vec4(v_UV * 2.0 - 1.0, 1.0, 1.0);
    vec4 world = u_InvViewProj * clip;
    return normalize(world.xyz / world.w - u_CameraPosition);
}

vec3 SampleSkybox(vec3 dir)
{
    // Rotate around Y axis by u_SkyboxRotation
    float c = cos(u_SkyboxRotation);
    float s = sin(u_SkyboxRotation);
    vec3 rotated = vec3(
        dir.x * c - dir.z * s,
        dir.y,
        dir.x * s + dir.z * c);

    return texture(u_EnvironmentMap, rotated).rgb * u_SkyboxBrightness;
}

// ---------------- Main ----------------

void main()
{
    vec3 worldPos = texture(u_PositionTexture, v_UV).rgb;
    vec3 N        = normalize(texture(u_NormalTexture, v_UV).rgb);

    vec4 albedoAO = texture(u_AlbedoTexture, v_UV);
    vec3 albedo   = albedoAO.rgb;
    float ao      = albedoAO.a;

    vec4 mat        = texture(u_MaterialTexture, v_UV);
    float metallic  = mat.r;
    float roughness = clamp(mat.g, 0.04, 1.0);
    float emissive  = mat.b;

    // .a = enabled ? strength : -1.0  (упаковано в geometry.frag)
    bool  skyReflEnabled  = mat.a >= 0.0;
    float skyReflStrength = max(mat.a, 0.0);

    // ------- Background pixel (no geometry) → skybox -------
    if (dot(N, N) < 1e-4)
    {
        vec3 color = vec3(0.0);

        if (u_HasEnvironment == 1)
        {
            vec3 dir = ComputeRayDirection();
            color = SampleSkybox(dir);
        }

        color = color / (color + vec3(1.0));
        color = pow(color, vec3(1.0 / 2.2));

        FragColor = vec4(color, 1.0);
        return;
    }

    // ------- Lit pixel -------
    vec3 V  = normalize(u_CameraPosition - worldPos);
    vec3 Lo = vec3(0.0);

    for (int i = 0; i < u_LightCount; ++i)
    {
        vec3 L;
        vec3 radiance;

        if (u_Lights[i].LightType == 0)
        {
            L = normalize(-u_Lights[i].Direction);
            radiance = u_Lights[i].Color * u_Lights[i].Intensity;
        }
        else if (u_Lights[i].LightType == 1)
        {
            vec3  toLight = u_Lights[i].Position - worldPos;
            float dist    = length(toLight);
            L = toLight / max(dist, 1e-4);
            radiance = u_Lights[i].Color * u_Lights[i].Intensity
                     * PointAttenuation(dist, u_Lights[i].Range);
        }
        else
        {
            vec3  toLight = u_Lights[i].Position - worldPos;
            float dist    = length(toLight);
            L = toLight / max(dist, 1e-4);

            float theta   = dot(L, normalize(-u_Lights[i].Direction));
            float epsilon = u_Lights[i].InnerCutoff - u_Lights[i].OuterCutoff;
            float spot    = clamp((theta - u_Lights[i].OuterCutoff)
                                  / max(epsilon, 1e-4), 0.0, 1.0);
            spot *= spot;

            radiance = u_Lights[i].Color * u_Lights[i].Intensity
                     * PointAttenuation(dist, u_Lights[i].Range)
                     * spot;
        }

        Lo += EvaluateBRDF(N, V, L, radiance, albedo, metallic, roughness);
    }

    // ---- Ambient ----
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 ambient = F0 * albedo * 0.03 * ao;

    if (u_HasEnvironment == 1)
    {
        // Мягкий диффузный tint от неба (низкочастотный)
        vec3 skyN = SampleSkybox(N);
        ambient += skyN * albedo * ao * 0.15;

        // ---- Зеркальное отражение неба (прямой сэмпл кубемапы) ----
        if (skyReflEnabled)
        {
            vec3 R    = reflect(-V, N);
            vec3 skyR = SampleSkybox(R);

            // Металлы отражают со своим цветом (F0 = albedo),
            // диэлектрики — бесцветно.
            vec3 reflTint = mix(vec3(1.0), albedo, metallic);

            // Лёгкий Fresnel-буст по касательной: 1.0 «в лоб» → 1.6 на краю.
            float NdotV   = max(dot(N, V), 1e-4);
            float graze   = pow(1.0 - NdotV, 5.0);
            float fresnel = mix(1.0, 1.6, graze);

            ambient += skyR * reflTint * fresnel * skyReflStrength * ao;
        }
    }

    vec3 color = ambient + Lo + albedo * emissive;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}