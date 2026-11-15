#pragma once

#include <entt/entt.hpp>
#include <algorithm>
#include <cstdint>
#include <vector>
#include <limits>

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
#include "ECS/Components/ReflectionProbeComponent.h"

namespace Binax {

struct FrameRenderData
{
    std::vector<InstanceGPU>          Instances;
    std::vector<entt::entity>         InstanceEntities;
    std::vector<DrawElementsIndirect> Draws;
    std::vector<uint32_t>             DrawVertexCounts;
        struct ReflectionProbeEntry
    {
        glm::vec3 Position;
        glm::vec3 Extents;
        uint32_t  ProbeIndex;
        bool      BoxProjection;
    };
    std::vector<ReflectionProbeEntry> ReflectionProbes;
    struct TransparentInstance
    {
        entt::entity Entity = entt::null;
        InstanceGPU  GPU{};
        // NOTE: расстояние до камеры здесь не храним — сортировка
        // делается в ForwardTransparentPass по актуальной cameraPosition.
    };
    std::vector<TransparentInstance> TransparentInstances;
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
        out.ReflectionProbes.clear();
        out.TransparentInstances.clear();

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

                // ---- Collect reflection probes ---------------------------------
        struct ProbeLookup {
            glm::vec3 Position;
            glm::vec3 Extents;
            uint32_t  ProbeIndex;
            bool      BoxProjection;
        };
        std::vector<ProbeLookup> probes;
        {
            auto pv = registry.view<Transform, ReflectionProbeComponent>();
            probes.reserve(pv.size_hint());
            for (auto pe : pv)
            {
                const auto& pc = pv.get<ReflectionProbeComponent>(pe);
                const auto& pt = pv.get<Transform>(pe);
                if (pc.ProbeIndex == 0) continue;

                probes.push_back({ pt.Position, pc.BoxExtents,
                                   pc.ProbeIndex, pc.BoxProjection });

                out.ReflectionProbes.push_back({
                    pt.Position, pc.BoxExtents, pc.ProbeIndex, pc.BoxProjection
                });
            }
        }

        auto findProbeFor = [&](const glm::vec3& worldPos) -> uint32_t
        {
            uint32_t best = 0;
            float    bestDist = std::numeric_limits<float>::max();
            for (const auto& p : probes)
            {
                glm::vec3 d = worldPos - p.Position;
                if (std::abs(d.x) <= p.Extents.x &&
                    std::abs(d.y) <= p.Extents.y &&
                    std::abs(d.z) <= p.Extents.z)
                {
                    float dist = glm::length(d);
                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        best = p.ProbeIndex;
                    }
                }
            }
            return best;
        };

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

            // ---- Transparent → отдельный список -----------------------
            const MaterialBlendMode bm =
                MaterialLibrary::Get().GetBlendMode(mat);
            if (bm == MaterialBlendMode::Transparent)
            {
                const uint32_t probeId = findProbeFor(glm::vec3(model[3]));

                InstanceGPU g{};
                g.Model        = model;
                g.PrevModel    = prevModel;
                g.NormalMatrix = glm::inverseTranspose(model);
                g.MaterialId   = mat;
                g.MeshId       = mc.MeshId;
                g.BoneOffset   = 0;
                g.Flags        = InstanceFlags::None | (probeId << 16);

                FrameRenderData::TransparentInstance ti;
                ti.Entity = e;
                ti.GPU    = g;
                out.TransparentInstances.push_back(ti);
                continue;
            }

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
                const uint32_t probeId = findProbeFor(glm::vec3(cands[k].Model[3]));

                InstanceGPU g{};
                g.Model        = cands[k].Model;
                g.PrevModel    = cands[k].PrevModel;
                g.NormalMatrix = glm::inverseTranspose(cands[k].Model);
                g.MaterialId   = cands[k].Material;
                g.MeshId       = meshId;
                g.BoneOffset   = 0;
                g.Flags        = InstanceFlags::None | (probeId << 16);
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

        // Свет должен светить из world-space. Предпочитаем кэш WorldTransform
        // (заполняется TransformSystem::UpdateWorldTransforms), с fallback
        // на локальный Transform — на случай 1-го кадра после создания.
        const glm::mat4 world = [&]() -> glm::mat4
        {
            if (const auto* wt = registry.try_get<WorldTransform>(entity))
                return wt->Matrix;
            return tr.GetModelMatrix();
        }();

        const glm::vec3 worldPos(world[3]);

        // Direction: (0,0,-1) под world-матрицей. w=0 → без трансляции.
        // Нормализуем, чтобы отбросить масштаб родителя.
        glm::vec3 worldDir = glm::vec3(world * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));
        const float dirLen2 = glm::dot(worldDir, worldDir);
        if (dirLen2 > 1e-8f)
            worldDir = glm::normalize(worldDir);
        else
            worldDir = glm::vec3(0.0f, 0.0f, -1.0f);

        GPULight gpu;
        gpu.Position    = worldPos;
        gpu.Range       = lc.Range;
        gpu.Color       = lc.Color;
        gpu.Intensity   = lc.Intensity;
        gpu.InnerCutoff = lc.InnerCutoff;
        gpu.OuterCutoff = lc.OuterCutoff;
        gpu.Direction   = worldDir;
        gpu.ShadowMode     = static_cast<int>(lc.Mode);
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

        if (frame.Instances.empty() && frame.TransparentInstances.empty())
            return entt::null;

        if (frame.InstanceEntities.empty() && frame.TransparentInstances.empty())
            return entt::null;

        const int w = (int)gbuffer.GetWidth();
        const int h = (int)gbuffer.GetHeight();
        if (w <= 0 || h <= 0)
            return entt::null;

        int px = (int)(u * (float)w);
        int py = (int)(v * (float)h);
        px = std::clamp(px, 0, w - 1);
        py = std::clamp(py, 0, h - 1);

        // FIX #7a: сохраняем не только READ_FRAMEBUFFER, но и READ_BUFFER.
        // glReadBuffer() — глобальное состояние; без restore все последующие
        // glReadPixels в других FBO будут вести себя непредсказуемо.
        GLint prevReadFBO = 0;
        GLint prevReadBuf = GL_COLOR_ATTACHMENT0;
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevReadFBO);
        glGetIntegerv(GL_READ_BUFFER,              &prevReadBuf);

        float worldPos[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

        glBindFramebuffer(GL_READ_FRAMEBUFFER, gbuffer.GetFramebufferID());
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(px, py, 1, 1, GL_RGBA, GL_FLOAT, worldPos);

        // FIX #7a: восстанавливаем READ_BUFFER тоже.
        glReadBuffer((GLenum)prevReadBuf);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)prevReadFBO);

        const glm::vec3 p(worldPos[0], worldPos[1], worldPos[2]);

        // Фон (skybox) — нет попадания
        if (p.x == 0.0f && p.y == 0.0f && p.z == 0.0f)
            return entt::null;

        // FIX #7a: раньше искали только среди opaque; прозрачные (стекло,
        // вода) не выбирались кликом. Теперь проверяем оба списка. При
        // коллизии предпочитаем тот, что ближе к камере — но т.к. на входе
        // нет cameraPosition, берём первое попадание в opaque (он был
        // отрисован раньше), затем fallback на transparent.
        auto pointInAABB = [&](const InstanceGPU& inst, const glm::vec3& wp) -> bool
        {
            if (!MeshLibrary::Get().IsValid(inst.MeshId))
                return false;

            const MeshGPU& mesh = MeshLibrary::Get().Get(inst.MeshId);
            const glm::mat4 invModel = glm::inverse(inst.Model);
            const glm::vec3 lp = glm::vec3(invModel * glm::vec4(wp, 1.0f));

            // 0.5% допуск — компенсирует FP-погрешность при обратной матрице
            const glm::vec3 ext = (mesh.AABBMax - mesh.AABBMin) * 0.005f;

            return lp.x >= mesh.AABBMin.x - ext.x && lp.x <= mesh.AABBMax.x + ext.x &&
                   lp.y >= mesh.AABBMin.y - ext.y && lp.y <= mesh.AABBMax.y + ext.y &&
                   lp.z >= mesh.AABBMin.z - ext.z && lp.z <= mesh.AABBMax.z + ext.z;
        };

        const size_t nOpaque = std::min(frame.Instances.size(),
                                        frame.InstanceEntities.size());
        for (size_t i = 0; i < nOpaque; ++i)
        {
            if (pointInAABB(frame.Instances[i], p))
                return frame.InstanceEntities[i];
        }

        // FIX #7a: прозрачные тоже выбираемы
        for (const auto& ti : frame.TransparentInstances)
        {
            if (ti.Entity == entt::null) continue;
            if (pointInAABB(ti.GPU, p))
                return ti.Entity;
        }

        return entt::null;
    }
};

} 