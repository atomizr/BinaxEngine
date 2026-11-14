#version 460 core

uniform vec3  u_LightPos;
uniform float u_Far;

void main()
{
    // Depth-only; линейную дистанцию восстанавливаем в lighting.frag
    // через LinearizeDepth() по near/far.
}