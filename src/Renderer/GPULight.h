
#pragma once

#include <glm/glm.hpp>

namespace Binax {




struct GPULight
{
    enum Type : int { Directional = 0, Point = 1, Spot = 2 };

    glm::vec3 Position    = glm::vec3(0.0f);
    float     Range       = 10.0f;

    glm::vec3 Direction   = glm::vec3(0.0f, -1.0f, 0.0f);
    float     Intensity   = 1.0f;

    glm::vec3 Color       = glm::vec3(1.0f);
    int       LightType   = Directional;

    float     InnerCutoff = 0.8f;
    float     OuterCutoff = 0.9f;
    float     _pad0       = 0.0f;
    float     _pad1       = 0.0f;
};

constexpr int kMaxLights = 32;

} 