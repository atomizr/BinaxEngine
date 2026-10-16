#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "Renderer/Vertex.h"

namespace Binax {

using MeshHandle = uint32_t;
constexpr MeshHandle kInvalidMesh = 0xFFFFFFFFu;

struct MeshGPU
{
    uint32_t  BaseVertex  = 0;
    uint32_t  VertexCount = 0;
    uint32_t  BaseIndex   = 0;
    uint32_t  IndexCount  = 0;
    glm::vec3 AABBMin     = glm::vec3(0.0f);
    float     _pad0       = 0.0f;
    glm::vec3 AABBMax     = glm::vec3(0.0f);
    float     _pad1       = 0.0f;
};
static_assert(sizeof(MeshGPU) == 48, "MeshGPU layout");

class MeshLibrary
{
public:
    static MeshLibrary& Get();

    MeshHandle Add(const std::string& name,
                   const std::vector<Vertex>& vertices,
                   const std::vector<uint32_t>& indices);
    MeshHandle Find(const std::string& name) const;

    const MeshGPU&     Get(MeshHandle h) const     { return m_Meshes[h]; }
    const std::string& GetName(MeshHandle h) const { return m_Names[h]; }
    uint32_t           GetMeshCount() const        { return (uint32_t)m_Meshes.size(); }

    void BindVAO() const;
    void Flush();
    void Clear();

private:
    MeshLibrary() = default;

    std::vector<Vertex>         m_CPUVertices;
    std::vector<uint32_t>       m_CPUIndices;
    std::vector<MeshGPU>        m_Meshes;
    std::vector<std::string>    m_Names;
    std::unordered_map<std::string, MeshHandle> m_ByName;

    uint32_t m_VAO = 0;
    uint32_t m_VBO = 0;
    uint32_t m_IBO = 0;
    uint32_t m_MeshSSBO = 0;
    bool     m_Dirty = false;
    bool     m_VAOReady = false;
};

} // namespace Binax