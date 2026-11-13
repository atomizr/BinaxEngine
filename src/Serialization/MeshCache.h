#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Renderer/Vertex.h"

namespace Binax {   // ← было Binax::Serialization

class MeshCache
{
public:
    static bool Save(const std::string& filepath,
                     const std::vector<Vertex>& vertices,
                     const std::vector<uint32_t>& indices);

    static bool Load(const std::string& filepath,
                     std::vector<Vertex>& outVertices,
                     std::vector<uint32_t>& outIndices);
};

}