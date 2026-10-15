#include "Mesh.h"
#include <glad/glad.h>

namespace Binax {

Mesh::Mesh(const std::vector<glm::vec3>& positions,
           const std::vector<glm::vec3>& normals,
           const std::vector<glm::vec2>& uvs,
           const std::vector<uint32_t>& indices)
    : m_VertexCount(static_cast<uint32_t>(positions.size()))
    , m_IndexCount(static_cast<uint32_t>(indices.size()))
{
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);

    glBindVertexArray(m_VAO);

    std::vector<float> vertexData;
    vertexData.reserve(positions.size() * (3 + 3 + 2));
    for (size_t i = 0; i < positions.size(); ++i)
    {
        vertexData.push_back(positions[i].x);
        vertexData.push_back(positions[i].y);
        vertexData.push_back(positions[i].z);
        vertexData.push_back(normals[i].x);
        vertexData.push_back(normals[i].y);
        vertexData.push_back(normals[i].z);
        vertexData.push_back(uvs[i].x);
        vertexData.push_back(uvs[i].y);
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

    glBindVertexArray(0);
}

Mesh::~Mesh()
{
    glDeleteVertexArrays(1, &m_VAO);
    glDeleteBuffers(1, &m_VBO);
    glDeleteBuffers(1, &m_EBO);
}

void Mesh::Bind() const
{
    glBindVertexArray(m_VAO);
}

void Mesh::Unbind() const
{
    glBindVertexArray(0);
}

void Mesh::Draw() const
{
    if (m_IndexCount == 0)
        return;
    glDrawElements(GL_TRIANGLES, m_IndexCount, GL_UNSIGNED_INT, nullptr);
}


Mesh Mesh::CreateCube()
{
    std::vector<glm::vec3> positions = {
        {-0.5f, -0.5f,  0.5f}, { 0.5f, -0.5f,  0.5f}, { 0.5f,  0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f},
        {-0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f, -0.5f}, { 0.5f,  0.5f, -0.5f}, {-0.5f,  0.5f, -0.5f},
        {-0.5f,  0.5f, -0.5f}, { 0.5f,  0.5f, -0.5f}, { 0.5f,  0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f},
        {-0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f,  0.5f}, {-0.5f, -0.5f,  0.5f},
        { 0.5f, -0.5f, -0.5f}, { 0.5f,  0.5f, -0.5f}, { 0.5f,  0.5f,  0.5f}, { 0.5f, -0.5f,  0.5f},
        {-0.5f, -0.5f, -0.5f}, {-0.5f,  0.5f, -0.5f}, {-0.5f,  0.5f,  0.5f}, {-0.5f, -0.5f,  0.5f}
    };

    std::vector<glm::vec3> normals = {
        { 0, 0, 1}, { 0, 0, 1}, { 0, 0, 1}, { 0, 0, 1},
        { 0, 0,-1}, { 0, 0,-1}, { 0, 0,-1}, { 0, 0,-1},
        { 0, 1, 0}, { 0, 1, 0}, { 0, 1, 0}, { 0, 1, 0},
        { 0,-1, 0}, { 0,-1, 0}, { 0,-1, 0}, { 0,-1, 0},
        { 1, 0, 0}, { 1, 0, 0}, { 1, 0, 0}, { 1, 0, 0},
        {-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}
    };

    std::vector<glm::vec2> uvs = {
        {0,0},{1,0},{1,1},{0,1},
        {0,0},{1,0},{1,1},{0,1},
        {0,0},{1,0},{1,1},{0,1},
        {0,0},{1,0},{1,1},{0,1},
        {0,0},{1,0},{1,1},{0,1},
        {0,0},{1,0},{1,1},{0,1}
    };

    std::vector<uint32_t> indices = {
        0, 1, 2,   2, 3, 0,
        4, 6, 5,   4, 7, 6,
        10, 9, 8,  8, 11, 10,
        12, 13, 14, 14, 15, 12,
        16, 17, 18, 18, 19, 16,
        20, 22, 21, 20, 23, 22
    };

    return Mesh(positions, normals, uvs, indices);
}

Mesh Mesh::CreateFullscreenQuad()
{
    std::vector<glm::vec3> positions = {
        {-1.0f, -1.0f, 0.0f},
        { 1.0f, -1.0f, 0.0f},
        { 1.0f,  1.0f, 0.0f},
        {-1.0f,  1.0f, 0.0f}
    };
    std::vector<glm::vec3> normals(4, glm::vec3(0,0,1));
    std::vector<glm::vec2> uvs = {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f}
    };
    std::vector<uint32_t> indices = {
        0, 1, 2,
        2, 3, 0
    };

    return Mesh(positions, normals, uvs, indices);
}

}