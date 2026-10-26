#version 460 core

// ---------------------------------------------------------------------------
// Motion blur по velocity-буферу.
//
// Velocity в G-buffer содержит и движение объектов, и субпиксельный джиттер
// TAA. Джиттер вычитается как константа (u_JitterDelta), иначе статичная
// сцена «размазывалась» бы из-за сглаживания.
//
// Выборки вдоль вектора движения; глубина отсекает фон, чтобы он не затекал
// на передний план (и наоборот).
// ---------------------------------------------------------------------------

in  vec2 v_UV;
out vec4 FragColor;

uniform sampler2D u_HDRTexture;
uniform sampler2D u_VelocityTexture;
uniform sampler2D u_DepthTexture;

uniform vec2  u_JitterDelta;   // (prevJitter - jitter) * 0.5, в UV
uniform float u_Strength;
uniform int   u_Samples;
uniform float u_NearPlane;
uniform float u_FarPlane;

float LinearDepth(float d)
{
    return u_NearPlane * u_FarPlane /
           max(u_FarPlane - d * (u_FarPlane - u_NearPlane), 1e-6);
}

void main()
{
    const vec3 center = texture(u_HDRTexture, v_UV).rgb;

    vec2 vel = texture(u_VelocityTexture, v_UV).rg - u_JitterDelta;
    vel *= u_Strength;

    const vec2 res = vec2(textureSize(u_HDRTexture, 0));
    const float lenPx = length(vel * res);

    // Меньше половины пикселя — движение незаметно, не тратим выборки.
    if (lenPx < 0.5)
    {
        FragColor = vec4(center, 1.0);
        return;
    }

    // Ограничение длины: резкий поворот камеры не должен давать полосы.
    const float kMaxPixels = 32.0;
    if (lenPx > kMaxPixels)
        vel *= kMaxPixels / lenPx;

    const float zc = LinearDepth(texture(u_DepthTexture, v_UV).r);

    vec3 sum  = center;
    float wsum = 1.0;

    const int samples = clamp(u_Samples, 2, 16);
    for (int i = 1; i < samples; ++i)
    {
        const float t = float(i) / float(samples - 1) - 0.5;   // -0.5..0.5
        const vec2 suv = v_UV + vel * t;

        if (any(lessThan(suv, vec2(0.0))) || any(greaterThan(suv, vec2(1.0))))
            continue;

        const float zs = LinearDepth(texture(u_DepthTexture, suv).r);
        if (abs(zs - zc) > 0.1 * zc)
            continue;   // другая поверхность — не подмешиваем

        sum  += texture(u_HDRTexture, suv).rgb;
        wsum += 1.0;
    }

    FragColor = vec4(sum / wsum, 1.0);
}
