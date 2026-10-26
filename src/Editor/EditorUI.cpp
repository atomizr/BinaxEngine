#include <glad/glad.h>
#include "EditorUI.h"
#include "Viewport.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include "Core/Input.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/MeshComponent.h"
#include "ECS/Components/MaterialComponent.h"
#include "ECS/Components/LightComponent.h"
#include "ECS/Components/CameraComponent.h"
#include "ECS/Components/NameComponent.h"
#include "ECS/Components/EditorCameraComponent.h"
#include "ECS/Components/SystemEntityTag.h"
#include "ECS/Components/SkyboxComponent.h"
#include "ECS/Components/PostProcessComponent.h"
#include "ECS/Components/ParentComponent.h"
#include "ECS/Systems/TransformSystem.h"
#include "Renderer/Mesh.h"
#include "Renderer/MeshLibrary.h"
#include "Renderer/MaterialLibrary.h"
#include "Utils/Log.h"
#include "Serialization/SceneSerializer.h"
#include "Serialization/MaterialSerializer.h"
#include "Serialization/PostProcessSerializer.h"
#include "Utils/AssetPaths.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <filesystem>
#include <fstream>
#include "ImGuizmo.h"
#include "imgui_internal.h"
#include "Reflect/Reflection.h"
#include "Reflect/ComponentRegistry.h"
#include "Reflect/EngineReflection.h"
#include "Editor/PropertyWidgets.h"
#include "Serialization/PropertySerializer.h"
#include "Utils/FileIO.h"
#include "Utils/Paths.h"
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace {



std::string PathToUtf8(const std::filesystem::path& p)
{
    return Binax::Paths::ToUtf8(p);
}

std::filesystem::path PathFromUtf8(const std::string& s)
{
    return Binax::Paths::FromUtf8(s);
}

std::string ToLowerAscii(const std::string& s)
{
    std::string out = s;
    for (char& c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

} 

namespace Binax {

EditorUI::EditorUI(GLFWwindow* window, entt::registry& registry)
    : m_Registry(registry)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    ApplyQtGrayStyle();
    LoadEditorFont();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    InitComponentFactory();
}

EditorUI::~EditorUI()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void EditorUI::InitComponentFactory()
{
    Reflect::RegisterEngineComponents();
}





void EditorUI::AddComponentToEntity(entt::entity entity, const std::string& componentName)
{
    if (IsEditorCamera(entity) && componentName != "EditorCameraComponent")
    {
        BINAX_LOG_WARN("Cannot add '{}' to EditorCamera", componentName);
        return;
    }
    if (m_Registry.any_of<SkyboxComponent>(entity))
    {
        BINAX_LOG_WARN("Cannot add '{}' to Skybox", componentName);
        return;
    }
    
    
    if (componentName == "Post Processing")
    {
        auto ppView = m_Registry.view<PostProcessComponent>();
        if (ppView.begin() != ppView.end())
        {
            BINAX_LOG_WARN("Post Processing config already exists — only one is allowed");
            return;
        }
    }

    const auto* entry = Reflect::ComponentRegistry::Get().Find(componentName);
    if (!entry)
    {
        BINAX_LOG_WARN("Unknown component '{}'", componentName);
        return;
    }
    entry->AddDefault(m_Registry, entity);
    BINAX_LOG_INFO("Added '{}' to entity {}", componentName, (uint32_t)entity);
}

void EditorUI::RemoveComponentFromEntity(entt::entity entity, const std::string& componentName)
{
    if (IsEditorCamera(entity))
    {
        if (componentName == "Transform" ||
            componentName == "EditorCameraComponent" ||
            componentName == "Camera Component")
        {
            BINAX_LOG_WARN("Cannot remove '{}' from EditorCamera", componentName);
            return;
        }
    }
    if (m_Registry.any_of<SkyboxComponent>(entity))
    {
        BINAX_LOG_WARN("Cannot remove '{}' from Skybox", componentName);
        return;
    }
    if (componentName == "Transform" || componentName == "Name" ||
        componentName == "Post Processing")
    {
        BINAX_LOG_WARN("Cannot remove '{}'", componentName);
        return;
    }

    const auto* entry = Reflect::ComponentRegistry::Get().Find(componentName);
    if (!entry || !entry->Has(m_Registry, entity))
        return;

    entry->Remove(m_Registry, entity);
    BINAX_LOG_INFO("Removed '{}' from entity {}", componentName, (uint32_t)entity);
}





void EditorUI::SaveScene()
{
    
    
    
    const std::string materialDir = Paths::Resolve(AssetFiles::kMaterialsDir);
    const std::string levelPath   = Paths::Resolve(AssetFiles::kLevelFile);
    const std::string ppPath      = Paths::Resolve(AssetFiles::kPostProcessFile);

    MaterialSerializer::SaveAll(MaterialLibrary::Get(), materialDir);
    SceneSerializer::Serialize(m_Registry, levelPath);
    PostProcessSerializer::Save(m_Registry, ppPath);
}

void EditorUI::LoadScene()
{
    m_SelectedEntity = entt::null;

    m_EditingMaterial    = kInvalidMaterial;
    m_ShowMaterialEditor = false;

    const std::string materialDir = Paths::Resolve(AssetFiles::kMaterialsDir);
    const std::string levelPath   = Paths::Resolve(AssetFiles::kLevelFile);
    const std::string ppPath      = Paths::Resolve(AssetFiles::kPostProcessFile);

    std::error_code ec;
    if (!std::filesystem::exists(Paths::FromUtf8(levelPath), ec) || ec)
    {
        BINAX_LOG_WARN("Level file not found: {}", levelPath);
        return;
    }

    
    MaterialSerializer::LoadAll(MaterialLibrary::Get(), materialDir);

    if (!SceneSerializer::Deserialize(m_Registry, levelPath))
    {
        BINAX_LOG_WARN("Level load failed — the registry may be partially updated");
        return;
    }

    
    if (std::filesystem::exists(Paths::FromUtf8(ppPath), ec) && !ec)
        PostProcessSerializer::Load(m_Registry, ppPath);

    
    {
        auto skyView = m_Registry.view<SkyboxComponent>();
        if (skyView.begin() == skyView.end())
        {
            auto e = m_Registry.create();
            m_Registry.emplace<NameComponent>(e, "Skybox");
            m_Registry.emplace<SkyboxComponent>(e);
            BINAX_LOG_INFO("Skybox recreated after level load");
        }
    }

    
    
    {
        auto ppView = m_Registry.view<PostProcessComponent>();
        if (ppView.begin() == ppView.end())
        {
            auto e = m_Registry.create();
            m_Registry.emplace<NameComponent>(e, "PostProcessingCfg");
            m_Registry.emplace<Transform>(e);
            m_Registry.emplace<PostProcessComponent>(e);
            m_Registry.emplace<SystemEntityTag>(e);
            BINAX_LOG_INFO("PostProcessingCfg recreated after level load");
        }
    }

    BINAX_LOG_INFO("Level loaded");
}





void EditorUI::DeleteEntity(entt::entity entity)
{
    
    
    if (!m_Registry.valid(entity))
    {
        BINAX_LOG_WARN("Entity {0} is not valid", (uint32_t)entity);
        return;
    }
    if (m_Registry.any_of<SystemEntityTag>(entity))
    {
        BINAX_LOG_WARN("Cannot delete system entity (tagged SystemEntityTag)");
        return;
    }
    
    
    
    
    if (IsProtectedEntity(entity))
    {
        BINAX_LOG_WARN("Cannot delete system entity (camera / skybox / post-process)");
        return;
    }

    BINAX_LOG_INFO("Deleting entity {0} (name: {1})",
                   (uint32_t)entity, GetEntityName(entity));

    
    
    const std::vector<entt::entity> subtree = CollectSubtree(entity);

    uint32_t destroyed = 0;
    for (auto e : subtree)
    {
        if (!m_Registry.valid(e))
            continue;
        if (m_Registry.any_of<SystemEntityTag>(e) || IsProtectedEntity(e))
            continue;

        if (m_SelectedEntity == e)
            m_SelectedEntity = entt::null;

        m_Registry.destroy(e);
        ++destroyed;
    }

    BINAX_LOG_INFO("Entity {0} destroyed (with subtree: {1} entities)",
                   (uint32_t)entity, destroyed);
}

std::vector<entt::entity> EditorUI::CollectSubtree(entt::entity root) const
{
    std::vector<entt::entity> out;
    if (!m_Registry.valid(root))
        return out;

    out.push_back(root);
    std::unordered_set<entt::entity> seen{ root };

    for (size_t i = 0; i < out.size(); ++i)
    {
        auto view = m_Registry.view<ParentComponent>();
        for (auto e : view)
        {
            if (view.get<ParentComponent>(e).Parent == out[i] && seen.insert(e).second)
                out.push_back(e);
        }
    }

    return out;
}

bool EditorUI::IsDescendantOf(entt::entity candidate, entt::entity ancestor) const
{
    int guard = 0;
    while (candidate != entt::null && m_Registry.valid(candidate) && guard++ < 256)
    {
        const auto* pc = m_Registry.try_get<ParentComponent>(candidate);
        if (!pc || pc->Parent == entt::null)
            return false;
        if (pc->Parent == ancestor)
            return true;
        candidate = pc->Parent;
    }
    return false;
}

void EditorUI::SetParent(entt::entity child, entt::entity parent)
{
    if (child == entt::null || !m_Registry.valid(child) || child == parent)
        return;
    if (!m_Registry.any_of<Transform>(child) || IsProtectedEntity(child))
        return;

    if (parent != entt::null)
    {
        if (!m_Registry.valid(parent) || !m_Registry.any_of<Transform>(parent))
            return;
        if (IsProtectedEntity(parent))
        {
            BINAX_LOG_WARN("Cannot parent to a system entity");
            return;
        }
        if (IsDescendantOf(parent, child))
        {
            BINAX_LOG_WARN("Cannot parent: would create a cycle");
            return;
        }
    }

    
    const glm::mat4 worldBefore = TransformSystem::GetWorldMatrix(m_Registry, child);

    if (parent == entt::null)
        m_Registry.remove<ParentComponent>(child);
    else
        m_Registry.get_or_emplace<ParentComponent>(child).Parent = parent;

    const glm::mat4 parentWorld = (parent == entt::null)
                                      ? glm::mat4(1.0f)
                                      : TransformSystem::GetWorldMatrix(m_Registry, parent);

    const glm::mat4 local = glm::inverse(parentWorld) * worldBefore;

    glm::vec3 scale, translation, skew;
    glm::vec4 perspective;
    glm::quat rotation;
    if (!glm::decompose(local, scale, rotation, translation, skew, perspective))
        return;   

    if (!std::isfinite(translation.x) || !std::isfinite(translation.y) ||
        !std::isfinite(translation.z))
        return;

    auto clampScale = [](float s)
    {
        constexpr float kMin = 1e-4f;
        if (!std::isfinite(s) || std::abs(s) < kMin)
            return (s < 0.0f) ? -kMin : kMin;
        return s;
    };
    scale.x = clampScale(scale.x);
    scale.y = clampScale(scale.y);
    scale.z = clampScale(scale.z);

    const float qlen2 = rotation.w * rotation.w + rotation.x * rotation.x +
                        rotation.y * rotation.y + rotation.z * rotation.z;

    Transform& tr = m_Registry.get<Transform>(child);
    tr.Position = translation;
    tr.Rotation = (qlen2 > 1e-12f) ? glm::normalize(rotation)
                                   : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    tr.Scale    = scale;

    BINAX_LOG_INFO("Reparented '{0}' -> '{1}'",
                   GetEntityName(child),
                   (parent == entt::null) ? std::string("<root>") : GetEntityName(parent));
}

bool EditorUI::IsEditorCamera(entt::entity entity) const
{
    return m_Registry.any_of<EditorCameraComponent>(entity);
}

bool EditorUI::IsProtectedEntity(entt::entity entity) const
{
    return m_Registry.any_of<EditorCameraComponent>(entity) ||
           m_Registry.any_of<SkyboxComponent>(entity) ||
           m_Registry.any_of<PostProcessComponent>(entity);
}

std::string EditorUI::GetEntityName(entt::entity entity) const
{
    if (m_Registry.any_of<NameComponent>(entity))
        return m_Registry.get<NameComponent>(entity).Name;
    return "Entity " + std::to_string((uint32_t)entity);
}

void EditorUI::HandleShortcuts()
{
    
    
    if (ImGui::GetIO().WantTextInput || ImGui::IsAnyItemActive()) return;

    
    
    
    if (PropertyWidgets::IsFileBrowserOpen()) return;

    const bool ctrl = Input::IsKeyPressed(GLFW_KEY_LEFT_CONTROL) ||
                      Input::IsKeyPressed(GLFW_KEY_RIGHT_CONTROL);

    if (ctrl && Input::IsKeyJustPressed(GLFW_KEY_D))
    {
        if (m_SelectedEntity != entt::null && m_Registry.valid(m_SelectedEntity))
            DuplicateEntity(m_SelectedEntity);
    }

    if (Input::IsKeyJustPressed(GLFW_KEY_DELETE))
    {
        if (m_SelectedEntity != entt::null && m_Registry.valid(m_SelectedEntity))
            DeleteEntity(m_SelectedEntity);
    }

    
    if (ctrl && Input::IsKeyJustPressed(GLFW_KEY_F))
    {
        m_FocusSearch = true;
    }
    else if (!ctrl && Input::IsKeyJustPressed(GLFW_KEY_F))
    {
        FocusOnSelection();
    }
}

void EditorUI::FocusOnSelection()
{
    entt::entity camEntity = entt::null;
    auto camView = m_Registry.view<Transform, EditorCameraComponent, CameraComponent>();
    for (auto e : camView)
    {
        camEntity = e;
        break;
    }
    if (camEntity == entt::null)
        return;

    Transform& camTransform = m_Registry.get<Transform>(camEntity);
    const CameraComponent& camera = m_Registry.get<CameraComponent>(camEntity);

    
    
    glm::vec3 boundsMin(0.0f);
    glm::vec3 boundsMax(0.0f);
    bool hasBounds = false;

    if (m_SelectedEntity != entt::null && m_Registry.valid(m_SelectedEntity))
    {
        for (auto e : CollectSubtree(m_SelectedEntity))
        {
            if (!m_Registry.valid(e) || !m_Registry.any_of<MeshComponent>(e))
                continue;

            const MeshHandle h = m_Registry.get<MeshComponent>(e).MeshId;
            if (!MeshLibrary::Get().IsValid(h))
                continue;

            const MeshGPU& mesh = MeshLibrary::Get().Get(h);
            const glm::mat4 world = TransformSystem::GetWorldMatrix(m_Registry, e);

            for (int corner = 0; corner < 8; ++corner)
            {
                const glm::vec3 local(
                    (corner & 1) ? mesh.AABBMax.x : mesh.AABBMin.x,
                    (corner & 2) ? mesh.AABBMax.y : mesh.AABBMin.y,
                    (corner & 4) ? mesh.AABBMax.z : mesh.AABBMin.z);
                const glm::vec3 p = glm::vec3(world * glm::vec4(local, 1.0f));

                if (!hasBounds)
                {
                    boundsMin = boundsMax = p;
                    hasBounds = true;
                }
                else
                {
                    boundsMin = glm::min(boundsMin, p);
                    boundsMax = glm::max(boundsMax, p);
                }
            }
        }
    }

    glm::vec3 target(0.0f);
    float radius = 0.5f;

    if (hasBounds)
    {
        target = (boundsMin + boundsMax) * 0.5f;
        radius = glm::max(glm::length(boundsMax - boundsMin) * 0.5f, 0.05f);
    }
    else if (m_SelectedEntity != entt::null && m_Registry.valid(m_SelectedEntity))
    {
        target = glm::vec3(TransformSystem::GetWorldMatrix(m_Registry, m_SelectedEntity)[3]);
    }

    
    
    const glm::vec3 forward = camTransform.Rotation * glm::vec3(0.0f, 0.0f, -1.0f);
    const float fovRad = glm::radians(glm::clamp(camera.FOV, 1.0f, 179.0f));
    const float distance = radius / glm::max(glm::tan(fovRad * 0.5f), 0.01f) * 1.8f;

    camTransform.Position = target - forward * glm::max(distance, 0.1f);

    BINAX_LOG_INFO("Focus camera on '{0}'",
                   (m_SelectedEntity == entt::null)
                       ? std::string("<origin>")
                       : GetEntityName(m_SelectedEntity));
}

bool EditorUI::IsNameTaken(const std::string& name) const
{
    auto view = m_Registry.view<NameComponent>();
    for (auto entity : view)
    {
        if (view.get<NameComponent>(entity).Name == name)
            return true;
    }
    return false;
}

std::string EditorUI::MakeUniqueName(const std::string& sourceName) const
{
    std::string base = sourceName;

    
    
    auto stripNumericSuffix = [](std::string& s, char separator)
    {
        const size_t pos = s.find_last_of(separator);
        if (pos == std::string::npos || pos == 0 || pos + 1 >= s.size())
            return;
        for (size_t i = pos + 1; i < s.size(); ++i)
        {
            if (!std::isdigit(static_cast<unsigned char>(s[i])))
                return;
        }
        s = s.substr(0, pos);
    };

    stripNumericSuffix(base, '_');
    stripNumericSuffix(base, ' ');

    if (base.empty())
        base = "Object";

    for (int index = 1; index < 100000; ++index)
    {
        std::string candidate = base + "_" + std::to_string(index);
        if (!IsNameTaken(candidate))
            return candidate;
    }

    return base;
}





void EditorUI::Draw()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuizmo::BeginFrame();

    HandleShortcuts();

    DrawViewport();
    DrawHierarchy();
    DrawInspector();

    if (m_ShowMaterialEditor && m_EditingMaterial != kInvalidMaterial)
        DrawMaterialEditor();

    PropertyWidgets::DrawFileBrowser();

    for (auto& cb : m_ExtraPanelDrawers)
        if (cb) cb();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}





void EditorUI::SyncViewportSize()
{
    if (!m_Viewport)
        return;

    const uint32_t w = static_cast<uint32_t>(m_PendingViewportW);
    const uint32_t h = static_cast<uint32_t>(m_PendingViewportH);
    if (w == 0 || h == 0)
        return;

    const glm::vec2 current = m_Viewport->GetSize();
    if (static_cast<uint32_t>(current.x) != w ||
        static_cast<uint32_t>(current.y) != h)
    {
        m_Viewport->Resize(w, h);
    }
}

void EditorUI::DrawViewport()
{
    if (!m_Viewport) return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar
                           | ImGuiWindowFlags_NoScrollWithMouse
                           | ImGuiWindowFlags_NoMove
                           | ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("Viewport", nullptr, flags))
    {
        
        auto OpButton = [&](const char* label, int op)
        {
            const bool active = (m_GizmoOperation == op);
            if (active)
            {
                ImGui::PushStyleColor(ImGuiCol_Button,
                                      ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            }
            if (ImGui::Button(label))
                m_GizmoOperation = op;
            if (active)
                ImGui::PopStyleColor();
        };

        OpButton("Move", 0);
        ImGui::SameLine();
        OpButton("Rotate", 1);
        ImGui::SameLine();
        OpButton("Scale", 2);
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        if (ImGui::Button(m_GizmoMode == 0 ? "World" : "Local"))
            m_GizmoMode = (m_GizmoMode == 0) ? 1 : 0;

        
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::Checkbox("Snap", &m_SnapEnabled);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Grid snapping for the current gizmo mode");

        if (m_SnapEnabled)
        {
            float& step = SnapStep();
            const bool rotation = (m_GizmoOperation == 1);

            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            ImGui::DragFloat("##SnapStep", &step,
                             rotation ? 0.5f : 0.01f,
                             rotation ? 1.0f : 0.01f,
                             rotation ? 180.0f : 100.0f,
                             rotation ? "%.1f" : "%.2f");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Snap step: units (move), degrees (rotate), factor (scale)");
            ImGui::SameLine();
            ImGui::TextDisabled(rotation ? "deg" : (m_GizmoOperation == 2 ? "x" : "units"));
        }

        ImGui::Separator();

        ImVec2 vpPos  = ImGui::GetCursorScreenPos();
        ImVec2 vpSize = ImGui::GetContentRegionAvail();

        m_ViewportScreenX = vpPos.x;
        m_ViewportScreenY = vpPos.y;
        m_ViewportScreenW = vpSize.x;
        m_ViewportScreenH = vpSize.y;

        m_ViewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

        if (vpSize.x > 0 && vpSize.y > 0)
        {
            
            
            
            
            
            
            
            
            
            
            
            const ImVec2 fbScale = ImGui::GetIO().DisplayFramebufferScale;
            m_PendingViewportW = vpSize.x * (fbScale.x > 0.0f ? fbScale.x : 1.0f);
            m_PendingViewportH = vpSize.y * (fbScale.y > 0.0f ? fbScale.y : 1.0f);
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddImage(
            reinterpret_cast<ImTextureID>((intptr_t)m_Viewport->GetTextureID()),
            vpPos,
            ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y),
            ImVec2(0.0f, 1.0f),
            ImVec2(1.0f, 0.0f));

        ImGui::Dummy(vpSize);

        if (m_ViewportHovered || ImGuizmo::IsUsing())
            DrawGizmo();
    }
    else
    {
        m_ViewportHovered = false;
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

float& EditorUI::SnapStep()
{
    switch (m_GizmoOperation)
    {
    case 1:  return m_SnapRotate;    
    case 2:  return m_SnapScale;     
    default: return m_SnapTranslate; 
    }
}

void EditorUI::DrawGizmo()
{
    if (m_SelectedEntity == entt::null || !m_Registry.valid(m_SelectedEntity))
        return;
    if (!m_Registry.any_of<Transform>(m_SelectedEntity))
        return;
    if (IsEditorCamera(m_SelectedEntity))
        return;

    ImGuiContext& g = *ImGui::GetCurrentContext();
    ImGuiWindow* win = ImGui::GetCurrentWindow();

    const bool overGizmo    = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
    const bool activeIsMove = (g.ActiveId != 0) && (g.ActiveId == win->MoveId);

    if (activeIsMove && overGizmo)
        ImGui::ClearActiveID();

    if (m_ViewportHovered)
    {
        g.HoveredWindow = win;
        g.HoveredWindowUnderMovingWindow = win;
    }

    const bool rmbHeld    = Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
    const bool textInput  = ImGui::GetIO().WantTextInput;
    const bool gizmoUsing = ImGuizmo::IsUsing();

    if (!rmbHeld && !textInput && !gizmoUsing)
    {
        if (Input::IsKeyJustPressed(GLFW_KEY_W)) m_GizmoOperation = 0;
        if (Input::IsKeyJustPressed(GLFW_KEY_E)) m_GizmoOperation = 1;
        if (Input::IsKeyJustPressed(GLFW_KEY_R)) m_GizmoOperation = 2;
        if (Input::IsKeyJustPressed(GLFW_KEY_X))
            m_GizmoMode = (m_GizmoMode == 0) ? 1 : 0;
    }

    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(m_ViewportScreenX, m_ViewportScreenY,
                      m_ViewportScreenW, m_ViewportScreenH);

    auto& transform = m_Registry.get<Transform>(m_SelectedEntity);

    
    
    glm::mat4 parentWorld = glm::mat4(1.0f);
    if (const auto* pc = m_Registry.try_get<ParentComponent>(m_SelectedEntity))
    {
        if (pc->Parent != entt::null && m_Registry.valid(pc->Parent))
            parentWorld = TransformSystem::GetWorldMatrix(m_Registry, pc->Parent);
    }

    glm::mat4 model = TransformSystem::GetWorldMatrix(m_Registry, m_SelectedEntity);

    ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
    switch (m_GizmoOperation)
    {
        case 0: op = ImGuizmo::TRANSLATE; break;
        case 1: op = ImGuizmo::ROTATE;    break;
        case 2: op = ImGuizmo::SCALE;     break;
    }
    const ImGuizmo::MODE mode = (m_GizmoMode == 0) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;

    const bool manipulated = ImGuizmo::Manipulate(
        glm::value_ptr(m_ViewMatrix),
        glm::value_ptr(m_ProjectionMatrix),
        op, mode, glm::value_ptr(model));

    if (manipulated)
    {
        glm::mat4 local = glm::inverse(parentWorld) * model;

        glm::vec3 translation, scale, skew;
        glm::vec4 perspective;
        glm::quat rotation;
        glm::decompose(local, scale, rotation, translation, skew, perspective);

        
        
        
        if (m_SnapEnabled)
        {
            const float step = SnapStep();
            if (step > 1e-5f)
            {
                if (m_GizmoOperation == 0)
                {
                    glm::vec3 worldPos(model[3]);
                    worldPos = glm::round(worldPos / step) * step;

                    glm::mat4 snappedWorld = model;
                    snappedWorld[3] = glm::vec4(worldPos, 1.0f);

                    const glm::mat4 snappedLocal =
                        glm::inverse(parentWorld) * snappedWorld;
                    glm::decompose(snappedLocal, scale, rotation, translation,
                                   skew, perspective);
                }
                else if (m_GizmoOperation == 1)
                {
                    const glm::vec3 euler = glm::degrees(glm::eulerAngles(rotation));
                    rotation = glm::quat(glm::radians(glm::round(euler / step) * step));
                }
                else
                {
                    const glm::vec3 snapped = glm::round(scale / step) * step;
                    auto keepNonZero = [step](float v)
                    {
                        
                        if (std::abs(v) < step)
                            return (v < 0.0f) ? -step : step;
                        return v;
                    };
                    scale.x = keepNonZero(snapped.x);
                    scale.y = keepNonZero(snapped.y);
                    scale.z = keepNonZero(snapped.z);
                }
            }
        }

        
        
        
        constexpr float kMinScale = 1e-4f;
        auto clampScale = [&](float s)
        {
            if (!std::isfinite(s) || std::abs(s) < kMinScale)
                return (s < 0.0f) ? -kMinScale : kMinScale;
            return s;
        };
        scale.x = clampScale(scale.x);
        scale.y = clampScale(scale.y);
        scale.z = clampScale(scale.z);

        if (std::isfinite(translation.x) && std::isfinite(translation.y) &&
            std::isfinite(translation.z))
            transform.Position = translation;

        
        
        const float qlen2 = rotation.w * rotation.w + rotation.x * rotation.x +
                            rotation.y * rotation.y + rotation.z * rotation.z;
        transform.Rotation = (qlen2 > 1e-12f) ? glm::normalize(rotation)
                                              : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        transform.Scale    = scale;
    }
}





void EditorUI::DrawCreateObjectDropdown()
{
    if (ImGui::Button("Create Object"))
        ImGui::OpenPopup("CreateObjectPopup");

    if (ImGui::BeginPopup("CreateObjectPopup"))
    {
        if (ImGui::MenuItem("Empty Object")) CreateEmptyEntity();
        if (ImGui::MenuItem("3D Cube"))      CreateCubeEntity();
        ImGui::EndPopup();
    }
}

void EditorUI::DrawHierarchy()
{
    ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoCollapse);

    if (ImGui::Button("Save Scene")) SaveScene();
    ImGui::SameLine();
    if (ImGui::Button("Load Scene")) LoadScene();

    DrawCreateObjectDropdown();
    ImGui::Separator();

    
    if (m_FocusSearch)
    {
        ImGui::SetKeyboardFocusHere();
        m_FocusSearch = false;
    }
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x -
                            (m_HierarchySearch[0] ? 24.0f : 0.0f));
    ImGui::InputTextWithHint("##HierarchySearch", "Search...",
                             m_HierarchySearch, IM_ARRAYSIZE(m_HierarchySearch));
    if (m_HierarchySearch[0] != '\0')
    {
        ImGui::SameLine();
        if (ImGui::SmallButton("X"))
            m_HierarchySearch[0] = '\0';
    }
    ImGui::Separator();

    if (m_HierarchySearch[0] != '\0')
    {
        const std::string filter = ToLowerAscii(m_HierarchySearch);
        bool anyMatch = false;

        for (auto entity : m_Registry.view<NameComponent>())
        {
            const std::string& name = m_Registry.get<NameComponent>(entity).Name;
            if (ToLowerAscii(name).find(filter) == std::string::npos)
                continue;

            anyMatch = true;
            ImGui::PushID(static_cast<int>(entity));
            if (ImGui::Selectable(name.c_str(), m_SelectedEntity == entity))
                m_SelectedEntity = entity;
            ImGui::PopID();
        }

        if (!anyMatch)
            ImGui::TextDisabled("No matches");
    }
    else
    {
        
        
        std::unordered_map<entt::entity, std::vector<entt::entity>> children;
        std::vector<entt::entity> roots;

        for (auto entity : m_Registry.view<NameComponent>())
        {
            const auto* pc = m_Registry.try_get<ParentComponent>(entity);
            if (pc && pc->Parent != entt::null && pc->Parent != entity &&
                m_Registry.valid(pc->Parent) &&
                m_Registry.any_of<NameComponent>(pc->Parent))
            {
                children[pc->Parent].push_back(entity);
            }
            else
            {
                roots.push_back(entity);
            }
        }

        std::unordered_set<entt::entity> visited;
        for (auto entity : roots)
            DrawEntityRow(entity, children, visited);

        
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::Dummy(ImVec2(avail.x > 1.0f ? avail.x : 1.0f, 28.0f));
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BINAX_ENTITY"))
            {
                const auto dragged = *static_cast<const entt::entity*>(payload->Data);
                SetParent(dragged, entt::null);
            }
            ImGui::EndDragDropTarget();
        }
    }

    
    if (m_PendingDuplicate != entt::null)
    {
        const entt::entity e = m_PendingDuplicate;
        m_PendingDuplicate = entt::null;
        DuplicateEntity(e);
    }
    else if (m_PendingDelete != entt::null)
    {
        const entt::entity e = m_PendingDelete;
        m_PendingDelete = entt::null;
        DeleteEntity(e);
    }

    ImGui::End();
}

void EditorUI::DrawEntityRow(
    entt::entity entity,
    const std::unordered_map<entt::entity, std::vector<entt::entity>>& children,
    std::unordered_set<entt::entity>& visited)
{
    if (!m_Registry.valid(entity) || !m_Registry.any_of<NameComponent>(entity))
        return;
    if (!visited.insert(entity).second)
        return;   

    const auto childIt = children.find(entity);
    const bool hasChildren = (childIt != children.end() && !childIt->second.empty());

    ImGui::PushID(static_cast<int>(entity));

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                               ImGuiTreeNodeFlags_SpanAvailWidth |
                               ImGuiTreeNodeFlags_DefaultOpen;
    if (m_SelectedEntity == entity)
        flags |= ImGuiTreeNodeFlags_Selected;
    if (!hasChildren)
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    const bool isProtected = IsProtectedEntity(entity);

    const bool open = ImGui::TreeNodeEx("##node", flags, "%s",
                                        GetEntityName(entity).c_str());

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        m_SelectedEntity = entity;

    
    if (!isProtected && ImGui::BeginDragDropSource())
    {
        ImGui::SetDragDropPayload("BINAX_ENTITY", &entity, sizeof(entt::entity));
        ImGui::TextUnformatted(GetEntityName(entity).c_str());
        ImGui::EndDragDropSource();
    }

    
    if (!isProtected && ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BINAX_ENTITY"))
        {
            const auto dragged = *static_cast<const entt::entity*>(payload->Data);
            if (dragged != entity)
                SetParent(dragged, entity);
        }
        ImGui::EndDragDropTarget();
    }

    if (ImGui::BeginPopupContextItem())
    {
        if (!isProtected)
        {
            if (ImGui::MenuItem("Duplicate Object")) m_PendingDuplicate = entity;
            if (ImGui::MenuItem("Delete Object"))    m_PendingDelete = entity;
            if (m_Registry.any_of<ParentComponent>(entity))
            {
                if (ImGui::MenuItem("Unparent")) SetParent(entity, entt::null);
            }
        }
        else
        {
            ImGui::Text("System object - cannot modify");
        }
        ImGui::EndPopup();
    }

    if (open && hasChildren)
    {
        for (auto child : childIt->second)
            DrawEntityRow(child, children, visited);
        ImGui::TreePop();
    }

    ImGui::PopID();
}

void EditorUI::DrawInspector()
{
    ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoCollapse);

    if (m_SelectedEntity == entt::null || !m_Registry.valid(m_SelectedEntity))
    {
        ImGui::TextUnformatted("No entity selected");
        ImGui::End();
        return;
    }

    const bool isCamera = IsEditorCamera(m_SelectedEntity);
    const bool isSkybox = m_Registry.any_of<SkyboxComponent>(m_SelectedEntity);

    std::string pendingRemove;

    for (const auto& entryPtr : Reflect::ComponentRegistry::Get().All())
    {
        const auto& entry = *entryPtr;
        if (!entry.Has(m_Registry, m_SelectedEntity)) continue;

        ImGui::PushID(entry.TypeId);
        ImGui::TextUnformatted(entry.Name.c_str());

        const bool isTransform = (entry.Name == "Transform");
        const bool isName      = (entry.Name == "Name");
        const bool canRemove   = !isCamera && !isSkybox && !isTransform && !isName;

        if (canRemove)
        {
            
            
            
            const float buttonW = ImGui::CalcTextSize("Remove").x +
                                  ImGui::GetStyle().FramePadding.x * 2.0f;
            ImGui::SameLine(ImGui::GetContentRegionMax().x - buttonW);
            if (ImGui::SmallButton("Remove"))
                pendingRemove = entry.Name;
        }

        ImGui::Indent();

        void* obj = entry.GetPtr(m_Registry, m_SelectedEntity);
        const Reflect::ClassInfo* info = Reflect::TypeRegistry::Get().Find(entry.TypeId);
        if (obj && info)
        {
            for (const auto& prop : info->Properties)
                PropertyWidgets::Draw(obj, prop);
        }

        if (entry.Name == "Material" &&
            m_Registry.any_of<MaterialComponent>(m_SelectedEntity))
        {
            auto& mc  = m_Registry.get<MaterialComponent>(m_SelectedEntity);
            auto& lib = MaterialLibrary::Get();

            
            const std::string current = lib.IsValid(mc.MaterialId)
                                            ? lib.GetName(mc.MaterialId)
                                            : std::string("(none)");

            ImGui::SetNextItemWidth(220.0f);
            if (ImGui::BeginCombo("Material Asset", current.c_str()))
            {
                const auto& editData = lib.GetEditData();
                for (uint32_t i = 0; i < (uint32_t)editData.size(); ++i)
                {
                    const bool selected = (mc.MaterialId == i);
                    if (ImGui::Selectable(editData[i].Name.c_str(), selected))
                        mc.MaterialId = i;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            if (lib.IsValid(mc.MaterialId))
            {
                if (ImGui::SmallButton("Edit Material..."))
                {
                    m_EditingMaterial    = mc.MaterialId;
                    m_ShowMaterialEditor = true;
                }
            }
        }

        ImGui::Unindent();
        ImGui::Separator();
        ImGui::PopID();

        if (!pendingRemove.empty()) break;
    }

    if (!pendingRemove.empty())
        RemoveComponentFromEntity(m_SelectedEntity, pendingRemove);

    if (!isCamera && !isSkybox)
        DrawAddComponentMenu();

    ImGui::End();
}

void EditorUI::DrawAddComponentMenu()
{
    if (ImGui::Button("Add Component"))
        ImGui::OpenPopup("AddComponentPopup");

    if (ImGui::BeginPopup("AddComponentPopup"))
    {
        for (const auto& entryPtr : Reflect::ComponentRegistry::Get().All())
        {
            const auto& entry = *entryPtr;
            if (entry.Hidden) continue;
            if (entry.Has(m_Registry, m_SelectedEntity)) continue;
            if (entry.Name == "Skybox") continue;
            if (entry.Name == "EditorCameraComponent") continue;

            if (ImGui::MenuItem(entry.Name.c_str()))
                AddComponentToEntity(m_SelectedEntity, entry.Name);
        }
        ImGui::EndPopup();
    }
}





void EditorUI::DrawMaterialEditor()
{
    
    
    
    if (!MaterialLibrary::Get().IsValid(m_EditingMaterial))
    {
        m_ShowMaterialEditor = false;
        m_EditingMaterial = kInvalidMaterial;
        return;
    }

    std::string title = "Material: " + MaterialLibrary::Get().GetName(m_EditingMaterial);

    bool open = true;
    ImGui::SetNextWindowSize(ImVec2(480, 620), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(title.c_str(), &open))
    {
        MaterialGPU&  m  = MaterialLibrary::Get().GetMutable(m_EditingMaterial);
        const auto&   ed = MaterialLibrary::Get().GetEdit(m_EditingMaterial);
        const MaterialHandle h = m_EditingMaterial;

        if (ImGui::CollapsingHeader("Base", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::ColorEdit3("Albedo", &m.AlbedoRoughness.x))
                MaterialLibrary::Get().MarkDirty();
            if (ImGui::SliderFloat("Roughness", &m.AlbedoRoughness.w, 0.04f, 1.0f))
                MaterialLibrary::Get().MarkDirty();
            if (ImGui::SliderFloat("Metallic",  &m.MetallicAOEms.x,   0.0f, 1.0f))
                MaterialLibrary::Get().MarkDirty();
            if (ImGui::SliderFloat("AO",        &m.MetallicAOEms.y,   0.0f, 1.0f))
                MaterialLibrary::Get().MarkDirty();
            if (ImGui::SliderFloat("Emissive",  &m.MetallicAOEms.z,   0.0f, 5.0f))
                MaterialLibrary::Get().MarkDirty();
            if (ImGui::SliderFloat("Tiling",    &m.MetallicAOEms.w,   0.01f, 20.0f))
                MaterialLibrary::Get().MarkDirty();
        }

        if (ImGui::CollapsingHeader("Sky reflections"))
        {
            bool skyEn = (m.SkyReflParams.x > 0.5f);
            if (ImGui::Checkbox("Enabled", &skyEn))
            {
                m.SkyReflParams.x = skyEn ? 1.0f : 0.0f;
                MaterialLibrary::Get().MarkDirty();
            }
            if (ImGui::SliderFloat("Strength", &m.SkyReflParams.y, 0.0f, 5.0f))
                MaterialLibrary::Get().MarkDirty();
        }

        if (ImGui::CollapsingHeader("Strengths"))
        {
            if (ImGui::SliderFloat("Albedo Str",    &m.StrengthB.x, 0.0f, 10.0f)) MaterialLibrary::Get().MarkDirty();
            if (ImGui::SliderFloat("Normal Str",    &m.StrengthB.y, 0.0f, 10.0f)) MaterialLibrary::Get().MarkDirty();
            if (ImGui::SliderFloat("Metallic Str",  &m.StrengthA.x, 0.0f, 10.0f)) MaterialLibrary::Get().MarkDirty();
            if (ImGui::SliderFloat("Roughness Str", &m.StrengthA.y, 0.0f, 10.0f)) MaterialLibrary::Get().MarkDirty();
            if (ImGui::SliderFloat("AO Str",        &m.StrengthA.z, 0.0f, 10.0f)) MaterialLibrary::Get().MarkDirty();
            if (ImGui::SliderFloat("Emissive Str",  &m.StrengthA.w, 0.0f, 10.0f)) MaterialLibrary::Get().MarkDirty();
        }

        if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto TextureRow = [&](const char* label,
                                  const std::string& currentPath,
                                  std::function<void(const std::string&)> setter)
            {
                ImGui::PushID(label);

                ImGui::Text("%s", label);
                ImGui::SameLine(110.0f);

std::string display = currentPath.empty()
    ? "(none)"
    : PathToUtf8(PathFromUtf8(currentPath).filename());
                ImGui::TextWrapped("%s", display.c_str());
                ImGui::SameLine();

                if (ImGui::SmallButton("..."))
                {
                    PropertyWidgets::OpenFileBrowser(
                        std::string("Select ") + label,
                        { ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".hdr" },
                        [setter](const std::string& path) { setter(path); });
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("X") && !currentPath.empty())
                    setter("");

                ImGui::PopID();
            };

            TextureRow("Albedo", ed.AlbedoMapPath,
                [h](const std::string& p){ MaterialLibrary::Get().SetAlbedoMap(h, p); MaterialLibrary::Get().Flush(); });
            TextureRow("Normal", ed.NormalMapPath,
                [h](const std::string& p){ MaterialLibrary::Get().SetNormalMap(h, p); MaterialLibrary::Get().Flush(); });
            TextureRow("Metallic", ed.MetallicMapPath,
                [h](const std::string& p){ MaterialLibrary::Get().SetMetallicMap(h, p); MaterialLibrary::Get().Flush(); });
            TextureRow("Roughness", ed.RoughnessMapPath,
                [h](const std::string& p){ MaterialLibrary::Get().SetRoughnessMap(h, p); MaterialLibrary::Get().Flush(); });
            TextureRow("AO", ed.AOMapPath,
                [h](const std::string& p){ MaterialLibrary::Get().SetAOMap(h, p); MaterialLibrary::Get().Flush(); });
            TextureRow("Emissive", ed.EmissiveMapPath,
                [h](const std::string& p){ MaterialLibrary::Get().SetEmissiveMap(h, p); MaterialLibrary::Get().Flush(); });
        }

        ImGui::Separator();
        ImGui::TextDisabled("Handle: %u", m_EditingMaterial);
    }
    ImGui::End();

    if (!open)
    {
        m_ShowMaterialEditor = false;
        m_EditingMaterial = kInvalidMaterial;
    }
}





void EditorUI::CreateEmptyEntity()
{
    auto entity = m_Registry.create();
    m_Registry.emplace<Transform>(entity);
    m_Registry.emplace<NameComponent>(entity,
        "Entity " + std::to_string((uint32_t)entity));
    m_SelectedEntity = entity;
    BINAX_LOG_INFO("Created empty entity {0}", (uint32_t)entity);
}

void EditorUI::CreateCubeEntity()
{
    MeshHandle cube = MeshLibrary::Get().Find("Cube");
    if (cube == kInvalidMesh)
    {
        BINAX_LOG_ERROR("Cube mesh not registered in MeshLibrary");
        return;
    }

    int nextIndex = 1;
    auto nameView = m_Registry.view<NameComponent>();
    for (auto e : nameView)
    {
        const std::string& n = nameView.get<NameComponent>(e).Name;
        
        constexpr size_t kPrefixLen = 5;
        if (n.size() <= kPrefixLen)
            continue;
        if (n.compare(0, kPrefixLen, "Cube ") != 0 &&
            n.compare(0, kPrefixLen, "Cube_") != 0)
            continue;

        try
        {
            const int v = std::stoi(n.substr(kPrefixLen));
            if (v >= nextIndex) nextIndex = v + 1;
        }
        catch (...) {}
    }

    entt::entity entity = m_Registry.create();

    const int gridIdx = nextIndex - 1;
    const float offsetX = 1.5f * (gridIdx % 5);
    const float offsetZ = 1.5f * ((gridIdx / 5) % 5);

    m_Registry.emplace<Transform>(entity,
                                  glm::vec3(offsetX, 0.0f, offsetZ),
                                  glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                  glm::vec3(1.0f));

    m_Registry.emplace<MeshComponent>(entity, MeshComponent{ cube });

    
    
    
    
    std::string matName = "CubeMaterial_" + std::to_string(nextIndex);
    for (int suffix = 1; MaterialLibrary::Get().Find(matName) != kInvalidMaterial; ++suffix)
        matName = "CubeMaterial_" + std::to_string(nextIndex) + "_" + std::to_string(suffix);

    MaterialGPU gpu{};
    gpu.AlbedoRoughness = glm::vec4(1.0f, 0.5f, 0.2f, 0.4f);
    gpu.MetallicAOEms   = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
    gpu.SkyReflParams   = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
    gpu.StrengthA       = glm::vec4(1.0f);
    gpu.StrengthB       = glm::vec4(1.0f);
    MaterialHandle mat  = MaterialLibrary::Get().Add(matName, gpu);
    MaterialLibrary::Get().Flush();

    m_Registry.emplace<MaterialComponent>(entity, MaterialComponent{ mat });

    m_Registry.emplace<NameComponent>(entity, MakeUniqueName("Cube"));

    m_SelectedEntity = entity;
    BINAX_LOG_INFO("Created cube entity {0} at ({1:.2f}, 0, {2:.2f})",
                   (uint32_t)entity, offsetX, offsetZ);
}

entt::entity EditorUI::DuplicateSingle(entt::entity src)
{
    const entt::entity dst = m_Registry.create();

    auto snapshot = Serialization::PropertySerializer::SerializeEntity(m_Registry, src);
    Serialization::PropertySerializer::DeserializeEntity(m_Registry, dst, snapshot);

    if (m_Registry.any_of<EditorCameraComponent>(dst))
        m_Registry.remove<EditorCameraComponent>(dst);

    
    if (m_Registry.any_of<MeshComponent>(src))
    {
        if (!m_Registry.any_of<MeshComponent>(dst))
            m_Registry.emplace<MeshComponent>(dst);
        m_Registry.get<MeshComponent>(dst).MeshId =
            m_Registry.get<MeshComponent>(src).MeshId;
    }

    
    if (m_Registry.any_of<MaterialComponent>(src))
    {
        auto srcH = m_Registry.get<MaterialComponent>(src).MaterialId;
        if (MaterialLibrary::Get().IsValid(srcH))
        {
            std::string baseName = MaterialLibrary::Get().GetName(srcH);
            std::string newName  = baseName + "_copy";
            int suffix = 1;
            while (MaterialLibrary::Get().Find(newName) != kInvalidMaterial)
                newName = baseName + "_copy" + std::to_string(suffix++);

            MaterialHandle nh = MaterialLibrary::Get().Copy(newName, srcH);
            MaterialLibrary::Get().Flush();

            if (!m_Registry.any_of<MaterialComponent>(dst))
                m_Registry.emplace<MaterialComponent>(dst);
            m_Registry.get<MaterialComponent>(dst).MaterialId = nh;
        }
    }

    if (m_Registry.any_of<CameraComponent>(dst))
        m_Registry.get<CameraComponent>(dst).Primary = false;

    if (m_Registry.any_of<NameComponent>(dst))
    {
        
        m_Registry.get<NameComponent>(dst).Name =
            MakeUniqueName(m_Registry.get<NameComponent>(dst).Name);
    }

    return dst;
}

void EditorUI::DuplicateEntity(entt::entity entity)
{
    if (IsProtectedEntity(entity))
    {
        BINAX_LOG_WARN("Cannot duplicate system entity");
        return;
    }

    if (!m_Registry.valid(entity) || !m_Registry.any_of<Transform>(entity))
    {
        BINAX_LOG_WARN("Invalid entity to duplicate");
        return;
    }

    
    const std::vector<entt::entity> subtree = CollectSubtree(entity);

    std::unordered_map<entt::entity, entt::entity> remap;
    remap.reserve(subtree.size());

    for (auto src : subtree)
    {
        if (!m_Registry.valid(src))
            continue;
        remap[src] = DuplicateSingle(src);
    }

    
    for (auto src : subtree)
    {
        const auto dstIt = remap.find(src);
        if (dstIt == remap.end())
            continue;

        const entt::entity dst = dstIt->second;
        const auto* pc = m_Registry.try_get<ParentComponent>(src);
        if (!pc || pc->Parent == entt::null)
        {
            m_Registry.remove<ParentComponent>(dst);
            continue;
        }

        const auto parentIt = remap.find(pc->Parent);
        const entt::entity newParent =
            (parentIt != remap.end()) ? parentIt->second : pc->Parent;

        m_Registry.get_or_emplace<ParentComponent>(dst).Parent = newParent;
    }

    
    const auto rootIt = remap.find(entity);
    if (rootIt != remap.end())
    {
        if (m_Registry.any_of<Transform>(rootIt->second))
            m_Registry.get<Transform>(rootIt->second).Position += glm::vec3(1.5f, 0.0f, 0.0f);

        m_SelectedEntity = rootIt->second;
    }

    BINAX_LOG_INFO("Duplicated '{0}' with {1} entities",
                   GetEntityName(entity), remap.size());
}





void EditorUI::ApplyQtGrayStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding    = 4.0f;
    style.ChildRounding     = 3.0f;
    style.FrameRounding     = 3.0f;
    style.PopupRounding     = 3.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding      = 3.0f;
    style.TabRounding       = 3.0f;
    style.FrameBorderSize   = 1.0f;
    style.WindowBorderSize  = 1.0f;

    colors[ImGuiCol_Text]                 = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_TextDisabled]         = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    colors[ImGuiCol_WindowBg]             = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    colors[ImGuiCol_ChildBg]              = ImVec4(0.94f, 0.94f, 0.94f, 0.00f);
    colors[ImGuiCol_PopupBg]              = ImVec4(1.00f, 1.00f, 1.00f, 0.98f);
    colors[ImGuiCol_Border]               = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
    colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]              = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.95f, 0.97f, 1.00f, 1.00f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.88f, 0.92f, 0.96f, 1.00f);
    colors[ImGuiCol_TitleBg]              = ImVec4(0.88f, 0.88f, 0.88f, 1.00f);
    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.82f, 0.85f, 0.88f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.88f, 0.88f, 0.88f, 0.40f);
    colors[ImGuiCol_Button]               = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.85f, 0.87f, 0.90f, 1.00f);
    colors[ImGuiCol_ButtonActive]         = ImVec4(0.75f, 0.80f, 0.85f, 1.00f);
    colors[ImGuiCol_Header]               = ImVec4(0.85f, 0.88f, 0.93f, 1.00f);
    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.78f, 0.84f, 0.92f, 1.00f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(0.68f, 0.77f, 0.88f, 1.00f);
    colors[ImGuiCol_Separator]            = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]     = ImVec4(0.60f, 0.65f, 0.75f, 1.00f);
    colors[ImGuiCol_SeparatorActive]      = ImVec4(0.50f, 0.55f, 0.65f, 1.00f);
    colors[ImGuiCol_ResizeGrip]           = ImVec4(0.80f, 0.80f, 0.80f, 0.56f);
    colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.70f, 0.75f, 0.85f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.60f, 0.65f, 0.75f, 0.95f);
    colors[ImGuiCol_Tab]                  = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
    colors[ImGuiCol_TabHovered]           = ImVec4(0.92f, 0.94f, 0.97f, 1.00f);
    colors[ImGuiCol_TabActive]            = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    colors[ImGuiCol_TabUnfocused]         = ImVec4(0.88f, 0.88f, 0.88f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.65f, 0.65f, 0.65f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
    colors[ImGuiCol_CheckMark]            = ImVec4(0.20f, 0.50f, 0.80f, 1.00f);
    colors[ImGuiCol_SliderGrab]           = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
}

void EditorUI::LoadEditorFont()
{
    
    
    const std::string fontPath = Paths::Resolve("resources/fonts/EditorFont.ttf");

    
    
    
    
    
    
    std::FILE* file = OpenFileUtf8(fontPath);
    if (!file)
    {
        BINAX_LOG_INFO("[Editor] Font not found at '{}', using default", fontPath);
        return;
    }

    std::fseek(file, 0, SEEK_END);
    const long sz = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);

    if (sz <= 0)
    {
        std::fclose(file);
        BINAX_LOG_WARN("[Editor] Font file '{}' is empty", fontPath);
        return;
    }

    char* buf = static_cast<char*>(std::malloc(static_cast<size_t>(sz)));
    if (!buf)
    {
        std::fclose(file);
        BINAX_LOG_WARN("[Editor] Failed to allocate {} bytes for font", sz);
        return;
    }

    const size_t readBytes = std::fread(buf, 1, static_cast<size_t>(sz), file);
    std::fclose(file);

    if (readBytes != static_cast<size_t>(sz))
    {
        std::free(buf);
        BINAX_LOG_WARN("[Editor] Short read while loading font '{}'", fontPath);
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImFont* font = io.Fonts->AddFontFromMemoryTTF(buf, static_cast<int>(sz), 16.0f);
    if (font)
    {
        BINAX_LOG_INFO("[Editor] Loaded font: {}", fontPath);
        io.FontDefault = font;
    }
    else
    {
        
        std::free(buf);
        BINAX_LOG_WARN("[Editor] Failed to parse font '{}'", fontPath);
    }
}

bool EditorUI::IsGizmoUsing() const   { return ImGuizmo::IsUsing(); }
bool EditorUI::IsGizmoHovered() const { return ImGuizmo::IsOver(); }

void EditorUI::AddPanelDrawer(std::function<void()> cb)
{
    m_ExtraPanelDrawers.push_back(std::move(cb));
}

} 
