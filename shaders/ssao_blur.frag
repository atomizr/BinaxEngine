#version 460 core

in  vec2 v_UV;
out vec4 FragColor;

uniform sampler2D u_AOTexture;
uniform sampler2D u_DepthTexture;
uniform vec2      u_TexelSize;      // 1/halfW, 1/halfH
uniform vec2      u_BlurAxis;       // (1,0) — H pass, (0,1) — V pass
uniform float     u_NearPlane;
uniform float     u_FarPlane;

const int RADIUS = 2;

float LinearizeDepth(float d)
{
    float z = d * 2.0 - 1.0;
    return (2.0 * u_NearPlane * u_FarPlane)
         / (u_FarPlane + u_NearPlane - z * (u_FarPlane - u_NearPlane));
}

void main()
{
    float centerZ = LinearizeDepth(texture(u_DepthTexture, v_UV).r);

    float result = 0.0;
    float weight = 0.0;

    for (int i = -RADIUS; i <= RADIUS; ++i)
    {
        vec2 offset = u_BlurAxis * u_TexelSize * float(i);
        vec2 suv    = v_UV + offset;

        float sampleZ = LinearizeDepth(texture(u_DepthTexture, suv).r);
        float ao      = texture(u_AOTexture, suv).r;

        // Bilateral — сохраняем края объектов.
        // Вес падает с квадратом разницы в линейной глубине (в метрах).
        float diff = abs(centerZ - sampleZ);
        float w    = exp(-diff * diff * 100.0);

        result += ao * w;
        weight += w;
    }

    FragColor = vec4(result / max(weight, 1e-5), 0.0, 0.0, 1.0);
}