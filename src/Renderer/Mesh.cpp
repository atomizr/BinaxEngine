#include "Renderer/Mesh.h"

namespace Binax {

MeshData Mesh::CreateCube()
{
    const glm::vec3 p[24] = {
        {-0.5f, -0.5f,  0.5f}, { 0.5f, -0.5f,  0.5f}, { 0.5f,  0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f},
        {-0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f, -0.5f}, { 0.5f,  0.5f, -0.5f}, {-0.5f,  0.5f, -0.5f},
        {-0.5f,  0.5f, -0.5f}, { 0.5f,  0.5f, -0.5f}, { 0.5f,  0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f},
        {-0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f,  0.5f}, {-0.5f, -0.5f,  0.5f},
        { 0.5f, -0.5f, -0.5f}, { 0.5f,  0.5f, -0.5f}, { 0.5f,  0.5f,  0.5f}, { 0.5f, -0.5f,  0.5f},
        {-0.5f, -0.5f, -0.5f}, {-0.5f,  0.5f, -0.5f}, {-0.5f,  0.5f,  0.5f}, {-0.5f, -0.5f,  0.5f}
    };
    const glm::vec3 n[6] = {
        {0,0,1},{0,0,-1},{0,1,0},{0,-1,0},{1,0,0},{-1,0,0}
    };
    const glm::vec2 uv[4] = { {0,0},{1,0},{1,1},{0,1} };

    MeshData out;
    out.Vertices.resize(24);
    for (int f = 0; f < 6; ++f)
        for (int v = 0; v < 4; ++v)
        {
            Vertex& V = out.Vertices[f * 4 + v];
            V.Position    = p[f * 4 + v];
            V.Normal      = n[f];
            V.UV          = uv[v];
            V.BoneIndices = glm::u8vec4(0);
            V.BoneWeights = glm::u8vec4(0);
        }

    out.Indices = {
         0, 1, 2,  2, 3, 0,
         4, 6, 5,  4, 7, 6,
        10, 9, 8,  8,11,10,
        12,13,14, 14,15,12,
        16,17,18, 18,19,16,
        20,22,21, 20,23,22
    };
    return out;
}

MeshData Mesh::CreateFullscreenQuad()
{
    MeshData out;
    out.Vertices.resize(4);
    const glm::vec3 pos[4] = {
        {-1,-1,0}, {1,-1,0}, {1,1,0}, {-1,1,0}
    };
    const glm::vec2 uvs[4] = { {0,0},{1,0},{1,1},{0,1} };
    for (int i = 0; i < 4; ++i)
    {
        out.Vertices[i].Position    = pos[i];
        out.Vertices[i].Normal      = glm::vec3(0,0,1);
        out.Vertices[i].UV          = uvs[i];
        out.Vertices[i].BoneIndices = glm::u8vec4(0);
        out.Vertices[i].BoneWeights = glm::u8vec4(0);
    }
    out.Indices = { 0, 1, 2, 2, 3, 0 };
    return out;
}

} 
