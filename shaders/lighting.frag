#version 460 core

in  vec2 v_UV;
out vec4 FragColor;

uniform sampler2D u_PositionTexture;
uniform sampler2D u_NormalTexture;
uniform sampler2D u_AlbedoTexture;
uniform sampler2D u_MaterialTexture;
uniform sampler2D u_OutlineTexture;
uniform sampler2D u_SSAOTexture;
uniform int       u_HasSSAO;
uniform float     u_SSAOIntensity;
uniform int       u_HasOutline;


const int MAX_PROBES = 4;
uniform samplerCubeArray u_ProbeCubeArray;
uniform int   u_ProbeCount;
uniform vec3  u_ProbePos[MAX_PROBES];
uniform vec3  u_ProbeExtents[MAX_PROBES];
uniform int   u_ProbeBoxProjection[MAX_PROBES];


uniform int u_DebugReflections;

uniform vec3  u_CameraPosition;
uniform mat4  u_InvViewProj;

uniform samplerCube u_EnvironmentMap;
uniform int   u_HasEnvironment;
uniform float u_SkyboxRotation;
uniform float u_SkyboxBrightness;

uniform int   u_FogEnabled;
uniform int   u_FogMode;
uniform vec3  u_FogColor;
uniform float u_FogDensity;
uniform float u_FogStart;
uniform float u_FogEnd;


uniform sampler2DArray u_ShadowMap;
uniform mat4  u_LightSpaceMatrices[4];
uniform float u_CascadeSplits[4];
uniform int   u_CascadeCount;
uniform int   u_ShadowMode;
uniform int   u_ShadowLightIndex;
uniform float u_DepthBias;
uniform float u_NormalBias;
uniform float u_ShadowSoftness;
uniform float u_MaxShadowDistance;
uniform float u_FadeOutDistance;
uniform float u_ShadowStrength;
uniform float u_BiasMultiplier;
uniform mat4  u_ViewMatrix;


uniform sampler2DArray u_SpotShadowMap;
uniform mat4           u_SpotLightSpaceMatrix[4];


uniform samplerCubeArray u_PointShadowMap;
uniform float            u_PointShadowNear[2];
uniform float            u_PointShadowFar [2];

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




vec3 OutlineComposite(vec3 baseColor, vec2 uv)
{
    if (u_HasOutline != 1) return baseColor;

    vec2 texel = 1.0 / vec2(textureSize(u_OutlineTexture, 0));

    float center = texture(u_OutlineTexture, uv).a;
    if (center > 0.5) return baseColor;

    const int R = 2;
    float found = 0.0;
    for (int y = -R; y <= R; ++y)
    for (int x = -R; x <= R; ++x)
    {
        if (x == 0 && y == 0) continue;
        if (float(x*x + y*y) > float(R*R)) continue;
        float a = texture(u_OutlineTexture, uv + vec2(x, y) * texel).a;
        if (a > 0.5) { found = 1.0; break; }
    }
    if (found < 0.5) return baseColor;

    return vec3(0.36, 0.56, 1.0);
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

vec3 ComputeRayDirection()
{
    vec4 clip  = vec4(v_UV * 2.0 - 1.0, 1.0, 1.0);
    vec4 world = u_InvViewProj * clip;
    return normalize(world.xyz / world.w - u_CameraPosition);
}

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




float SampleDirectionalShadow(vec3 worldPos, vec3 N, vec3 L, float viewZ)
{
    if (u_ShadowMode == 0 || u_CascadeCount == 0) return 1.0;

    int cascadeIdx = u_CascadeCount - 1;
    for (int i = 0; i < u_CascadeCount; ++i)
    {
        if (viewZ < u_CascadeSplits[i]) { cascadeIdx = i; break; }
    }

    vec4 lsPos = u_LightSpaceMatrices[cascadeIdx]
                 * vec4(worldPos + N * u_NormalBias, 1.0);
    vec3 projCoords = lsPos.xyz / lsPos.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0 ||
        projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0)
        return 1.0;

    float NdotL = max(dot(N, L), 0.0);
    float bias  = u_DepthBias * u_BiasMultiplier * (1.0 + (1.0 - NdotL) * 2.0);
    float depth = projCoords.z - bias;

    float shadow = 1.0;
    if (u_ShadowMode == 1)
    {
        float d = texture(u_ShadowMap,
                          vec3(projCoords.xy, float(cascadeIdx))).r;
        shadow = (depth > d) ? 0.0 : 1.0;
    }
    else
    {
        float softness = clamp(u_ShadowSoftness, 0.5, 4.0);
        int rad = int(ceil(softness));
        vec2 texel = softness / vec2(textureSize(u_ShadowMap, 0).xy);
        float sum = 0.0, cnt = 0.0;
        for (int y = -rad; y <= rad; ++y)
        for (int x = -rad; x <= rad; ++x)
        {
            float d = texture(u_ShadowMap,
                              vec3(projCoords.xy + vec2(x,y)*texel,
                                   float(cascadeIdx))).r;
            sum += (depth > d) ? 0.0 : 1.0;
            cnt += 1.0;
        }
        shadow = sum / max(cnt, 1.0);
    }

    if (viewZ > u_MaxShadowDistance - u_FadeOutDistance)
    {
        float fade = 1.0 - (viewZ - (u_MaxShadowDistance - u_FadeOutDistance))
                            / max(u_FadeOutDistance, 1e-3);
        shadow = mix(1.0, shadow, clamp(fade, 0.0, 1.0));
    }
    return mix(1.0, shadow, u_ShadowStrength);
}




float SampleSpotShadow(int lightIdx, vec3 worldPos, vec3 N, vec3 L)
{
    int slot = u_Lights[lightIdx].ShadowSlot;
    if (slot < 0 || slot >= 4) return 1.0;

    vec4 lsPos = u_SpotLightSpaceMatrix[slot]
                 * vec4(worldPos + N * u_NormalBias, 1.0);
    vec3 projCoords = lsPos.xyz / lsPos.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0 ||
        projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0)
        return 1.0;

    float NdotL = max(dot(N, L), 0.0);
    float bias  = u_DepthBias * u_Lights[lightIdx].BiasMultiplier
                * (1.0 + (1.0 - NdotL) * 2.0);
    float depth = projCoords.z - bias;

    float shadow = 1.0;
    if (u_Lights[lightIdx].ShadowMode == 1)
    {
        float d = texture(u_SpotShadowMap, vec3(projCoords.xy, float(slot))).r;
        shadow = (depth > d) ? 0.0 : 1.0;
    }
    else
    {
        float softness = clamp(u_ShadowSoftness, 0.5, 4.0);
        int rad = int(ceil(softness));
        vec2 texel = softness / vec2(textureSize(u_SpotShadowMap, 0).xy);
        float sum = 0.0, cnt = 0.0;
        for (int y = -rad; y <= rad; ++y)
        for (int x = -rad; x <= rad; ++x)
        {
            float d = texture(u_SpotShadowMap,
                              vec3(projCoords.xy + vec2(x,y)*texel,
                                   float(slot))).r;
            sum += (depth > d) ? 0.0 : 1.0;
            cnt += 1.0;
        }
        shadow = sum / max(cnt, 1.0);
    }
    return mix(1.0, shadow, u_Lights[lightIdx].ShadowStrength);
}




float LinearizeDepth(float d, float nearP, float farP)
{
    float z = d * 2.0 - 1.0;
    return (2.0 * nearP * farP) / (farP + nearP - z * (farP - nearP));
}

float SamplePointShadow(int lightIdx, vec3 worldPos, vec3 N, vec3 L)
{
    int slot = u_Lights[lightIdx].ShadowSlot;
    if (slot < 0 || slot >= 2) return 1.0;

    vec3 lightPos = u_Lights[lightIdx].Position;
    vec3 fragToLight = worldPos - lightPos;

    float fragZ = max(max(abs(fragToLight.x), abs(fragToLight.y)),
                      abs(fragToLight.z));
    float nearP = u_PointShadowNear[slot];
    float farP  = u_PointShadowFar[slot];

    float NdotL = max(dot(N, L), 0.0);
    float relBias = u_DepthBias * u_Lights[lightIdx].BiasMultiplier
                  * (1.0 + (1.0 - NdotL) * 2.0);
    float biasWorld = relBias * fragZ;

    float shadow = 1.0;
    if (u_Lights[lightIdx].ShadowMode == 1)
    {
        float d = texture(u_PointShadowMap,
                          vec4(fragToLight, float(slot))).r;
        float storedZ = LinearizeDepth(d, nearP, farP);
        shadow = (fragZ > storedZ + biasWorld) ? 0.0 : 1.0;
    }
    else
    {
        float softness = clamp(u_ShadowSoftness, 0.5, 2.0);
        float angularRadius = softness * 0.005;

        vec3 up = (abs(fragToLight.y) < 0.99)
                  ? vec3(0,1,0) : vec3(1,0,0);
        vec3 tangent   = normalize(cross(fragToLight, up));
        vec3 bitangent = cross(fragToLight, tangent);

        const vec2 offsets[4] = vec2[4](
            vec2(-0.5,-0.5), vec2(0.5,-0.5),
            vec2(-0.5, 0.5), vec2(0.5, 0.5));

        float sum = 0.0;
        for (int k = 0; k < 4; ++k)
        {
            vec3 dir = fragToLight
                     + (offsets[k].x * tangent + offsets[k].y * bitangent)
                       * angularRadius * fragZ;
            float d = texture(u_PointShadowMap,
                              vec4(dir, float(slot))).r;
            float storedZ = LinearizeDepth(d, nearP, farP);
            sum += (fragZ > storedZ + biasWorld) ? 0.0 : 1.0;
        }
        shadow = sum / 4.0;
    }
    return mix(1.0, shadow, u_Lights[lightIdx].ShadowStrength);
}




float SampleSSAO_DepthAware(vec2 uv, float centerViewZ)
{
    vec2 ssaoSize = vec2(textureSize(u_SSAOTexture, 0));
    vec2 texel    = 1.0 / ssaoSize;

    vec2 scaledUV = uv * ssaoSize - 0.5;
    vec2 baseUV   = (floor(scaledUV) + 0.5) / ssaoSize;
    vec2 fracUV   = fract(scaledUV);

    vec2 uvs[4] = vec2[4](
        baseUV,
        baseUV + vec2(texel.x, 0.0),
        baseUV + vec2(0.0, texel.y),
        baseUV + texel
    );

    float bilinW[4] = float[4](
        (1.0 - fracUV.x) * (1.0 - fracUV.y),
        fracUV.x * (1.0 - fracUV.y),
        (1.0 - fracUV.x) * fracUV.y,
        fracUV.x * fracUV.y
    );

    float result = 0.0;
    float totalW = 0.0;

    for (int i = 0; i < 4; ++i)
    {
        vec3  sampleWorld = texture(u_PositionTexture, uvs[i]).rgb;
        float sampleViewZ = -(u_ViewMatrix * vec4(sampleWorld, 1.0)).z;

        float depthDiff = abs(centerViewZ - sampleViewZ);
        float depthW = exp(-depthDiff * depthDiff * 100.0);

        float w = bilinW[i] * depthW;
        result += texture(u_SSAOTexture, uvs[i]).r * w;
        totalW += w;
    }

    if (totalW < 1e-5)
        return texture(u_SSAOTexture, uv).r;

    return result / totalW;
}




bool IsValidProbe(uint pIdx)
{
    return u_ProbeCount > 0 && pIdx < uint(u_ProbeCount);
}

vec3 SampleReflectionProbe(uint pIdx, vec3 worldPos, vec3 R, float roughness)
{
    vec3 pPos = u_ProbePos[pIdx];
    vec3 pExt = u_ProbeExtents[pIdx];

    vec3 reflDir = R;

    if (u_ProbeBoxProjection[pIdx] != 0)
    {
        
        vec3 originToWorld = worldPos - pPos;

        
        
        if (all(lessThanEqual(abs(originToWorld), pExt)))
        {
            vec3 t1   = (pExt - originToWorld) / R;
            vec3 t2   = (-pExt - originToWorld) / R;
            vec3 tmax = max(t1, t2);
            float dist = min(min(tmax.x, tmax.y), tmax.z);

            
            if (dist > 1e-4 && !isinf(dist) && !isnan(dist))
            {
                vec3 hitPos = worldPos + R * dist;
                reflDir = hitPos - pPos;
            }
        }
    }

    
    float len2 = dot(reflDir, reflDir);
    if (len2 < 1e-6)
        reflDir = R;
    else
        reflDir = reflDir * inversesqrt(len2);

    float lod = clamp(roughness * 4.0, 0.0, 4.0);
    return textureLod(u_ProbeCubeArray, vec4(reflDir, float(pIdx)), lod).rgb;
}




void main()
{
    vec4  posA     = texture(u_PositionTexture, v_UV);
    vec3  worldPos = posA.rgb;
    uint  probeIdx = uint(posA.a + 0.5);
    vec4  normalA  = texture(u_NormalTexture,   v_UV);
    vec3  Nraw     = normalA.rgb;
    bool  isUnlit  = normalA.a > 0.5;

    vec4 albedoAO = texture(u_AlbedoTexture, v_UV);
    vec3 albedo   = albedoAO.rgb;
    float ao      = albedoAO.a;

    vec4 mat        = texture(u_MaterialTexture, v_UV);
    float metallic  = mat.r;
    float roughness = clamp(mat.g, 0.04, 1.0);
    float emissive  = mat.b;

    bool  skyReflEnabled  = mat.a >= 0.0;
    float skyReflStrength = max(mat.a, 0.0);

    
    if (u_DebugReflections == 1)
    {
        FragColor = (probeIdx > 0u) ? vec4(0.0, 1.0, 0.0, 1.0)
                                    : vec4(1.0, 0.0, 0.0, 1.0);
        return;
    }

    
    if (u_DebugReflections == 2 && probeIdx > 0u && IsValidProbe(probeIdx - 1u))
    {
        if (dot(Nraw, Nraw) > 1e-4)
        {
            vec3 N = normalize(Nraw);
            vec3 V = normalize(u_CameraPosition - worldPos);
            vec3 R = reflect(-V, N);
            vec3 raw = SampleReflectionProbe(probeIdx - 1u, worldPos, R, 0.0);
            FragColor = vec4(raw, 1.0);
            return;
        }
    }

    
    if (isUnlit)
    {
        vec3 color = albedo * (1.0 + emissive);
        color = OutlineComposite(color, v_UV);
        FragColor = vec4(color, 1.0);
        return;
    }

    
    if (dot(Nraw, Nraw) < 1e-4)
    {
        vec3 color = vec3(0.0);
        if (u_HasEnvironment == 1)
        {
            vec3 dir = ComputeRayDirection();
            color = SampleSkybox(dir);
        }
        color = OutlineComposite(color, v_UV);
        FragColor = vec4(color, 1.0);
        return;
    }

    vec3 N = normalize(Nraw);
    vec3 V = normalize(u_CameraPosition - worldPos);
    vec3 Lo = vec3(0.0);

    const float viewZ = -(u_ViewMatrix * vec4(worldPos, 1.0)).z;

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

        float lightShadow = 1.0;
        if (u_Lights[i].ShadowMode != 0)
        {
            if (u_Lights[i].LightType == 0)
            {
                if (i == u_ShadowLightIndex)
                    lightShadow = SampleDirectionalShadow(worldPos, N, L, viewZ);
            }
            else if (u_Lights[i].LightType == 2)
            {
                lightShadow = SampleSpotShadow(i, worldPos, N, L);
            }
            else
            {
                lightShadow = SamplePointShadow(i, worldPos, N, L);
            }
        }

        Lo += EvaluateBRDF(N, V, L, radiance, albedo, metallic, roughness)
              * lightShadow;
    }

    
    float ssaoFactor = 1.0;
    if (u_HasSSAO == 1)
    {
        float rawAO  = SampleSSAO_DepthAware(v_UV, viewZ);
        ssaoFactor   = mix(1.0, rawAO, clamp(u_SSAOIntensity, 0.0, 3.0));
    }

    
    vec3 skyColor    = vec3(0.35, 0.42, 0.55);
    vec3 groundColor = vec3(0.12, 0.10, 0.08);
    float hemi       = N.y * 0.5 + 0.5;
    vec3 ambient     = mix(groundColor, skyColor, hemi) * albedo * ao;

    if (u_HasEnvironment == 1)
    {
        vec3 skyN = SampleSkybox(N);
        ambient += skyN * albedo * ao * 0.15;
    }

    
    vec3 specContrib = vec3(0.0);
    if (skyReflEnabled)
    {
        vec3 R = reflect(-V, N);
        vec3 reflColor = vec3(0.0);
        bool gotReflection = false;

        
        if (probeIdx > 0u && IsValidProbe(probeIdx - 1u))
        {
            reflColor = SampleReflectionProbe(probeIdx - 1u, worldPos, R, roughness);
            gotReflection = true;
        }

        
        if (!gotReflection && u_HasEnvironment == 1)
        {
            reflColor = SampleSkybox(R);
        }

        vec3 reflTint = mix(vec3(1.0), albedo, metallic);

        float NdotV   = max(dot(N, V), 1e-4);
        float graze   = pow(1.0 - NdotV, 5.0);
        float fresnel = mix(1.0, 1.6, graze);

        specContrib = reflColor * reflTint * fresnel * skyReflStrength * ao;
        ambient += specContrib;
    }

    ambient *= ssaoFactor;

    
    if (u_DebugReflections == 3)
    {
        FragColor = vec4(specContrib, 1.0);
        return;
    }

    vec3 color = ambient + Lo + albedo * emissive;

    
    if (u_FogEnabled == 1)
    {
        const float dist = length(u_CameraPosition - worldPos);
        float fog = 0.0;
        if (u_FogMode == 0)
            fog = clamp((dist - u_FogStart) / max(u_FogEnd - u_FogStart, 1e-4), 0.0, 1.0);
        else if (u_FogMode == 1)
            fog = 1.0 - exp(-max(dist, 0.0) * max(u_FogDensity, 0.0));
        else
        {
            const float d = max(dist, 0.0) * max(u_FogDensity, 0.0);
            fog = 1.0 - exp(-d * d);
        }
        const vec3 fogLinear = pow(max(u_FogColor, vec3(0.0)), vec3(2.2));
        color = mix(color, fogLinear, clamp(fog, 0.0, 1.0));
    }

    color = OutlineComposite(color, v_UV);

    FragColor = vec4(color, 1.0);
}