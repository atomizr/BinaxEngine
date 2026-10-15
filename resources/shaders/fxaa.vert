#version 460 core

layout(location = 0) in vec3 a_Position;
layout(location = 2) in vec2 a_UV;

out vec2 v_UV;

void main()
{
    v_UV = a_UV;
    gl_Position = vec4(a_Position.xy, 0.0, 1.0);
}