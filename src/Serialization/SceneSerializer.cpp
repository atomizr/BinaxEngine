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
    nlohmann::json root;
    root["version"]  = kSceneVersion;
    root["entities"] = nlohmann::json::array();

    
    
    std::vector<entt::entity> entities;
    for (auto entity : registry.view<entt::entity>())
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

    std::ofstream file(Paths::FromUtf8(filepath));
    if (file.is_open())
    {
        file << root.dump(4);
        file.flush();
        const bool writeOk = file.good();
        file.close();

        if (!writeOk)
            BINAX_LOG_ERROR("Failed to write level to {0}", filepath);
        else
            BINAX_LOG_INFO("Level saved to {0} (v{1}, {2} entities, {3} materials)",
                           filepath, kSceneVersion, nextId,
                           MaterialLibrary::Get().GetMaterialCount());
    }
    else
    {
        BINAX_LOG_ERROR("Failed to save level to {0}", filepath);
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

bool SceneSerializer::Deserialize(entt::registry& registry, const std::string& filepath)
{
    std::ifstream file(Paths::FromUtf8(filepath));
    if (!file.is_open())
    {
        BINAX_LOG_ERROR("Failed to open level file '{0}'", filepath);
        return false;
    }

    nlohmann::json root;
    try
    {
        file >> root;
    }
    catch (const std::exception& e)
    {
        BINAX_LOG_ERROR("Failed to parse level '{0}': {1}", filepath, e.what());
        return false;
    }
    file.close();

    if (!ValidateSceneJson(root))
    {
        BINAX_LOG_ERROR("Level '{0}' has invalid structure (expected an object "
                        "with integer 'version' and array of objects 'entities')",
                        filepath);
        return false;
    }

    const int fileVersion = root["version"].get<int>();
    if (fileVersion != kSceneVersion)
    {
        BINAX_LOG_ERROR("Level '{0}' version {1} != engine version {2}. "
                        "Re-save the level with the current engine.",
                        filepath, fileVersion, kSceneVersion);
        return false;
    }

    
    
    std::vector<entt::entity> toDestroy;
    for (auto e : registry.view<entt::entity>())
    {
        if (!registry.any_of<SystemEntityTag>(e))
            toDestroy.push_back(e);
    }
    for (auto e : toDestroy)
        registry.destroy(e);

    const auto entitiesIt = root.find("entities");
    if (entitiesIt == root.end() || entitiesIt->empty())
    {
        BINAX_LOG_INFO("Level '{0}' loaded (no entities)", filepath);
        return true;
    }

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

    
    for (auto& [entity, entityJson] : created)
    {
        const int parentId = Int(*entityJson, "parent", -1);
        if (parentId < 0)
            continue;

        const auto it = byId.find(parentId);
        if (it == byId.end() || it->second == entity)
            continue;

        registry.emplace_or_replace<ParentComponent>(entity, ParentComponent{ it->second });
    }

    BINAX_LOG_INFO("Level loaded from {0} (v{1}, {2} entities)",
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
