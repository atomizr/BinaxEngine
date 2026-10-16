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
#include "Renderer/Mesh.h"
#include "Renderer/MeshLibrary.h"
#include "Renderer/MaterialLibrary.h"
#include "Utils/Log.h"
#include "Serialization/SceneSerializer.h"
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

namespace {

// [FIX] Безопасное преобразование path → UTF-8 std::string.
//       .string() на MSVC может бросить std::system_error 1113
//       "No mapping for the Unicode character..." на путях с
//       кириллицей/эмодзи при не-соответствующей ANSI-кодировке.
std::string PathToUtf8(const std::filesystem::path& p)
{
    const auto u8 = p.u8string();
    return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

std::filesystem::path PathFromUtf8(const std::string& s)
{
    return std::filesystem::path(
        std::u8string(reinterpret_cast<const char8_t*>(s.data()), s.size()));
}

} // namespace

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

// ---------------------------------------------------------------------------
// Component add / remove
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Scene IO
// ---------------------------------------------------------------------------

void EditorUI::SaveScene()
{
    SceneSerializer::Serialize(m_Registry, "scene.json");
}

void EditorUI::LoadScene()
{
    m_SelectedEntity = entt::null;

    // Сбрасываем material editor: handle'ы старой сцены могут стать
    // невалидными после перезагрузки.
    m_EditingMaterial    = kInvalidMaterial;
    m_ShowMaterialEditor = false;

    if (!SceneSerializer::Deserialize(m_Registry, "scene.json"))
    {
        BINAX_LOG_WARN("Scene load failed — registry left unchanged");
        return;
    }

    // Восстанавливаем Skybox, если он был утерян
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

    BINAX_LOG_INFO("Scene loaded");
}

// ---------------------------------------------------------------------------
// Entity ops
// ---------------------------------------------------------------------------

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
        BINAX_LOG_WARN("Entity {0} is not valid", (uint32_t)entity);
        return;
    }

    BINAX_LOG_INFO("Deleting entity {0} (name: {1})",
                   (uint32_t)entity, GetEntityName(entity));

    uint32_t id = (uint32_t)entity;

    // Сравнение делаем ДО destroy — на случай переиспользования handle.
    if (m_SelectedEntity == entity)
        m_SelectedEntity = entt::null;

    m_Registry.destroy(entity);

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
    return "Entity " + std::to_string((uint32_t)entity);
}

void EditorUI::HandleShortcuts()
{
    if (ImGui::GetIO().WantTextInput) return;

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
}

// ---------------------------------------------------------------------------
// Main draw
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Viewport + Gizmo
// ---------------------------------------------------------------------------

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
            if ((uint32_t)vpSize.x != (uint32_t)m_Viewport->GetSize().x ||
                (uint32_t)vpSize.y != (uint32_t)m_Viewport->GetSize().y)
            {
                m_Viewport->Resize((uint32_t)vpSize.x, (uint32_t)vpSize.y);
            }
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
    const ImGuizmo::MODE mode = (m_GizmoMode == 0) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;

    const bool manipulated = ImGuizmo::Manipulate(
        glm::value_ptr(m_ViewMatrix),
        glm::value_ptr(m_ProjectionMatrix),
        op, mode, glm::value_ptr(model));

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

// ---------------------------------------------------------------------------
// Hierarchy / Inspector
// ---------------------------------------------------------------------------

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

    auto view = m_Registry.view<NameComponent>();
    for (auto entity : view)
    {
        std::string label = GetEntityName(entity);
        bool isSelected = (m_SelectedEntity == entity);
        bool isCamera   = IsEditorCamera(entity);
        bool isSkybox   = m_Registry.any_of<SkyboxComponent>(entity);
        bool isProtected = isCamera || isSkybox;

        ImGui::PushID((int)entity);
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
        const bool canRemove   = !isCamera && !isSkybox && !isTransform && !isName;

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

        if (entry.Name == "Material" &&
            m_Registry.any_of<MaterialComponent>(m_SelectedEntity))
        {
            auto h = m_Registry.get<MaterialComponent>(m_SelectedEntity).MaterialId;
            if (h != kInvalidMaterial)
            {
                ImGui::Text("Name: %s", MaterialLibrary::Get().GetName(h).c_str());
                if (ImGui::SmallButton("Edit Material..."))
                {
                    m_EditingMaterial = h;
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

// ---------------------------------------------------------------------------
// Material editor
// ---------------------------------------------------------------------------

void EditorUI::DrawMaterialEditor()
{
    if (m_EditingMaterial == kInvalidMaterial)
    {
        m_ShowMaterialEditor = false;
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

// ---------------------------------------------------------------------------
// Create / duplicate
// ---------------------------------------------------------------------------

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
        constexpr const char* kPrefix = "Cube ";
        constexpr size_t kPrefixLen = 5;
        if (n.size() > kPrefixLen && n.compare(0, kPrefixLen, kPrefix) == 0)
        {
            try
            {
                const int v = std::stoi(n.substr(kPrefixLen));
                if (v >= nextIndex) nextIndex = v + 1;
            }
            catch (...) {}
        }
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
    MaterialGPU gpu{};
    gpu.AlbedoRoughness = glm::vec4(1.0f, 0.5f, 0.2f, 0.4f);
    gpu.MetallicAOEms   = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
    gpu.SkyReflParams   = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
    gpu.StrengthA       = glm::vec4(1.0f);
    gpu.StrengthB       = glm::vec4(1.0f);
    MaterialHandle mat  = MaterialLibrary::Get().Add(matName, gpu);
    MaterialLibrary::Get().Flush();

    m_Registry.emplace<MaterialComponent>(entity, MaterialComponent{ mat });

    m_Registry.emplace<NameComponent>(entity, "Cube " + std::to_string(nextIndex));

    m_SelectedEntity = entity;
    BINAX_LOG_INFO("Created cube entity {0} at ({1:.2f}, 0, {2:.2f})",
                   (uint32_t)entity, offsetX, offsetZ);
}

void EditorUI::DuplicateEntity(entt::entity entity)
{
    if (IsEditorCamera(entity)) { BINAX_LOG_WARN("Cannot duplicate EditorCamera"); return; }
    if (m_Registry.any_of<SkyboxComponent>(entity)) { BINAX_LOG_WARN("Cannot duplicate Skybox"); return; }

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

    // Mesh
    if (m_Registry.any_of<MeshComponent>(entity))
    {
        if (!m_Registry.any_of<MeshComponent>(newEntity))
            m_Registry.emplace<MeshComponent>(newEntity);
        m_Registry.get<MeshComponent>(newEntity).MeshId =
            m_Registry.get<MeshComponent>(entity).MeshId;
    }

    // Material: независимая копия (GPU + пути к текстурам).
    if (m_Registry.any_of<MaterialComponent>(entity))
    {
        auto srcH = m_Registry.get<MaterialComponent>(entity).MaterialId;
        if (srcH != kInvalidMaterial)
        {
            std::string baseName = MaterialLibrary::Get().GetName(srcH);
            std::string newName  = baseName + "_copy";
            int suffix = 1;
            while (MaterialLibrary::Get().Find(newName) != kInvalidMaterial)
                newName = baseName + "_copy" + std::to_string(suffix++);

            MaterialHandle nh = MaterialLibrary::Get().Copy(newName, srcH);
            MaterialLibrary::Get().Flush();

            if (!m_Registry.any_of<MaterialComponent>(newEntity))
                m_Registry.emplace<MaterialComponent>(newEntity);
            m_Registry.get<MaterialComponent>(newEntity).MaterialId = nh;
        }
    }

    if (m_Registry.any_of<Transform>(newEntity))
        m_Registry.get<Transform>(newEntity).Position += glm::vec3(1.5f, 0.0f, 0.0f);

    if (m_Registry.any_of<CameraComponent>(newEntity))
        m_Registry.get<CameraComponent>(newEntity).Primary = false;

    if (m_Registry.any_of<NameComponent>(newEntity))
        m_Registry.get<NameComponent>(newEntity).Name += " (copy)";

    m_SelectedEntity = newEntity;
    BINAX_LOG_INFO("Duplicated entity {} -> {}", (uint32_t)entity, (uint32_t)newEntity);
}

// ---------------------------------------------------------------------------
// Style (без изменений)
// ---------------------------------------------------------------------------

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
}

bool EditorUI::IsGizmoUsing() const   { return ImGuizmo::IsUsing(); }
bool EditorUI::IsGizmoHovered() const { return ImGuizmo::IsOver(); }

void EditorUI::AddPanelDrawer(std::function<void()> cb)
{
    m_ExtraPanelDrawers.push_back(std::move(cb));
}

} // namespace Binax
