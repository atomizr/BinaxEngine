#pragma once

#include <glm/glm.hpp>

namespace Binax {

enum class LightType
{
    Directional,
    Point,
    Spot
};

enum class ShadowMode
{
    None = 0,
    Hard = 1,
    Soft = 2,
};

enum class ShadowResolutionMode
{
    Global = 0,   // использовать ShadowSettings::GlobalResolution
    Custom = 1,   // ShadowResolution для этого источника
};

struct LightComponent
{
    LightType Type = LightType::Directional;
    glm::vec3 Color = glm::vec3(1.0f);
    float Intensity = 1.0f;
    float Range = 10.0f;
    float InnerCutoff = 0.9f;
    float OuterCutoff = 0.8f;

    // ---- Shadow (Directional only, пока) ---------------------------------
    // FIX #11: не называем поле так же, как тип — иначе затенение типа
    // и сюрпризы при `decltype`, `using namespace`, ADL.
    ShadowMode           Mode           = ShadowMode::None;
    ShadowResolutionMode ResolutionMode = ShadowResolutionMode::Global;
    int                  ShadowResolution     = 2048;   // при Custom: 512/1024/2048/4096
    float                ShadowStrength       = 1.0f;   // 0..1 = насколько тёмная тень
    float                BiasMultiplier       = 1.0f;   // × глобальный DepthBias
};

}