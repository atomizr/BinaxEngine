#pragma once

#include <glm/glm.hpp>
#include <cstdint>

namespace Binax {

struct ReflectionProbeComponent
{
    glm::vec3 BoxExtents    = glm::vec3(5.0f);

    bool      Realtime      = false;
    bool      BoxProjection = true;
    bool      ReflectSkybox = true;
    uint32_t  Resolution    = 256;     // 128/256/512/1024 — первая задаёт для всех

    uint32_t  ProbeIndex    = 0;
    bool      NeedsBake     = true;
};

}