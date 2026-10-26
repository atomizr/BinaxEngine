
#pragma once

#include <entt/entt.hpp>
#include <nlohmann/json.hpp>
#include <string>

namespace Binax::Serialization {

class PropertySerializer
{
public:
    
    static nlohmann::json SerializeEntity(entt::registry& reg, entt::entity e);

    
    static void DeserializeEntity(entt::registry& reg, entt::entity e,
                                  const nlohmann::json& j);
};

} 
