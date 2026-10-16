#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <entt/entt.hpp>
#include <imgui.h>
#include <array>
#include <fstream>
#include <vector>
#include <string>
#include <exception>

#include "Core/Window.h"
#include "Core/Input.h"
#include "Core/Timer.h"
#include "Utils/Log.h"
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
#include "ECS/Systems/RenderSystem.h"
#include "Editor/EditorUI.h"
#include "Editor/Viewport.h"
#include "Editor/EditorCamera.h"
#include "Serialization/SceneSerializer.h"
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

        params.CubemapPtr      = Binax::AssetRegistry::Get().GetCubemap(faces, /*srgb*/ true);
        params.RotationRadians = glm::radians(sc.Rotation);
        params.Brightness      = sc.Brightness;
        return params;
    }
    return params;
}

} // anon namespace

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
            return 1;
        }

        BINAX_LOG_INFO("OpenGL: {0}",
            reinterpret_cast<const char*>(glGetString(GL_VERSION)));

        if (GLAD_GL_ARB_bindless_texture)
            BINAX_LOG_INFO("ARB_bindless_texture: available");
        else
            BINAX_LOG_WARN("ARB_bindless_texture: NOT available — textures will be ignored");

        if (GLAD_GL_ARB_shader_draw_parameters)
            BINAX_LOG_INFO("ARB_shader_draw_parameters: available");
        else
            BINAX_LOG_WARN("ARB_shader_draw_parameters: NOT available — gl_DrawID broken");

        Binax::Timer timer;
        Binax::DeferredRenderer renderer(props.Width, props.Height);

        // ---- Register built-in meshes BEFORE anything else ----
        {
            auto cube = Binax::Mesh::CreateCube();
            Binax::MeshLibrary::Get().Add("Cube", cube.Vertices, cube.Indices);

            auto quad = Binax::Mesh::CreateFullscreenQuad();
            Binax::MeshLibrary::Get().Add("FullscreenQuad", quad.Vertices, quad.Indices);

            Binax::MeshLibrary::Get().Flush();
            BINAX_LOG_INFO("Built-in meshes registered");
        }

        entt::registry registry;

        CreateEditorCameraEntity(registry, window.GetAspect());
        BINAX_LOG_INFO("EditorCamera created");

        EnsureSkyboxExists(registry);
        BINAX_LOG_INFO("Skybox entity ensured");

        // ---- Scene load ----
        {
            std::ifstream check("scene.json");
            const bool sceneExists = check.good();
            check.close();

            if (sceneExists)
            {
                if (Binax::SceneSerializer::Deserialize(registry, "scene.json"))
                    BINAX_LOG_INFO("Scene loaded from disk");
                else
                    BINAX_LOG_WARN("Scene load failed — starting empty");
            }
            else
            {
                BINAX_LOG_INFO("No scene.json found — starting empty");
            }
        }

        EnsureSkyboxExists(registry);

        Binax::Viewport viewport(props.Width, props.Height);
        Binax::EditorCamera editorCamera(viewport.GetAspect());
        Binax::EditorUI editorUI(window.GetGLFWWindow(), registry);
        editorUI.SetViewport(&viewport);

        // ---- Stats panel ----
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

            ImGui::Separator();
            const Binax::FrameStats& s = renderer.GetFrameStats();
            ImGui::Text("Draw calls: %u", s.DrawCalls);
            ImGui::Text("Vertices:   %u", s.Vertices);
            ImGui::Text("Triangles:  %u", s.Triangles);

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

        bool running = true;
        while (!window.ShouldClose() && running)
        {
            timer.Update();
            float delta = timer.GetDelta();

            Binax::Input::Update();
            window.Update();

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

            if (Binax::Input::IsKeyPressed(GLFW_KEY_ESCAPE))
                window.SetShouldClose(true);

            // ---- Sync EditorCamera <-> ECS ----
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

            // ---- Render ----
            renderer.ClearGBuffer();

            Binax::RenderSystem::BuildFrameData(registry, frameData);

            renderer.GeometryPass(frameData,
                                  editorCamera.GetViewMatrix(),
                                  editorCamera.GetProjectionMatrix());

            collectedLights.clear();
            Binax::RenderSystem::CollectLights(registry, collectedLights);

            Binax::SkyboxParams skyboxParams = GatherSkyboxParams(registry);

            const glm::mat4 viewProj    = editorCamera.GetProjectionMatrix()
                                        * editorCamera.GetViewMatrix();
            const glm::mat4 invViewProj = glm::inverse(viewProj);

            renderer.LightingPass(collectedLights,
                                  editorCamera.GetPosition(),
                                  invViewProj,
                                  skyboxParams,
                                  viewport.GetFBO(),
                                  (uint32_t)viewport.GetSize().x,
                                  (uint32_t)viewport.GetSize().y,
                                  frameFXAA);

            // ---- UI ----
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, window.GetWidth(), window.GetHeight());
            glClear(GL_COLOR_BUFFER_BIT);

            editorUI.Draw();

            window.SwapBuffers();
        }

        BINAX_LOG_INFO("Shutting down...");

        // [FIX] Порядок важен: сначала освобождаем ВСЕ GPU-ресурсы, что
        //       держат GL-объекты, пока контекст ещё валиден. Раньше
        //       AssetRegistry (текстуры + кубемапы) очищался неявно в
        //       статическом деструкторе уже после glfwTerminate() — 
        //       glDeleteTextures без контекста = UB.
        Binax::InstanceBuffer::Get().Clear();
        Binax::AssetRegistry::Get().Clear();   // [FIX]
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
