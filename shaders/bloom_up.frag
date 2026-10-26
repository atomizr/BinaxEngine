#version 460 core

// ---------------------------------------------------------------------------
// Bloom: upsample с 3x3 tent-фильтром. Рисуется с аддитивным блендингом
// поверх уже даунсемпленной мип-текстуры.
// ---------------------------------------------------------------------------

in  vec2 v_UV;
out vec4 FragColor;

uniform sampler2D u_Source;
uniform vec2 u_TexelSize;    // 1 / размер источника

void main()
{
    const vec2 t = u_TexelSize;

    vec3 c = texture(u_Source, v_UV).rgb * 4.0;
    c += texture(u_Source, v_UV + vec2(-t.x, 0.0)).rgb * 2.0;
    c += texture(u_Source, v_UV + vec2( t.x, 0.0)).rgb * 2.0;
    c += texture(u_Source, v_UV + vec2(0.0, -t.y)).rgb * 2.0;
    c += texture(u_Source, v_UV + vec2(0.0,  t.y)).rgb * 2.0;
    c += texture(u_Source, v_UV + vec2(-t.x, -t.y)).rgb;
    c += texture(u_Source, v_UV + vec2( t.x, -t.y)).rgb;
    c += texture(u_Source, v_UV + vec2(-t.x,  t.y)).rgb;
    c += texture(u_Source, v_UV + vec2( t.x,  t.y)).rgb;

    FragColor = vec4(c / 16.0, 1.0);
}
