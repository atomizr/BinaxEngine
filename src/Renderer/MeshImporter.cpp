#include "Renderer/MeshImporter.h"
#include "Utils/Log.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>

#include <filesystem>

namespace Binax {

namespace {

void ExtractMeshData(const aiMesh* src, MeshData& dst)
{
    dst.Vertices.resize(src->mNumVertices);

    for (unsigned i = 0; i < src->mNumVertices; ++i)
    {
        Vertex& v = dst.Vertices[i];

        const aiVector3D& p = src->mVertices[i];
        v.Position = glm::vec3(p.x, p.y, p.z);

        if (src->HasNormals())
        {
            const aiVector3D& n = src->mNormals[i];
            v.Normal = glm::vec3(n.x, n.y, n.z);
        }
        else
        {
            v.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }

        if (src->HasTextureCoords(0))
        {
            const aiVector3D& t = src->mTextureCoords[0][i];
            // aiProcess_FlipUVs уже сделал V-flip под GL
            v.UV = glm::vec2(t.x, t.y);
        }
        else
        {
            v.UV = glm::vec2(0.0f);
        }

        v.BoneIndices = glm::u8vec4(0);
        v.BoneWeights = glm::u8vec4(0);
    }

    dst.Indices.reserve(src->mNumFaces * 3);
    for (unsigned f = 0; f < src->mNumFaces; ++f)
    {
        const aiFace& face = src->mFaces[f];
        if (face.mNumIndices != 3)
            continue;
        dst.Indices.push_back(face.mIndices[0]);
        dst.Indices.push_back(face.mIndices[1]);
        dst.Indices.push_back(face.mIndices[2]);
    }
}

std::string GetTexturePath(const aiMaterial* mat, aiTextureType type)
{
    aiString texPath;
    if (mat->GetTexture(type, 0, &texPath) != AI_SUCCESS)
        return {};

    const std::string p = texPath.C_Str();
    if (p.empty() || p[0] == '*')
        return {};   // embedded — пока не поддерживаем

    return p;
}

} 

bool MeshImporter::LoadFromFile(const std::string& path, ImportedModel& out)
{
    Assimp::Importer importer;

    // ВАЖНО: НЕ ставим aiProcess_FlipUVs.
    // Текстуры загружаются с stbi_set_flip_vertically_on_load(1) в
    // Texture::LoadFromFile — т.е. изображение уже перевёрнуто в памяти.
    // UV-флип применяется отдельно (по выбору пользователя) в
    // EditorUI::ExecutePendingImport(), чтобы не было двойного переворота.
    const unsigned int flags =
        aiProcess_Triangulate           |
        aiProcess_GenSmoothNormals      |
        aiProcess_CalcTangentSpace      |
        aiProcess_JoinIdenticalVertices |
        aiProcess_SortByPType;

    const aiScene* scene = importer.ReadFile(path, flags);
    if (!scene ||
        (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) ||
        !scene->mRootNode)
    {
        BINAX_LOG_ERROR("MeshImporter: '{}' failed: {}",
                        path, importer.GetErrorString());
        return false;
    }

    if (scene->mNumMeshes == 0)
    {
        BINAX_LOG_ERROR("MeshImporter: '{}' contains no meshes", path);
        return false;
    }

    out = {};
    out.Name = std::filesystem::path(path).stem().string();

    // ---- Материалы ---------------------------------------------------------
    out.Materials.resize(scene->mNumMaterials);
    for (unsigned i = 0; i < scene->mNumMaterials; ++i)
    {
        const aiMaterial* m = scene->mMaterials[i];
        ImportedMaterial& im = out.Materials[i];

        aiString name;
        if (m->Get(AI_MATKEY_NAME, name) == AI_SUCCESS && name.length > 0)
            im.Name = name.C_Str();
        else
            im.Name = out.Name + "_Mat" + std::to_string(i);

        im.AlbedoPath    = GetTexturePath(m, aiTextureType_BASE_COLOR);
        if (im.AlbedoPath.empty())
            im.AlbedoPath = GetTexturePath(m, aiTextureType_DIFFUSE);

        im.NormalPath    = GetTexturePath(m, aiTextureType_NORMALS);
        if (im.NormalPath.empty())
            im.NormalPath = GetTexturePath(m, aiTextureType_HEIGHT);

        im.MetallicPath  = GetTexturePath(m, aiTextureType_METALNESS);
        im.RoughnessPath = GetTexturePath(m, aiTextureType_DIFFUSE_ROUGHNESS);
        im.AOPath        = GetTexturePath(m, aiTextureType_AMBIENT_OCCLUSION);
        im.EmissivePath  = GetTexturePath(m, aiTextureType_EMISSIVE);

        aiColor3D color(1.0f, 1.0f, 1.0f);
        if (m->Get(AI_MATKEY_BASE_COLOR, color) == AI_SUCCESS ||
            m->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
        {
            im.Albedo[0] = color.r;
            im.Albedo[1] = color.g;
            im.Albedo[2] = color.b;
        }

        float factor = 0.5f;
        if (m->Get(AI_MATKEY_ROUGHNESS_FACTOR, factor) == AI_SUCCESS)
            im.Roughness = factor;

        factor = 0.0f;
        if (m->Get(AI_MATKEY_METALLIC_FACTOR, factor) == AI_SUCCESS)
            im.Metallic = factor;
    }

    // ---- Меши --------------------------------------------------------------
    out.Meshes.resize(scene->mNumMeshes);
    size_t totalTris = 0;
    for (unsigned i = 0; i < scene->mNumMeshes; ++i)
    {
        const aiMesh* mesh = scene->mMeshes[i];
        ImportedMesh& im = out.Meshes[i];

        im.Name = mesh->mName.C_Str();
        if (im.Name.empty())
            im.Name = "Mesh_" + std::to_string(i);

        im.MaterialIndex = mesh->mMaterialIndex;
        ExtractMeshData(mesh, im.Data);
        totalTris += im.Data.Indices.size() / 3;
    }

        // ---- Общий bounding box ------------------------------------------------
    out.BoundsMin = glm::vec3( std::numeric_limits<float>::max());
    out.BoundsMax = glm::vec3(-std::numeric_limits<float>::max());

    for (const auto& im : out.Meshes)
    {
        for (const auto& v : im.Data.Vertices)
        {
            if (!std::isfinite(v.Position.x) ||
                !std::isfinite(v.Position.y) ||
                !std::isfinite(v.Position.z))
                continue;

            out.BoundsMin = glm::min(out.BoundsMin, v.Position);
            out.BoundsMax = glm::max(out.BoundsMax, v.Position);
        }
    }

    // Защита от пустой/битой геометрии
    if (out.BoundsMin.x > out.BoundsMax.x)
    {
        out.BoundsMin    = glm::vec3(-0.5f);
        out.BoundsMax    = glm::vec3( 0.5f);
    }

    out.BoundsCenter = (out.BoundsMin + out.BoundsMax) * 0.5f;
    out.BoundsSize   = out.BoundsMax - out.BoundsMin;
    out.MaxDimension = glm::max(glm::max(out.BoundsSize.x, out.BoundsSize.y),
                                out.BoundsSize.z);

    if (!std::isfinite(out.MaxDimension) || out.MaxDimension < 1e-6f)
    {
        BINAX_LOG_WARN("MeshImporter: '{}' has degenerate bounds, "
                       "using 1.0 as max dimension", path);
        out.MaxDimension = 1.0f;
    }

    BINAX_LOG_INFO("MeshImporter: '{}' loaded — {} meshes, {} materials, {} tris",
                   path, out.Meshes.size(), out.Materials.size(), totalTris);
    return true;
}

std::vector<std::string> MeshImporter::GetSupportedExtensions()
{
    return { ".obj", ".fbx", ".gltf", ".glb", ".dae",
             ".3ds", ".stl", ".ply", ".blend", ".x" };
}

}