#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

#include "Renderer/Cubemap.h"
#include "Renderer/DrawCommand.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/GBuffer.h"
#include "Renderer/GPULight.h"
#include "Renderer/InstanceBuffer.h"
#include "Renderer/MaterialLibrary.h"
#include "Renderer/MeshLibrary.h"
#include "Renderer/Shader.h"

#include "ECS/Systems/RenderSystem.h"

namespace Binax {

struct SkyboxParams
{
    const Cubemap* CubemapPtr = nullptr;
    float RotationRadians     = 0.0f;
    float Brightness          = 1.0f;
};

struct FXAAParams
{
    bool  Enabled            = false;
    float EdgeThreshold      = 0.063f;
    float EdgeThresholdMin   = 0.0312f;
    float SubpixelBlending   = 1.0f;
};

struct FrameStats
{
    uint32_t DrawCalls = 0;
    uint32_t Vertices  = 0;
    uint32_t Triangles = 0;
};

class DeferredRenderer
{
public:
    DeferredRenderer(uint32_t width, uint32_t height);
    ~DeferredRenderer();

    void Resize(uint32_t width, uint32_t height);
    void ClearGBuffer();

    void GeometryPass(FrameRenderData& frame,
                      const glm::mat4& viewMatrix,
                      const glm::mat4& projectionMatrix);

    void LightingPass(const std::vector<GPULight>& lights,
                      const glm::vec3& cameraPosition,
                      const glm::mat4& invViewProj,
                      const SkyboxParams& skybox = {},
                      uint32_t targetFBO = 0,
                      uint32_t width = 0,
                      uint32_t height = 0,
                      const FXAAParams& fxaa = {});

    const GBuffer& GetGBuffer() const { return *m_GBuffer; }
    const FrameStats& GetFrameStats() const { return m_FrameStats; }
    void ResetFrameStats() { m_FrameStats = {}; }

private:
    std::unique_ptr<GBuffer>     m_GBuffer;
    std::unique_ptr<Shader>      m_GeometryShader;
    std::unique_ptr<Shader>      m_LightingShader;
    std::unique_ptr<Shader>      m_FXAAShader;
    std::unique_ptr<Framebuffer> m_LightingFBO;

    MeshHandle m_QuadHandle = kInvalidMesh;

    uint32_t m_DrawIndirectSSBO = 0;
    uint32_t m_DrawMetaSSBO     = 0;
    uint32_t m_BonesDummySSBO   = 0;
    uint32_t m_DrawCapacity     = 0;

    FrameStats m_FrameStats;

    void InitShaders();
    void EnsureQuad();
    void EnsureDrawBuffers(uint32_t drawCount);
    void DrawFullscreenQuad();

    void ApplyFXAA(uint32_t sourceTexture,
                   uint32_t targetFBO,
                   uint32_t width, uint32_t height,
                   const FXAAParams& params);
};

} // namespace Binax
