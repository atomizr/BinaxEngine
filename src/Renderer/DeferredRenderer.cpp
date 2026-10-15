#include "DeferredRenderer.h"
#include "../Utils/Log.h"
#include <glad/glad.h>
#include "AssetRegistry.h"
#include <algorithm>
#include <array>
#include <string>

namespace {

struct LightUniformNames
{
    std::string Position;
    std::string Range;
    std::string Direction;
    std::string Intensity;
    std::string Color;
    std::string LightType;
    std::string InnerCutoff;
    std::string OuterCutoff;
};

const std::array<LightUniformNames, Binax::kMaxLights>& GetLightUniformNames()
{
    static const auto names = []() {
        std::array<LightUniformNames, Binax::kMaxLights> arr;
        for (int i = 0; i < Binax::kMaxLights; ++i)
        {
            const std::string base = "u_Lights[" + std::to_string(i) + "].";
            auto& n = arr[i];
            n.Position    = base + "Position";
            n.Range       = base + "Range";
            n.Direction   = base + "Direction";
            n.Intensity   = base + "Intensity";
            n.Color       = base + "Color";
            n.LightType   = base + "LightType";
            n.InnerCutoff = base + "InnerCutoff";
            n.OuterCutoff = base + "OuterCutoff";
        }
        return arr;
    }();
    return names;
}

}

namespace Binax {

DeferredRenderer::DeferredRenderer(uint32_t width, uint32_t height)
    : m_FullscreenQuad(Mesh::CreateFullscreenQuad())
{
    m_GBuffer = std::make_unique<GBuffer>(width, height);
    InitShaders();
}

DeferredRenderer::~DeferredRenderer() = default;

void DeferredRenderer::Resize(uint32_t width, uint32_t height)
{
    m_GBuffer.reset();
    m_GBuffer = std::make_unique<GBuffer>(width, height);
}

void DeferredRenderer::ClearGBuffer()
{
    // Reset per-frame stats at the beginning of every frame.
    m_FrameStats = {};

    m_GBuffer->Bind();

    glViewport(0, 0, static_cast<GLsizei>(m_GBuffer->GetWidth()),
                     static_cast<GLsizei>(m_GBuffer->GetHeight()));

    GLfloat prevColor[4];
    glGetFloatv(GL_COLOR_CLEAR_VALUE, prevColor);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glClearColor(prevColor[0], prevColor[1], prevColor[2], prevColor[3]);

    m_GBuffer->Unbind();
}

void DeferredRenderer::InitShaders()
{
    const std::string shaderDir = "resources/shaders/";

    m_GeometryShader = Shader::FromFiles(shaderDir + "geometry.vert",
                                         shaderDir + "geometry.frag");

    m_LightingShader = Shader::FromFiles(shaderDir + "lighting.vert",
                                         shaderDir + "lighting.frag");

    m_FXAAShader     = Shader::FromFiles(shaderDir + "fxaa.vert",
                                         shaderDir + "fxaa.frag");   // <-- NEW
}

void DeferredRenderer::GeometryPass(const Mesh& mesh,
                                    const glm::mat4& modelMatrix,
                                    const glm::mat4& viewMatrix,
                                    const glm::mat4& projectionMatrix,
                                    const MaterialComponent& material)
{
    m_GBuffer->Bind();

    glViewport(0, 0, static_cast<GLsizei>(m_GBuffer->GetWidth()),
                     static_cast<GLsizei>(m_GBuffer->GetHeight()));

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);

    m_GeometryShader->Bind();
    m_GeometryShader->SetMat4 ("u_Model",      modelMatrix);
    m_GeometryShader->SetMat4 ("u_View",       viewMatrix);
    m_GeometryShader->SetMat4 ("u_Projection", projectionMatrix);

    // ---- Scalar params + uniform multipliers ----
    m_GeometryShader->SetVec3 ("u_Albedo",           material.Albedo);
    m_GeometryShader->SetFloat("u_Metallic",         material.Metallic);
    m_GeometryShader->SetFloat("u_Roughness",        material.Roughness);
    m_GeometryShader->SetFloat("u_AO",               material.AO);
    m_GeometryShader->SetFloat("u_Emissive",         material.Emissive);

    m_GeometryShader->SetFloat("u_Tiling",           material.Tiling);

    m_GeometryShader->SetFloat("u_AlbedoStrength",   material.AlbedoStrength);
    m_GeometryShader->SetFloat("u_NormalStrength",   material.NormalStrength);
    m_GeometryShader->SetFloat("u_MetallicStrength", material.MetallicStrength);
    m_GeometryShader->SetFloat("u_RoughnessStrength",material.RoughnessStrength);
    m_GeometryShader->SetFloat("u_AOStrength",       material.AOStrength);
    m_GeometryShader->SetFloat("u_EmissiveStrength", material.EmissiveStrength);

    // ---- Skybox reflections ----
    m_GeometryShader->SetInt  ("u_SkyReflectionEnabled",
                               material.SkyReflectionsEnabled ? 1 : 0);
    m_GeometryShader->SetFloat("u_SkyReflectionStrength",
                               material.SkyReflectionStrength);

    auto& assets = AssetRegistry::Get();

    struct TexSlot
    {
        const Texture* tex;
        int slot;
        const char* uniformSampler;
        const char* uniformHas;
    };

    const Texture* texAlbedo    = assets.GetTexture(material.AlbedoMapPath,    true);
    const Texture* texNormal    = assets.GetTexture(material.NormalMapPath,    false);
    const Texture* texMetallic  = assets.GetTexture(material.MetallicMapPath,  false);
    const Texture* texRoughness = assets.GetTexture(material.RoughnessMapPath, false);
    const Texture* texAO        = assets.GetTexture(material.AOMapPath,        false);
    const Texture* texEmissive  = assets.GetTexture(material.EmissiveMapPath,  true);

    const TexSlot slots[] = {
        { texAlbedo,    0, "u_AlbedoMap",    "u_HasAlbedoMap"    },
        { texNormal,    1, "u_NormalMap",    "u_HasNormalMap"    },
        { texMetallic,  2, "u_MetallicMap",  "u_HasMetallicMap"  },
        { texRoughness, 3, "u_RoughnessMap", "u_HasRoughnessMap" },
        { texAO,        4, "u_AOMap",        "u_HasAOMap"        },
        { texEmissive,  5, "u_EmissiveMap",  "u_HasEmissiveMap"  },
    };

    for (const auto& s : slots)
    {
        if (s.tex)
        {
            s.tex->Bind(static_cast<uint32_t>(s.slot));
            m_GeometryShader->SetInt(s.uniformSampler, s.slot);
            m_GeometryShader->SetInt(s.uniformHas, 1);
        }
        else
        {
            m_GeometryShader->SetInt(s.uniformHas, 0);
        }
    }

    mesh.Bind();
    mesh.Draw();
    mesh.Unbind();

    // ---- Per-frame stats ----
    m_FrameStats.DrawCalls += 1;
    m_FrameStats.Vertices  += mesh.GetVertexCount();
    m_FrameStats.Triangles += mesh.GetTriangleCount();

    for (const auto& s : slots)
        if (s.tex)
            s.tex->Unbind(static_cast<uint32_t>(s.slot));

    m_GeometryShader->Unbind();
    m_GBuffer->Unbind();
}

void DeferredRenderer::EndGeometryPass()
{
}

// LightingPass(): если FXAA включён — рендерим в промежуточный FBO,
// затем одним проходом FXAA переносим в targetFBO.
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

    // ---- 1. Готовим промежуточный FBO, если нужен FXAA ----
    if (useFXAA)
    {
        if (!m_LightingFBO)
        {
            m_LightingFBO = std::make_unique<Framebuffer>(targetW, targetH);
            m_LightingFBO->Bind();
            m_LightingFBO->AttachColorTexture(GL_RGBA8, GL_RGBA,
                                              GL_UNSIGNED_BYTE, 0);
            m_LightingFBO->CheckStatus();
            m_LightingFBO->Unbind();
        }
        else
        {
            m_LightingFBO->Resize(targetW, targetH);
        }

        m_LightingFBO->Bind();
    }
    else
    {
        glBindFramebuffer(GL_FRAMEBUFFER, targetFBO);
    }

    glViewport(0, 0, static_cast<GLsizei>(targetW),
                     static_cast<GLsizei>(targetH));
    glClear(GL_COLOR_BUFFER_BIT);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDepthMask(GL_FALSE);

    // ---- 2. Обычный lighting-пасс (без изменений) ----
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
        m_LightingShader->SetInt  ("u_EnvironmentMap",    4);
        m_LightingShader->SetInt  ("u_HasEnvironment",    1);
        m_LightingShader->SetFloat("u_SkyboxRotation",    skybox.RotationRadians);
        m_LightingShader->SetFloat("u_SkyboxBrightness",  skybox.Brightness);
    }
    else
    {
        m_LightingShader->SetInt("u_HasEnvironment", 0);
    }

    const int count = static_cast<int>(std::min<size_t>(lights.size(),
                                                        static_cast<size_t>(kMaxLights)));
    m_LightingShader->SetInt("u_LightCount", count);

    const auto& uniformNames = GetLightUniformNames();
    for (int i = 0; i < count; ++i)
    {
        const auto& L = lights[i];
        const auto& n = uniformNames[i];
        m_LightingShader->SetVec3 (n.Position,    L.Position);
        m_LightingShader->SetFloat(n.Range,       L.Range);
        m_LightingShader->SetVec3 (n.Direction,   L.Direction);
        m_LightingShader->SetFloat(n.Intensity,   L.Intensity);
        m_LightingShader->SetVec3 (n.Color,       L.Color);
        m_LightingShader->SetInt  (n.LightType,   L.LightType);
        m_LightingShader->SetFloat(n.InnerCutoff, L.InnerCutoff);
        m_LightingShader->SetFloat(n.OuterCutoff, L.OuterCutoff);
    }

    m_FullscreenQuad.Bind();
    m_FullscreenQuad.Draw();
    m_FullscreenQuad.Unbind();

    if (skybox.CubemapPtr && skybox.CubemapPtr->GetID())
        skybox.CubemapPtr->Unbind(4);

    m_LightingShader->Unbind();

    // ---- 3. FXAA-пасс ----
    if (useFXAA)
    {
        ApplyFXAA(m_LightingFBO->GetColorAttachment(0),
                  targetFBO, targetW, targetH, fxaa);
    }

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
}

void DeferredRenderer::ApplyFXAA(uint32_t sourceTexture,
                                 uint32_t targetFBO,
                                 uint32_t width, uint32_t height,
                                 const FXAAParams& params)
{
    glBindFramebuffer(GL_FRAMEBUFFER, targetFBO);
    glViewport(0, 0, static_cast<GLsizei>(width),
                     static_cast<GLsizei>(height));

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    // FXAA требует LINEAR-фильтрацию для промежуточных выборок.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    m_FXAAShader->Bind();
    m_FXAAShader->SetInt  ("u_ColorTexture",      0);
    m_FXAAShader->SetVec2 ("u_TexelSize",
                           glm::vec2(1.0f / static_cast<float>(width),
                                     1.0f / static_cast<float>(height)));
    m_FXAAShader->SetFloat("u_EdgeThreshold",     params.EdgeThreshold);
    m_FXAAShader->SetFloat("u_EdgeThresholdMin",  params.EdgeThresholdMin);
    m_FXAAShader->SetFloat("u_SubpixelBlending",  params.SubpixelBlending);

    m_FullscreenQuad.Bind();
    m_FullscreenQuad.Draw();
    m_FullscreenQuad.Unbind();

    m_FXAAShader->Unbind();

    glBindTexture(GL_TEXTURE_2D, 0);
}

}
