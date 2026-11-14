#version 460 core

in  vec2 v_UV;
out vec4 FragColor;

uniform sampler2D u_DepthTexture;
uniform sampler2D u_NormalTexture;

uniform mat4 u_View;
uniform mat4 u_Projection;
uniform mat4 u_InvProjection;

uniform vec3  u_Kernel[32];
uniform int   u_Samples;
uniform float u_Radius;
uniform float u_Bias;
uniform float u_Power;

const float PI = 3.14159265359;

vec3 ReconstructViewPos(vec2 uv, float depth)
{
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view = u_InvProjection * clip;
    return view.xyz / view.w;
}

float Hash(vec2 p)
{
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
    float depth = texture(u_DepthTexture, v_UV).r;
    if (depth >= 0.99999)
    {
        FragColor = vec4(1.0, 0.0, 0.0, 1.0);
        return;
    }

    vec3 viewPos = ReconstructViewPos(v_UV, depth);

    vec3 worldNormal = texture(u_NormalTexture, v_UV).rgb;
    if (dot(worldNormal, worldNormal) < 1e-4)
    {
        FragColor = vec4(1.0, 0.0, 0.0, 1.0);
        return;
    }

    vec3 normal = normalize(mat3(u_View) * worldNormal);

    // ---- Per-pixel random rotation around normal -----------------------
    float angle = Hash(v_UV * 1024.0) * 2.0 * PI;

    vec3 baseTangent = (abs(normal.y) < 0.99)
        ? normalize(cross(normal, vec3(0, 1, 0)))
        : normalize(cross(normal, vec3(1, 0, 0)));
    vec3 baseBitangent = cross(normal, baseTangent);

    float ca = cos(angle);
    float sa = sin(angle);
    vec3 tangent   = ca * baseTangent + sa * baseBitangent;
    vec3 bitangent = -sa * baseTangent + ca * baseBitangent;

    mat3 TBN = mat3(tangent, bitangent, normal);

    // ---- SSAO sampling -------------------------------------------------
    float occlusion = 0.0;
    int samples = min(u_Samples, 32);

    for (int i = 0; i < samples; ++i)
    {
        vec3 sampleViewPos = viewPos + TBN * (u_Kernel[i] * u_Radius);

        vec4 projClip = u_Projection * vec4(sampleViewPos, 1.0);
        vec2 sampleUV = projClip.xy / projClip.w * 0.5 + 0.5;

        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 ||
            sampleUV.y < 0.0 || sampleUV.y > 1.0)
            continue;

        float sampleDepthRaw = texture(u_DepthTexture, sampleUV).r;
        vec3  sampleDepthPos = ReconstructViewPos(sampleUV, sampleDepthRaw);

        // Occluded, если в этой точке сцены поверхность ближе к камере,
        // чем наш пробник
        if (sampleDepthPos.z > sampleViewPos.z + u_Bias)
        {
            float rangeCheck = smoothstep(
                0.0, 1.0,
                u_Radius / abs(viewPos.z - sampleDepthPos.z));
            occlusion += rangeCheck;
        }
    }

    occlusion = 1.0 - (occlusion / float(samples));
    occlusion = pow(clamp(occlusion, 0.0, 1.0), u_Power);

    FragColor = vec4(occlusion, 0.0, 0.0, 1.0);
}