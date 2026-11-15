#include "Renderer/DeferredRenderer.h"
#include "Renderer/AssetRegistry.h"
#include "Renderer/Mesh.h"
#include "Renderer/MeshLibrary.h"
#include "Renderer/InstanceBuffer.h"
#include "Renderer/ReflectionProbeLibrary.h"
#include "Utils/Log.h"
#include "Utils/Paths.h"
#include "Utils/Frustum.h"

#include <glad/glad.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <random>

namespace {

glm::vec2 HaltonJitter(uint32_t frame, uint32_t width, uint32_t height)
{
    auto halton = [](uint32_t index, uint32_t base)
    {
        float f = 1.0f;
        float r = 0.0f;
        while (index > 0u)
        {
            f /= static_cast<float>(base);
            r += f * static_cast<float>(index % base);
            index /= base;
        }
        return r;
    };

    const uint32_t index = (frame % 16u) + 1u;
    const float jx = halton(index, 2u) - 0.5f;
    const float jy = halton(index, 3u) - 0.5f;

    constexpr float kAmplitude = 0.7f;
    return glm::vec2(jx * kAmplitude * 2.0f / static_cast<float>(width > 0 ? width : 1),
                     jy * kAmplitude * 2.0f / static_cast<float>(height > 0 ? height : 1));
}

struct LightUniformNames
{
    std::string Position, Range, Direction, Intensity, Color, LightType, InnerCutoff, OuterCutoff;
    std::string ShadowMode, ShadowStrength, BiasMultiplier, ShadowSlot;
};

const std::array<LightUniformNames, Binax::kMaxLights>& GetLightUniformNames()
{
    static const auto names = []() {
        std::array<LightUniformNames, Binax::kMaxLights> arr;
        for (int i = 0; i < Binax::kMaxLights; ++i)
        {
            const std::string base = "u_Lights[" + std::to_string(i) + "].";
            auto& n = arr[i];
            n.Position = base + "Position";       n.Range = base + "Range";
            n.Direction = base + "Direction";     n.Intensity = base + "Intensity";
            n.Color = base + "Color";             n.LightType = base + "LightType";
            n.InnerCutoff = base + "InnerCutoff"; n.OuterCutoff = base + "OuterCutoff";
            n.ShadowMode     = base + "ShadowMode";
            n.ShadowStrength = base + "ShadowStrength";
            n.BiasMultiplier = base + "BiasMultiplier";
            n.ShadowSlot     = base + "ShadowSlot";
        }
        return arr;
    }();
    return names;
}

} 

namespace Binax {

DeferredRenderer::DeferredRenderer(uint32_t width, uint32_t height)
{
    m_GBuffer = std::make_unique<GBuffer>(width, height);
    InitShaders();
    InitCullResources();
    EnsureQuad();
    EnsurePostTargets(width, height);
    GenerateSSAOKernel();
}

DeferredRenderer::~DeferredRenderer()
{
    if (m_CullFence)        glDeleteSync(m_CullFence);
    if (m_DrawIndirectSSBO) glDeleteBuffers(1, &m_DrawIndirectSSBO);
    if (m_BonesDummySSBO)   glDeleteBuffers(1, &m_BonesDummySSBO);
    if (m_DrawStateSSBO)    glDeleteBuffers(1, &m_DrawStateSSBO);
    if (m_GroupDataSSBO)    glDeleteBuffers(1, &m_GroupDataSSBO);   
    if (m_ShadowIndirectSSBO) glDeleteBuffers(1, &m_ShadowIndirectSSBO);
}

void DeferredRenderer::InitCullResources()
{
    glGenBuffers(1, &m_DrawStateSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_DrawStateSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 4 * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // dummy bones — нужен и shadow pass, и geometry pass
    if (m_BonesDummySSBO == 0)
    {
        glGenBuffers(1, &m_BonesDummySSBO);
        glm::mat4 dummy(1.0f);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_BonesDummySSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::mat4), &dummy, GL_STATIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
}

void DeferredRenderer::EnsureQuad()
{
    m_QuadHandle = MeshLibrary::Get().Find("FullscreenQuad");
    if (m_QuadHandle == kInvalidMesh)
    {
        auto data = Mesh::CreateFullscreenQuad();
        m_QuadHandle = MeshLibrary::Get().Add("FullscreenQuad", data.Vertices, data.Indices);
        MeshLibrary::Get().Flush();
    }
}

void DeferredRenderer::Resize(uint32_t width, uint32_t height)
{
    if (width == m_GBuffer->GetWidth() && height == m_GBuffer->GetHeight())
        return;

    m_GBuffer->Resize(width, height);

    if (m_HDRFBO)          m_HDRFBO->Resize(width, height);
    if (m_LDRFBO)          m_LDRFBO->Resize(width, height);
    if (m_TAAHistory[0])   m_TAAHistory[0]->Resize(width, height);
    if (m_TAAHistory[1])   m_TAAHistory[1]->Resize(width, height);

    m_TAAHistoryValid = false;
    m_HasPrevViewProj = false;
    m_TaaFrameIndex   = 0;
    m_PrevJitter      = glm::vec2(0.0f);   
}

void DeferredRenderer::ClearGBuffer()
{
    m_FrameStats = {};

    UpdateJitter(m_GBuffer->GetWidth(), m_GBuffer->GetHeight());

    m_GBuffer->Bind();
    glViewport(0, 0, (GLsizei)m_GBuffer->GetWidth(), (GLsizei)m_GBuffer->GetHeight());

    GLfloat prev[4]; glGetFloatv(GL_COLOR_CLEAR_VALUE, prev);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(prev[0], prev[1], prev[2], prev[3]);

    m_GBuffer->Unbind();
}

void DeferredRenderer::UpdateJitter(uint32_t width, uint32_t height)
{
    m_Jitter = m_TAAEnabled ? HaltonJitter(m_TaaFrameIndex, width, height)
                            : glm::vec2(0.0f);
    m_TaaFrameIndex++;
}

glm::mat4 DeferredRenderer::JitteredProjection(const glm::mat4& projection) const
{
    glm::mat4 jitter(1.0f);
    jitter[3][0] = m_Jitter.x;
    jitter[3][1] = m_Jitter.y;
    return jitter * projection;
}

void DeferredRenderer::EnsurePostTargets(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    const bool sizeChanged =
        !m_HDRFBO || m_HDRFBO->GetWidth() != width || m_HDRFBO->GetHeight() != height;

    if (!m_HDRFBO)
    {
        m_HDRFBO = std::make_unique<Framebuffer>(width, height);
        m_HDRFBO->Bind();
        m_HDRFBO->AttachColorTexture(GL_RGBA16F, GL_RGBA, GL_FLOAT, 0);
        m_HDRFBO->CheckStatus();
        m_HDRFBO->Unbind();
    }
    else m_HDRFBO->Resize(width, height);

    if (!m_LDRFBO)
    {
        m_LDRFBO = std::make_unique<Framebuffer>(width, height);
        m_LDRFBO->Bind();
        m_LDRFBO->AttachColorTexture(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        m_LDRFBO->CheckStatus();
        m_LDRFBO->Unbind();
    }
    else m_LDRFBO->Resize(width, height);

    
    
    
    glBindTexture(GL_TEXTURE_2D, m_LDRFBO->GetColorAttachment(0));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (!m_PostFBO)
    {
        m_PostFBO = std::make_unique<Framebuffer>(width, height);
        m_PostFBO->Bind();
        m_PostFBO->AttachColorTexture(GL_RGBA16F, GL_RGBA, GL_FLOAT, 0);
        m_PostFBO->CheckStatus();
        m_PostFBO->Unbind();
    }
    else m_PostFBO->Resize(width, height);

    for (uint32_t i = 0; i < kBloomMipCount; ++i)
    {
        const uint32_t mipW = std::max(1u, width  >> (i + 1));
        const uint32_t mipH = std::max(1u, height >> (i + 1));

        if (!m_BloomMips[i])
        {
            m_BloomMips[i] = std::make_unique<Framebuffer>(mipW, mipH);
            m_BloomMips[i]->Bind();
            m_BloomMips[i]->AttachColorTexture(GL_RGBA16F, GL_RGBA, GL_FLOAT, 0);
            m_BloomMips[i]->CheckStatus();
            m_BloomMips[i]->Unbind();

            glBindTexture(GL_TEXTURE_2D, m_BloomMips[i]->GetColorAttachment(0));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        else m_BloomMips[i]->Resize(mipW, mipH);
    }

    for (int i = 0; i < 2; ++i)
    {
        if (!m_TAAHistory[i])
        {
            m_TAAHistory[i] = std::make_unique<Framebuffer>(width, height);
            m_TAAHistory[i]->Bind();
            m_TAAHistory[i]->AttachColorTexture(GL_RGBA16F, GL_RGBA, GL_FLOAT, 0);
            m_TAAHistory[i]->CheckStatus();
            m_TAAHistory[i]->Unbind();

            glBindTexture(GL_TEXTURE_2D, m_TAAHistory[i]->GetColorAttachment(0));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        else m_TAAHistory[i]->Resize(width, height);
    }

    if (sizeChanged)
    {
        m_TAAHistoryValid = false;
        m_HasPrevViewProj = false;
        m_TaaFrameIndex   = 0;
    }

    if (!m_OutlineFBO)
    {
        m_OutlineFBO = std::make_unique<Framebuffer>(width, height);
        m_OutlineFBO->Bind();
        m_OutlineFBO->AttachColorTexture(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        m_OutlineFBO->CheckStatus();
        m_OutlineFBO->Unbind();

        glBindTexture(GL_TEXTURE_2D, m_OutlineFBO->GetColorAttachment(0));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    else
    {
        m_OutlineFBO->Resize(width, height);
    }
}

void DeferredRenderer::InitShaders()
{
    const std::string dir = "shaders/";

    const std::string geometryVert = Paths::Resolve(dir + "geometry.vert");
    const std::string geometryFrag = Paths::Resolve(dir + "geometry.frag");
    const std::string lightingVert = Paths::Resolve(dir + "lighting.vert");
    const std::string lightingFrag = Paths::Resolve(dir + "lighting.frag");
    const std::string fxaaVert     = Paths::Resolve(dir + "fxaa.vert");
    const std::string fxaaFrag     = Paths::Resolve(dir + "fxaa.frag");

    m_GeometryShader = Shader::FromFiles(geometryVert, geometryFrag);
    m_LightingShader = Shader::FromFiles(lightingVert, lightingFrag);
    m_FXAAShader     = Shader::FromFiles(fxaaVert, fxaaFrag);

    m_TAAShader     = Shader::FromFiles(lightingVert, Paths::Resolve(dir + "taa.frag"));
    m_TonemapShader = Shader::FromFiles(lightingVert, Paths::Resolve(dir + "tonemap.frag"));
    m_MotionBlurShader = Shader::FromFiles(lightingVert, Paths::Resolve(dir + "motion_blur.frag"));
    m_BloomDownShader  = Shader::FromFiles(lightingVert, Paths::Resolve(dir + "bloom_down.frag"));
    m_BloomUpShader    = Shader::FromFiles(lightingVert, Paths::Resolve(dir + "bloom_up.frag"));
     m_OutlineShader = Shader::FromFiles(
    Paths::Resolve(dir + "outline.vert"),
    Paths::Resolve(dir + "outline.frag"));
    m_ShadowDepthShader = Shader::FromFiles(
    Paths::Resolve(dir + "shadow_depth.vert"),
    Paths::Resolve(dir + "shadow_depth.frag"));
    m_ShadowDepthPointShader = Shader::FromFiles(
    Paths::Resolve(dir + "shadow_depth_point.vert"),
    Paths::Resolve(dir + "shadow_depth_point.frag"));
        m_ForwardShader = Shader::FromFiles(
        Paths::Resolve(dir + "forward.vert"),
        Paths::Resolve(dir + "forward.frag"));
            m_SSAOShader     = Shader::FromFiles(lightingVert, Paths::Resolve(dir + "ssao.frag"));
    m_SSAOBlurShader = Shader::FromFiles(lightingVert, Paths::Resolve(dir + "ssao_blur.frag"));
    m_VolumetricFogShader = Shader::FromFiles(lightingVert, Paths::Resolve(dir + "volumetric_fog.frag"));
    m_VolumetricFogBlurShader = Shader::FromFiles(lightingVert, Paths::Resolve(dir + "volumetric_fog_blur.frag"));
    m_VolumetricFogCompositeShader = Shader::FromFiles(lightingVert, Paths::Resolve(dir + "volumetric_fog_composite.frag"));
        m_ProbeSkyboxShader = Shader::FromFiles(
        Paths::Resolve(dir + "lighting.vert"),
        Paths::Resolve(dir + "probe_skybox.frag"));

    const std::string cullPath = Paths::Resolve(dir + "frustum_cull.comp");
    try
    {
        m_CullShader = Shader::FromFiles(cullPath, cullPath, Shader::Stage::Compute);
        BINAX_LOG_INFO("Frustum culling: compute shader loaded");
    }
    catch (const std::exception& e)
    {
        m_CullShader.reset();
        m_FrustumCullingEnabled = false;
        BINAX_LOG_WARN("Frustum culling disabled — compute shader failed: {0}", e.what());
    }
}

void DeferredRenderer::EnsureDrawBuffers(uint32_t drawCount)
{
    if (m_DrawCapacity >= drawCount && m_DrawIndirectSSBO != 0)
        return;

    uint32_t cap = (m_DrawCapacity == 0) ? 256u : m_DrawCapacity;
    while (cap < drawCount) cap *= 2;
    m_DrawCapacity = cap;

    if (m_DrawIndirectSSBO == 0) glGenBuffers(1, &m_DrawIndirectSSBO);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_DrawIndirectSSBO);
    glBufferData(GL_DRAW_INDIRECT_BUFFER,
                 (GLsizeiptr)(cap * sizeof(DrawElementsIndirect)),
                 nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

    
    if (m_GroupDataSSBO == 0) glGenBuffers(1, &m_GroupDataSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_GroupDataSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 (GLsizeiptr)(cap * sizeof(DrawElementsIndirect)),
                 nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    if (m_BonesDummySSBO == 0)
    {
        glGenBuffers(1, &m_BonesDummySSBO);
        glm::mat4 dummy(1.0f);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_BonesDummySSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::mat4), &dummy, GL_STATIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
}

void DeferredRenderer::GeometryPass(FrameRenderData& frame,
                                    const glm::mat4& viewMatrix,
                                    const glm::mat4& projectionMatrix)
{
    if (frame.Draws.empty()) return;

    MaterialLibrary::Get().Flush();
    // FIX #57: instances уже загружены main.cpp.

    const uint32_t groupCount = (uint32_t)frame.Draws.size();   
    const bool gpuCull = m_FrustumCullingEnabled && m_CullShader != nullptr &&
                         !frame.Instances.empty();

    EnsureDrawBuffers(groupCount);   

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_DrawIndirectSSBO);

    if (!gpuCull)
    {
        glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0,
                        (GLsizeiptr)(groupCount * sizeof(DrawElementsIndirect)),
                        frame.Draws.data());
    }
    else
    {
        
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_GroupDataSSBO);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                        (GLsizeiptr)(groupCount * sizeof(DrawElementsIndirect)),
                        frame.Draws.data());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, InstanceBuffer::Get().GetSSBO());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, MaterialLibrary::Get().GetMaterialSSBO());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_BonesDummySSBO);

    m_GBuffer->Bind();
    glViewport(0, 0, (GLsizei)m_GBuffer->GetWidth(), (GLsizei)m_GBuffer->GetHeight());

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glFrontFace(GL_CCW);

    if (gpuCull)
        PrepareCullPass(viewMatrix, projectionMatrix, groupCount);

    const glm::mat4 projectionJittered = JitteredProjection(projectionMatrix);

    if (!m_HasPrevViewProj)
    {
        m_PrevViewProj    = projectionJittered * viewMatrix;
        // FIX(TAA): синхронизируем PrevJitter с тем jitter'ом, который
        // реально использован для m_PrevViewProj. Без этого на первом
        // кадре после включения TAA motion blur видит delta = (0 - jitter)/2
        // и накладывает фантомную скорость на статичную сцену.
        m_PrevJitter      = m_Jitter;
        m_HasPrevViewProj = true;
    }

    m_GeometryShader->Bind();
    m_GeometryShader->SetMat4("u_View",          viewMatrix);
    m_GeometryShader->SetMat4("u_Projection",    projectionJittered);
    m_GeometryShader->SetMat4("u_PrevViewProj",  m_PrevViewProj);

    MeshLibrary::Get().BindVAO();
    if (gpuCull)
    {
        glBindBuffer(GL_PARAMETER_BUFFER, m_DrawStateSSBO);
        glMultiDrawElementsIndirectCount(GL_TRIANGLES, GL_UNSIGNED_INT,
                                         (void*)0,
                                          0,
                                         (GLsizei)groupCount,   
                                         0);
        glBindBuffer(GL_PARAMETER_BUFFER, 0);
    }
    else
    {
        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT,
                                    (void*)0,
                                    (GLsizei)groupCount,
                                    0);
    }
    glBindVertexArray(0);

    if (gpuCull)
    {
        m_FrameStats.GpuCulling       = true;
        m_FrameStats.DrawCalls        = m_LastCullStats.Draws;
        
        
        m_FrameStats.VisibleInstances = m_LastCullStats.VisibleInstances;
        m_FrameStats.Vertices         = m_LastCullStats.Vertices;
        m_FrameStats.Triangles        = m_LastCullStats.Indices / 3;

        if (m_CullFence == nullptr)
            m_CullFence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }
    else
    {
        m_FrameStats.GpuCulling = false;
        m_FrameStats.DrawCalls += groupCount;
        for (size_t i = 0; i < frame.Draws.size(); ++i)
        {
            const auto& d = frame.Draws[i];
            m_FrameStats.Triangles += (d.Count / 3) * d.InstanceCount;
            const uint32_t verts = (i < frame.DrawVertexCounts.size())
                                       ? frame.DrawVertexCounts[i]
                                       : 0u;
            m_FrameStats.Vertices += verts * d.InstanceCount;
        }
    }

    m_GeometryShader->Unbind();

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    m_GBuffer->Unbind();
}

void DeferredRenderer::PrepareCullPass(const glm::mat4& viewMatrix,
                                       const glm::mat4& projectionMatrix,
                                       uint32_t groupCount)
{
    PollCullStats();

    
    static const GLuint kZeroState[4] = { 0u, 0u, 0u, 0u };
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_DrawStateSSBO);
    glClearBufferSubData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, 0,
                         sizeof(kZeroState), GL_RED_INTEGER, GL_UNSIGNED_INT,
                         kZeroState);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

    if (MeshLibrary::Get().GetMeshSSBO() == 0)
        MeshLibrary::Get().Flush();

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, MeshLibrary::Get().GetMeshSSBO());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, m_DrawIndirectSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, m_DrawStateSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, m_GroupDataSSBO);   

    const glm::mat4 viewProj  = projectionMatrix * viewMatrix;
    const Frustum   frustum   = Frustum::FromViewProjection(viewProj);

    m_CullShader->Bind();
    for (int i = 0; i < 6; ++i)
    {
        const std::string name = "u_Frustum[" + std::to_string(i) + "]";
        m_CullShader->SetVec4(name, frustum.Planes[i]);
    }
    
m_CullShader->SetUInt("u_GroupCount", groupCount);
m_CullShader->SetUInt("u_MaxDraws",   groupCount);
m_CullShader->SetUInt("u_MeshCount",  MeshLibrary::Get().GetMeshCount());

    
    glDispatchCompute(groupCount, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

    m_CullShader->Unbind();
}

void DeferredRenderer::PollCullStats()
{
    if (m_CullFence == nullptr)
        return;

    const GLenum status = glClientWaitSync(m_CullFence, GL_SYNC_FLUSH_COMMANDS_BIT, 0);

    if (status == GL_ALREADY_SIGNALED || status == GL_CONDITION_SATISFIED)
    {
        uint32_t values[4] = { 0u, 0u, 0u, 0u };   
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_DrawStateSSBO);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(values), values);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        m_LastCullStats.Draws            = values[0];
        m_LastCullStats.Vertices         = values[1];
        m_LastCullStats.Indices          = values[2];
        m_LastCullStats.VisibleInstances = values[3];   

        glDeleteSync(m_CullFence);
        m_CullFence = nullptr;
    }
    else if (status == GL_WAIT_FAILED)
    {
        glDeleteSync(m_CullFence);
        m_CullFence = nullptr;
    }
}

void DeferredRenderer::DrawFullscreenQuad()
{
    if (!MeshLibrary::Get().IsValid(m_QuadHandle))
        return;

    const MeshGPU& q = MeshLibrary::Get().Get(m_QuadHandle);
    MeshLibrary::Get().BindVAO();

    glDrawElementsBaseVertex(
        GL_TRIANGLES,
        q.IndexCount,
        GL_UNSIGNED_INT,
        (void*)(uintptr_t)(q.BaseIndex * sizeof(uint32_t)),
        (GLint)q.BaseVertex);

    glBindVertexArray(0);
}

void DeferredRenderer::EnsureShadowMap(uint32_t size, uint32_t layers)
{
    if (m_ShadowMap.GetSize() == size && m_ShadowMap.GetLayers() == layers)
        return;
    m_ShadowMap.Create(size, layers);
    BINAX_LOG_INFO("ShadowMap: recreated {}×{}×{} layers",
                   size, size, layers);
}

void DeferredRenderer::EnsureSpotShadowMap(uint32_t size)
{
    if (m_SpotShadowMap.GetSize() == size &&
        m_SpotShadowMap.GetLayers() == (uint32_t)kMaxSpotShadows)
        return;
    m_SpotShadowMap.Create(size, (uint32_t)kMaxSpotShadows);
    BINAX_LOG_INFO("SpotShadowMap: recreated {}×{}×{} layers",
                   size, size, kMaxSpotShadows);
}

void DeferredRenderer::EnsurePointShadowMap(uint32_t size)
{
    if (m_PointShadowMap.GetSize() == size &&
        m_PointShadowMap.GetCubes() == (uint32_t)kMaxPointShadows)
        return;
    m_PointShadowMap.Create(size, (uint32_t)kMaxPointShadows);
    BINAX_LOG_INFO("PointShadowMap: recreated {}×{}×{} cubes",
                   size, size, kMaxPointShadows);
}

void DeferredRenderer::RenderSpotShadow(const GPULight& light, int slot,
                                        FrameRenderData& frame,
                                        const ShadowSettings& settings)
{
    if (slot < 0 || slot >= kMaxSpotShadows) return;

    const float range   = glm::max(light.Range, 0.1f);
    const float nearP   = 0.1f;
    const float outerCos = glm::clamp(light.OuterCutoff, -0.9999f, 0.9999f);
    const float outerAng = std::acos(outerCos);
    const float fovDeg   = glm::degrees(outerAng * 2.0f);

    glm::vec3 pos = light.Position;
    glm::vec3 dir = glm::normalize(light.Direction);
    glm::vec3 up  = (std::abs(dir.y) < 0.99f) ? glm::vec3(0,1,0) : glm::vec3(1,0,0);

    glm::mat4 view = glm::lookAt(pos, pos + dir, up);
    glm::mat4 proj = glm::perspective(glm::radians(fovDeg), 1.0f, nearP, range);
    glm::mat4 lightSpace = proj * view;

    m_SpotLightSpaceMatrix[slot] = lightSpace;

    m_SpotShadowMap.BindLayer((uint32_t)slot);
    glClear(GL_DEPTH_BUFFER_BIT);

    m_ShadowDepthShader->Bind();
    m_ShadowDepthShader->SetMat4("u_LightViewProj", lightSpace);

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_ShadowIndirectSSBO);
    glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT,
                                (void*)0, (GLsizei)frame.Draws.size(), 0);
    m_ShadowDepthShader->Unbind();
}

void DeferredRenderer::RenderPointShadow(const GPULight& light, int slot,
                                         FrameRenderData& frame,
                                         const ShadowSettings& settings)
{
    if (slot < 0 || slot >= kMaxPointShadows) return;

    const float range = glm::max(light.Range, 0.1f);
    const float nearP = 0.1f;

    m_PointShadowNear[slot] = nearP;
    m_PointShadowFar [slot] = range;

    const glm::vec3 pos = light.Position;
    const glm::mat4 proj = glm::perspective(glm::radians(90.0f),
                                            1.0f, nearP, range);

    static const glm::vec3 kDirs[6] = {
        { 1, 0, 0}, {-1, 0, 0},
        { 0, 1, 0}, { 0,-1, 0},
        { 0, 0, 1}, { 0, 0,-1}
    };
    static const glm::vec3 kUps[6] = {
        {0,-1, 0}, {0,-1, 0},
        {0, 0, 1}, {0, 0,-1},
        {0,-1, 0}, {0,-1, 0}
    };

    m_ShadowDepthPointShader->Bind();
    m_ShadowDepthPointShader->SetVec3("u_LightPos", pos);
    m_ShadowDepthPointShader->SetFloat("u_Far", range);

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_ShadowIndirectSSBO);

    for (int f = 0; f < 6; ++f)
    {
        glm::mat4 view = glm::lookAt(pos, pos + kDirs[f], kUps[f]);
        glm::mat4 lightSpace = proj * view;

        m_PointShadowMap.BindCubeFace((uint32_t)slot, (uint32_t)f);
        glClear(GL_DEPTH_BUFFER_BIT);

        m_ShadowDepthPointShader->SetMat4("u_LightViewProj", lightSpace);

        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT,
                                    (void*)0, (GLsizei)frame.Draws.size(), 0);
    }

    m_ShadowDepthPointShader->Unbind();
}

void DeferredRenderer::EnsureShadowDrawBuffers(uint32_t drawCount)
{
    if (m_ShadowIndirectCapacity >= drawCount && m_ShadowIndirectSSBO != 0)
        return;
    uint32_t cap = (m_ShadowIndirectCapacity == 0) ? 256u : m_ShadowIndirectCapacity;
    while (cap < drawCount) cap *= 2;
    m_ShadowIndirectCapacity = cap;

    if (m_ShadowIndirectSSBO == 0)
        glGenBuffers(1, &m_ShadowIndirectSSBO);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_ShadowIndirectSSBO);
    glBufferData(GL_DRAW_INDIRECT_BUFFER,
                 (GLsizeiptr)(cap * sizeof(DrawElementsIndirect)),
                 nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void DeferredRenderer::ComputeCascades(
    const glm::mat4& camView,
    float nearPlane, float farPlane,
    float fovY, float aspect,
    const glm::vec3& lightDir,
    const ShadowSettings& settings,
    int cascadeCount,
    float* splits,
    glm::mat4* outMatrices)
{
    // 1. Cascade splits (practical split scheme)
    for (int i = 1; i <= cascadeCount; ++i)
    {
        float p = float(i) / float(cascadeCount);
        float logSplit = nearPlane * std::pow(farPlane / nearPlane, p);
        float uniSplit = nearPlane + (farPlane - nearPlane) * p;
        splits[i - 1] = settings.CascadeSplitLambda * logSplit
                      + (1.0f - settings.CascadeSplitLambda) * uniSplit;
    }

    // 2. Frustum corners in view space for given Z
    const float tanHalfFovY = std::tan(fovY * 0.5f);
    const float tanHalfFovX = tanHalfFovY * aspect;
    const float ndcX[4] = { -1.0f,  1.0f,  1.0f, -1.0f };
    const float ndcY[4] = {  1.0f,  1.0f, -1.0f, -1.0f };

    const glm::mat4 invView = glm::inverse(camView);
    const glm::vec3 lightDirN = glm::normalize(lightDir);

    for (int c = 0; c < cascadeCount; ++c)
    {
        const float zNear = (c == 0) ? nearPlane : splits[c - 1];
        const float zFar  = splits[c];

        glm::vec3 nearC[4], farC[4];
        for (int i = 0; i < 4; ++i)
        {
            const glm::vec3 nv(ndcX[i] * tanHalfFovX * zNear,
                               ndcY[i] * tanHalfFovY * zNear,
                               -zNear);
            const glm::vec3 fv(ndcX[i] * tanHalfFovX * zFar,
                               ndcY[i] * tanHalfFovY * zFar,
                               -zFar);
            nearC[i] = glm::vec3(invView * glm::vec4(nv, 1.0f));
            farC[i]  = glm::vec3(invView * glm::vec4(fv, 1.0f));
        }

        // 3. Frustum slice center + bounding sphere radius
        glm::vec3 center(0.0f);
        for (int i = 0; i < 4; ++i) center += nearC[i] + farC[i];
        center *= 0.125f;

        float radius = 0.0f;
        for (int i = 0; i < 4; ++i)
        {
            radius = std::max(radius, glm::length(nearC[i] - center));
            radius = std::max(radius, glm::length(farC[i]  - center));
        }

        // 4. Snap radius up to whole texels
        const uint32_t res = (settings.GlobalResolution > 0)
                             ? (uint32_t)settings.GlobalResolution : 2048u;
        float texelWorld = (2.0f * radius) / float(res);
        radius = std::ceil(radius / texelWorld) * texelWorld;

        // 5. Light view — looking from center - dir*radius to center
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        if (std::abs(glm::dot(lightDirN, up)) > 0.99f)
            up = glm::vec3(0.0f, 0.0f, 1.0f);

        glm::mat4 lightView = glm::lookAt(center - lightDirN * radius, center, up);

        // 6. Texel snapping — сдвигаем frustum center к целому текселю в light space
        glm::vec4 cLS = lightView * glm::vec4(center, 1.0f);
        cLS.x = std::floor(cLS.x / texelWorld) * texelWorld;
        cLS.y = std::floor(cLS.y / texelWorld) * texelWorld;
        glm::vec3 snappedCenter = glm::vec3(glm::inverse(lightView) * cLS);

        lightView = glm::lookAt(snappedCenter - lightDirN * radius, snappedCenter, up);

        // 7. Ortho projection
        glm::mat4 lightProj = glm::ortho(-radius, radius,
                                         -radius, radius,
                                          0.0f, 2.0f * radius);

        outMatrices[c] = lightProj * lightView;
    }
}

void DeferredRenderer::RenderShadowPass(
    FrameRenderData& frame,
    const glm::mat4& camView,
    const glm::mat4& camProj,
    std::vector<GPULight>& lights,
    const ShadowSettings& settings)
{
    m_ShadowCaster   = {};
    m_SpotShadowCount  = 0;
    m_PointShadowCount = 0;

    for (auto& L : lights) L.ShadowSlot = -1;

    if (!settings.Enabled || frame.Draws.empty())
        return;

    // ---- Разбор кастеров ---------------------------------------------------
    int directionalIdx = -1;
    std::vector<int> spotIdxs, pointIdxs;

    for (size_t i = 0; i < lights.size(); ++i)
    {
        if (lights[i].ShadowMode == 0) continue;
        if (lights[i].LightType == GPULight::Directional && directionalIdx < 0)
            directionalIdx = (int)i;
        else if (lights[i].LightType == GPULight::Spot &&
                 (int)spotIdxs.size() < kMaxSpotShadows)
            spotIdxs.push_back((int)i);
        else if (lights[i].LightType == GPULight::Point &&
                 (int)pointIdxs.size() < kMaxPointShadows)
            pointIdxs.push_back((int)i);
    }

    if (directionalIdx < 0 && spotIdxs.empty() && pointIdxs.empty())
        return;
    EnsureShadowDrawBuffers((uint32_t)frame.Draws.size());
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_ShadowIndirectSSBO);
    glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0,
                    (GLsizeiptr)(frame.Draws.size() * sizeof(DrawElementsIndirect)),
                    frame.Draws.data());
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, InstanceBuffer::Get().GetSSBO());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_BonesDummySSBO);

    // ---- Common state -----------------------------------------------------
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.5f, 2.0f);

    MeshLibrary::Get().BindVAO();

    // ---- Directional CSM --------------------------------------------------
    if (directionalIdx >= 0)
    {
        const GPULight& light = lights[directionalIdx];
        const int cascadeCount = std::clamp(settings.CascadeCount, 1, 4);

        const float nearPlane = camProj[3][2] / (camProj[2][2] - 1.0f);
        const float farPlane  = camProj[3][2] / (camProj[2][2] + 1.0f);
        const float fovY      = 2.0f * std::atan(1.0f / camProj[1][1]);
        const float aspect    = camProj[1][1] / camProj[0][0];
        const float effFar    = std::min(farPlane, settings.MaxShadowDistance);

        if (effFar > nearPlane && fovY > 0.0f)
        {
            EnsureShadowMap((uint32_t)settings.GlobalResolution,
                            (uint32_t)cascadeCount);

            float splits[4] = {};
            glm::mat4 lightSpace[4]{};
            ComputeCascades(camView, nearPlane, effFar, fovY, aspect,
                            light.Direction, settings, cascadeCount,
                            splits, lightSpace);

            m_ShadowDepthShader->Bind();
            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_ShadowIndirectSSBO);

            for (int c = 0; c < cascadeCount; ++c)
            {
                m_ShadowMap.BindLayer((uint32_t)c);
                glClear(GL_DEPTH_BUFFER_BIT);
                m_ShadowDepthShader->SetMat4("u_LightViewProj", lightSpace[c]);
                glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT,
                                            (void*)0,
                                            (GLsizei)frame.Draws.size(), 0);
            }
            m_ShadowDepthShader->Unbind();

            m_ShadowCaster.LightIndex        = directionalIdx;
            m_ShadowCaster.ShadowMode        = light.ShadowMode;
            m_ShadowCaster.CascadeCount      = cascadeCount;
            m_ShadowCaster.ShadowStrength    = light.ShadowStrength;
            m_ShadowCaster.BiasMultiplier    = light.BiasMultiplier;
            m_ShadowCaster.MaxShadowDistance = settings.MaxShadowDistance;
            m_ShadowCaster.FadeOutDistance   = settings.FadeOutDistance;
            m_ShadowCaster.DepthBias         = settings.DepthBias;
            m_ShadowCaster.NormalBias        = settings.NormalBias;
            m_ShadowCaster.ShadowSoftness    = settings.ShadowSoftness;
            for (int i = 0; i < 4; ++i)
            {
                m_ShadowCaster.LightSpaceMatrices[i] = lightSpace[i];
                m_ShadowCaster.CascadeSplits[i] =
                    (i < cascadeCount) ? splits[i] : 0.0f;
            }
        }
    }

    // ---- Spot shadows -----------------------------------------------------
    if (!spotIdxs.empty())
    {
        EnsureSpotShadowMap((uint32_t)settings.GlobalResolution);

        for (size_t i = 0; i < spotIdxs.size(); ++i)
        {
            const int li = spotIdxs[i];
            lights[li].ShadowSlot = (int)i;
            RenderSpotShadow(lights[li], (int)i, frame, settings);
        }
        m_SpotShadowCount = (uint32_t)spotIdxs.size();
    }

    // ---- Point shadows ----------------------------------------------------
    if (!pointIdxs.empty())
    {
        const uint32_t pointRes = std::max<uint32_t>(256u,
            (uint32_t)settings.GlobalResolution / 2u);
        EnsurePointShadowMap(pointRes);

        for (size_t i = 0; i < pointIdxs.size(); ++i)
        {
            const int li = pointIdxs[i];
            lights[li].ShadowSlot = (int)i;
            RenderPointShadow(lights[li], (int)i, frame, settings);
        }
        m_PointShadowCount = (uint32_t)pointIdxs.size();
    }

    // ---- Restore ----------------------------------------------------------
    glDisable(GL_POLYGON_OFFSET_FILL);
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void DeferredRenderer::LightingPass(
    const std::vector<GPULight>& lights,
    const glm::vec3& cameraPosition,
    const glm::mat4& viewMatrix,
    const glm::mat4& projectionMatrix,
    const SkyboxParams& skybox,
    uint32_t targetFBO,
    uint32_t width,
    uint32_t height,
    const FXAAParams& fxaa,
    const TAAParams& taa,
    const PostProcessSettings& post,
    float timeSeconds,
    const FrameRenderData& frameData)
{
    const uint32_t targetW = width  ? width  : m_GBuffer->GetWidth();
    const uint32_t targetH = height ? height : m_GBuffer->GetHeight();

    EnsurePostTargets(targetW, targetH);

    const glm::mat4 projJittered = JitteredProjection(projectionMatrix);
    const glm::mat4 viewProj     = projJittered * viewMatrix;
    const glm::mat4 invViewProj  = glm::inverse(viewProj);

        // ---- SSAO pre-pass ------------------------------------------------
    if (post.Enabled && post.SSAOEnabled)
        SSAOPass(viewMatrix, projectionMatrix, post);
    else
        m_HasSSAO = false;

    if (!m_HasPrevViewProj)
    {
        m_PrevViewProj    = viewProj;
        m_HasPrevViewProj = true;
    }

    m_HDRFBO->Bind();
    glViewport(0, 0, (GLsizei)targetW, (GLsizei)targetH);
    glClear(GL_COLOR_BUFFER_BIT);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    m_GBuffer->BindTextures(0);

    m_LightingShader->Bind();
    m_LightingShader->SetInt("u_PositionTexture", 0);
    m_LightingShader->SetInt("u_NormalTexture",   1);
    m_LightingShader->SetInt("u_AlbedoTexture",   2);
    m_LightingShader->SetInt("u_MaterialTexture", 3);

    if (m_HasSSAO && m_SSAOFBO)
    {
        glActiveTexture(GL_TEXTURE0 + 9);
        glBindTexture(GL_TEXTURE_2D, m_SSAOFBO->GetColorAttachment(0));
        m_LightingShader->SetInt  ("u_SSAOTexture",   9);
        m_LightingShader->SetInt  ("u_HasSSAO",       1);
        m_LightingShader->SetFloat("u_SSAOIntensity", glm::clamp(post.SSAOIntensity, 0.0f, 3.0f));
    }
    else
    {
        m_LightingShader->SetInt("u_HasSSAO", 0);
    }

    if (m_HasOutline && m_OutlineFBO)
    {
        glActiveTexture(GL_TEXTURE0 + 5);
        glBindTexture(GL_TEXTURE_2D, m_OutlineFBO->GetColorAttachment(0));
        m_LightingShader->SetInt("u_OutlineTexture", 5);
        m_LightingShader->SetInt("u_HasOutline",     1);
    }
    else
    {
        m_LightingShader->SetInt("u_HasOutline", 0);
    }

    m_LightingShader->SetVec3("u_CameraPosition", cameraPosition);
    m_LightingShader->SetMat4("u_InvViewProj", invViewProj);

    if (skybox.CubemapPtr && skybox.CubemapPtr->GetID())
    {
        skybox.CubemapPtr->Bind(4);
        m_LightingShader->SetInt("u_EnvironmentMap", 4);
        m_LightingShader->SetInt("u_HasEnvironment", 1);
        m_LightingShader->SetFloat("u_SkyboxRotation", skybox.RotationRadians);
        m_LightingShader->SetFloat("u_SkyboxBrightness", skybox.Brightness);
    }
    else m_LightingShader->SetInt("u_HasEnvironment", 0);

    const int count = (int)std::min<size_t>(lights.size(), (size_t)kMaxLights);
    m_LightingShader->SetInt("u_LightCount", count);

    m_LightingShader->SetInt  ("u_FogEnabled", post.FogEnabled ? 1 : 0);
    m_LightingShader->SetInt  ("u_FogMode",    post.FogMode);
    m_LightingShader->SetVec3 ("u_FogColor",   post.FogColor);
    m_LightingShader->SetFloat("u_FogDensity", post.FogDensity);
    m_LightingShader->SetFloat("u_FogStart",   post.FogStart);
    m_LightingShader->SetFloat("u_FogEnd",     post.FogEnd);

    const auto& N = GetLightUniformNames();
    for (int i = 0; i < count; ++i)
    {
        const auto& L = lights[i];
        const auto& n = N[i];
        m_LightingShader->SetVec3 (n.Position, L.Position);
        m_LightingShader->SetFloat(n.Range, L.Range);
        m_LightingShader->SetVec3 (n.Direction, L.Direction);
        m_LightingShader->SetFloat(n.Intensity, L.Intensity);
        m_LightingShader->SetVec3 (n.Color, L.Color);
        m_LightingShader->SetInt  (n.LightType, L.LightType);
        m_LightingShader->SetFloat(n.InnerCutoff, L.InnerCutoff);
        m_LightingShader->SetFloat(n.OuterCutoff, L.OuterCutoff);
        m_LightingShader->SetInt  (n.ShadowMode,      L.ShadowMode);
        m_LightingShader->SetFloat(n.ShadowStrength,  L.ShadowStrength);
        m_LightingShader->SetFloat(n.BiasMultiplier,  L.BiasMultiplier);
        m_LightingShader->SetInt  (n.ShadowSlot,      L.ShadowSlot);
    }

    // ---- Directional CSM ------------------------------------------------
    {
        const bool shadowsActive = m_ShadowCaster.LightIndex >= 0 &&
                                   m_ShadowCaster.ShadowMode > 0;

        if (shadowsActive)
            m_ShadowMap.Bind(6);

        m_LightingShader->SetInt("u_ShadowMap",  6);
        m_LightingShader->SetInt("u_ShadowMode",
            shadowsActive ? m_ShadowCaster.ShadowMode : 0);
        m_LightingShader->SetInt("u_ShadowLightIndex",
            shadowsActive ? m_ShadowCaster.LightIndex : -1);
        m_LightingShader->SetInt("u_CascadeCount",
            shadowsActive ? m_ShadowCaster.CascadeCount : 0);
        m_LightingShader->SetMat4("u_ViewMatrix", viewMatrix);

        if (shadowsActive)
        {
            m_LightingShader->SetMat4Array("u_LightSpaceMatrices",
                m_ShadowCaster.LightSpaceMatrices, m_ShadowCaster.CascadeCount);
            m_LightingShader->SetFloatArray("u_CascadeSplits",
                m_ShadowCaster.CascadeSplits, m_ShadowCaster.CascadeCount);
            m_LightingShader->SetFloat("u_DepthBias",        m_ShadowCaster.DepthBias);
            m_LightingShader->SetFloat("u_NormalBias",       m_ShadowCaster.NormalBias);
            m_LightingShader->SetFloat("u_ShadowSoftness",   m_ShadowCaster.ShadowSoftness);
            m_LightingShader->SetFloat("u_MaxShadowDistance",m_ShadowCaster.MaxShadowDistance);
            m_LightingShader->SetFloat("u_FadeOutDistance",  m_ShadowCaster.FadeOutDistance);
            m_LightingShader->SetFloat("u_ShadowStrength",   m_ShadowCaster.ShadowStrength);
            m_LightingShader->SetFloat("u_BiasMultiplier",   m_ShadowCaster.BiasMultiplier);
        }
    }

    // ---- Spot shadows ---------------------------------------------------
    if (m_SpotShadowCount > 0)
    {
        m_SpotShadowMap.Bind(7);
        m_LightingShader->SetInt("u_SpotShadowMap", 7);
        m_LightingShader->SetMat4Array("u_SpotLightSpaceMatrix",
                                       m_SpotLightSpaceMatrix,
                                       (int)m_SpotShadowCount);
    }
    else
    {
        m_LightingShader->SetInt("u_SpotShadowMap", 7);
    }

    // ---- Point shadows --------------------------------------------------
    if (m_PointShadowCount > 0)
    {
        m_PointShadowMap.Bind(8);
        m_LightingShader->SetInt("u_PointShadowMap", 8);
        m_LightingShader->SetFloatArray("u_PointShadowNear",
                                        m_PointShadowNear,
                                        (int)m_PointShadowCount);
        m_LightingShader->SetFloatArray("u_PointShadowFar",
                                        m_PointShadowFar,
                                        (int)m_PointShadowCount);
    }
    else
    {
        m_LightingShader->SetInt("u_PointShadowMap", 8);
    }

    // ---- Reflection probes (MUST be set BEFORE DrawFullscreenQuad) ----
    {
        auto& lib = ReflectionProbeLibrary::Get();
        if (lib.GetCount() > 0)
        {
            lib.BindTextureArray(10);
            m_LightingShader->SetInt("u_ProbeCubeArray", 10);
            m_LightingShader->SetInt("u_ProbeCount", (int)lib.GetCount());

            int pIdx = 0;
            for (const auto& p : frameData.ReflectionProbes)
            {
                if (pIdx >= 4) break;
                std::string pn = "u_ProbePos["         + std::to_string(pIdx) + "]";
                std::string en = "u_ProbeExtents["     + std::to_string(pIdx) + "]";
                std::string bn = "u_ProbeBoxProjection[" + std::to_string(pIdx) + "]";
                m_LightingShader->SetVec3(pn, p.Position);
                m_LightingShader->SetVec3(en, p.Extents);
                m_LightingShader->SetInt (bn, p.BoxProjection ? 1 : 0);
                ++pIdx;
            }
        }
        else
        {
            m_LightingShader->SetInt("u_ProbeCount", 0);
        }
    }

    // Debug: 0=off, 1=probeIdx, 2=raw probe, 3=reflections contribution.
    m_LightingShader->SetInt("u_DebugReflections", 0);   // ← временно 1 для теста

    // ---- THE DRAW — все uniform'ы применены ---------------------------
    DrawFullscreenQuad();

    if (skybox.CubemapPtr && skybox.CubemapPtr->GetID())
        skybox.CubemapPtr->Unbind(4);

    m_LightingShader->Unbind();
    m_HDRFBO->Unbind();

    // ---- Transparent forward pass -------------------------------------
    ForwardTransparentPass(frameData, cameraPosition,
                           viewMatrix, projectionMatrix,
                           lights, skybox);

    // ---- Volumetric Fog / God Rays ------------------------------------
    if (post.Enabled && post.VolumetricFogEnabled)
        VolumetricFogPass(viewMatrix, projectionMatrix, cameraPosition,
                          lights, post, timeSeconds);

    const bool useTAA  = taa.Enabled && m_TAAEnabled &&
                         m_TAAShader != nullptr && m_TonemapShader != nullptr;
    const bool useFXAA = !useTAA && fxaa.Enabled && m_FXAAShader != nullptr;

    uint32_t source = m_HDRFBO->GetColorAttachment(0);

    const float projC = projectionMatrix[2][2];
    const float projD = projectionMatrix[3][2];
    const float nearPlane = (projC != 1.0f) ? projD / (projC - 1.0f) : 0.1f;
    const float farPlane  = (projC != -1.0f) ? projD / (projC + 1.0f) : 100.0f;

    if (useTAA)
    {
        ApplyTAA(targetW, targetH, cameraPosition, viewProj, invViewProj,
                 taa, source);
    }

    if (post.Enabled && post.MotionBlurEnabled &&
        m_MotionBlurShader && m_PostFBO)
    {
        source = ApplyMotionBlur(source, targetW, targetH, post,
                                 nearPlane, farPlane);
    }

    uint32_t bloomTexture = 0;
    if (post.Enabled && post.BloomEnabled &&
        m_BloomDownShader && m_BloomUpShader && m_BloomMips[0])
    {
        BuildBloom(source, targetW, targetH, post);
        bloomTexture = m_BloomMips[0]->GetColorAttachment(0);
    }

    if (useFXAA && m_LDRFBO)
    {
        ApplyTonemap(source, m_LDRFBO->GetFramebufferID(), targetW, targetH,
                     post, bloomTexture, timeSeconds);
        ApplyFXAA(m_LDRFBO->GetColorAttachment(0), targetFBO,
                  targetW, targetH, fxaa);
    }
    else
    {
        ApplyTonemap(source, targetFBO, targetW, targetH,
                     post, bloomTexture, timeSeconds);
    }

    m_FrameStats.TaaActive     = useTAA;
    m_FrameStats.TaaFrameIndex = m_TaaFrameIndex;
    m_FrameStats.PostFXActive  = post.Enabled;

    m_PrevViewProj = viewProj;
    m_PrevJitter   = m_Jitter;

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
}

void DeferredRenderer::ApplyTAA(uint32_t width, uint32_t height,
                                const glm::vec3& cameraPosition,
                                const glm::mat4& viewProj,
                                const glm::mat4& invViewProj,
                                const TAAParams& params,
                                uint32_t& outSourceTexture)
{
    const int readIndex  = m_TAAHistoryIndex;
    const int writeIndex = 1 - readIndex;

    m_TAAHistory[writeIndex]->Bind();
    glViewport(0, 0, (GLsizei)width, (GLsizei)height);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_HDRFBO->GetColorAttachment(0));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_GBuffer->GetTexture(GBuffer::VELOCITY));
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_GBuffer->GetDepthAttachment());
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, m_TAAHistory[readIndex]->GetColorAttachment(0));

    m_TAAShader->Bind();
    m_TAAShader->SetInt  ("u_CurrentTexture",  0);
    m_TAAShader->SetInt  ("u_VelocityTexture", 1);
    m_TAAShader->SetInt  ("u_DepthTexture",    2);
    m_TAAShader->SetInt  ("u_HistoryTexture",  3);
    m_TAAShader->SetVec2 ("u_TexelSize",
                          glm::vec2(1.0f / (float)width, 1.0f / (float)height));
    m_TAAShader->SetVec2 ("u_Resolution",
                          glm::vec2((float)width, (float)height));
    m_TAAShader->SetFloat("u_HistoryWeight",
                          glm::clamp(params.HistoryWeight, 0.0f, 0.98f));
    m_TAAShader->SetInt  ("u_HistoryValid", m_TAAHistoryValid ? 1 : 0);
        // FIX(TAA): 1.25 слишком агрессивно — history клампится к дисперсии
    // 3×3 neighborhood, и на статике «новый» jittered-кадр успевает
    // пролезть вместо накопленной истории → видимый sub-pixel jitter на
    // резких краях. 1.5 — стандарт (UE/Unity), история живёт дольше.
    m_TAAShader->SetFloat("u_ClampGamma", 1.5f);
    m_TAAShader->SetMat4 ("u_ViewProj",     viewProj);
    m_TAAShader->SetMat4 ("u_PrevViewProj", m_PrevViewProj);
    m_TAAShader->SetMat4 ("u_InvViewProj",  invViewProj);
    m_TAAShader->SetVec3 ("u_CameraPosition", cameraPosition);

    DrawFullscreenQuad();
    m_TAAShader->Unbind();

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);

    m_TAAHistoryIndex = writeIndex;
    m_TAAHistoryValid = true;

    outSourceTexture = m_TAAHistory[writeIndex]->GetColorAttachment(0);
}

uint32_t DeferredRenderer::ApplyMotionBlur(uint32_t sourceTexture,
                                           uint32_t width, uint32_t height,
                                           const PostProcessSettings& params,
                                           float nearPlane, float farPlane)
{
    m_PostFBO->Bind();
    glViewport(0, 0, (GLsizei)width, (GLsizei)height);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_GBuffer->GetTexture(GBuffer::VELOCITY));
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_GBuffer->GetDepthAttachment());

    m_MotionBlurShader->Bind();
    m_MotionBlurShader->SetInt  ("u_HDRTexture",      0);
    m_MotionBlurShader->SetInt  ("u_VelocityTexture", 1);
    m_MotionBlurShader->SetInt  ("u_DepthTexture",    2);
    m_MotionBlurShader->SetVec2 ("u_JitterDelta",
                                 (m_PrevJitter - m_Jitter) * 0.5f);
    m_MotionBlurShader->SetFloat("u_Strength",
                                 glm::clamp(params.MotionBlurStrength, 0.0f, 2.0f));
    m_MotionBlurShader->SetInt  ("u_Samples",
                                 std::clamp(params.MotionBlurSamples, 2, 16));
    m_MotionBlurShader->SetFloat("u_NearPlane", nearPlane);
    m_MotionBlurShader->SetFloat("u_FarPlane",  farPlane);

    DrawFullscreenQuad();
    m_MotionBlurShader->Unbind();

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);

    return m_PostFBO->GetColorAttachment(0);
}

void DeferredRenderer::BuildBloom(uint32_t sourceTexture,
                                  uint32_t width, uint32_t height,
                                  const PostProcessSettings& params)
{
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    const uint32_t mip0W = std::max(1u, width  >> 1);
    const uint32_t mip0H = std::max(1u, height >> 1);

    m_BloomMips[0]->Bind();
    glViewport(0, 0, (GLsizei)mip0W, (GLsizei)mip0H);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);

    m_BloomDownShader->Bind();
    m_BloomDownShader->SetInt  ("u_Source",    0);
    m_BloomDownShader->SetVec2 ("u_TexelSize",
                                glm::vec2(1.0f / (float)width, 1.0f / (float)height));
    m_BloomDownShader->SetInt  ("u_FirstPass", 1);
    m_BloomDownShader->SetFloat("u_Threshold", glm::max(params.BloomThreshold, 0.0f));
    DrawFullscreenQuad();
    m_BloomDownShader->Unbind();

    for (uint32_t i = 1; i < kBloomMipCount; ++i)
    {
        const uint32_t srcW = std::max(1u, width  >> i);
        const uint32_t srcH = std::max(1u, height >> i);

        m_BloomMips[i]->Bind();
        glViewport(0, 0, (GLsizei)std::max(1u, width >> (i + 1)),
                         (GLsizei)std::max(1u, height >> (i + 1)));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_BloomMips[i - 1]->GetColorAttachment(0));

        m_BloomDownShader->Bind();
        m_BloomDownShader->SetInt  ("u_Source",    0);
        m_BloomDownShader->SetVec2 ("u_TexelSize",
                                    glm::vec2(1.0f / (float)srcW, 1.0f / (float)srcH));
        m_BloomDownShader->SetInt  ("u_FirstPass", 0);
        m_BloomDownShader->SetFloat("u_Threshold", 0.0f);
        DrawFullscreenQuad();
        m_BloomDownShader->Unbind();
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    for (uint32_t i = kBloomMipCount - 1; i > 0; --i)
    {
        const uint32_t dstW = std::max(1u, width  >> i);
        const uint32_t dstH = std::max(1u, height >> i);
        const uint32_t srcW = std::max(1u, width  >> (i + 1));
        const uint32_t srcH = std::max(1u, height >> (i + 1));

        m_BloomMips[i - 1]->Bind();
        glViewport(0, 0, (GLsizei)dstW, (GLsizei)dstH);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_BloomMips[i]->GetColorAttachment(0));

        m_BloomUpShader->Bind();
        m_BloomUpShader->SetInt ("u_Source", 0);
        m_BloomUpShader->SetVec2("u_TexelSize",
                                 glm::vec2(1.0f / (float)srcW, 1.0f / (float)srcH));
        DrawFullscreenQuad();
        m_BloomUpShader->Unbind();
    }

    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void DeferredRenderer::ApplyTonemap(uint32_t sourceTexture, uint32_t targetFBO,
                                    uint32_t width, uint32_t height,
                                    const PostProcessSettings& params,
                                    uint32_t bloomTexture,
                                    float timeSeconds)
{
    glBindFramebuffer(GL_FRAMEBUFFER, targetFBO);
    glViewport(0, 0, (GLsizei)width, (GLsizei)height);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloomTexture);

    m_TonemapShader->Bind();
    m_TonemapShader->SetInt  ("u_HDRTexture",   0);
    m_TonemapShader->SetInt  ("u_BloomTexture", 1);
    m_TonemapShader->SetFloat("u_Exposure", glm::max(params.Exposure, 0.01f));

    m_TonemapShader->SetInt  ("u_TonemapMode", params.TonemapMode);
    m_TonemapShader->SetFloat("u_Contrast",    params.Contrast);
    m_TonemapShader->SetFloat("u_Saturation",  params.Saturation);
    m_TonemapShader->SetFloat("u_Temperature", params.Temperature);
    m_TonemapShader->SetFloat("u_Tint",        params.Tint);
    m_TonemapShader->SetVec3 ("u_Lift",  params.Lift);
    m_TonemapShader->SetVec3 ("u_Gamma", params.Gamma);
    m_TonemapShader->SetVec3 ("u_Gain",  params.Gain);
    m_TonemapShader->SetVec2 ("u_Resolution",
                              glm::vec2((float)width, (float)height));
    m_TonemapShader->SetFloat("u_Time", timeSeconds);

    m_TonemapShader->SetInt  ("u_BloomEnabled",
                              (params.Enabled && params.BloomEnabled && bloomTexture != 0) ? 1 : 0);
    m_TonemapShader->SetFloat("u_BloomIntensity",
                              glm::max(params.BloomIntensity, 0.0f));

    m_TonemapShader->SetInt  ("u_LensEnabled",
                              (params.Enabled && params.LensDistortionEnabled) ? 1 : 0);
    m_TonemapShader->SetFloat("u_LensDistortion", params.LensDistortion);

    m_TonemapShader->SetInt  ("u_CAEnabled",
                              (params.Enabled && params.ChromaticAberrationEnabled) ? 1 : 0);
    m_TonemapShader->SetFloat("u_CAStrength",
                              glm::clamp(params.ChromaticAberration, 0.0f, 1.0f));

    m_TonemapShader->SetInt  ("u_VignetteEnabled",
                              (params.Enabled && params.VignetteEnabled) ? 1 : 0);
    m_TonemapShader->SetFloat("u_VignetteIntensity",
                              glm::clamp(params.VignetteIntensity, 0.0f, 1.0f));
    m_TonemapShader->SetFloat("u_VignetteSmoothness",
                              glm::clamp(params.VignetteSmoothness, 0.0f, 1.0f));

    m_TonemapShader->SetInt  ("u_GrainEnabled",
                              (params.Enabled && params.FilmGrainEnabled) ? 1 : 0);
    m_TonemapShader->SetFloat("u_GrainIntensity",
                              glm::max(params.FilmGrainIntensity, 0.0f));

    DrawFullscreenQuad();
    m_TonemapShader->Unbind();

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void DeferredRenderer::BakeReflectionProbe(
    uint32_t probeIdx,
    const glm::vec3& probePos,
    const glm::vec3& probeExtents,
    const FrameRenderData& frame,
    const std::vector<GPULight>& lights,
    const SkyboxParams& skybox,
    bool reflectSkybox)
{
    auto& lib = ReflectionProbeLibrary::Get();
    if (probeIdx == 0 || probeIdx > lib.GetCount())
    {
        BINAX_LOG_WARN("BakeReflectionProbe: invalid slot {} (max {})",
                       probeIdx, lib.GetCount());
        return;
    }
    if (!m_ForwardShader || !m_ProbeSkyboxShader)
    {
        BINAX_LOG_ERROR("BakeReflectionProbe: shader missing");
        return;
    }

    BINAX_LOG_INFO("BakeReflectionProbe: slot {} at ({:.2f}, {:.2f}, {:.2f}) "
                   "extents ({:.2f}, {:.2f}, {:.2f}), {} instances, skybox={}",
                   probeIdx,
                   probePos.x, probePos.y, probePos.z,
                   probeExtents.x, probeExtents.y, probeExtents.z,
                   frame.Instances.size(), reflectSkybox);

    // FIX #57: instances уже загружены main.cpp.
    MaterialLibrary::Get().Flush();

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, InstanceBuffer::Get().GetSSBO());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, MaterialLibrary::Get().GetMaterialSSBO());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_BonesDummySSBO);

    // Save GL state
    GLfloat prevClear[4]; glGetFloatv(GL_COLOR_CLEAR_VALUE, prevClear);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);

    // ВАЖНО: culling ON — иначе проба внутри геометрии видит
    // backfaces (сплошной цвет) вместо того, что снаружи
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    static const glm::vec3 kDirs[6] = {
        { 1, 0, 0}, {-1, 0, 0},
        { 0, 1, 0}, { 0,-1, 0},
        { 0, 0, 1}, { 0, 0,-1}
    };
    static const glm::vec3 kUps[6] = {
        {0,-1, 0}, {0,-1, 0},
        {0, 0, 1}, {0, 0,-1},
        {0,-1, 0}, {0,-1, 0}
    };

    const float farPlane = glm::length(probeExtents) * 4.0f + 100.0f;
    const glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, farPlane);

    const int lightCount = (int)std::min<size_t>(lights.size(), (size_t)kMaxLights);
    const auto& N = GetLightUniformNames();

    for (int f = 0; f < 6; ++f)
    {
        if (!lib.BindCubeFace(probeIdx, (uint32_t)f))
        {
            BINAX_LOG_WARN("BakeReflectionProbe: bind face {} failed", f);
            break;
        }

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const glm::mat4 view = glm::lookAt(probePos, probePos + kDirs[f], kUps[f]);

        // ---- Skybox background -----------------------------------------
        if (reflectSkybox && skybox.CubemapPtr && skybox.CubemapPtr->GetID())
        {
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);

            skybox.CubemapPtr->Bind(0);
            m_ProbeSkyboxShader->Bind();
            m_ProbeSkyboxShader->SetInt  ("u_EnvironmentMap", 0);
            m_ProbeSkyboxShader->SetInt  ("u_HasEnvironment", 1);
            m_ProbeSkyboxShader->SetMat4 ("u_InvViewProj", glm::inverse(proj * view));
            m_ProbeSkyboxShader->SetVec3 ("u_ProbePos", probePos);
            m_ProbeSkyboxShader->SetFloat("u_SkyboxRotation", skybox.RotationRadians);
            m_ProbeSkyboxShader->SetFloat("u_SkyboxBrightness", skybox.Brightness);
            DrawFullscreenQuad();
            m_ProbeSkyboxShader->Unbind();
            skybox.CubemapPtr->Unbind(0);

            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
        }

        // ---- Geometry (forward PBR) ------------------------------------
        m_ForwardShader->Bind();
        m_ForwardShader->SetMat4("u_View",          view);
        m_ForwardShader->SetMat4("u_Projection",    proj);
        m_ForwardShader->SetVec3("u_CameraPosition", probePos);

        if (skybox.CubemapPtr && skybox.CubemapPtr->GetID())
        {
            skybox.CubemapPtr->Bind(4);
            m_ForwardShader->SetInt  ("u_EnvironmentMap",    4);
            m_ForwardShader->SetInt  ("u_HasEnvironment",    1);
            m_ForwardShader->SetFloat("u_SkyboxRotation",    skybox.RotationRadians);
            m_ForwardShader->SetFloat("u_SkyboxBrightness",  skybox.Brightness);
        }
        else m_ForwardShader->SetInt("u_HasEnvironment", 0);

        m_ForwardShader->SetInt("u_LightCount", lightCount);
        for (int i = 0; i < lightCount; ++i)
        {
            const auto& L = lights[i];
            const auto& n = N[i];
            m_ForwardShader->SetVec3 (n.Position, L.Position);
            m_ForwardShader->SetFloat(n.Range, L.Range);
            m_ForwardShader->SetVec3 (n.Direction, L.Direction);
            m_ForwardShader->SetFloat(n.Intensity, L.Intensity);
            m_ForwardShader->SetVec3 (n.Color, L.Color);
            m_ForwardShader->SetInt  (n.LightType, L.LightType);
            m_ForwardShader->SetFloat(n.InnerCutoff, L.InnerCutoff);
            m_ForwardShader->SetFloat(n.OuterCutoff, L.OuterCutoff);
            m_ForwardShader->SetInt  (n.ShadowMode,      L.ShadowMode);
            m_ForwardShader->SetFloat(n.ShadowStrength,  L.ShadowStrength);
            m_ForwardShader->SetFloat(n.BiasMultiplier,  L.BiasMultiplier);
            m_ForwardShader->SetInt  (n.ShadowSlot,      L.ShadowSlot);
        }

        MeshLibrary::Get().BindVAO();
        for (size_t i = 0; i < frame.Instances.size(); ++i)
        {
            const InstanceGPU& inst = frame.Instances[i];
            if (!MeshLibrary::Get().IsValid(inst.MeshId)) continue;
            const MeshGPU& mesh = MeshLibrary::Get().Get(inst.MeshId);
            if (mesh.IndexCount == 0) continue;

            m_ForwardShader->SetUInt("u_InstanceIndex", (uint32_t)i);
            const void* indexOffset =
                (const void*)(uintptr_t)(mesh.BaseIndex * sizeof(uint32_t));
            glDrawElementsBaseVertex(GL_TRIANGLES,
                                     (GLsizei)mesh.IndexCount,
                                     GL_UNSIGNED_INT,
                                     indexOffset,
                                     (GLint)mesh.BaseVertex);
        }
        glBindVertexArray(0);

        if (skybox.CubemapPtr && skybox.CubemapPtr->GetID())
            skybox.CubemapPtr->Unbind(4);

        m_ForwardShader->Unbind();
    }

    lib.Unbind();
    lib.GenerateMipmaps();
    BINAX_LOG_INFO("BakeReflectionProbe: slot {} baked OK", probeIdx);

    glClearColor(prevClear[0], prevClear[1], prevClear[2], prevClear[3]);
}

void DeferredRenderer::EnsureVolumetricFogTargets(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) return;
    const uint32_t halfW = std::max(1u, width / 2u);
    const uint32_t halfH = std::max(1u, height / 2u);

    for (int i = 0; i < 2; ++i)
    {
        if (!m_VolumetricFogFBO[i])
        {
            m_VolumetricFogFBO[i] = std::make_unique<Framebuffer>(halfW, halfH);
            m_VolumetricFogFBO[i]->Bind();
            m_VolumetricFogFBO[i]->AttachColorTexture(GL_RGBA16F, GL_RGBA, GL_FLOAT, 0);
            m_VolumetricFogFBO[i]->CheckStatus();
            m_VolumetricFogFBO[i]->Unbind();

            glBindTexture(GL_TEXTURE_2D, m_VolumetricFogFBO[i]->GetColorAttachment(0));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        else m_VolumetricFogFBO[i]->Resize(halfW, halfH);
    }
}

void DeferredRenderer::VolumetricFogPass(
    const glm::mat4& viewMatrix,
    const glm::mat4& projectionMatrix,
    const glm::vec3& cameraPosition,
    const std::vector<GPULight>& lights,
    const PostProcessSettings& post,
    float timeSeconds)
{
    if (!post.VolumetricFogEnabled) return;
    if (!m_VolumetricFogShader || !m_VolumetricFogBlurShader ||
        !m_VolumetricFogCompositeShader) return;

    // ---- Find sun (first directional with shadows, fallback to first directional)
    int sunIdx = -1;
    for (size_t i = 0; i < lights.size(); ++i)
    {
        if (lights[i].LightType == GPULight::Directional &&
            lights[i].ShadowMode != 0)
        {
            sunIdx = (int)i;
            break;
        }
    }
    if (sunIdx < 0)
    {
        for (size_t i = 0; i < lights.size(); ++i)
        {
            if (lights[i].LightType == GPULight::Directional)
            {
                sunIdx = (int)i;
                break;
            }
        }
    }
    if (sunIdx < 0) return;

    const GPULight& sun = lights[sunIdx];

    const uint32_t fullW = m_GBuffer->GetWidth();
    const uint32_t fullH = m_GBuffer->GetHeight();
    if (fullW == 0 || fullH == 0) return;

    EnsureVolumetricFogTargets(fullW, fullH);

    const uint32_t halfW = std::max(1u, fullW / 2u);
    const uint32_t halfH = std::max(1u, fullH / 2u);

    // Use non-jittered projection — fog не должен джиттерить
    const glm::mat4 invViewProj = glm::inverse(projectionMatrix * viewMatrix);

    // ---- Pass 1: raymarch ---------------------------------------------
    m_VolumetricFogFBO[0]->Bind();
    glViewport(0, 0, (GLsizei)halfW, (GLsizei)halfH);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    GLfloat prevClear[4]; glGetFloatv(GL_COLOR_CLEAR_VALUE, prevClear);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Bind G-Buffer depth
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_GBuffer->GetDepthAttachment());

    // Bind CSM
    if (m_ShadowCaster.LightIndex >= 0 && m_ShadowCaster.ShadowMode > 0)
        m_ShadowMap.Bind(1);

    m_VolumetricFogShader->Bind();
    m_VolumetricFogShader->SetInt  ("u_DepthTexture", 0);
    m_VolumetricFogShader->SetInt  ("u_ShadowMap",    1);
    m_VolumetricFogShader->SetMat4 ("u_InvViewProj",  invViewProj);
    m_VolumetricFogShader->SetMat4 ("u_ViewMatrix",   viewMatrix);
    m_VolumetricFogShader->SetVec3 ("u_CameraPosition", cameraPosition);

    // CSM
    const bool shadowsActive = m_ShadowCaster.LightIndex >= 0 &&
                               m_ShadowCaster.ShadowMode > 0;
    m_VolumetricFogShader->SetInt  ("u_ShadowMode",
        shadowsActive ? m_ShadowCaster.ShadowMode : 0);
    m_VolumetricFogShader->SetInt  ("u_CascadeCount",
        shadowsActive ? m_ShadowCaster.CascadeCount : 0);
    if (shadowsActive)
    {
        m_VolumetricFogShader->SetMat4Array("u_LightSpaceMatrices",
            m_ShadowCaster.LightSpaceMatrices, m_ShadowCaster.CascadeCount);
        m_VolumetricFogShader->SetFloatArray("u_CascadeSplits",
            m_ShadowCaster.CascadeSplits, m_ShadowCaster.CascadeCount);
        m_VolumetricFogShader->SetFloat("u_MaxShadowDistance",
            m_ShadowCaster.MaxShadowDistance);
    }

    // Sun
    m_VolumetricFogShader->SetVec3 ("u_SunDirection",  glm::normalize(sun.Direction));
    m_VolumetricFogShader->SetVec3 ("u_SunColor",      sun.Color);
    m_VolumetricFogShader->SetFloat("u_SunIntensity",  sun.Intensity);

    // Fog params
    m_VolumetricFogShader->SetVec3 ("u_FogColor",     post.VolumetricFogColor);
    m_VolumetricFogShader->SetFloat("u_FogIntensity", post.VolumetricFogIntensity);
    m_VolumetricFogShader->SetFloat("u_FogDensity",   post.VolumetricFogDensity);
    m_VolumetricFogShader->SetFloat("u_FogMinHeight", post.VolumetricFogMinHeight);
    m_VolumetricFogShader->SetFloat("u_FogMaxHeight", post.VolumetricFogMaxHeight);
    m_VolumetricFogShader->SetFloat("u_FogDistance",  post.VolumetricFogDistance);
    m_VolumetricFogShader->SetInt  ("u_FogSteps",     std::clamp(post.VolumetricFogSteps, 8, 128));
    m_VolumetricFogShader->SetFloat("u_Scattering",   glm::clamp(post.VolumetricFogScattering, 0.0f, 0.95f));
    m_VolumetricFogShader->SetInt  ("u_NoiseEnabled",   post.VolumetricFogNoiseEnabled ? 1 : 0);
    m_VolumetricFogShader->SetFloat("u_NoiseSpeed",     post.VolumetricFogNoiseSpeed);
    m_VolumetricFogShader->SetFloat("u_NoiseScale",     glm::max(post.VolumetricFogNoiseScale, 0.001f));
    m_VolumetricFogShader->SetFloat("u_NoiseIntensity", glm::clamp(post.VolumetricFogNoiseIntensity, 0.0f, 1.0f));
    m_VolumetricFogShader->SetFloat("u_Time",           timeSeconds);

    DrawFullscreenQuad();
    m_VolumetricFogShader->Unbind();

    // ---- Pass 2: separable blur, N iterations -------------------------
    const glm::vec2 halfTexel(1.0f / (float)halfW, 1.0f / (float)halfH);
    const int iterations = std::clamp(post.VolumetricFogBlurIterations, 0, 8);

    m_VolumetricFogBlurShader->Bind();
    m_VolumetricFogBlurShader->SetVec2("u_TexelSize", halfTexel);

    for (int iter = 0; iter < iterations; ++iter)
    {
        // H: FBO[0] → FBO[1]
        m_VolumetricFogFBO[1]->Bind();
        glViewport(0, 0, (GLsizei)halfW, (GLsizei)halfH);
        glClear(GL_COLOR_BUFFER_BIT);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_VolumetricFogFBO[0]->GetColorAttachment(0));
        m_VolumetricFogBlurShader->SetInt ("u_Texture",  0);
        m_VolumetricFogBlurShader->SetVec2("u_Direction", glm::vec2(1.0f, 0.0f));
        DrawFullscreenQuad();

        // V: FBO[1] → FBO[0]
        m_VolumetricFogFBO[0]->Bind();
        glClear(GL_COLOR_BUFFER_BIT);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_VolumetricFogFBO[1]->GetColorAttachment(0));
        m_VolumetricFogBlurShader->SetInt ("u_Texture",  0);
        m_VolumetricFogBlurShader->SetVec2("u_Direction", glm::vec2(0.0f, 1.0f));
        DrawFullscreenQuad();
    }

    m_VolumetricFogBlurShader->Unbind();

    // ---- Pass 3: composite (additive) into HDR ------------------------
    m_HDRFBO->Bind();
    glViewport(0, 0, (GLsizei)fullW, (GLsizei)fullH);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);   // не трогаем alpha
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_VolumetricFogFBO[0]->GetColorAttachment(0));

    m_VolumetricFogCompositeShader->Bind();
    m_VolumetricFogCompositeShader->SetInt("u_FogTexture", 0);
    DrawFullscreenQuad();
    m_VolumetricFogCompositeShader->Unbind();

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_BLEND);
    m_HDRFBO->Unbind();

    glClearColor(prevClear[0], prevClear[1], prevClear[2], prevClear[3]);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
}

void DeferredRenderer::ApplyFXAA(uint32_t sourceTexture,
                                 uint32_t targetFBO,
                                 uint32_t width, uint32_t height,
                                 const FXAAParams& params)
{
    glBindFramebuffer(GL_FRAMEBUFFER, targetFBO);
    glViewport(0, 0, (GLsizei)width, (GLsizei)height);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
    
    
    

    m_FXAAShader->Bind();
    m_FXAAShader->SetInt  ("u_ColorTexture",     0);
    m_FXAAShader->SetVec2 ("u_TexelSize",
                           glm::vec2(1.0f / (float)width, 1.0f / (float)height));
    m_FXAAShader->SetFloat("u_EdgeThreshold",    params.EdgeThreshold);
    m_FXAAShader->SetFloat("u_EdgeThresholdMin", params.EdgeThresholdMin);
    m_FXAAShader->SetFloat("u_SubpixelBlending", params.SubpixelBlending);

    DrawFullscreenQuad();
    m_FXAAShader->Unbind();
    glBindTexture(GL_TEXTURE_2D, 0);
}

void DeferredRenderer::GenerateSSAOKernel()
{
    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> randN(-1.0f, 1.0f);
    std::uniform_real_distribution<float> rand01(0.0f, 1.0f);

    for (int i = 0; i < 32; ++i)
    {
        glm::vec3 s(randN(rng), randN(rng), rand01(rng));
        s = glm::normalize(s);
        s *= rand01(rng);

        // Больше сэмплов ближе к центру (лучшая детализация)
        float scale = static_cast<float>(i) / 32.0f;
        scale = glm::mix(0.1f, 1.0f, scale * scale);
        s *= scale;

        m_SSAOKernel[i] = s;
    }
}

void DeferredRenderer::EnsureSSAOTargets(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    const uint32_t halfW = std::max(1u, width  / 2u);
    const uint32_t halfH = std::max(1u, height / 2u);

    // m_SSAOFBO: raw AO → после V-blur содержит финальный результат
    if (!m_SSAOFBO)
    {
        m_SSAOFBO = std::make_unique<Framebuffer>(halfW, halfH);
        m_SSAOFBO->Bind();
        m_SSAOFBO->AttachColorTexture(GL_R8, GL_RED, GL_UNSIGNED_BYTE, 0);
        m_SSAOFBO->CheckStatus();
        m_SSAOFBO->Unbind();

        glBindTexture(GL_TEXTURE_2D, m_SSAOFBO->GetColorAttachment(0));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    else m_SSAOFBO->Resize(halfW, halfH);

    // m_SSAOBlurFBO: промежуточный H-проход (потом читается в V-проходе)
    if (!m_SSAOBlurFBO)
    {
        m_SSAOBlurFBO = std::make_unique<Framebuffer>(halfW, halfH);
        m_SSAOBlurFBO->Bind();
        m_SSAOBlurFBO->AttachColorTexture(GL_R8, GL_RED, GL_UNSIGNED_BYTE, 0);
        m_SSAOBlurFBO->CheckStatus();
        m_SSAOBlurFBO->Unbind();

        glBindTexture(GL_TEXTURE_2D, m_SSAOBlurFBO->GetColorAttachment(0));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    else m_SSAOBlurFBO->Resize(halfW, halfH);
}

void DeferredRenderer::SSAOPass(const glm::mat4& viewMatrix,
                                const glm::mat4& projectionMatrix,
                                const PostProcessSettings& post)
{
    m_HasSSAO = false;
    if (!m_SSAOShader || !m_SSAOBlurShader) return;

    const uint32_t fullW = m_GBuffer->GetWidth();
    const uint32_t fullH = m_GBuffer->GetHeight();
    if (fullW == 0 || fullH == 0) return;

    EnsureSSAOTargets(fullW, fullH);

    const uint32_t halfW = std::max(1u, fullW / 2u);
    const uint32_t halfH = std::max(1u, fullH / 2u);

    const float projC = projectionMatrix[2][2];
    const float projD = projectionMatrix[3][2];
    const float nearPlane = (projC != 1.0f) ? projD / (projC - 1.0f) : 0.1f;
    const float farPlane  = (projC != -1.0f) ? projD / (projC + 1.0f) : 100.0f;

    GLfloat prev[4]; glGetFloatv(GL_COLOR_CLEAR_VALUE, prev);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    // ---- 1. Raw AO at half-res --------------------------------------
    m_SSAOFBO->Bind();
    glViewport(0, 0, (GLsizei)halfW, (GLsizei)halfH);
    glClearColor(1.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_GBuffer->GetDepthAttachment());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_GBuffer->GetTexture(GBuffer::NORMAL));

    m_SSAOShader->Bind();
    m_SSAOShader->SetInt  ("u_DepthTexture",  0);
    m_SSAOShader->SetInt  ("u_NormalTexture", 1);
    m_SSAOShader->SetMat4 ("u_View",          viewMatrix);
    m_SSAOShader->SetMat4 ("u_Projection",    projectionMatrix);
    m_SSAOShader->SetMat4 ("u_InvProjection", glm::inverse(projectionMatrix));
    m_SSAOShader->SetFloat("u_Radius",        glm::max(post.SSAORadius, 0.01f));
    m_SSAOShader->SetFloat("u_Bias",          glm::max(post.SSAOBias,   0.0f));
    m_SSAOShader->SetFloat("u_Power",         glm::max(post.SSAOPower,  0.1f));
    m_SSAOShader->SetInt  ("u_Samples",       std::clamp(post.SSAOSamples, 4, 32));

    for (int i = 0; i < 32; ++i)
    {
        const std::string n = "u_Kernel[" + std::to_string(i) + "]";
        m_SSAOShader->SetVec3(n, m_SSAOKernel[i]);
    }

    DrawFullscreenQuad();
    m_SSAOShader->Unbind();

    // ---- 2. Bilateral blur — separable, 2 passes --------------------
    const glm::vec2 halfTexel(1.0f / (float)halfW, 1.0f / (float)halfH);

    // Pass H: m_SSAOFBO → m_SSAOBlurFBO
    m_SSAOBlurFBO->Bind();
    glViewport(0, 0, (GLsizei)halfW, (GLsizei)halfH);
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_SSAOFBO->GetColorAttachment(0));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_GBuffer->GetDepthAttachment());

    m_SSAOBlurShader->Bind();
    m_SSAOBlurShader->SetInt  ("u_AOTexture",    0);
    m_SSAOBlurShader->SetInt  ("u_DepthTexture", 1);
    m_SSAOBlurShader->SetVec2 ("u_TexelSize",    halfTexel);
    m_SSAOBlurShader->SetVec2 ("u_BlurAxis",     glm::vec2(1.0f, 0.0f));
    m_SSAOBlurShader->SetFloat("u_NearPlane",    nearPlane);
    m_SSAOBlurShader->SetFloat("u_FarPlane",     farPlane);

    DrawFullscreenQuad();
    m_SSAOBlurShader->Unbind();

    // Pass V: m_SSAOBlurFBO → m_SSAOFBO (final result back in m_SSAOFBO)
    m_SSAOFBO->Bind();
    glViewport(0, 0, (GLsizei)halfW, (GLsizei)halfH);
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_SSAOBlurFBO->GetColorAttachment(0));
    // depth уже на unit 1 — не перепривязываем

    m_SSAOBlurShader->Bind();
    m_SSAOBlurShader->SetInt  ("u_AOTexture",    0);
    m_SSAOBlurShader->SetInt  ("u_DepthTexture", 1);
    m_SSAOBlurShader->SetVec2 ("u_TexelSize",    halfTexel);
    m_SSAOBlurShader->SetVec2 ("u_BlurAxis",     glm::vec2(0.0f, 1.0f));
    m_SSAOBlurShader->SetFloat("u_NearPlane",    nearPlane);
    m_SSAOBlurShader->SetFloat("u_FarPlane",     farPlane);

    DrawFullscreenQuad();
    m_SSAOBlurShader->Unbind();

    glBindTexture(GL_TEXTURE_2D, 0);
    m_SSAOFBO->Unbind();

    glClearColor(prev[0], prev[1], prev[2], prev[3]);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    m_HasSSAO = true;
}

void DeferredRenderer::ForwardTransparentPass(
    const FrameRenderData& frame,
    const glm::vec3& cameraPosition,
    const glm::mat4& viewMatrix,
    const glm::mat4& projectionMatrix,
    const std::vector<GPULight>& lights,
    const SkyboxParams& skybox)
{
    if (frame.TransparentInstances.empty())
        return;

    // ---- Sort back-to-front ------------------------------------------
    std::vector<size_t> order(frame.TransparentInstances.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;

    const glm::vec3 camPos = cameraPosition;
    std::sort(order.begin(), order.end(),
        [&](size_t a, size_t b)
        {
            const glm::vec3 pa(frame.TransparentInstances[a].GPU.Model[3]);
            const glm::vec3 pb(frame.TransparentInstances[b].GPU.Model[3]);
            const float da = glm::dot(pa - camPos, pa - camPos);
            const float db = glm::dot(pb - camPos, pb - camPos);
            return da > db;   // дальние первыми
        });

    // ---- Upload instances в тот же SSBO ------------------------------
    std::vector<InstanceGPU> sortedGPU;
    sortedGPU.reserve(order.size());
    for (size_t idx : order)
        sortedGPU.push_back(frame.TransparentInstances[idx].GPU);

    InstanceBuffer::Get().Upload(sortedGPU);

    // ---- Render into HDR FBO (opaque уже там) ------------------------
    m_HDRFBO->Bind();
    glViewport(0, 0, (GLsizei)m_GBuffer->GetWidth(), (GLsizei)m_GBuffer->GetHeight());

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_FALSE);                        // не пишем depth
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, InstanceBuffer::Get().GetSSBO());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, MaterialLibrary::Get().GetMaterialSSBO());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_BonesDummySSBO);

    m_ForwardShader->Bind();
    m_ForwardShader->SetMat4("u_View",       viewMatrix);
    m_ForwardShader->SetMat4("u_Projection", projectionMatrix);
    m_ForwardShader->SetVec3("u_CameraPosition", cameraPosition);

    if (skybox.CubemapPtr && skybox.CubemapPtr->GetID())
    {
        skybox.CubemapPtr->Bind(4);
        m_ForwardShader->SetInt("u_EnvironmentMap", 4);
        m_ForwardShader->SetInt("u_HasEnvironment", 1);
        m_ForwardShader->SetFloat("u_SkyboxRotation", skybox.RotationRadians);
        m_ForwardShader->SetFloat("u_SkyboxBrightness", skybox.Brightness);
    }
    else m_ForwardShader->SetInt("u_HasEnvironment", 0);

    const int lightCount = (int)std::min<size_t>(lights.size(), (size_t)kMaxLights);
    m_ForwardShader->SetInt("u_LightCount", lightCount);
    const auto& N = GetLightUniformNames();
    for (int i = 0; i < lightCount; ++i)
    {
        const auto& L = lights[i];
        const auto& n = N[i];
        m_ForwardShader->SetVec3 (n.Position, L.Position);
        m_ForwardShader->SetFloat(n.Range, L.Range);
        m_ForwardShader->SetVec3 (n.Direction, L.Direction);
        m_ForwardShader->SetFloat(n.Intensity, L.Intensity);
        m_ForwardShader->SetVec3 (n.Color, L.Color);
        m_ForwardShader->SetInt  (n.LightType, L.LightType);
        m_ForwardShader->SetFloat(n.InnerCutoff, L.InnerCutoff);
        m_ForwardShader->SetFloat(n.OuterCutoff, L.OuterCutoff);
        m_ForwardShader->SetInt  (n.ShadowMode,      L.ShadowMode);
        m_ForwardShader->SetFloat(n.ShadowStrength,  L.ShadowStrength);
        m_ForwardShader->SetFloat(n.BiasMultiplier,  L.BiasMultiplier);
        m_ForwardShader->SetInt  (n.ShadowSlot,      L.ShadowSlot);
    }

    MeshLibrary::Get().BindVAO();

    // Индивидуальные draw (без MDI — сортировка важна)
    for (size_t i = 0; i < sortedGPU.size(); ++i)
    {
        const InstanceGPU& inst = sortedGPU[i];
        if (!MeshLibrary::Get().IsValid(inst.MeshId))
            continue;
        const MeshGPU& mesh = MeshLibrary::Get().Get(inst.MeshId);
        if (mesh.IndexCount == 0)
            continue;

        m_ForwardShader->SetUInt("u_InstanceIndex", (uint32_t)i);

        const void* indexOffset =
            (const void*)(uintptr_t)(mesh.BaseIndex * sizeof(uint32_t));

        glDrawElementsBaseVertex(GL_TRIANGLES,
                                 (GLsizei)mesh.IndexCount,
                                 GL_UNSIGNED_INT,
                                 indexOffset,
                                 (GLint)mesh.BaseVertex);
    }

    glBindVertexArray(0);
    m_ForwardShader->Unbind();

    if (skybox.CubemapPtr && skybox.CubemapPtr->GetID())
        skybox.CubemapPtr->Unbind(4);

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    m_HDRFBO->Unbind();
}

void DeferredRenderer::RenderOutlinePass(const FrameRenderData& frame,
                                         const std::vector<entt::entity>& selectedEntities,
                                         const glm::mat4& viewMatrix,
                                         const glm::mat4& projectionMatrix,
                                         uint32_t width, uint32_t height)
{
    m_HasOutline = false;

    if (!m_OutlineShader || !m_OutlineFBO)
        return;

    if (selectedEntities.empty())
        return;

    // Резолвим размеры ОДИН раз и используем везде
    const uint32_t targetW = width  ? width  : m_GBuffer->GetWidth();
    const uint32_t targetH = height ? height : m_GBuffer->GetHeight();
    if (targetW == 0 || targetH == 0)
        return;

    EnsurePostTargets(targetW, targetH);

    // Сохраняем глобальный clear-color — мы его временно меняем
    GLfloat prevClearColor[4];
    glGetFloatv(GL_COLOR_CLEAR_VALUE, prevClearColor);

    m_OutlineFBO->Bind();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                           GL_TEXTURE_2D,
                           m_GBuffer->GetDepthAttachment(), 0);

    glViewport(0, 0, (GLsizei)targetW, (GLsizei)targetH);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);

    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -1.0f);

    m_OutlineShader->Bind();
    m_OutlineShader->SetMat4("u_View",       viewMatrix);
    m_OutlineShader->SetMat4("u_Projection", projectionMatrix);

    MeshLibrary::Get().BindVAO();

    bool anyDrawn = false;
    for (entt::entity sel : selectedEntities)
    {
        if (sel == entt::null) continue;

        int selectedIdx = -1;
        for (size_t i = 0; i < frame.InstanceEntities.size(); ++i)
        {
            if (frame.InstanceEntities[i] == sel)
            {
                selectedIdx = (int)i;
                break;
            }
        }
        if (selectedIdx < 0) continue;

        const InstanceGPU& inst = frame.Instances[selectedIdx];
        if (!MeshLibrary::Get().IsValid(inst.MeshId))
            continue;
        const MeshGPU& mesh = MeshLibrary::Get().Get(inst.MeshId);
        if (mesh.IndexCount == 0) continue;

        m_OutlineShader->SetMat4("u_Model", inst.Model);

        const void* indexOffset =
            (const void*)(uintptr_t)(mesh.BaseIndex * sizeof(uint32_t));

        glDrawElementsBaseVertex(GL_TRIANGLES,
                                 (GLsizei)mesh.IndexCount,
                                 GL_UNSIGNED_INT,
                                 indexOffset,
                                 (GLint)mesh.BaseVertex);
        anyDrawn = true;
    }

    glBindVertexArray(0);
    m_OutlineShader->Unbind();

    glDisable(GL_POLYGON_OFFSET_FILL);
    glDepthMask(GL_TRUE);
    m_OutlineFBO->Unbind();

    // Возвращаем глобальный clear-color — иначе главный framebuffer
    // будет очищаться прозрачным чёрным, а не 0.08/0.08/0.10 из main.cpp
        glDisable(GL_CULL_FACE);
    glClearColor(prevClearColor[0], prevClearColor[1],
                 prevClearColor[2], prevClearColor[3]);

    m_HasOutline = anyDrawn;
}

void DeferredRenderer::ReloadShaders()
{
    auto reloadSafe = [](std::unique_ptr<Shader>& shader)
    {
        if (shader)
        {
            try { shader->Reload(); }
            catch (const std::exception& e)
            {
                BINAX_LOG_ERROR("Shader reload failed: {0}", e.what());
            }
        }
    };

    reloadSafe(m_GeometryShader);
    reloadSafe(m_LightingShader);
    reloadSafe(m_FXAAShader);
    reloadSafe(m_TAAShader);
    reloadSafe(m_TonemapShader);
    reloadSafe(m_CullShader);
    reloadSafe(m_MotionBlurShader);
    reloadSafe(m_BloomDownShader);
    reloadSafe(m_BloomUpShader);
    reloadSafe(m_OutlineShader);
    reloadSafe(m_ShadowDepthShader);
    reloadSafe(m_ShadowDepthPointShader);
    reloadSafe(m_SSAOShader);
    reloadSafe(m_SSAOBlurShader);
    reloadSafe(m_VolumetricFogShader);
    reloadSafe(m_VolumetricFogBlurShader);
    reloadSafe(m_VolumetricFogCompositeShader);
    reloadSafe(m_ForwardShader);
    reloadSafe(m_ProbeSkyboxShader);

    BINAX_LOG_INFO("Shaders reloaded from disk");
}

}
