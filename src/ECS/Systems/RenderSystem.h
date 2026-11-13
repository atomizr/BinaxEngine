#pragma once

#include <entt/entt.hpp>
#include <algorithm>
#include <cstdint>
#include <vector>

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "Renderer/DeferredRenderer.h"
#include "Renderer/DrawCommand.h"
#include "Renderer/GPULight.h"
#include "Renderer/InstanceBuffer.h"
#include "Renderer/MaterialLibrary.h"
#include "Renderer/MeshLibrary.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/PrevTransform.h"
#include "ECS/Components/WorldTransform.h"
#include "ECS/Components/MeshComponent.h"
#include "ECS/Components/MaterialComponent.h"
#include "ECS/Components/LightComponent.h"
#include "ECS/Components/EntityFlags.h"

namespace Binax {

struct FrameRenderData
{
    std::vector<InstanceGPU>          Instances;
    std::vector<entt::entity>         InstanceEntities;
    std::vector<DrawElementsIndirect> Draws;
    std::vector<uint32_t>             DrawVertexCounts;
};

class RenderSystem
{
public:
    static void BuildFrameData(entt::registry& registry, FrameRenderData& out)
    {
        out.Instances.clear();
        out.InstanceEntities.clear();
        out.Draws.clear();
        out.DrawVertexCounts.clear();

        struct Candidate
        {
            entt::entity   Entity;
            MeshHandle     Mesh;
            MaterialHandle Material;
            glm::mat4      Model;
            glm::mat4      PrevModel;
        };

        std::vector<Candidate> cands;
        auto view = registry.view<Transform, MeshComponent>();
        cands.reserve(view.size_hint());
        out.Instances.reserve(view.size_hint());

        const MaterialHandle fallbackMat =
            MaterialLibrary::Get().GetOrCreateDefault();

        for (auto e : view)
        {
            if (registry.any_of<EntityFlags>(e) &&
                !registry.get<EntityFlags>(e).Enabled)
                continue;

            const auto& mc = view.get<MeshComponent>(e);
            if (mc.MeshId == kInvalidMesh || !MeshLibrary::Get().IsValid(mc.MeshId))
                continue;

            const auto& tr = view.get<Transform>(e);

            MaterialHandle mat = kInvalidMaterial;
            if (registry.any_of<MaterialComponent>(e))
                mat = registry.get<MaterialComponent>(e).MaterialId;
            if (mat == kInvalidMaterial)
                mat = fallbackMat;

            const glm::mat4 model =
                [&]() -> glm::mat4
                {
                    if (const auto* wt = registry.try_get<WorldTransform>(e))
                        return wt->Matrix;
                    return tr.GetModelMatrix();
                }();

            auto& history = registry.get_or_emplace<PrevTransform>(e);
            const glm::mat4 prevModel = history.Valid ? history.PrevModel : model;
            history.PrevModel = model;
            history.Valid     = true;

            cands.push_back({ e, mc.MeshId, mat, model, prevModel });
        }

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
                g.Model        = cands[k].Model;
                g.PrevModel    = cands[k].PrevModel;
                g.NormalMatrix = glm::inverseTranspose(cands[k].Model);
                g.MaterialId   = cands[k].Material;
                g.MeshId       = meshId;
                g.BoneOffset   = 0;
                g.Flags        = InstanceFlags::None;
                out.Instances.push_back(g);
                out.InstanceEntities.push_back(cands[k].Entity);
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
            out.DrawVertexCounts.push_back(mesh.VertexCount);

            i = j;
        }
    }

    static void CollectLights(entt::registry& registry, std::vector<GPULight>& out)
    {
        out.clear();
        out.reserve(kMaxLights);

        auto view = registry.view<Transform, LightComponent>();
        for (auto entity : view)
        {
            if (registry.any_of<EntityFlags>(entity) &&
                !registry.get<EntityFlags>(entity).Enabled)
                continue;

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
            gpu.ShadowMode     = static_cast<int>(lc.ShadowMode);
            gpu.ShadowStrength = lc.ShadowStrength;
            gpu.BiasMultiplier = lc.BiasMultiplier;

            switch (lc.Type)
            {
                case LightType::Directional: gpu.LightType = GPULight::Directional; break;
                case LightType::Point:       gpu.LightType = GPULight::Point;       break;
                case LightType::Spot:        gpu.LightType = GPULight::Spot;        break;
            }

            out.push_back(gpu);
        }
    }







    static entt::entity PickEntityAtUV(const FrameRenderData& frame,
                                       const GBuffer& gbuffer,
                                       float u, float v)
    {
        if (gbuffer.GetFramebufferID() == 0)
            return entt::null;

        if (frame.Instances.empty() || frame.InstanceEntities.empty())
            return entt::null;

        const int w = (int)gbuffer.GetWidth();
        const int h = (int)gbuffer.GetHeight();
        if (w <= 0 || h <= 0)
            return entt::null;

        int px = (int)(u * (float)w);
        int py = (int)(v * (float)h);
        px = std::clamp(px, 0, w - 1);
        py = std::clamp(py, 0, h - 1);

        float worldPos[4] = { 0.0f, 0.0f, 0.0f, 0.0f };


        GLint prevReadFBO = 0;
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevReadFBO);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, gbuffer.GetFramebufferID());
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(px, py, 1, 1, GL_RGBA, GL_FLOAT, worldPos);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)prevReadFBO);

        const glm::vec3 p(worldPos[0], worldPos[1], worldPos[2]);


        if (p.x == 0.0f && p.y == 0.0f && p.z == 0.0f)
            return entt::null;

        const size_t n = std::min(frame.Instances.size(),
                                  frame.InstanceEntities.size());

        for (size_t i = 0; i < n; ++i)
        {
            const auto& inst = frame.Instances[i];
            if (!MeshLibrary::Get().IsValid(inst.MeshId))
                continue;

            const MeshGPU& mesh = MeshLibrary::Get().Get(inst.MeshId);


            const glm::mat4 invModel = glm::inverse(inst.Model);
            const glm::vec3 lp = glm::vec3(invModel * glm::vec4(p, 1.0f));


            const glm::vec3 ext = (mesh.AABBMax - mesh.AABBMin) * 0.005f;

            if (lp.x >= mesh.AABBMin.x - ext.x && lp.x <= mesh.AABBMax.x + ext.x &&
                lp.y >= mesh.AABBMin.y - ext.y && lp.y <= mesh.AABBMax.y + ext.y &&
                lp.z >= mesh.AABBMin.z - ext.z && lp.z <= mesh.AABBMax.z + ext.z)
            {
                return frame.InstanceEntities[i];
            }
        }

        return entt::null;
    }
};

} 