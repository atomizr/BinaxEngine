#include "SceneSerializer.h"
#include "Serialization/JsonHelpers.h"
#include "ECS/Components/SystemEntityTag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/NameComponent.h"
#include "ECS/Components/MeshComponent.h"
#include "ECS/Components/MaterialComponent.h"
#include "ECS/Components/ParentComponent.h"
#include "Serialization/PropertySerializer.h"
#include "Reflect/ComponentRegistry.h"
#include "Renderer/MeshLibrary.h"
#include "Renderer/MaterialLibrary.h"
#include "Utils/Log.h"
#include "Utils/Paths.h"

#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <filesystem>

namespace Binax
{

using namespace Serialization::Json;




static bool ValidateSceneJson(const nlohmann::json& root)
{
    if (!root.is_object())
        return false;

    const auto version = root.find("version");
    if (version == root.end() || !version->is_number_integer())
        return false;

    const auto entities = root.find("entities");
    if (entities != root.end())
    {
        if (!entities->is_array())
            return false;
        for (const auto& ej : *entities)
            if (!ej.is_object())
                return false;
    }

    return true;
}




void SceneSerializer::Serialize(entt::registry& registry, const std::string& filepath)
{
    const std::string json = SerializeToString(registry);

    std::ofstream file(Paths::FromUtf8(filepath));
    if (!file.is_open())
    {
        BINAX_LOG_ERROR("Failed to save level to {0}", filepath);
        return;
    }

    file << json;
    file.flush();
    const bool writeOk = file.good();
    file.close();

    if (!writeOk)
    {
        BINAX_LOG_ERROR("Failed to write level to {0}", filepath);
        return;
    }


    size_t entityCount = 0;
    for (auto e : registry.storage<entt::entity>())
        if (!registry.any_of<SystemEntityTag>(e))
            ++entityCount;

    BINAX_LOG_INFO("Level saved to {0} (v{1}, {2} entities, {3} materials)",
                   filepath, kSceneVersion, entityCount,
                   MaterialLibrary::Get().GetMaterialCount());
}




std::string SceneSerializer::SerializeToString(entt::registry& registry)
{
    nlohmann::json root;
    root["version"]  = kSceneVersion;
    root["entities"] = nlohmann::json::array();

    std::vector<entt::entity> entities;
    for (auto entity : registry.storage<entt::entity>())
    {
        if (!registry.any_of<SystemEntityTag>(entity))
            entities.push_back(entity);
    }

    std::unordered_map<entt::entity, int> ids;
    ids.reserve(entities.size());
    for (size_t i = 0; i < entities.size(); ++i)
        ids[entities[i]] = static_cast<int>(i);

    int nextId = 0;
    for (auto entity : entities)
    {
        nlohmann::json entityJson;
        entityJson["id"] = nextId++;
        SerializeEntity(registry, entity, entityJson);

        if (const auto* pc = registry.try_get<ParentComponent>(entity))
        {
            const auto it = ids.find(pc->Parent);
            if (it != ids.end() && pc->Parent != entity)
                entityJson["parent"] = it->second;
        }

        root["entities"].push_back(std::move(entityJson));
    }

    return root.dump();
}




bool SceneSerializer::Deserialize(entt::registry& registry, const std::string& filepath)
{
    std::ifstream file(Paths::FromUtf8(filepath));
    if (!file.is_open())
    {
        BINAX_LOG_ERROR("Failed to open level file '{0}'", filepath);
        return false;
    }

    std::stringstream ss;
    ss << file.rdbuf();
    file.close();

    return DeserializeFromString(registry, ss.str());
}




bool SceneSerializer::DeserializeFromString(entt::registry& registry,
                                            const std::string& json_string)
{
    nlohmann::json root;
    try
    {
        root = nlohmann::json::parse(json_string);
    }
    catch (const std::exception& e)
    {
        BINAX_LOG_ERROR("Failed to parse level JSON: {0}", e.what());
        return false;
    }

    if (!ValidateSceneJson(root))
    {
        BINAX_LOG_ERROR("Level JSON has invalid structure (expected object with "
                        "integer 'version' and array of objects 'entities')");
        return false;
    }

    const int fileVersion = root["version"].get<int>();
    if (fileVersion != kSceneVersion)
    {
        BINAX_LOG_ERROR("Level version {0} != engine version {1}. "
                        "Re-save the level with the current engine.",
                        fileVersion, kSceneVersion);
        return false;
    }

    // Снимаем бэкап ДО каких-либо разрушений. Это гарантирует,
    // что при любом сбое ниже мы восстановим ровно то, что было.
    const std::string backup = SerializeToString(registry);

    // Уничтожаем все non-system сущности
    std::vector<entt::entity> toDestroy;
    for (auto e : registry.storage<entt::entity>())
    {
        if (!registry.any_of<SystemEntityTag>(e))
            toDestroy.push_back(e);
    }
    for (auto e : toDestroy)
        registry.destroy(e);

    const auto entitiesIt = root.find("entities");
    if (entitiesIt == root.end() || entitiesIt->empty())
        return true;

    std::vector<std::pair<entt::entity, const nlohmann::json*>> created;
    created.reserve(entitiesIt->size());

    std::unordered_map<int, entt::entity> byId;
    byId.reserve(entitiesIt->size());

    int fallbackId = 0;
    for (const auto& entityJson : *entitiesIt)
    {
        const entt::entity entity = registry.create();
        const int localId = Int(entityJson, "id", fallbackId);
        ++fallbackId;
        byId[localId] = entity;
        created.emplace_back(entity, &entityJson);
    }

    bool applyOk = true;
    try
    {
        for (auto& [entity, entityJson] : created)
            ApplyEntityData(registry, entity, *entityJson);
    }
    catch (const std::exception& e)
    {
        BINAX_LOG_ERROR("Failed to load entities from JSON: {0}", e.what());
        applyOk = false;
    }

    if (!applyOk)
    {
        // Защита от бесконечной рекурсии, если бэкап окажется невалидным
        static thread_local bool s_IsRestoring = false;
        if (s_IsRestoring)
        {
            BINAX_LOG_CRITICAL("Restore-from-backup also failed — scene is broken");
            return false;
        }

        BINAX_LOG_WARN("Restoring previous scene after failed load");
        s_IsRestoring = true;
        const bool restored = DeserializeFromString(registry, backup);
        s_IsRestoring = false;

        if (!restored)
            BINAX_LOG_CRITICAL("Restore-from-backup failed — scene is broken");
        return false;
    }

    // Привязка родителей
    for (auto& [entity, entityJson] : created)
    {
        const int parentId = Int(*entityJson, "parent", -1);
        if (parentId < 0)
            continue;

        const auto it = byId.find(parentId);
        if (it == byId.end() || it->second == entity)
            continue;

        registry.emplace_or_replace<ParentComponent>(entity,
            ParentComponent{ it->second });
    }

    return true;
}




void SceneSerializer::SerializeEntity(entt::registry& registry,
                                      entt::entity entity,
                                      nlohmann::json& outJson)
{
    outJson["components"] = Serialization::PropertySerializer::SerializeEntity(registry, entity);

    if (registry.any_of<MeshComponent>(entity))
    {
        auto h = registry.get<MeshComponent>(entity).MeshId;
        const std::string name = MeshLibrary::Get().IsValid(h)
                                     ? MeshLibrary::Get().GetName(h)
                                     : std::string{};
        outJson["components"]["Mesh Component"]["MeshName"] = name;
    }

    if (registry.any_of<MaterialComponent>(entity))
    {
        auto h = registry.get<MaterialComponent>(entity).MaterialId;
        const std::string name = MaterialLibrary::Get().IsValid(h)
                                     ? MaterialLibrary::Get().GetName(h)
                                     : std::string{};
        outJson["components"]["Material"]["MaterialName"] = name;
    }
}




void SceneSerializer::ApplyEntityData(entt::registry& registry,
                                      entt::entity entity,
                                      const nlohmann::json& json)
{
    if (json.contains("components") && json["components"].is_object())
    {
        Serialization::PropertySerializer::DeserializeEntity(registry, entity, json["components"]);

        const auto& c = json["components"];

        if (c.contains("Mesh Component") && registry.any_of<MeshComponent>(entity))
        {
            auto name = String(c["Mesh Component"], "MeshName");
            if (!name.empty())
            {
                MeshHandle h = MeshLibrary::Get().Find(name);
                registry.get<MeshComponent>(entity).MeshId = h;
                if (h == kInvalidMesh)
                    BINAX_LOG_WARN("Level: unknown mesh '{}'", name);
            }
        }

        if (c.contains("Material") && registry.any_of<MaterialComponent>(entity))
        {
            auto name = String(c["Material"], "MaterialName");
            if (!name.empty())
            {
                MaterialHandle h = MaterialLibrary::Get().Find(name);
                registry.get<MaterialComponent>(entity).MaterialId = h;
                if (h == kInvalidMaterial)
                    BINAX_LOG_WARN("Level: unknown material '{}' (.bxmat not found?)", name);
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

} 