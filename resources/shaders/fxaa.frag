#version 460 core

// FXAA 3.11 — Timothy Lottes (NVIDIA), адаптировано под GLSL 460.
// Вход: LDR-картинка после тонамаппинга/гаммы (RGBA8).

in vec2 v_UV;
out vec4 FragColor;

uniform sampler2D u_ColorTexture;

uniform vec2  u_TexelSize;          // 1.0 / resolution
uniform float u_EdgeThreshold;      // относительный порог (0.063)
uniform float u_EdgeThresholdMin;   // абсолютный порог   (0.0312)
uniform float u_SubpixelBlending;   // 0..1, субпиксельное сглаживание

float Luma(vec3 rgb)
{
    return dot(rgb, vec3(0.299, 0.587, 0.114));
}

void main()
{
    vec3 rgbM  = texture(u_ColorTexture, v_UV).rgb;
    vec3 rgbNW = textureOffset(u_ColorTexture, v_UV, ivec2(-1, -1)).rgb;
    vec3 rgbNE = textureOffset(u_ColorTexture, v_UV, ivec2( 1, -1)).rgb;
    vec3 rgbSW = textureOffset(u_ColorTexture, v_UV, ivec2(-1,  1)).rgb;
    vec3 rgbSE = textureOffset(u_ColorTexture, v_UV, ivec2( 1,  1)).rgb;

    float lumaM  = Luma(rgbM);
    float lumaNW = Luma(rgbNW);
    float lumaNE = Luma(rgbNE);
    float lumaSW = Luma(rgbSW);
    float lumaSE = Luma(rgbSE);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    float lumaRange = lumaMax - lumaMin;

    // ---- Ранний выход на плоских участках ----
    if (lumaRange < max(u_EdgeThresholdMin, lumaMax * u_EdgeThreshold))
    {
        FragColor = vec4(rgbM, 1.0);
        return;
    }

    // ---- Субпиксельный алиасинг ----
    float lumaAvg = (lumaNW + lumaNE + lumaSW + lumaSE + lumaM * 4.0) * 0.125;
    float subpixel = clamp(abs(lumaAvg - lumaM) / max(lumaRange, 1e-4), 0.0, 1.0);
    subpixel = subpixel * subpixel * u_SubpixelBlending;

    // ---- Направление поиска (перпендикуляр к границе) ----
    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.25 * (1.0 / 8.0),
                          1.0 / 128.0);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, vec2(-8.0), vec2(8.0)) * u_TexelSize;

    // ---- Двойная выборка вдоль кромки ----
    vec3 rgbA = 0.5 * (
        texture(u_ColorTexture, v_UV + dir * (1.0 / 3.0 - 0.5)).rgb +
        texture(u_ColorTexture, v_UV + dir * (2.0 / 3.0 - 0.5)).rgb);

    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture(u_ColorTexture, v_UV + dir * -0.5).rgb +
        texture(u_ColorTexture, v_UV + dir *  0.5).rgb);

    float lumaB = Luma(rgbB);
    vec3 result = ((lumaB < lumaMin) || (lumaB > lumaMax)) ? rgbA : rgbB;

    result = mix(result, rgbM, subpixel);

    FragColor = vec4(result, 1.0);
}