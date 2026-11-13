#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Renderer/Mesh.h"

namespace Binax {

struct ImportedMesh
{
    std::string Name;
    uint32_t    MaterialIndex = 0;
    MeshData    Data;
};

struct ImportedMaterial
{
    std::string Name;
    std::string AlbedoPath;
    std::string NormalPath;
    std::string MetallicPath;
    std::string RoughnessPath;
    std::string AOPath;
    std::string EmissivePath;

    float Albedo[3] = { 1.0f, 1.0f, 1.0f };
    float Roughness = 0.5f;
    float Metallic  = 0.0f;
    float Emissive  = 0.0f;
};

struct ImportedModel
{
    std::string Name;
    std::vector<ImportedMesh>     Meshes;
    std::vector<ImportedMaterial> Materials;

    // Общий bounding box по ВСЕМ мешам модели (в локальных координатах вершин)
    glm::vec3 BoundsMin = glm::vec3(0.0f);
    glm::vec3 BoundsMax = glm::vec3(0.0f);
    glm::vec3 BoundsCenter = glm::vec3(0.0f);
    glm::vec3 BoundsSize = glm::vec3(0.0f);
    float     MaxDimension = 1.0f;
};

class MeshImporter
{
public:
    // Возвращает false + лог при любой ошибке assimp.
    static bool LoadFromFile(const std::string& path, ImportedModel& out);

    // Список расширений для FileBrowser-фильтра (без звёздочки).
    static std::vector<std::string> GetSupportedExtensions();
};

}