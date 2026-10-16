#pragma once
#include <vector>
#include <cstdint>
#include "Renderer/Vertex.h"

namespace Binax {

struct MeshData
{
    std::vector<Vertex>   Vertices;
    std::vector<uint32_t> Indices;
};

class Mesh
{
public:
    static MeshData CreateCube();
    static MeshData CreateFullscreenQuad();
private:
    Mesh() = delete;
};

}
