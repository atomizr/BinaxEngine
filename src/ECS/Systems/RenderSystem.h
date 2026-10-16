#pragma once

#include <entt/entt.hpp>
#include <algorithm>
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

#include "Renderer/DeferredRenderer.h"
#include "Renderer/DrawCommand.h"
#include "Renderer/GPULight.h"
#include "Renderer/InstanceBuffer.h"
#include "Renderer/MaterialLibrary.h"
#include "Renderer/MeshLibrary.h"

#include "ECS/Components/Transform.h"
#include "ECS/Components/MeshComponent.h"
#include "ECS/Components/MaterialComponent.h"
#include "ECS/Components/LightComponent.h"

namespace Binax {

struct FrameRenderData
{
    std::vector<InstanceGPU>          Instances;
    std::vector<DrawElementsIndirect> Draws;
};

class RenderSystem
{
public:
    static void BuildFrameData(entt::registry& registry, FrameRenderData& out)
    {
        out.Instances.clear();
        out.Draws.clear();

        struct Candidate
        {
            MeshHandle     Mesh;
            MaterialHandle Material;
            glm::mat4      Model;
        };

        std::vector<Candidate> cands;
        auto view = registry.view<Transform, MeshComponent>();
        cands.reserve(view.size_hint());

        for (auto e : view)
        {
            const auto& mc = view.get<MeshComponent>(e);
            if (mc.MeshId == kInvalidMesh) continue;

            const auto& tr = view.get<Transform>(e);

            MaterialHandle mat = kInvalidMaterial;
            if (registry.any_of<MaterialComponent>(e))
                mat = registry.get<MaterialComponent>(e).MaterialId;

            cands.push_back({ mc.MeshId, mat, tr.GetModelMatrix() });
        }

        // Сортируем по mesh id → instance'ы одного меша идут подряд
        std::sort(cands.begin(), cands.end(),
                  [](const Candidate& a, const Candidate& b){ return a.Mesh < b.Mesh; });

        uint32_t instanceCursor = 0;
        size_t i = 0;
        while (i < cands.size())
        {
            const MeshHandle meshId = cands[i].Mesh;
            size_t j = i;
            while (j < cands.size() && cands[j].Mesh == meshId) ++j;

            const uint32_t baseInstance = instanceCursor;
            const uint32_t instCount    = (uint32_t)(j - i);

            for (size_t k = i; k < j; ++k)
            {
                InstanceGPU g{};
                g.Model       = cands[k].Model;
                g.MaterialId  = cands[k].Material;
                g.MeshId      = meshId;
                g.BoneOffset  = 0;
                g.Flags       = InstanceFlags::None;
                out.Instances.push_back(g);
            }
            instanceCursor += instCount;

            const MeshGPU& mesh = MeshLibrary::Get().Get(meshId);

            DrawElementsIndirect cmd{};
            cmd.Count         = mesh.IndexCount;
            cmd.InstanceCount = instCount;
            cmd.FirstIndex    = mesh.BaseIndex;
            cmd.BaseVertex    = mesh.BaseVertex;
            cmd.BaseInstance  = baseInstance;
            out.Draws.push_back(cmd);

            i = j;
        }
    }

    static void CollectLights(entt::registry& registry, std::vector<GPULight>& out)
    {
        out.reserve(kMaxLights);

        auto view = registry.view<Transform, LightComponent>();
        for (auto entity : view)
        {
            if ((int)out.size() >= kMaxLights) break;

            const auto& tr = view.get<Transform>(entity);
            const auto& lc = view.get<LightComponent>(entity);

            GPULight gpu;
            gpu.Position    = tr.Position;
            gpu.Range       = lc.Range;
            gpu.Color       = lc.Color;
            gpu.Intensity   = lc.Intensity;
            gpu.InnerCutoff = lc.InnerCutoff;
            gpu.OuterCutoff = lc.OuterCutoff;
            gpu.Direction   = tr.Rotation * glm::vec3(0.0f, 0.0f, -1.0f);

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

} // namespace Binax
