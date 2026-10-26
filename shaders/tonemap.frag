#version 460 core

// ---------------------------------------------------------------------------
// Финальный пост-проход: lens → CA → bloom → tonemap → color grade →
// vignette → film grain.
//
// Тонамаппер выбирается в Post Processing (u_TonemapMode):
//   0 ACES, 1 Reinhard, 2 Filmic (Uncharted 2), 3 AgX, 4 Linear.
// ---------------------------------------------------------------------------

in  vec2 v_UV;
out vec4 FragColor;

uniform sampler2D u_HDRTexture;
uniform sampler2D u_BloomTexture;

uniform float u_Exposure;
uniform int   u_TonemapMode;
uniform int   u_BloomEnabled;
uniform float u_BloomIntensity;

// ---- Color grade ----
uniform float u_Contrast;
uniform float u_Saturation;
uniform float u_Temperature;
uniform float u_Tint;
uniform vec3  u_Lift;
uniform vec3  u_Gamma;
uniform vec3  u_Gain;

uniform int   u_LensEnabled;
uniform float u_LensDistortion;

uniform int   u_CAEnabled;
uniform float u_CAStrength;

uniform int   u_VignetteEnabled;
uniform float u_VignetteIntensity;
uniform float u_VignetteSmoothness;

uniform int   u_GrainEnabled;
uniform float u_GrainIntensity;
uniform float u_Time;
uniform vec2  u_Resolution;

// ---------------------------------------------------------------------------
// Тонамапперы
// ---------------------------------------------------------------------------

// ACES fitted (Narkowicz 2015).
vec3 TonemapACES(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 TonemapReinhard(vec3 x)
{
    return x / (1.0 + x);
}

// Uncharted 2 filmic curve (Hable).
vec3 TonemapFilmic(vec3 x)
{
    const float A = 0.15;
    const float B = 0.50;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.02;
    const float F = 0.30;

    const vec3 curve = ((x * (A * x + C * B) + D * E) /
                        (x * (A * x + B) + D * F)) - E / F;

    const float W = 11.2;
    const float white = ((W * (A * W + C * B) + D * E) /
                         (W * (A * W + B) + D * F)) - E / F;
    return curve / white;
}

// AgX (приближение Blender): inset-матрица → лог-кодирование → сигмоида →
// outset-матрица. Не эталонная реализация, но даёт характерный AgX-вид.
vec3 TonemapAgX(vec3 x)
{
    const mat3 agxIn = mat3(
        0.842479062253094, 0.0784335999999992, 0.0792237451477643,
        0.0423282422610123, 0.878468636469772, 0.0791661274605434,
        0.0423756549057051, 0.0784336, 0.879142973793104);

    const mat3 agxOut = mat3(
         1.19687900512017, -0.0980208811401368, -0.0990297440797205,
        -0.0528968517574562, 1.15190312990417, -0.0989611768448433,
        -0.0529716355144438, -0.0980434501171241, 1.15107367264116);

    const float minEv = -12.47393;
    const float maxEv = 4.026069;

    x = agxIn * max(x, vec3(0.0));
    x = clamp(log2(max(x, vec3(1e-10))), minEv, maxEv);
    x = (x - minEv) / (maxEv - minEv);

    const vec3 x2 = x * x;
    const vec3 x4 = x2 * x2;
    x = 15.5 * x4 * x2 - 40.14 * x4 * x
      + 31.96 * x4 - 6.868 * x2 * x
      + 0.4298 * x2 + 0.1191 * x - 0.00232;

    x = agxOut * x;
    return clamp(x, 0.0, 1.0);
}

vec3 ApplyTonemap(vec3 x)
{
    if (u_TonemapMode == 0) return TonemapACES(x);
    if (u_TonemapMode == 1) return TonemapReinhard(x);
    if (u_TonemapMode == 2) return TonemapFilmic(x);
    if (u_TonemapMode == 3) return TonemapAgX(x);
    return clamp(x, 0.0, 1.0);
}

// Бочка/подушка: смещение пропорционально квадрату радиуса.
vec2 DistortUV(vec2 uv)
{
    if (u_LensEnabled == 0)
        return uv;

    const vec2  c  = uv - 0.5;
    const float r2 = dot(c, c);
    return clamp(uv + c * r2 * u_LensDistortion * 4.0, vec2(0.0), vec2(1.0));
}

float Hash(vec2 p)
{
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
    const vec2 uv = DistortUV(v_UV);

    // ---- Chromatic aberration ----
    const vec2  dir = uv - 0.5;
    const float ca  = (u_CAEnabled != 0) ? u_CAStrength * 0.01 : 0.0;

    vec3 hdr;
    hdr.r = texture(u_HDRTexture, clamp(uv + dir * ca, vec2(0.0), vec2(1.0))).r;
    hdr.g = texture(u_HDRTexture, uv).g;
    hdr.b = texture(u_HDRTexture, clamp(uv - dir * ca, vec2(0.0), vec2(1.0))).b;

    // ---- Bloom (аддитивно, в HDR до тонамаппинга) ----
    if (u_BloomEnabled != 0)
        hdr += texture(u_BloomTexture, uv).rgb * u_BloomIntensity;

    // ---- Экспозиция + выбранный тонамаппер ----
    hdr = max(hdr * u_Exposure, vec3(0.0));

    vec3 color = ApplyTonemap(hdr);
    color = pow(color, vec3(1.0 / 2.2));

    // ---- Color grade (в display space) ----
    // Баланс белого: температура тянет R↑/B↓, tint — G↑.
    color *= vec3(1.0 + 0.20 * u_Temperature,
                  1.0 + 0.10 * u_Tint,
                  1.0 - 0.20 * u_Temperature);

    // Lift / Gamma / Gain — классическая формула цветокоррекции.
    color = color * u_Gain + u_Lift * (1.0 - color);
    color = pow(max(color, vec3(0.0)), 1.0 / max(u_Gamma, vec3(0.01)));

    // Контраст и насыщенность.
    color = (color - 0.5) * u_Contrast + 0.5;
    const float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luma), color, u_Saturation);
    color = max(color, vec3(0.0));

    // ---- Vignette ----
    if (u_VignetteEnabled != 0)
    {
        const float d     = length(v_UV - 0.5) * 1.41421356;   // 0 в центре, 1 в углу
        const float inner = clamp(1.0 - u_VignetteSmoothness, 0.0, 1.0);
        const float vig   = smoothstep(inner, 1.0, d);
        color *= 1.0 - u_VignetteIntensity * vig;
    }

    // ---- Film grain ----
    if (u_GrainEnabled != 0)
    {
        const float n = Hash(v_UV * u_Resolution + vec2(u_Time * 137.0)) - 0.5;
        color += n * u_GrainIntensity;
    }

    FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
