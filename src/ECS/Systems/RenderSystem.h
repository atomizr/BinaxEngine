#pragma once

#include <entt/entt.hpp>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Renderer/DeferredRenderer.h"
#include "Renderer/GPULight.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/MeshComponent.h"
#include "ECS/Components/MaterialComponent.h"
#include "ECS/Components/LightComponent.h"

namespace Binax {

class RenderSystem
{
public:
    static void Render(entt::registry& registry,
                       DeferredRenderer& renderer,
                       const glm::mat4& viewMatrix,
                       const glm::mat4& projectionMatrix)
    {
        // Fallback material for entities without MaterialComponent
        static const MaterialComponent kDefaultMaterial{};

        auto view = registry.view<Transform, MeshComponent>();
        for (auto entity : view)
        {
            const auto& transform = view.get<Transform>(entity);
            const auto& meshComp  = view.get<MeshComponent>(entity);
            if (!meshComp.MeshPtr)
                continue;

            const MaterialComponent* mat = &kDefaultMaterial;
            if (registry.any_of<MaterialComponent>(entity))
                mat = &registry.get<MaterialComponent>(entity);

            renderer.GeometryPass(*meshComp.MeshPtr,
                                  transform.GetModelMatrix(),
                                  viewMatrix,
                                  projectionMatrix,
                                  *mat);
        }
    }

    static void CollectLights(entt::registry& registry,
                              std::vector<GPULight>& out)
    {
        out.reserve(kMaxLights);

        auto view = registry.view<Transform, LightComponent>();
        for (auto entity : view)
        {
            if (static_cast<int>(out.size()) >= kMaxLights)
                break;

            const auto& transform = view.get<Transform>(entity);
            const auto& lc        = view.get<LightComponent>(entity);

            GPULight gpu;
            gpu.Position    = transform.Position;
            gpu.Range       = lc.Range;
            gpu.Color       = lc.Color;
            gpu.Intensity   = lc.Intensity;
            gpu.InnerCutoff = lc.InnerCutoff;
            gpu.OuterCutoff = lc.OuterCutoff;

            gpu.Direction = transform.Rotation * glm::vec3(0.0f, 0.0f, -1.0f);

            switch (lc.Type)
            {
                case LightType::Directional: gpu.LightType = GPULight::Directional; break;
                case LightType::Point:       gpu.LightType = GPULight::Point;       break;
                case LightType::Spot:        gpu.LightType = GPULight::Spot;        break;
            }

            out.push_back(gpu);
        }

        if (out.empty())
        {
            GPULight def;
            def.LightType = GPULight::Directional;
            def.Direction = glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f));
            def.Color     = glm::vec3(1.0f, 0.9f, 0.8f);
            def.Intensity = 1.2f;
            out.push_back(def);
        }
    }
};

}