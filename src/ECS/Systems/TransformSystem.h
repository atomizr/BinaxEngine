#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include <unordered_map>

#include "ECS/Components/Transform.h"
#include "ECS/Components/ParentComponent.h"
#include "ECS/Components/WorldTransform.h"

namespace Binax
{




class TransformSystem
{
public:
    static constexpr int kMaxDepth = 64;

    
    static glm::mat4 GetWorldMatrix(entt::registry& registry, entt::entity entity,
                                    int depth = 0)
    {
        if (entity == entt::null || !registry.valid(entity) ||
            !registry.any_of<Transform>(entity))
        {
            return glm::mat4(1.0f);
        }

        const Transform& tr = registry.get<Transform>(entity);
        glm::mat4 world = tr.GetModelMatrix();

        if (depth < kMaxDepth)
        {
            if (const auto* pc = registry.try_get<ParentComponent>(entity))
            {
                if (pc->Parent != entt::null && pc->Parent != entity &&
                    registry.valid(pc->Parent))
                {
                    world = GetWorldMatrix(registry, pc->Parent, depth + 1) * world;
                }
            }
        }

        return world;
    }

    
    static void UpdateWorldTransforms(entt::registry& registry)
    {
        auto view = registry.view<Transform>();

        std::unordered_map<entt::entity, glm::mat4> cache;
        cache.reserve(64);

        for (auto entity : view)
            ComputeCached(registry, entity, cache, 0);

        for (auto entity : view)
        {
            WorldTransform& wt = registry.get_or_emplace<WorldTransform>(entity);
            const auto it = cache.find(entity);
            wt.Matrix = (it != cache.end()) ? it->second : glm::mat4(1.0f);
        }
    }

private:
    static const glm::mat4& ComputeCached(entt::registry& registry,
                                          entt::entity entity,
                                          std::unordered_map<entt::entity, glm::mat4>& cache,
                                          int depth)
    {
        const auto cached = cache.find(entity);
        if (cached != cache.end())
            return cached->second;

        glm::mat4 world = registry.get<Transform>(entity).GetModelMatrix();

        if (depth < kMaxDepth)
        {
            if (const auto* pc = registry.try_get<ParentComponent>(entity))
            {
                if (pc->Parent != entt::null && pc->Parent != entity &&
                    registry.valid(pc->Parent) &&
                    registry.any_of<Transform>(pc->Parent))
                {
                    world = ComputeCached(registry, pc->Parent, cache, depth + 1) * world;
                }
            }
        }

        return cache.emplace(entity, world).first->second;
    }
};

}
