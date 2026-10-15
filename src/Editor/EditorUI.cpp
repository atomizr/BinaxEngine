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
#include "Renderer/Mesh.h"
#include "Utils/Log.h"
#include "Serialization/SceneSerializer.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <glm/gtx/matrix_decompose.hpp>
#include "ImGuizmo.h"
#include "imgui_internal.h"
#include <iostream>
#include "Reflect/Reflection.h"
#include "Reflect/ComponentRegistry.h"
#include "Reflect/EngineReflection.h"
#include "Editor/PropertyWidgets.h"
#include "Serialization/PropertySerializer.h"
#include "ECS/Components/SkyboxComponent.h"

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
    if (componentName == "Transform" || componentName == "Name")
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
    SceneSerializer::Serialize(m_Registry, "scene.json");
}

void EditorUI::LoadScene()
{
    m_SelectedEntity = entt::null;

    if (!SceneSerializer::Deserialize(m_Registry, "scene.json"))
    {
        BINAX_LOG_WARN("Scene load failed — registry left unchanged (or partially loaded)");
        return;
    }

    auto meshView = m_Registry.view<MeshComponent>();
    for (auto entity : meshView)
    {
        auto& meshComp = meshView.get<MeshComponent>(entity);
        if (meshComp.MeshPtr == nullptr)
            meshComp.MeshPtr = m_CubeMesh;
    }

    // ---- Восстанавливаем Skybox, если он был утерян ----
    {
        auto skyView = m_Registry.view<SkyboxComponent>();
        if (skyView.begin() == skyView.end())
        {
            auto e = m_Registry.create();
            m_Registry.emplace<NameComponent>(e, "Skybox");
            m_Registry.emplace<SkyboxComponent>(e);
            BINAX_LOG_INFO("Skybox recreated after scene load");
        }
    }

    BINAX_LOG_INFO("Scene loaded (mesh pointers restored)");
}

void EditorUI::DeleteEntity(entt::entity entity)
{
    if (m_Registry.any_of<SystemEntityTag>(entity))
    {
        BINAX_LOG_WARN("Cannot delete system entity (tagged SystemEntityTag)");
        return;
    }
    if (m_Registry.any_of<SkyboxComponent>(entity))
    {
        BINAX_LOG_WARN("Cannot delete Skybox entity");
        return;
    }

    if (!m_Registry.valid(entity))
    {
        BINAX_LOG_WARN("Entity {0} is not valid", static_cast<uint32_t>(entity));
        return;
    }

    BINAX_LOG_INFO("Deleting entity {0} (name: {1})",
                   static_cast<uint32_t>(entity),
                   GetEntityName(entity));

    uint32_t id = static_cast<uint32_t>(entity);
    m_Registry.destroy(entity);

    if (m_SelectedEntity == entity)
        m_SelectedEntity = entt::null;

    BINAX_LOG_INFO("Entity {0} destroyed", id);
}

bool EditorUI::IsEditorCamera(entt::entity entity) const
{
    return m_Registry.any_of<EditorCameraComponent>(entity);
}

std::string EditorUI::GetEntityName(entt::entity entity) const
{
    if (m_Registry.any_of<NameComponent>(entity))
        return m_Registry.get<NameComponent>(entity).Name;
    else
        return "Entity " + std::to_string(static_cast<uint32_t>(entity));
}

void EditorUI::HandleShortcuts()
{
    if (ImGui::GetIO().WantTextInput)
        return;

    const bool ctrlPressed = Input::IsKeyPressed(GLFW_KEY_LEFT_CONTROL) ||
                             Input::IsKeyPressed(GLFW_KEY_RIGHT_CONTROL);

    if (ctrlPressed && Input::IsKeyJustPressed(GLFW_KEY_D))
    {
        if (m_SelectedEntity != entt::null && m_Registry.valid(m_SelectedEntity))
            DuplicateEntity(m_SelectedEntity);
    }

    if (Input::IsKeyJustPressed(GLFW_KEY_DELETE))
    {
        if (m_SelectedEntity != entt::null && m_Registry.valid(m_SelectedEntity))
            DeleteEntity(m_SelectedEntity);
    }
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

    PropertyWidgets::DrawFileBrowser();

    // ---- Пользовательские панели, зарегистрированные извне (Stats и др.) ----
    for (auto& cb : m_ExtraPanelDrawers)
    {
        if (cb) cb();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
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
        ImVec2 vpPos  = ImGui::GetCursorScreenPos();
        ImVec2 vpSize = ImGui::GetContentRegionAvail();

        m_ViewportScreenX = vpPos.x;
        m_ViewportScreenY = vpPos.y;
        m_ViewportScreenW = vpSize.x;
        m_ViewportScreenH = vpSize.y;

        m_ViewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

        if (vpSize.x > 0 && vpSize.y > 0)
        {
            if (static_cast<uint32_t>(vpSize.x) != m_Viewport->GetSize().x ||
                static_cast<uint32_t>(vpSize.y) != m_Viewport->GetSize().y)
            {
                m_Viewport->Resize(static_cast<uint32_t>(vpSize.x),
                                   static_cast<uint32_t>(vpSize.y));
            }
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddImage(
            reinterpret_cast<ImTextureID>(static_cast<intptr_t>(m_Viewport->GetTextureID())),
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
    glm::mat4 model = transform.GetModelMatrix();

    ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
    switch (m_GizmoOperation)
    {
        case 0: op = ImGuizmo::TRANSLATE; break;
        case 1: op = ImGuizmo::ROTATE;    break;
        case 2: op = ImGuizmo::SCALE;     break;
    }
    const ImGuizmo::MODE mode = (m_GizmoMode == 0) ? ImGuizmo::WORLD
                                                   : ImGuizmo::LOCAL;

    const bool manipulated = ImGuizmo::Manipulate(
        glm::value_ptr(m_ViewMatrix),
        glm::value_ptr(m_ProjectionMatrix),
        op, mode,
        glm::value_ptr(model));

    if (manipulated)
    {
        glm::vec3 translation, scale, skew;
        glm::vec4 perspective;
        glm::quat rotation;

        glm::decompose(model, scale, rotation, translation, skew, perspective);

        transform.Position = translation;
        transform.Rotation = rotation;
        transform.Scale    = scale;
    }
}

void EditorUI::DrawCreateObjectDropdown()
{
    if (ImGui::Button("Create Object"))
        ImGui::OpenPopup("CreateObjectPopup");

    if (ImGui::BeginPopup("CreateObjectPopup"))
    {
        if (ImGui::MenuItem("Empty Object"))
            CreateEmptyEntity();
        if (ImGui::MenuItem("3D Cube"))
            CreateCubeEntity();
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

    // View по NameComponent покрывает и обычные сущности, и Skybox (у него есть Name).
    // Fallback — если у сущности нет имени, используем её entity id.
    auto view = m_Registry.view<NameComponent>();
    for (auto entity : view)
    {
        std::string label = GetEntityName(entity);
        bool isSelected = (m_SelectedEntity == entity);
        bool isCamera   = IsEditorCamera(entity);
        bool isSkybox   = m_Registry.any_of<SkyboxComponent>(entity);
        bool isProtected = isCamera || isSkybox;

        ImGui::PushID(static_cast<int>(entity));
        if (ImGui::Selectable(label.c_str(), &isSelected))
            m_SelectedEntity = entity;

        if (ImGui::BeginPopupContextItem())
        {
            if (!isProtected)
            {
                if (ImGui::MenuItem("Duplicate Object")) DuplicateEntity(entity);
                if (ImGui::MenuItem("Delete Object"))    DeleteEntity(entity);
            }
            else
            {
                ImGui::Text("System object - cannot modify");
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }

    ImGui::End();
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
        const bool canRemove   = !isCamera && !isSkybox
                              && !isTransform && !isName;

        if (canRemove)
        {
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
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

        ImGui::Unindent();
        ImGui::Separator();
        ImGui::PopID();

        if (!pendingRemove.empty())
            break;
    }

    if (!pendingRemove.empty())
        RemoveComponentFromEntity(m_SelectedEntity, pendingRemove);

    // Skybox тоже не должен получать «Add Component» (защита консистентности)
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

            // Skybox и EditorCameraComponent нельзя добавлять к произвольным сущностям
            if (entry.Name == "Skybox") continue;
            if (entry.Name == "EditorCameraComponent") continue;

            if (ImGui::MenuItem(entry.Name.c_str()))
                AddComponentToEntity(m_SelectedEntity, entry.Name);
        }
        ImGui::EndPopup();
    }
}

void EditorUI::CreateEmptyEntity()
{
    auto entity = m_Registry.create();
    m_Registry.emplace<Transform>(entity);
    m_Registry.emplace<NameComponent>(entity, "Entity " + std::to_string(static_cast<uint32_t>(entity)));
    m_SelectedEntity = entity;
    BINAX_LOG_INFO("Created empty entity {0}", static_cast<uint32_t>(entity));
}

void EditorUI::CreateCubeEntity()
{
    if (!m_CubeMesh)
    {
        BINAX_LOG_ERROR("Cube mesh not set in EditorUI!");
        return;
    }

    int nextIndex = 1;
    auto nameView = m_Registry.view<NameComponent>();
    for (auto e : nameView)
    {
        const std::string& n = nameView.get<NameComponent>(e).Name;
        constexpr const char* kPrefix = "Cube ";
        constexpr size_t kPrefixLen = 5;
        if (n.size() > kPrefixLen && n.compare(0, kPrefixLen, kPrefix) == 0)
        {
            try
            {
                const int v = std::stoi(n.substr(kPrefixLen));
                if (v >= nextIndex) nextIndex = v + 1;
            }
            catch (...) {  }
        }
    }

    entt::entity entity = m_Registry.create();

    const int gridIdx = nextIndex - 1;
    const float offsetX = 1.5f * (gridIdx % 5);
    const float offsetZ = 1.5f * ((gridIdx / 5) % 5);
    const glm::vec3 position(offsetX, 0.0f, offsetZ);

    m_Registry.emplace<Transform>(entity, position,
                                  glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                  glm::vec3(1.0f));

    m_Registry.emplace<MeshComponent>(entity, m_CubeMesh);

    // PBR-материал по умолчанию
    auto& mat = m_Registry.emplace<MaterialComponent>(entity);
    mat.Albedo    = glm::vec3(1.0f, 0.5f, 0.2f);
    mat.Metallic  = 0.0f;
    mat.Roughness = 0.4f;
    mat.AO        = 1.0f;
    mat.Emissive  = 0.0f;

    m_Registry.emplace<NameComponent>(entity, "Cube " + std::to_string(nextIndex));

    m_SelectedEntity = entity;
    BINAX_LOG_INFO("Created cube entity {0} at ({1:.2f}, 0, {2:.2f})",
                   static_cast<uint32_t>(entity), offsetX, offsetZ);
}

void EditorUI::DuplicateEntity(entt::entity entity)
{
    if (IsEditorCamera(entity))
    {
        BINAX_LOG_WARN("Cannot duplicate EditorCamera");
        return;
    }
    if (m_Registry.any_of<SkyboxComponent>(entity))
    {
        BINAX_LOG_WARN("Cannot duplicate Skybox");
        return;
    }

    if (!m_Registry.valid(entity) || !m_Registry.any_of<Transform>(entity))
    {
        BINAX_LOG_WARN("Invalid entity to duplicate");
        return;
    }

    entt::entity newEntity = m_Registry.create();

    auto snapshot = Serialization::PropertySerializer::SerializeEntity(m_Registry, entity);
    Serialization::PropertySerializer::DeserializeEntity(m_Registry, newEntity, snapshot);

    if (m_Registry.any_of<EditorCameraComponent>(newEntity))
        m_Registry.remove<EditorCameraComponent>(newEntity);

    // MeshComponent хранит сырой указатель — сериализатор его не сохраняет,
    // поэтому копируем вручную.
    if (m_Registry.any_of<MeshComponent>(entity) &&
        m_Registry.any_of<MeshComponent>(newEntity))
    {
        m_Registry.get<MeshComponent>(newEntity).MeshPtr =
            m_Registry.get<MeshComponent>(entity).MeshPtr;
    }

    if (m_Registry.any_of<Transform>(newEntity))
        m_Registry.get<Transform>(newEntity).Position += glm::vec3(1.5f, 0.0f, 0.0f);

    if (m_Registry.any_of<CameraComponent>(newEntity))
        m_Registry.get<CameraComponent>(newEntity).Primary = false;

    if (m_Registry.any_of<NameComponent>(newEntity))
        m_Registry.get<NameComponent>(newEntity).Name += " (copy)";

    m_SelectedEntity = newEntity;
    BINAX_LOG_INFO("Duplicated entity {} -> {}",
                   (uint32_t)entity, (uint32_t)newEntity);
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
    std::string fontPath = "resources/fonts/EditorFont.ttf";

    std::ifstream file(fontPath);
    if (!file.good())
    {
        BINAX_LOG_INFO("[Editor] Font not found at '{}', using default", fontPath);
        return;
    }
    file.close();

    ImGuiIO& io = ImGui::GetIO();
    ImFont* font = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f);
    if (font)
    {
        BINAX_LOG_INFO("[Editor] Loaded font: {}", fontPath);
        io.FontDefault = font;
    }
    else
    {
        BINAX_LOG_INFO("[Editor] Failed to load font: {}", fontPath);
    }
}

bool EditorUI::IsGizmoUsing() const
{
    return ImGuizmo::IsUsing();
}

bool EditorUI::IsGizmoHovered() const
{
    return ImGuizmo::IsOver();
}

void EditorUI::AddPanelDrawer(std::function<void()> cb)
{
    m_ExtraPanelDrawers.push_back(std::move(cb));
}

}