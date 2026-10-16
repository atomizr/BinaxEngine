#pragma once

#include <entt/entt.hpp>
#include <string>
#include <nlohmann/json.hpp>

namespace Binax {

class SceneSerializer
{
public:
    // v3 = MeshLibrary/MaterialLibrary-based format.
    // v1/v2 не читаются.
    static constexpr int kSceneVersion = 3;

    static void Serialize(entt::registry& registry, const std::string& filepath);

    // Возвращает false только при фатальной ошибке (не удалось открыть/распарсить).
    // Отсутствие полей и незнакомые компоненты — не фатальны.
    static bool Deserialize(entt::registry& registry, const std::string& filepath);

private:
    static void SerializeEntity(entt::registry& registry,
                                entt::entity entity,
                                nlohmann::json& outJson);

    static void ApplyEntityData(entt::registry& registry,
                                entt::entity entity,
                                const nlohmann::json& json);

    // Вспомогательные таблицы
    static void SerializeMaterials(nlohmann::json& root);
    static void DeserializeMaterials(const nlohmann::json& root);
};

}
