#version 460 core

// ---------------------------------------------------------------------------
// TAA (temporal anti-aliasing).
//
// Вход: текущий HDR-кадр, velocity-буфер и история предыдущего кадра.
// Выход: сглаженный HDR.
//
// Клампинг истории — variance clipping в YCoCg (mean ± gamma*sigma): держит
// историю в пределах локального диапазона текущего кадра, убирая ghosting
// на движущихся объектах, но не «замораживая» плоские области.
//
// ВАЖНО (эмпирически проверено): репроекция по Y инвертируется —
// prevUV = uv + vec2(vel.x, -vel.y). Вариант с +vel по обеим осям даёт
// в разы больше остаточного дрожания.
// ---------------------------------------------------------------------------

in  vec2 v_UV;
out vec4 FragColor;

uniform sampler2D u_CurrentTexture;
uniform sampler2D u_VelocityTexture;
uniform sampler2D u_HistoryTexture;
uniform sampler2D u_DepthTexture;

uniform vec2  u_TexelSize;      // 1 / resolution
uniform vec2  u_Resolution;
uniform float u_HistoryWeight;  // 0..1
uniform int   u_HistoryValid;
uniform float u_ClampGamma;     // ширина variance-клампа

uniform mat4  u_ViewProj;       // текущая (с jitter) view-projection
uniform mat4  u_PrevViewProj;   // предыдущая (с jitter)
uniform mat4  u_InvViewProj;
uniform vec3  u_CameraPosition;

vec3 RGBToYCoCg(vec3 c)
{
    return vec3(dot(c, vec3(0.25,  0.5,  0.25)),
                dot(c, vec3(0.5,   0.0, -0.5)),
                dot(c, vec3(-0.25, 0.5, -0.25)));
}

vec3 YCoCgToRGB(vec3 c)
{
    return vec3(c.x + c.y - c.z,
                c.x + c.z,
                c.x - c.y - c.z);
}

bool IsFinite(vec3 v)
{
    const bvec3 isNan = notEqual(v, v);
    const bvec3 isInf = greaterThanEqual(abs(v), vec3(1e30));
    return !(any(isNan) || any(isInf));
}

// Velocity для неба: репроецируем направление луча (w = 0) — без параллакса
// от перемещения камеры. Та же UV-конвенция, что и у геометрии.
vec2 SkyVelocity(vec2 uv)
{
    const vec4 far = u_InvViewProj * vec4(uv * 2.0 - 1.0, 1.0, 1.0);
    const vec3 dir = normalize(far.xyz / far.w - u_CameraPosition);

    const vec4 prevClip = u_PrevViewProj * vec4(dir, 0.0);
    const vec4 currClip = u_ViewProj     * vec4(dir, 0.0);

    if (prevClip.w <= 1e-5 || currClip.w <= 1e-5)
        return vec2(0.0);

    return (prevClip.xy / prevClip.w - currClip.xy / currClip.w) * 0.5;
}

void main()
{
    const vec3  current = texture(u_CurrentTexture, v_UV).rgb;
    const float depth   = texture(u_DepthTexture, v_UV).r;

    vec2 vel = texture(u_VelocityTexture, v_UV).rg;
    if (depth >= 0.999999)
        vel = SkyVelocity(v_UV);

    if (any(isnan(vel)) || any(isinf(vel)))
        vel = vec2(0.0);

    const vec2 prevUV = v_UV + vec2(vel.x, -vel.y);

    if (u_HistoryValid == 0 ||
        !IsFinite(current) ||
        any(lessThan(prevUV, vec2(0.0))) ||
        any(greaterThan(prevUV, vec2(1.0))))
    {
        FragColor = vec4(current, 1.0);
        return;
    }

    // ---- Neighborhood statistics в YCoCg ----
    const vec3 curY = RGBToYCoCg(max(current, vec3(0.0)));

    vec3 m1 = vec3(0.0);
    vec3 m2 = vec3(0.0);
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            const vec3 s = RGBToYCoCg(max(texture(u_CurrentTexture,
                                                  v_UV + vec2(x, y) * u_TexelSize).rgb,
                                           vec3(0.0)));
            m1 += s;
            m2 += s * s;
        }
    }
    m1 /= 9.0;
    m2 /= 9.0;

    const vec3 sigma = sqrt(max(m2 - m1 * m1, vec3(0.0)));
    vec3 lo = m1 - u_ClampGamma * sigma;
    vec3 hi = m1 + u_ClampGamma * sigma;
    lo = min(lo, curY);
    hi = max(hi, curY);

    // ---- История ----
    const vec3 historyY = RGBToYCoCg(max(texture(u_HistoryTexture, prevUV).rgb,
                                         vec3(0.0)));
    const vec3 clampedHistory = clamp(historyY, lo, hi);

    // ---- Вес истории ----
    const float speedPx = length(vel * u_Resolution);
    float weight = u_HistoryWeight;
    weight *= 1.0 - smoothstep(24.0, 96.0, speedPx);
    weight = clamp(weight, 0.0, 0.98);

    FragColor = vec4(YCoCgToRGB(mix(curY, clampedHistory, weight)), 1.0);
}
