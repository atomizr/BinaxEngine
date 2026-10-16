#include "SceneSerializer.h"
#include "SceneEntityMap.h"
#include "ECS/Components/SystemEntityTag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/NameComponent.h"
#include "ECS/Components/MeshComponent.h"
#include "ECS/Components/MaterialComponent.h"
#include "Serialization/PropertySerializer.h"
#include "Reflect/ComponentRegistry.h"
#include "Renderer/MeshLibrary.h"
#include "Renderer/MaterialLibrary.h"
#include "Utils/Log.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>

namespace Binax {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static glm::vec3 JsonToVec3(const nlohmann::json& j, glm::vec3 def)
{
    if (!j.is_array() || j.size() < 3) return def;
    return glm::vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
}

// ---------------------------------------------------------------------------
// Materials
// ---------------------------------------------------------------------------

void SceneSerializer::SerializeMaterials(nlohmann::json& root)
{
    nlohmann::json arr = nlohmann::json::array();
    auto& lib = MaterialLibrary::Get();

    for (uint32_t i = 0; i < lib.GetMaterialCount(); ++i)
    {
        MaterialHandle h = i;
        const auto& gpu = lib.Get(h);
        const auto& ed  = lib.GetEdit(h);

        nlohmann::json mj;
        mj["name"] = ed.Name;

        mj["albedo"]    = { gpu.AlbedoRoughness.x, gpu.AlbedoRoughness.y, gpu.AlbedoRoughness.z };
        mj["roughness"] = gpu.AlbedoRoughness.w;

        mj["metallic"]  = gpu.MetallicAOEms.x;
        mj["ao"]        = gpu.MetallicAOEms.y;
        mj["emissive"]  = gpu.MetallicAOEms.z;
        mj["tiling"]    = gpu.MetallicAOEms.w;

        mj["skyReflectionsEnabled"] = (gpu.SkyReflParams.x > 0.5f);
        mj["skyReflectionStrength"] = gpu.SkyReflParams.y;

        mj["metallicStrength"]  = gpu.StrengthA.x;
        mj["roughnessStrength"] = gpu.StrengthA.y;
        mj["aoStrength"]        = gpu.StrengthA.z;
        mj["emissiveStrength"]  = gpu.StrengthA.w;

        mj["albedoStrength"] = gpu.StrengthB.x;
        mj["normalStrength"] = gpu.StrengthB.y;

        mj["albedoMap"]    = ed.AlbedoMapPath;
        mj["normalMap"]    = ed.NormalMapPath;
        mj["metallicMap"]  = ed.MetallicMapPath;
        mj["roughnessMap"] = ed.RoughnessMapPath;
        mj["aoMap"]        = ed.AOMapPath;
        mj["emissiveMap"]  = ed.EmissiveMapPath;

        arr.push_back(std::move(mj));
    }

    root["materials"] = std::move(arr);
}

void SceneSerializer::DeserializeMaterials(const nlohmann::json& root)
{
    if (!root.contains("materials") || !root["materials"].is_array())
        return;

    auto& lib = MaterialLibrary::Get();

    for (const auto& mj : root["materials"])
    {
        std::string name = mj.value("name", std::string{});
        if (name.empty()) continue;

        MaterialGPU gpu{};

        glm::vec3 albedo = JsonToVec3(mj.value("albedo", nlohmann::json::array()),
                                      glm::vec3(1.0f));

        gpu.AlbedoRoughness = glm::vec4(albedo, mj.value("roughness", 0.5f));
        gpu.MetallicAOEms   = glm::vec4(mj.value("metallic",  0.0f),
                                        mj.value("ao",        1.0f),
                                        mj.value("emissive",  0.0f),
                                        mj.value("tiling",    1.0f));
        gpu.SkyReflParams   = glm::vec4(mj.value("skyReflectionsEnabled", true) ? 1.0f : 0.0f,
                                        mj.value("skyReflectionStrength", 1.0f),
                                        0.0f, 0.0f);
        gpu.StrengthA       = glm::vec4(mj.value("metallicStrength",  1.0f),
                                        mj.value("roughnessStrength", 1.0f),
                                        mj.value("aoStrength",        1.0f),
                                        mj.value("emissiveStrength",  1.0f));
        gpu.StrengthB       = glm::vec4(mj.value("albedoStrength", 1.0f),
                                        mj.value("normalStrength", 1.0f),
                                        0.0f, 0.0f);

        MaterialHandle h = lib.Add(name, gpu);

        lib.SetAlbedoMap   (h, mj.value("albedoMap",    std::string{}));
        lib.SetNormalMap   (h, mj.value("normalMap",    std::string{}));
        lib.SetMetallicMap (h, mj.value("metallicMap",  std::string{}));
        lib.SetRoughnessMap(h, mj.value("roughnessMap", std::string{}));
        lib.SetAOMap       (h, mj.value("aoMap",        std::string{}));
        lib.SetEmissiveMap (h, mj.value("emissiveMap",  std::string{}));
    }

    lib.Flush();
}

// ---------------------------------------------------------------------------
// Serialize
// ---------------------------------------------------------------------------

void SceneSerializer::Serialize(entt::registry& registry, const std::string& filepath)
{
    nlohmann::json root;
    root["version"] = kSceneVersion;

    SerializeMaterials(root);

    int nextId = 0;
    for (auto entity : registry.view<entt::entity>())
    {
        if (registry.any_of<SystemEntityTag>(entity))
            continue;

        nlohmann::json entityJson;
        entityJson["id"] = nextId++;
        SerializeEntity(registry, entity, entityJson);
        root["entities"].push_back(std::move(entityJson));
    }

    // [FIX] Явный UTF-8-конструктор filesystem::path — Unicode-пути
//       не бросают std::system_error 1113 и корректно открываются.
std::filesystem::path outPathFs(
    std::u8string(reinterpret_cast<const char8_t*>(filepath.data()),
                  filepath.size()));
std::ofstream file(outPathFs);
    if (file.is_open())
    {
        file << root.dump(4);
        file.close();
        BINAX_LOG_INFO("Scene saved to {0} (v{1}, {2} entities, {3} materials)",
                       filepath, kSceneVersion, nextId,
                       MaterialLibrary::Get().GetMaterialCount());
    }
    else
    {
        BINAX_LOG_ERROR("Failed to save scene to {0}", filepath);
    }
}

void SceneSerializer::SerializeEntity(entt::registry& registry,
                                      entt::entity entity,
                                      nlohmann::json& outJson)
{
    outJson["components"] = Serialization::PropertySerializer::SerializeEntity(registry, entity);

    if (registry.any_of<MeshComponent>(entity))
    {
        auto h = registry.get<MeshComponent>(entity).MeshId;
        std::string name = (h != kInvalidMesh) ? MeshLibrary::Get().GetName(h) : std::string{};
        outJson["components"]["Mesh Component"]["MeshName"] = name;
    }

    if (registry.any_of<MaterialComponent>(entity))
    {
        auto h = registry.get<MaterialComponent>(entity).MaterialId;
        std::string name = (h != kInvalidMaterial)
                            ? MaterialLibrary::Get().GetName(h)
                            : std::string{};
        outJson["components"]["Material"]["MaterialName"] = name;
    }
}

// ---------------------------------------------------------------------------
// Deserialize
// ---------------------------------------------------------------------------

bool SceneSerializer::Deserialize(entt::registry& registry, const std::string& filepath)
{
    // [FIX] На MSVC std::ifstream(const std::string&) использует ANSI-путь
//       и не открывает файлы с Unicode-именами. Открываем через
//       явный std::filesystem::path, сконструированный из UTF-8.
std::filesystem::path filePathFs(
    std::u8string(reinterpret_cast<const char8_t*>(filepath.data()),
                  filepath.size()));
std::ifstream file(filePathFs);
    if (!file.is_open())
    {
        BINAX_LOG_ERROR("Failed to open scene file '{0}'", filepath);
        return false;
    }

    nlohmann::json root;
    try
    {
        file >> root;
    }
    catch (const std::exception& e)
    {
        BINAX_LOG_ERROR("Failed to parse scene '{0}': {1}", filepath, e.what());
        return false;
    }
    file.close();

    const int fileVersion = root.value("version", 0);
    if (fileVersion != kSceneVersion)
    {
        BINAX_LOG_ERROR("Scene '{0}' version {1} != engine version {2}. "
                        "Re-save the scene with the current engine.",
                        filepath, fileVersion, kSceneVersion);
        return false;
    }

    // Удаляем все НЕ-системные сущности (сохраняем EditorCamera и Skybox)
    std::vector<entt::entity> toDestroy;
    for (auto e : registry.view<entt::entity>())
    {
        if (!registry.any_of<SystemEntityTag>(e))
            toDestroy.push_back(e);
    }
    for (auto e : toDestroy)
        registry.destroy(e);

    // [FIX] Очищаем MaterialLibrary перед загрузкой — иначе материалы
    //       старой сцены остаются, а Add() по совпадающему имени
    //       перезаписывает существующий handle, ломая индексацию и
    //       накапливая мусор при повторных загрузках.
    MaterialLibrary::Get().Clear();

    // Материалы — до создания сущностей.
    DeserializeMaterials(root);

    if (!root.contains("entities") || !root["entities"].is_array())
    {
        auto& ctx = registry.ctx();
        ctx.erase<SceneEntityMap>();
        ctx.emplace<SceneEntityMap>(SceneEntityMap{});
        BINAX_LOG_INFO("Scene '{0}' loaded (no entities)", filepath);
        return true;
    }

    SceneEntityMap idMap;
    idMap.ByLocalId.reserve(root["entities"].size());

    std::vector<std::pair<entt::entity, const nlohmann::json*>> created;
    created.reserve(root["entities"].size());

    int fallbackId = 0;
    for (const auto& entityJson : root["entities"])
    {
        entt::entity entity = registry.create();
        const int localId = entityJson.value("id", fallbackId++);
        idMap.ByLocalId[localId] = entity;
        created.emplace_back(entity, &entityJson);
    }

    {
        auto& ctx = registry.ctx();
        ctx.erase<SceneEntityMap>();
        ctx.emplace<SceneEntityMap>(std::move(idMap));
    }

    try
    {
        for (auto& [entity, entityJson] : created)
            ApplyEntityData(registry, entity, *entityJson);
    }
    catch (const std::exception& e)
    {
        BINAX_LOG_ERROR("Failed to load entities from '{0}': {1}", filepath, e.what());
        return false;
    }

    BINAX_LOG_INFO("Scene loaded from {0} (v{1}, {2} entities)",
                   filepath, fileVersion, created.size());
    return true;
}

void SceneSerializer::ApplyEntityData(entt::registry& registry,
                                      entt::entity entity,
                                      const nlohmann::json& json)
{
    if (json.contains("components") && json["components"].is_object())
    {
        Serialization::PropertySerializer::DeserializeEntity(registry, entity, json["components"]);

        const auto& c = json["components"];

        if (c.contains("Mesh Component") &&
            c["Mesh Component"].contains("MeshName") &&
            registry.any_of<MeshComponent>(entity))
        {
            auto name = c["Mesh Component"]["MeshName"].get<std::string>();
            if (!name.empty())
            {
                MeshHandle h = MeshLibrary::Get().Find(name);
                registry.get<MeshComponent>(entity).MeshId = h;
                if (h == kInvalidMesh)
                    BINAX_LOG_WARN("Scene: unknown mesh '{}'", name);
            }
        }

        if (c.contains("Material") &&
            c["Material"].contains("MaterialName") &&
            registry.any_of<MaterialComponent>(entity))
        {
            auto name = c["Material"]["MaterialName"].get<std::string>();
            if (!name.empty())
            {
                MaterialHandle h = MaterialLibrary::Get().Find(name);
                registry.get<MaterialComponent>(entity).MaterialId = h;
                if (h == kInvalidMaterial)
                    BINAX_LOG_WARN("Scene: unknown material '{}'", name);
            }
        }
    }

    if (!registry.any_of<Transform>(entity))
        registry.emplace<Transform>(entity);

    if (!registry.any_of<NameComponent>(entity))
    {
        registry.emplace<NameComponent>(entity,
            "Entity " + std::to_string(static_cast<uint32_t>(entity)));
    }
}

} // namespace Binax
