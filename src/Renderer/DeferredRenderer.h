#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>
#include "GBuffer.h"
#include "Framebuffer.h"      // <-- NEW
#include "Shader.h"
#include "Mesh.h"
#include "GPULight.h"
#include "Cubemap.h"
#include "ECS/Components/MaterialComponent.h"

namespace Binax {

struct SkyboxParams
{
    const Cubemap* CubemapPtr = nullptr;
    float RotationRadians     = 0.0f;
    float Brightness          = 1.0f;
};

// <-- NEW
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

    void GeometryPass(const Mesh& mesh,
                      const glm::mat4& modelMatrix,
                      const glm::mat4& viewMatrix,
                      const glm::mat4& projectionMatrix,
                      const MaterialComponent& material);

    void LightingPass(const std::vector<GPULight>& lights,
                      const glm::vec3& cameraPosition,
                      const glm::mat4& invViewProj,
                      const SkyboxParams& skybox = {},
                      uint32_t targetFBO = 0,
                      uint32_t width = 0,
                      uint32_t height = 0,
                      const FXAAParams& fxaa = {});   // <-- NEW

    void EndGeometryPass();

    const GBuffer& GetGBuffer() const { return *m_GBuffer; }

    const FrameStats& GetFrameStats() const { return m_FrameStats; }
    void ResetFrameStats() { m_FrameStats = {}; }

private:
    std::unique_ptr<GBuffer>     m_GBuffer;
    std::unique_ptr<Shader>      m_GeometryShader;
    std::unique_ptr<Shader>      m_LightingShader;
    std::unique_ptr<Shader>      m_FXAAShader;      // <-- NEW
    std::unique_ptr<Framebuffer> m_LightingFBO;     // <-- NEW (когда FXAA включён)
    Mesh m_FullscreenQuad;

    FrameStats m_FrameStats;

    void InitShaders();

    // <-- NEW
    void ApplyFXAA(uint32_t sourceTexture,
                   uint32_t targetFBO,
                   uint32_t width, uint32_t height,
                   const FXAAParams& params);
};

}
