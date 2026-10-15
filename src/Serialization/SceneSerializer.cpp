
#include "SceneSerializer.h"
#include "SceneEntityMap.h"
#include "ECS/Components/SystemEntityTag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/NameComponent.h"
#include "Serialization/PropertySerializer.h"
#include "Reflect/ComponentRegistry.h"
#include "Utils/Log.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <vector>
#include <string>

namespace Binax {





void SceneSerializer::Serialize(entt::registry& registry, const std::string& filepath)
{
    nlohmann::json root;
    root["version"] = kSceneVersion;

    int nextId = 0;
    auto view = registry.view<entt::entity>();
    for (auto entity : view)
    {
        
        if (registry.any_of<SystemEntityTag>(entity))
            continue;

        nlohmann::json entityJson;
        entityJson["id"] = nextId++;
        SerializeEntity(registry, entity, entityJson);
        root["entities"].push_back(std::move(entityJson));
    }

    std::ofstream file(filepath);
    if (file.is_open())
    {
        file << root.dump(4);
        file.close();
        BINAX_LOG_INFO("Scene saved to {0} (v{1}, {2} entities)",
                       filepath, kSceneVersion, nextId);
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
}






bool SceneSerializer::Deserialize(entt::registry& registry, const std::string& filepath)
{
    
    std::ifstream file(filepath);
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
    if (fileVersion == 0)
    {
        BINAX_LOG_WARN("Scene '{0}' has no version field — assuming v0 (legacy)", filepath);
    }
    else if (fileVersion > kSceneVersion)
    {
        BINAX_LOG_WARN("Scene '{0}' version {1} is newer than engine ({2}) — "
                       "loading best-effort, unknown components may be skipped",
                       filepath, fileVersion, kSceneVersion);
    }

    
    std::vector<entt::entity> toDestroy;
    for (auto e : registry.view<entt::entity>())
    {
        if (!registry.any_of<SystemEntityTag>(e))
            toDestroy.push_back(e);
    }
    for (auto e : toDestroy)
        registry.destroy(e);

    
    if (!root.contains("entities") || !root["entities"].is_array())
    {
        BINAX_LOG_WARN("No entities found in scene file '{0}'", filepath);

        
        
        auto& ctx = registry.ctx();
        ctx.erase<SceneEntityMap>();
        ctx.emplace<SceneEntityMap>(SceneEntityMap{});

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
        {
            ApplyEntityData(registry, entity, *entityJson);
        }
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