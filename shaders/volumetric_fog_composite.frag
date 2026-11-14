#version 460 core

in  vec2 v_UV;
out vec4 FragColor;

uniform sampler2D u_FogTexture;

void main()
{
    vec3 fog = texture(u_FogTexture, v_UV).rgb;
    // additive blending через glBlendFunc(GL_ONE, GL_ONE) — alpha=0
    FragColor = vec4(fog, 0.0);
}