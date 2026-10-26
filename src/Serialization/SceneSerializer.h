#pragma once

#include <entt/entt.hpp>
#include <nlohmann/json.hpp>
#include <string>

namespace Binax
{




class SceneSerializer
{
public:
    static constexpr int kSceneVersion = 4;

    static void Serialize(entt::registry& registry, const std::string& filepath);

    static bool Deserialize(entt::registry& registry, const std::string& filepath);

private:
    static void SerializeEntity(entt::registry& registry,
                                entt::entity entity,
                                nlohmann::json& outJson);

    static void ApplyEntityData(entt::registry& registry,
                                entt::entity entity,
                                const nlohmann::json& json);
};

}
