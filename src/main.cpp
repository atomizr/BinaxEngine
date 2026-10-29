#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <entt/entt.hpp>
#include <imgui.h>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <exception>

#include "Core/Window.h"
#include "Core/Input.h"
#include "Core/Timer.h"
#include "Utils/Log.h"
#include "Utils/Paths.h"
#include "Renderer/DeferredRenderer.h"
#include "Renderer/Mesh.h"
#include "Renderer/MeshLibrary.h"
#include "Renderer/MaterialLibrary.h"
#include "Renderer/InstanceBuffer.h"
#include "Renderer/GPULight.h"
#include "Renderer/AssetRegistry.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/MeshComponent.h"
#include "ECS/Components/MaterialComponent.h"
#include "ECS/Components/CameraComponent.h"
#include "ECS/Components/NameComponent.h"
#include "ECS/Components/EditorCameraComponent.h"
#include "ECS/Components/SystemEntityTag.h"
#include "ECS/Components/SkyboxComponent.h"
#include "ECS/Components/PostProcessComponent.h"
#include "ECS/Systems/RenderSystem.h"
#include "ECS/Systems/TransformSystem.h"
#include "Editor/EditorUI.h"
#include "Editor/Viewport.h"
#include "Editor/EditorCamera.h"
#include "Serialization/SceneSerializer.h"
#include "Serialization/MaterialSerializer.h"
#include "Serialization/PostProcessSerializer.h"
#include "Utils/AssetPaths.h"
#include "Reflect/EngineReflection.h"

namespace {

entt::entity CreateEditorCameraEntity(entt::registry& registry, float aspect)
{
    auto e = registry.create();
    registry.emplace<Binax::NameComponent>(e, "EditorCamera");
    registry.emplace<Binax::Transform>(e,
                                       glm::vec3(0.0f, 1.0f, 3.0f),
                                       glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                       glm::vec3(1.0f));
    registry.emplace<Binax::EditorCameraComponent>(e);
    registry.emplace<Binax::CameraComponent>(e,
                                             Binax::ProjectionType::Perspective,
                                             45.0f, 0.1f, 100.0f, 5.0f,
                                             aspect,
                                             true);
    registry.emplace<Binax::SystemEntityTag>(e);
    return e;
}

entt::entity CreateSkyboxEntity(entt::registry& registry)
{
    auto e = registry.create();
    registry.emplace<Binax::NameComponent>(e, "Skybox");
    registry.emplace<Binax::SkyboxComponent>(e);
    return e;
}

void EnsureSkyboxExists(entt::registry& registry)
{
    auto view = registry.view<Binax::SkyboxComponent>();
    if (view.begin() == view.end())
        CreateSkyboxEntity(registry);
}

entt::entity CreatePostProcessCfgEntity(entt::registry& registry)
{
    auto e = registry.create();
    registry.emplace<Binax::NameComponent>(e, "PostProcessingCfg");
    registry.emplace<Binax::Transform>(e);
    registry.emplace<Binax::PostProcessComponent>(e);
    registry.emplace<Binax::SystemEntityTag>(e);
    return e;
}

void EnsurePostProcessCfgExists(entt::registry& registry)
{
    auto view = registry.view<Binax::PostProcessComponent>();
    if (view.begin() == view.end())
        CreatePostProcessCfgEntity(registry);
}

Binax::PostProcessSettings GatherPostProcessSettings(entt::registry& registry)
{
    Binax::PostProcessSettings s;

    // FIX: дефолтный tonemap — Linear (просто clamp), а не ACES.
    // Раньше при post.Enabled == false ACES всё равно применялся, потому что
    // ApplyTonemap вызывается всегда, а TonemapMode по умолчанию был 0 (ACES).
    s.TonemapMode = static_cast<int>(Binax::TonemapMode::Linear);

    auto view = registry.view<Binax::PostProcessComponent>();
    for (auto e : view)
    {
        const auto& c = view.get<Binax::PostProcessComponent>(e);
        if (!c.Enabled)
            return s;   // Linear tonemap + дефолтные громкости → passthrough-ish

        s.Enabled  = true;
        s.Exposure = c.Exposure;

        s.TonemapMode = static_cast<int>(c.Tonemap);
        s.Contrast    = c.Contrast;
        s.Saturation  = c.Saturation;
        s.Temperature = c.Temperature;
        s.Tint        = c.Tint;
        s.Lift        = c.Lift;
        s.Gamma       = c.Gamma;
        s.Gain        = c.Gain;

        s.BloomEnabled   = c.BloomEnabled;
        s.BloomThreshold = c.BloomThreshold;
        s.BloomIntensity = c.BloomIntensity;

        s.MotionBlurEnabled  = c.MotionBlurEnabled;
        s.MotionBlurStrength = c.MotionBlurStrength;
        s.MotionBlurSamples  = c.MotionBlurSamples;

        s.LensDistortionEnabled = c.LensDistortionEnabled;
        s.LensDistortion        = c.LensDistortion;

        s.ChromaticAberrationEnabled = c.ChromaticAberrationEnabled;
        s.ChromaticAberration        = c.ChromaticAberration;

        s.VignetteEnabled    = c.VignetteEnabled;
        s.VignetteIntensity  = c.VignetteIntensity;
        s.VignetteSmoothness = c.VignetteSmoothness;

        s.FilmGrainEnabled   = c.FilmGrainEnabled;
        s.FilmGrainIntensity = c.FilmGrainIntensity;

        s.FogEnabled = c.FogEnabled;
        s.FogMode    = static_cast<int>(c.Fog);
        s.FogColor   = c.FogColor;
        s.FogDensity = c.FogDensity;
        s.FogStart   = c.FogStart;
        s.FogEnd     = c.FogEnd;

        return s;
    }

    return s;
}

Binax::SkyboxParams GatherSkyboxParams(entt::registry& registry)
{
    Binax::SkyboxParams params;

    auto view = registry.view<Binax::SkyboxComponent>();
    for (auto e : view)
    {
        auto& sc = view.get<Binax::SkyboxComponent>(e);

        if (!sc.Enabled) return params;

        std::array<std::string, 6> faces = {
            sc.FacePX, sc.FaceNX,
            sc.FacePY, sc.FaceNY,
            sc.FacePZ, sc.FaceNZ
        };

        bool complete = true;
        for (const auto& p : faces)
            if (p.empty()) { complete = false; break; }
        if (!complete) return params;

        params.CubemapPtr      = Binax::AssetRegistry::Get().GetCubemap(faces, true);
        params.RotationRadians = glm::radians(sc.Rotation);
        params.Brightness      = sc.Brightness;
        return params;
    }
    return params;
}

} 

int main()
{
    Binax::Log::Init();
    Binax::Reflect::RegisterEngineComponents();
    BINAX_LOG_INFO("Binax Engine Starting...");

    int exitCode = 0;

    try
    {
        Binax::Window::Props props;
        props.Title  = "Binax Engine";
        props.Width  = 1920;
        props.Height = 1080;
        props.VSync  = true;

        Binax::Window window(props);
        Binax::Input::SetWindow(window.GetGLFWWindow());

        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            BINAX_LOG_ERROR("Failed to initialize GLAD");
            throw std::runtime_error("Failed to initialize GLAD");
        }

        BINAX_LOG_INFO("OpenGL: {0}",
            reinterpret_cast<const char*>(glGetString(GL_VERSION)));

        if (!GLAD_GL_ARB_bindless_texture)
        {
            BINAX_LOG_ERROR("GL_ARB_bindless_texture required but not available");
            throw std::runtime_error("Missing required extension: GL_ARB_bindless_texture");
        }
        BINAX_LOG_INFO("ARB_bindless_texture: available");

        if (!GLAD_GL_ARB_shader_draw_parameters)
        {
            BINAX_LOG_ERROR("GL_ARB_shader_draw_parameters required but not available");
            throw std::runtime_error("Missing required extension: GL_ARB_shader_draw_parameters");
        }
        BINAX_LOG_INFO("ARB_shader_draw_parameters: available");

        {
            GLint major = 0, minor = 0;
            glGetIntegerv(GL_MAJOR_VERSION, &major);
            glGetIntegerv(GL_MINOR_VERSION, &minor);
            BINAX_LOG_INFO("Context version: {}.{} (bindless handles as uvec2)",
                           major, minor);
        }

        Binax::Timer timer;
        Binax::DeferredRenderer renderer(props.Width, props.Height);

        {
            auto cube = Binax::Mesh::CreateCube();
            Binax::MeshLibrary::Get().Add("Cube", cube.Vertices, cube.Indices);

            auto quad = Binax::Mesh::CreateFullscreenQuad();
            Binax::MeshLibrary::Get().Add("FullscreenQuad", quad.Vertices, quad.Indices);

            Binax::MeshLibrary::Get().Flush();
            BINAX_LOG_INFO("Built-in meshes registered");
        }

        Binax::MaterialSerializer::LoadAll(
            Binax::MaterialLibrary::Get(),
            Binax::Paths::Resolve(Binax::AssetFiles::kMaterialsDir));

        entt::registry registry;

        CreateEditorCameraEntity(registry, window.GetAspect());
        BINAX_LOG_INFO("EditorCamera created");

        EnsureSkyboxExists(registry);
        BINAX_LOG_INFO("Skybox entity ensured");

        EnsurePostProcessCfgExists(registry);
        BINAX_LOG_INFO("PostProcessingCfg entity ensured");

        {
            const std::string ppPath =
                Binax::Paths::Resolve(Binax::AssetFiles::kPostProcessFile);

            std::error_code ec;
            if (std::filesystem::exists(Binax::Paths::FromUtf8(ppPath), ec) && !ec)
            {
                if (!Binax::PostProcessSerializer::Load(registry, ppPath))
                    BINAX_LOG_WARN("Post-processing load failed — using defaults");
            }
            else
            {
                BINAX_LOG_INFO("No {} found — using default post-processing",
                               Binax::AssetFiles::kPostProcessFile);
            }
        }

        {
            const std::string levelPath =
                Binax::Paths::Resolve(Binax::AssetFiles::kLevelFile);

            std::error_code ec;
            const bool levelExists =
                std::filesystem::exists(Binax::Paths::FromUtf8(levelPath), ec) && !ec;

            if (levelExists)
            {
                if (Binax::SceneSerializer::Deserialize(registry, levelPath))
                    BINAX_LOG_INFO("Level loaded from disk");
                else
                    BINAX_LOG_WARN("Level load failed — starting empty");
            }
            else
            {
                BINAX_LOG_INFO("No {} found — starting empty", Binax::AssetFiles::kLevelFile);
            }
        }

        EnsureSkyboxExists(registry);
        EnsurePostProcessCfgExists(registry);

        Binax::Viewport viewport(props.Width, props.Height);
        Binax::EditorCamera editorCamera(viewport.GetAspect());
        Binax::EditorUI editorUI(window.GetGLFWWindow(), registry);
        editorUI.SetViewport(&viewport);

        bool vsyncEnabled = props.VSync;
        float smoothedFps = 0.0f;

        editorUI.AddPanelDrawer([&]()
        {
            ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_NoCollapse);

            const float dt = timer.GetDelta();
            const float ms = dt * 1000.0f;
            const float instFps = (dt > 1e-6f) ? (1.0f / dt) : 0.0f;

            smoothedFps = (smoothedFps <= 0.0f)
                              ? instFps
                              : smoothedFps * 0.9f + instFps * 0.1f;

            ImGui::Text("FPS:        %.1f", smoothedFps);
            ImGui::Text("Frame time: %.2f ms", ms);

            ImGui::Separator();
            if (ImGui::Checkbox("VSync", &vsyncEnabled))
                window.SetVSync(vsyncEnabled);

            bool gpuCull = renderer.IsFrustumCullingEnabled();
            if (ImGui::Checkbox("GPU Frustum Culling", &gpuCull))
                renderer.SetFrustumCullingEnabled(gpuCull);
            if (!renderer.IsFrustumCullingAvailable())
                ImGui::TextDisabled("  (compute shader unavailable)");

            ImGui::Separator();
            const Binax::FrameStats& s = renderer.GetFrameStats();
            ImGui::Text("Draw calls: %u", s.DrawCalls);
            ImGui::Text("Vertices:   %u", s.Vertices);
            ImGui::Text("Triangles:  %u", s.Triangles);

            if (s.GpuCulling)
            {
                const uint32_t total = Binax::InstanceBuffer::Get().GetCount();
                ImGui::Text("Visible:    %u (GPU)", s.VisibleInstances);
                ImGui::Text("Culled:     %u",
                            total > s.VisibleInstances ? total - s.VisibleInstances : 0u);
            }

            ImGui::Text("TAA:        %s", s.TaaActive ? "on" : "off");
            ImGui::Text("PostFX:     %s", s.PostFXActive ? "on" : "off");

            ImGui::Separator();
            ImGui::Text("Meshes:     %u", Binax::MeshLibrary::Get().GetMeshCount());
            ImGui::Text("Materials:  %u", Binax::MaterialLibrary::Get().GetMaterialCount());
            ImGui::Text("Instances:  %u", Binax::InstanceBuffer::Get().GetCount());

            ImGui::End();
        });

        std::vector<Binax::GPULight> collectedLights;
        collectedLights.reserve(Binax::kMaxLights);

        Binax::FrameRenderData frameData;

        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);

        while (!window.ShouldClose())
        {
            timer.Update();
            float delta = timer.GetDelta();

            Binax::Input::Update();
            window.Update();

            editorUI.SyncViewportSize();

            const float vpAspect = viewport.GetAspect();
            editorCamera.OnResize(vpAspect);

            {
                glm::vec2 vpSize = viewport.GetSize();
                uint32_t vpW = (uint32_t)vpSize.x;
                uint32_t vpH = (uint32_t)vpSize.y;
                if (vpW > 0 && vpH > 0 &&
                    (vpW != renderer.GetGBuffer().GetWidth() ||
                     vpH != renderer.GetGBuffer().GetHeight()))
                {
                    renderer.Resize(vpW, vpH);
                }
            }

            if (Binax::Input::IsKeyPressed(GLFW_KEY_ESCAPE) &&
                !ImGui::GetIO().WantTextInput)
                window.SetShouldClose(true);

            auto cameraView = registry.view<Binax::Transform,
                                            Binax::EditorCameraComponent,
                                            Binax::CameraComponent>();

            if (cameraView.begin() == cameraView.end())
            {
                BINAX_LOG_ERROR("EditorCamera missing! Recreating...");
                CreateEditorCameraEntity(registry, vpAspect);
                cameraView = registry.view<Binax::Transform,
                                           Binax::EditorCameraComponent,
                                           Binax::CameraComponent>();
            }

            Binax::FXAAParams frameFXAA{};
            Binax::TAAParams  frameTAA{};

            for (auto entity : cameraView)
            {
                auto& transform     = cameraView.get<Binax::Transform>(entity);
                auto& editorCamComp = cameraView.get<Binax::EditorCameraComponent>(entity);
                auto& camComp       = cameraView.get<Binax::CameraComponent>(entity);

                camComp.AspectRatio = vpAspect;

                editorCamera.SetPosition(transform.Position);
                editorCamera.SetOrientation(transform.Rotation);
                editorCamera.SetFOV(camComp.FOV);
                editorCamera.SetNearFar(camComp.NearPlane, camComp.FarPlane);
                editorCamera.SetMoveSpeed(editorCamComp.MoveSpeed);
                editorCamera.SetRotationSpeed(editorCamComp.RotationSpeed);
                editorCamera.SetSpeedMultiplier(editorCamComp.SpeedMultiplier);

                frameFXAA.Enabled          = camComp.FXAAEnabled;
                frameFXAA.EdgeThreshold    = camComp.FXAAEdgeThreshold;
                frameFXAA.EdgeThresholdMin = camComp.FXAAEdgeThresholdMin;
                frameFXAA.SubpixelBlending = camComp.FXAASubpixelBlending;

                frameTAA.Enabled       = camComp.TAAEnabled;
                frameTAA.HistoryWeight = camComp.TAAHistoryWeight;

                break;
            }

            editorCamera.SetMouseOver(editorUI.IsViewportHovered());
            editorCamera.SetGizmoActive(editorUI.IsGizmoUsing());
            editorCamera.Update(delta);

            for (auto entity : cameraView)
            {
                auto& transform = cameraView.get<Binax::Transform>(entity);
                transform.Position = editorCamera.GetPosition();
                transform.Rotation = editorCamera.GetOrientation();
                break;
            }

            editorUI.SetCameraMatrices(editorCamera.GetViewMatrix(),
                                       editorCamera.GetProjectionMatrix());

            renderer.SetTAAEnabled(frameTAA.Enabled);
            renderer.ClearGBuffer();

            Binax::TransformSystem::UpdateWorldTransforms(registry);

            Binax::RenderSystem::BuildFrameData(registry, frameData);

            renderer.GeometryPass(frameData,
                                  editorCamera.GetViewMatrix(),
                                  editorCamera.GetProjectionMatrix());

            collectedLights.clear();
            Binax::RenderSystem::CollectLights(registry, collectedLights);

            Binax::SkyboxParams skyboxParams = GatherSkyboxParams(registry);

            const Binax::PostProcessSettings postSettings =
                GatherPostProcessSettings(registry);

            renderer.LightingPass(collectedLights,
                                  editorCamera.GetPosition(),
                                  editorCamera.GetViewMatrix(),
                                  editorCamera.GetProjectionMatrix(),
                                  skyboxParams,
                                  viewport.GetFBO(),
                                  (uint32_t)viewport.GetSize().x,
                                  (uint32_t)viewport.GetSize().y,
                                  frameFXAA,
                                  frameTAA,
                                  postSettings,
                                  timer.GetElapsed());

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, window.GetWidth(), window.GetHeight());
            glClear(GL_COLOR_BUFFER_BIT);

            editorUI.Draw();

            window.SwapBuffers();
        }

        BINAX_LOG_INFO("Shutting down...");

        Binax::InstanceBuffer::Get().Clear();
        Binax::AssetRegistry::Get().Clear();
        Binax::MaterialLibrary::Get().Clear();
        Binax::MeshLibrary::Get().Clear();
    }
    catch (const std::exception& e)
    {
        BINAX_LOG_ERROR("Exception: {0}", e.what());
        exitCode = 1;
    }

    glfwTerminate();
    return exitCode;
}
