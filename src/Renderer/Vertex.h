#pragma once

#include <glm/glm.hpp>

namespace Binax {

// 40 байт. Раскладка должна совпадать с VAO-настройкой в MeshLibrary.cpp.
struct Vertex
{
    glm::vec3 Position;        // loc 0
    glm::vec3 Normal;          // loc 1
    glm::vec2 UV;              // loc 2
    glm::u8vec4 BoneIndices;   // loc 3 (integer attribute)
    glm::u8vec4 BoneWeights;   // loc 4 (normalized 0..255 → 0..1)
};

static_assert(sizeof(Vertex) == 40, "Vertex must be exactly 40 bytes");

} // namespace Binax