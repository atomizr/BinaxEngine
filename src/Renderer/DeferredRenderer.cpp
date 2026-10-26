#include "Renderer/DeferredRenderer.h"
#include "Renderer/AssetRegistry.h"
#include "Renderer/Mesh.h"
#include "Renderer/MeshLibrary.h"
#include "Renderer/InstanceBuffer.h"
#include "Utils/Log.h"
#include "Utils/Paths.h"
#include "Utils/Frustum.h"

#include <glad/glad.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <string>

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
}

DeferredRenderer::~DeferredRenderer()
{
    if (m_CullFence)        glDeleteSync(m_CullFence);
    if (m_DrawIndirectSSBO) glDeleteBuffers(1, &m_DrawIndirectSSBO);
    if (m_BonesDummySSBO)   glDeleteBuffers(1, &m_BonesDummySSBO);
    if (m_DrawStateSSBO)    glDeleteBuffers(1, &m_DrawStateSSBO);
}

void DeferredRenderer::InitCullResources()
{
    
    glGenBuffers(1, &m_DrawStateSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_DrawStateSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 3 * sizeof(uint32_t),
                 nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
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
}

void DeferredRenderer::InitShaders()
{
    const std::string dir = "resources/shaders/";

    
    
    
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

    
    InstanceBuffer::Get().Upload(frame.Instances);

    const uint32_t instanceCount = (uint32_t)frame.Instances.size();
    const bool gpuCull = m_FrustumCullingEnabled && m_CullShader != nullptr &&
                         instanceCount > 0;

    
    
    EnsureDrawBuffers(gpuCull ? instanceCount : (uint32_t)frame.Draws.size());

    
    
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_DrawIndirectSSBO);

    if (!gpuCull)
    {
        glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0,
                        (GLsizeiptr)(frame.Draws.size() * sizeof(DrawElementsIndirect)),
                        frame.Draws.data());
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
        PrepareCullPass(viewMatrix, projectionMatrix, instanceCount);

    
    
    
    const glm::mat4 projectionJittered = JitteredProjection(projectionMatrix);

    
    
    
    
    if (!m_HasPrevViewProj)
    {
        m_PrevViewProj    = projectionJittered * viewMatrix;
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
                                         (GLsizei)instanceCount,
                                         0);
        glBindBuffer(GL_PARAMETER_BUFFER, 0);
    }
    else
    {
        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT,
                                    (void*)0,
                                    (GLsizei)frame.Draws.size(),
                                    0);
    }
    glBindVertexArray(0);

    
    if (gpuCull)
    {
        
        m_FrameStats.GpuCulling       = true;
        m_FrameStats.DrawCalls        = m_LastCullStats.Draws;
        m_FrameStats.VisibleInstances = m_LastCullStats.Draws;
        m_FrameStats.Vertices         = m_LastCullStats.Vertices;
        m_FrameStats.Triangles        = m_LastCullStats.Indices / 3;

        
        
        
        
        if (m_CullFence == nullptr)
            m_CullFence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }
    else
    {
        m_FrameStats.GpuCulling = false;
        m_FrameStats.DrawCalls += (uint32_t)frame.Draws.size();
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
                                       uint32_t instanceCount)
{
    
    PollCullStats();

    
    static const GLuint kZeroState[3] = { 0u, 0u, 0u };
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

    
    const glm::mat4 viewProj  = projectionMatrix * viewMatrix;
    const Frustum   frustum   = Frustum::FromViewProjection(viewProj);

    m_CullShader->Bind();
    for (int i = 0; i < 6; ++i)
    {
        const std::string name = "u_Frustum[" + std::to_string(i) + "]";
        m_CullShader->SetVec4(name, frustum.Planes[i]);
    }
    m_CullShader->SetUInt("u_InstanceCount", instanceCount);
    m_CullShader->SetUInt("u_MaxDraws",      m_DrawCapacity);

    
    
    
    const GLuint groups = (instanceCount + kCullGroupSize - 1u) / kCullGroupSize;
    glDispatchCompute(groups, 1, 1);
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
        uint32_t values[3] = { 0u, 0u, 0u };
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_DrawStateSSBO);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(values), values);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        m_LastCullStats.Draws    = values[0];
        m_LastCullStats.Vertices = values[1];
        m_LastCullStats.Indices  = values[2];

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

void DeferredRenderer::LightingPass(const std::vector<GPULight>& lights,
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
                                    float timeSeconds)
{
    const uint32_t targetW = width  ? width  : m_GBuffer->GetWidth();
    const uint32_t targetH = height ? height : m_GBuffer->GetHeight();

    EnsurePostTargets(targetW, targetH);

    
    
    
    const glm::mat4 projJittered = JitteredProjection(projectionMatrix);
    const glm::mat4 viewProj     = projJittered * viewMatrix;
    const glm::mat4 invViewProj  = glm::inverse(viewProj);

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
    }

    DrawFullscreenQuad();
    if (skybox.CubemapPtr && skybox.CubemapPtr->GetID())
        skybox.CubemapPtr->Unbind(4);

    m_LightingShader->Unbind();
    m_HDRFBO->Unbind();

    
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
        ApplyFXAA(m_LDRFBO->GetColorAttachment(0), targetFBO, targetW, targetH, fxaa);
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
    m_TAAShader->SetFloat("u_ClampGamma", 1.25f);
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

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

} 
