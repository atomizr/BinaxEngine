#pragma once

#include <entt/entt.hpp>
#include <string>

namespace Binax
{



class PostProcessSerializer
{
public:
    static constexpr int kVersion = 1;

    static bool Save(entt::registry& registry, const std::string& filepath);
    static bool Load(entt::registry& registry, const std::string& filepath);
};

}
