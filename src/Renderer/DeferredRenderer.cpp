#include "Renderer/DeferredRenderer.h"
#include "Renderer/AssetRegistry.h"
#include "Renderer/Mesh.h"
#include "Renderer/MeshLibrary.h"
#include "Renderer/InstanceBuffer.h"
#include "Utils/Log.h"

#include <glad/glad.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

namespace {

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

} // namespace

namespace Binax {

DeferredRenderer::DeferredRenderer(uint32_t width, uint32_t height)
{
    m_GBuffer = std::make_unique<GBuffer>(width, height);
    InitShaders();
    EnsureQuad();
}

DeferredRenderer::~DeferredRenderer()
{
    if (m_DrawIndirectSSBO) glDeleteBuffers(1, &m_DrawIndirectSSBO);
    if (m_DrawMetaSSBO)     glDeleteBuffers(1, &m_DrawMetaSSBO);
    if (m_BonesDummySSBO)   glDeleteBuffers(1, &m_BonesDummySSBO);
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
    m_GBuffer.reset();
    m_GBuffer = std::make_unique<GBuffer>(width, height);
}

void DeferredRenderer::ClearGBuffer()
{
    m_FrameStats = {};

    m_GBuffer->Bind();
    glViewport(0, 0, (GLsizei)m_GBuffer->GetWidth(), (GLsizei)m_GBuffer->GetHeight());

    GLfloat prev[4]; glGetFloatv(GL_COLOR_CLEAR_VALUE, prev);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(prev[0], prev[1], prev[2], prev[3]);

    m_GBuffer->Unbind();
}

void DeferredRenderer::InitShaders()
{
    const std::string dir = "resources/shaders/";
    m_GeometryShader = Shader::FromFiles(dir + "geometry.vert", dir + "geometry.frag");
    m_LightingShader = Shader::FromFiles(dir + "lighting.vert", dir + "lighting.frag");
    m_FXAAShader     = Shader::FromFiles(dir + "fxaa.vert",     dir + "fxaa.frag");
}

void DeferredRenderer::EnsureDrawBuffers(uint32_t drawCount)
{
    if (m_DrawCapacity >= drawCount && m_DrawIndirectSSBO != 0 && m_DrawMetaSSBO != 0)
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

    if (m_DrawMetaSSBO == 0) glGenBuffers(1, &m_DrawMetaSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_DrawMetaSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 (GLsizeiptr)(cap * sizeof(uint32_t)),
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

    // Ensure bindless textures are uploaded once per frame (cheap if nothing dirty)
    MaterialLibrary::Get().Flush();

    // ---- Upload instances ----
    InstanceBuffer::Get().Upload(frame.Instances);

    // ---- Upload draw buffers ----
    EnsureDrawBuffers((uint32_t)frame.Draws.size());

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_DrawIndirectSSBO);
    glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0,
                    (GLsizeiptr)(frame.Draws.size() * sizeof(DrawElementsIndirect)),
                    frame.Draws.data());

    std::vector<uint32_t> baseInstances(frame.Draws.size());
    for (size_t i = 0; i < frame.Draws.size(); ++i)
        baseInstances[i] = frame.Draws[i].BaseInstance;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_DrawMetaSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                    (GLsizeiptr)(baseInstances.size() * sizeof(uint32_t)),
                    baseInstances.data());

    // ---- Bindings ----
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, InstanceBuffer::Get().GetSSBO());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, MaterialLibrary::Get().GetMaterialSSBO());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_BonesDummySSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, m_DrawMetaSSBO);

    // ---- FBO ----
    m_GBuffer->Bind();
    glViewport(0, 0, (GLsizei)m_GBuffer->GetWidth(), (GLsizei)m_GBuffer->GetHeight());

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glFrontFace(GL_CCW);

    // ---- Shader ----
    m_GeometryShader->Bind();
    m_GeometryShader->SetMat4("u_View",       viewMatrix);
    m_GeometryShader->SetMat4("u_Projection", projectionMatrix);

    // ---- Draw ----
    MeshLibrary::Get().BindVAO();
    glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT,
                                (void*)0,
                                (GLsizei)frame.Draws.size(),
                                0);
    glBindVertexArray(0);

    // ---- Stats ----
    m_FrameStats.DrawCalls += (uint32_t)frame.Draws.size();
    for (const auto& d : frame.Draws)
    {
        m_FrameStats.Triangles += (d.Count / 3) * d.InstanceCount;
        m_FrameStats.Vertices  += d.Count * d.InstanceCount;
    }

    m_GeometryShader->Unbind();

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    m_GBuffer->Unbind();
}

void DeferredRenderer::DrawFullscreenQuad()
{
    const MeshGPU& q = MeshLibrary::Get().Get(m_QuadHandle);
    MeshLibrary::Get().BindVAO();

    // ВАЖНО: BaseIndex — это смещение в общем индексном буфере (в элементах,
    // не в байтах). Для glDrawElementsBaseVertex нужно передать байтовое
    // смещение, поэтому умножаем на sizeof(uint32_t). BaseVertex — сдвиг
    // вершинной части.
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
                                    const glm::mat4& invViewProj,
                                    const SkyboxParams& skybox,
                                    uint32_t targetFBO,
                                    uint32_t width,
                                    uint32_t height,
                                    const FXAAParams& fxaa)
{
    const uint32_t targetW = width  ? width  : m_GBuffer->GetWidth();
    const uint32_t targetH = height ? height : m_GBuffer->GetHeight();
    const bool useFXAA = fxaa.Enabled && m_FXAAShader != nullptr;

    if (useFXAA)
    {
        if (!m_LightingFBO)
        {
            m_LightingFBO = std::make_unique<Framebuffer>(targetW, targetH);
            m_LightingFBO->Bind();
            m_LightingFBO->AttachColorTexture(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 0);
            m_LightingFBO->CheckStatus();
            m_LightingFBO->Unbind();
        }
        else m_LightingFBO->Resize(targetW, targetH);

        m_LightingFBO->Bind();
    }
    else
    {
        glBindFramebuffer(GL_FRAMEBUFFER, targetFBO);
    }

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

    if (useFXAA)
        ApplyFXAA(m_LightingFBO->GetColorAttachment(0), targetFBO, targetW, targetH, fxaa);

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

} // namespace Binax
