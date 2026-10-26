#version 460 core

// ---------------------------------------------------------------------------
// Bloom: bright pass + downsample (13 taps, COD-стиль).
// Первый проход (u_FirstPass = 1) дополнительно применяет порог яркости.
// ---------------------------------------------------------------------------

in  vec2 v_UV;
out vec4 FragColor;

uniform sampler2D u_Source;
uniform vec2  u_TexelSize;    // 1 / размер источника
uniform int   u_FirstPass;
uniform float u_Threshold;

vec3 Downsample13(vec2 uv, vec2 texel)
{
    const vec3 a = texture(u_Source, uv + texel * vec2(-2.0, -2.0)).rgb;
    const vec3 b = texture(u_Source, uv + texel * vec2( 0.0, -2.0)).rgb;
    const vec3 c = texture(u_Source, uv + texel * vec2( 2.0, -2.0)).rgb;
    const vec3 d = texture(u_Source, uv + texel * vec2(-2.0,  0.0)).rgb;
    const vec3 e = texture(u_Source, uv).rgb;
    const vec3 f = texture(u_Source, uv + texel * vec2( 2.0,  0.0)).rgb;
    const vec3 g = texture(u_Source, uv + texel * vec2(-2.0,  2.0)).rgb;
    const vec3 h = texture(u_Source, uv + texel * vec2( 0.0,  2.0)).rgb;
    const vec3 i = texture(u_Source, uv + texel * vec2( 2.0,  2.0)).rgb;

    const vec3 j = texture(u_Source, uv + texel * vec2(-1.0, -1.0)).rgb;
    const vec3 k = texture(u_Source, uv + texel * vec2( 1.0, -1.0)).rgb;
    const vec3 l = texture(u_Source, uv + texel * vec2(-1.0,  1.0)).rgb;
    const vec3 m = texture(u_Source, uv + texel * vec2( 1.0,  1.0)).rgb;

    vec3 result  = e * 0.125;
    result += (a + c + g + i) * 0.03125;
    result += (b + d + f + h) * 0.0625;
    result += (j + k + l + m) * 0.125;
    return result;
}

vec3 BrightPass(vec3 c, float threshold)
{
    const float br = max(c.r, max(c.g, c.b));
    const float knee = max(threshold * 0.5, 1e-4);

    float soft = clamp(br - threshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee);

    const float contrib = max(soft, br - threshold) / max(br, 1e-4);
    return c * contrib;
}

void main()
{
    vec3 c = Downsample13(v_UV, u_TexelSize);

    if (u_FirstPass != 0)
        c = BrightPass(c, u_Threshold);

    FragColor = vec4(c, 1.0);
}
