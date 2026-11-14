#version 460 core

in  vec2 v_UV;
out vec4 FragColor;

uniform sampler2D u_Texture;
uniform vec2      u_TexelSize;
uniform vec2      u_Direction;   // (1,0) H pass, (0,1) V pass

void main()
{
    vec2 step = u_TexelSize * u_Direction;

    vec3 c = texture(u_Texture, v_UV).rgb * 0.2270270270;
    c += texture(u_Texture, v_UV + step * 1.3846153846).rgb * 0.3162162162;
    c += texture(u_Texture, v_UV - step * 1.3846153846).rgb * 0.3162162162;
    c += texture(u_Texture, v_UV + step * 3.2307692308).rgb * 0.0702702703;
    c += texture(u_Texture, v_UV - step * 3.2307692308).rgb * 0.0702702703;

    FragColor = vec4(c, 1.0);
}