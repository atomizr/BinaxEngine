#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace Binax {

class Mesh
{
public:
    Mesh(const std::vector<glm::vec3>& positions,
         const std::vector<glm::vec3>& normals,
         const std::vector<glm::vec2>& uvs,
         const std::vector<uint32_t>& indices);
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void Bind() const;
    void Unbind() const;
    void Draw() const;

    uint32_t GetVertexCount()   const { return m_VertexCount; }
    uint32_t GetIndexCount()    const { return m_IndexCount;  }
    uint32_t GetTriangleCount() const { return m_IndexCount / 3; }

    static Mesh CreateCube();
    static Mesh CreateFullscreenQuad();

private:
    uint32_t m_VAO         = 0;
    uint32_t m_VBO         = 0;
    uint32_t m_EBO         = 0;
    uint32_t m_VertexCount = 0;
    uint32_t m_IndexCount  = 0;
};

}