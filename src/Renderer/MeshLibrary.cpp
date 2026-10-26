#include "Renderer/MeshLibrary.h"
#include "Utils/Log.h"

#include <glad/glad.h>
#include <algorithm>

namespace Binax {

MeshLibrary& MeshLibrary::Get()
{
    static MeshLibrary instance;
    return instance;
}

MeshHandle MeshLibrary::Add(const std::string& name,
                            const std::vector<Vertex>& vertices,
                            const std::vector<uint32_t>& indices)
{
    auto it = m_ByName.find(name);
    if (it != m_ByName.end())
        return it->second;

    if (vertices.empty() || indices.empty())
    {
        BINAX_LOG_WARN("MeshLibrary::Add('{}') — empty mesh", name);
        return kInvalidMesh;
    }

    MeshGPU gpu;
    gpu.BaseVertex  = (uint32_t)m_CPUVertices.size();
    gpu.VertexCount = (uint32_t)vertices.size();
    gpu.BaseIndex   = (uint32_t)m_CPUIndices.size();
    gpu.IndexCount  = (uint32_t)indices.size();

    glm::vec3 mn = vertices[0].Position;
    glm::vec3 mx = vertices[0].Position;
    for (const auto& v : vertices)
    {
        mn = glm::min(mn, v.Position);
        mx = glm::max(mx, v.Position);
    }
    gpu.AABBMin = mn;
    gpu.AABBMax = mx;

    
    
    
    
    
    
    
    
    m_CPUVertices.insert(m_CPUVertices.end(), vertices.begin(), vertices.end());
    m_CPUIndices.insert(m_CPUIndices.end(), indices.begin(), indices.end());

    MeshHandle h = (MeshHandle)m_Meshes.size();
    m_Meshes.push_back(gpu);
    m_Names.push_back(name);
    m_ByName[name] = h;
    m_Dirty = true;

    BINAX_LOG_INFO("MeshLibrary: added '{}' (verts={}, idx={})",
                   name, gpu.VertexCount, gpu.IndexCount);
    return h;
}

MeshHandle MeshLibrary::Find(const std::string& name) const
{
    auto it = m_ByName.find(name);
    return it == m_ByName.end() ? kInvalidMesh : it->second;
}

void MeshLibrary::BindVAO() const
{
    glBindVertexArray(m_VAO);
}

void MeshLibrary::Flush()
{
    if (!m_Dirty && m_VAOReady) return;

    
    
    
    
    
    
    if (!m_VAOReady)
    {
        glGenVertexArrays(1, &m_VAO);
        glGenBuffers(1, &m_VBO);
        glGenBuffers(1, &m_IBO);

        glBindVertexArray(m_VAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IBO);

        
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(Vertex, Position));
        
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(Vertex, Normal));
        
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(Vertex, UV));
        
        glEnableVertexAttribArray(3);
        glVertexAttribIPointer(3, 4, GL_UNSIGNED_BYTE, sizeof(Vertex),
                               (void*)offsetof(Vertex, BoneIndices));
        
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex),
                              (void*)offsetof(Vertex, BoneWeights));

        m_VAOReady = true;
    }

    
    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(m_CPUVertices.size() * sizeof(Vertex)),
                 m_CPUVertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 (GLsizeiptr)(m_CPUIndices.size() * sizeof(uint32_t)),
                 m_CPUIndices.data(), GL_STATIC_DRAW);

    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    
    if (m_MeshSSBO == 0)
        glGenBuffers(1, &m_MeshSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_MeshSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 (GLsizeiptr)(m_Meshes.size() * sizeof(MeshGPU)),
                 m_Meshes.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    m_Dirty = false;
}

void MeshLibrary::Clear()
{
    m_CPUVertices.clear();
    m_CPUIndices.clear();
    m_Meshes.clear();
    m_Names.clear();
    m_ByName.clear();
    m_Dirty = true;

    if (m_VAO)      { glDeleteVertexArrays(1, &m_VAO);      m_VAO = 0; }
    if (m_VBO)      { glDeleteBuffers(1, &m_VBO);           m_VBO = 0; }
    if (m_IBO)      { glDeleteBuffers(1, &m_IBO);           m_IBO = 0; }
    if (m_MeshSSBO) { glDeleteBuffers(1, &m_MeshSSBO);      m_MeshSSBO = 0; }
    m_VAOReady = false;
}

} 
