#pragma once

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <string>



namespace Binax::Serialization::Json
{

inline float Number(const nlohmann::json& j, const char* key, float def)
{
    const auto it = j.find(key);
    return (it != j.end() && it->is_number()) ? it->get<float>() : def;
}

inline int Int(const nlohmann::json& j, const char* key, int def)
{
    const auto it = j.find(key);
    return (it != j.end() && it->is_number_integer()) ? it->get<int>() : def;
}

inline bool Bool(const nlohmann::json& j, const char* key, bool def)
{
    const auto it = j.find(key);
    return (it != j.end() && it->is_boolean()) ? it->get<bool>() : def;
}

inline std::string String(const nlohmann::json& j, const char* key)
{
    const auto it = j.find(key);
    return (it != j.end() && it->is_string()) ? it->get<std::string>() : std::string{};
}

inline glm::vec2 Vec2(const nlohmann::json& j, const char* key, glm::vec2 def)
{
    const auto it = j.find(key);
    if (it == j.end() || !it->is_array() || it->size() < 2)
        return def;
    for (int i = 0; i < 2; ++i)
        if (!(*it)[i].is_number())
            return def;
    return glm::vec2((*it)[0].get<float>(), (*it)[1].get<float>());
}

inline glm::vec3 Vec3(const nlohmann::json& j, const char* key, glm::vec3 def)
{
    const auto it = j.find(key);
    if (it == j.end() || !it->is_array() || it->size() < 3)
        return def;
    for (int i = 0; i < 3; ++i)
        if (!(*it)[i].is_number())
            return def;
    return glm::vec3((*it)[0].get<float>(), (*it)[1].get<float>(), (*it)[2].get<float>());
}

inline void SetVec3(nlohmann::json& j, const char* key, const glm::vec3& v)
{
    j[key] = { v.x, v.y, v.z };
}

} 
