#version 460 core

in  vec2 v_UV;
out vec4 FragColor;

uniform samplerCube u_EnvironmentMap;
uniform int   u_HasEnvironment;
uniform mat4  u_InvViewProj;
uniform vec3  u_ProbePos;
uniform float u_SkyboxRotation;
uniform float u_SkyboxBrightness;

void main()
{
    if (u_HasEnvironment == 0)
    {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec4 clip  = vec4(v_UV * 2.0 - 1.0, 1.0, 1.0);
    vec4 world = u_InvViewProj * clip;
    vec3 dir   = normalize(world.xyz / world.w - u_ProbePos);

    float c = cos(u_SkyboxRotation);
    float s = sin(u_SkyboxRotation);
    vec3 rotated = vec3(
        dir.x * c - dir.z * s,
        dir.y,
        dir.x * s + dir.z * c);

    FragColor = vec4(texture(u_EnvironmentMap, rotated).rgb * u_SkyboxBrightness, 1.0);
}