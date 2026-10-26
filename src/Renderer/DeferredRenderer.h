#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

#include <glad/glad.h>

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

struct TAAParams
{
    bool  Enabled       = true;
    
    float HistoryWeight = 0.9f;
};



struct PostProcessSettings
{
    bool  Enabled  = false;
    float Exposure = 1.0f;

    
    int TonemapMode = 0;   

    
    float Contrast    = 1.0f;
    float Saturation  = 1.0f;
    float Temperature = 0.0f;
    float Tint        = 0.0f;
    glm::vec3 Lift{ 0.0f };
    glm::vec3 Gamma{ 1.0f };
    glm::vec3 Gain{ 1.0f };

    
    bool  BloomEnabled   = false;
    float BloomThreshold = 1.0f;
    float BloomIntensity = 0.15f;

    
    bool  MotionBlurEnabled  = false;
    float MotionBlurStrength = 0.5f;
    int   MotionBlurSamples  = 8;

    
    bool  LensDistortionEnabled = false;
    float LensDistortion        = 0.0f;

    
    bool  ChromaticAberrationEnabled = false;
    float ChromaticAberration        = 0.0f;

    
    bool  VignetteEnabled    = false;
    float VignetteIntensity  = 0.35f;
    float VignetteSmoothness = 0.5f;

    
    bool  FilmGrainEnabled   = false;
    float FilmGrainIntensity = 0.08f;

    
    bool  FogEnabled = false;
    int   FogMode    = 1;   
    glm::vec3 FogColor{ 0.62f, 0.70f, 0.80f };
    float FogDensity = 0.04f;
    float FogStart   = 5.0f;
    float FogEnd     = 60.0f;
};

struct FrameStats
{
    uint32_t DrawCalls = 0;
    uint32_t Vertices  = 0;
    uint32_t Triangles = 0;

    
    
    uint32_t VisibleInstances = 0;
    bool     GpuCulling       = false;

    
    uint32_t TaaFrameIndex = 0;
    bool     TaaActive     = false;

    bool     PostFXActive  = false;
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
                      const glm::mat4& viewMatrix,
                      const glm::mat4& projectionMatrix,
                      const SkyboxParams& skybox = {},
                      uint32_t targetFBO = 0,
                      uint32_t width = 0,
                      uint32_t height = 0,
                      const FXAAParams& fxaa = {},
                      const TAAParams& taa = {},
                      const PostProcessSettings& post = {},
                      float timeSeconds = 0.0f);

    const GBuffer& GetGBuffer() const { return *m_GBuffer; }
    const FrameStats& GetFrameStats() const { return m_FrameStats; }
    void ResetFrameStats() { m_FrameStats = {}; }

    
    
    
    void SetFrustumCullingEnabled(bool enabled) { m_FrustumCullingEnabled = enabled; }
    bool IsFrustumCullingEnabled() const { return m_FrustumCullingEnabled; }
    bool IsFrustumCullingAvailable() const { return m_CullShader != nullptr; }

    
    void ResetTAAHistory() { m_TAAHistoryValid = false; m_TaaFrameIndex = 0; }

    
    
    void SetTAAEnabled(bool enabled)
    {
        if (m_TAAEnabled == enabled)
            return;
        m_TAAEnabled      = enabled;
        m_TAAHistoryValid = false;
        m_HasPrevViewProj = false;
    }
    bool IsTAAEnabled() const { return m_TAAEnabled; }

private:
    static constexpr uint32_t kCullGroupSize  = 64;
    static constexpr uint32_t kBloomMipCount  = 5;

    std::unique_ptr<GBuffer>     m_GBuffer;
    std::unique_ptr<Shader>      m_GeometryShader;
    std::unique_ptr<Shader>      m_LightingShader;
    std::unique_ptr<Shader>      m_FXAAShader;
    std::unique_ptr<Shader>      m_TAAShader;
    std::unique_ptr<Shader>      m_TonemapShader;
    std::unique_ptr<Shader>      m_CullShader;
    std::unique_ptr<Shader>      m_MotionBlurShader;
    std::unique_ptr<Shader>      m_BloomDownShader;
    std::unique_ptr<Shader>      m_BloomUpShader;

    
    std::unique_ptr<Framebuffer> m_HDRFBO;
    std::unique_ptr<Framebuffer> m_LDRFBO;

    
    std::unique_ptr<Framebuffer> m_PostFBO;

    
    std::unique_ptr<Framebuffer> m_BloomMips[kBloomMipCount];

    
    
    std::unique_ptr<Framebuffer> m_TAAHistory[2];
    int  m_TAAHistoryIndex = 0;
    bool m_TAAHistoryValid = false;
    uint32_t m_TaaFrameIndex = 0;

    
    glm::vec2 m_Jitter{ 0.0f, 0.0f };
    glm::vec2 m_PrevJitter{ 0.0f, 0.0f };
    glm::mat4 m_PrevViewProj{ 1.0f };
    bool      m_HasPrevViewProj = false;

    MeshHandle m_QuadHandle = kInvalidMesh;

    uint32_t m_DrawIndirectSSBO = 0;
    uint32_t m_BonesDummySSBO   = 0;
    uint32_t m_DrawCapacity     = 0;

    uint32_t m_DrawStateSSBO = 0;
    GLsync   m_CullFence     = nullptr;

    struct CullStats
    {
        uint32_t Draws    = 0;
        uint32_t Vertices = 0;
        uint32_t Indices  = 0;
    };
    CullStats m_LastCullStats;

    bool m_FrustumCullingEnabled = true;
    bool m_TAAEnabled            = true;

    FrameStats m_FrameStats;

    void InitShaders();
    void InitCullResources();
    void EnsureQuad();
    void EnsureDrawBuffers(uint32_t drawCount);
    void DrawFullscreenQuad();

    
    
    void EnsurePostTargets(uint32_t width, uint32_t height);

    
    void UpdateJitter(uint32_t width, uint32_t height);
    glm::mat4 JitteredProjection(const glm::mat4& projection) const;

    void ApplyTAA(uint32_t width, uint32_t height,
                  const glm::vec3& cameraPosition,
                  const glm::mat4& viewProj,
                  const glm::mat4& invViewProj,
                  const TAAParams& params,
                  uint32_t& outSourceTexture);

    
    
    uint32_t ApplyMotionBlur(uint32_t sourceTexture,
                             uint32_t width, uint32_t height,
                             const PostProcessSettings& params,
                             float nearPlane, float farPlane);

    
    
    void BuildBloom(uint32_t sourceTexture,
                    uint32_t width, uint32_t height,
                    const PostProcessSettings& params);

    void ApplyTonemap(uint32_t sourceTexture, uint32_t targetFBO,
                      uint32_t width, uint32_t height,
                      const PostProcessSettings& params,
                      uint32_t bloomTexture,
                      float timeSeconds);

    void PrepareCullPass(const glm::mat4& viewMatrix,
                         const glm::mat4& projectionMatrix,
                         uint32_t instanceCount);

    void PollCullStats();

    void ApplyFXAA(uint32_t sourceTexture,
                   uint32_t targetFBO,
                   uint32_t width, uint32_t height,
                   const FXAAParams& params);
};

} 
