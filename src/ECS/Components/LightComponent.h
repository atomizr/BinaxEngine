
#pragma once

#include <glm/glm.hpp>

namespace Binax {

enum class LightType
{
    Directional,
    Point,
    Spot
};

struct LightComponent
{
    LightType Type = LightType::Directional;
    glm::vec3 Color = glm::vec3(1.0f);
    float Intensity = 1.0f;
    float Range = 10.0f;      
    float InnerCutoff = 0.8f; 
    float OuterCutoff = 0.9f; 
};

} 