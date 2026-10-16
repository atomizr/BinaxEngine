
#pragma once

#include <entt/entt.hpp>
#include <unordered_map>

namespace Binax {

struct SceneEntityMap
{
    std::unordered_map<int, entt::entity> ByLocalId;

    entt::entity Resolve(int localId) const
    {
        auto it = ByLocalId.find(localId);
        return it != ByLocalId.end() ? it->second : entt::null;
    }
};

} 
