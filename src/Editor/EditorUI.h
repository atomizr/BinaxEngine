#pragma once

#include <entt/entt.hpp>
#include <string>
#include <glm/glm.hpp>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <imgui.h>
#include "Editor/ShaderEditor.h"
#include "Editor/Console.h"

#include "Renderer/MaterialLibrary.h"
#include "Renderer/MeshLibrary.h"
#include "Renderer/MeshImporter.h"
#include "Renderer/ShadowSettings.h"
#include "ECS/Components/EntityFlags.h"
#include "ECS/Components/LightComponent.h"

struct GLFWwindow;

namespace Binax {

class Viewport;

class EditorUI
{
public:
    EditorUI(GLFWwindow* window, entt::registry& registry);
    ~EditorUI();
    void Draw();
    void AddPanelDrawer(std::function<void()> cb);
    void SetViewport(Viewport* viewport)   { m_Viewport  = viewport;  }
    bool IsViewportHovered() const         { return m_ViewportHovered; }
    void SyncViewportSize();
    void SetCameraMatrices(const glm::mat4& view, const glm::mat4& projection)
    {
        m_ViewMatrix = view;
        m_ProjectionMatrix = projection;
    }
    void HandleShortcuts();
    bool IsGizmoUsing() const;
    bool IsGizmoHovered() const;
    void Notify(int type, int dismissMs, const char* format = nullptr, ...);
    Console& GetConsole() { return m_Console; }
    ShaderEditor& GetShaderEditor() { return m_ShaderEditor; }
    bool IsViewportStatsVisible() const { return m_ShowViewportStats; }
    void SetViewportStatsVisible(bool v) { m_ShowViewportStats = v; }
    bool HasViewport() const { return m_Viewport != nullptr; }
    ImVec4 GetViewportScreenRect() const
    {
        return ImVec4(m_ViewportScreenX, m_ViewportScreenY,
                      m_ViewportScreenW, m_ViewportScreenH);
    }
    bool CanUndo() const { return m_UndoCursor > 0; }
    bool CanRedo() const { return m_UndoCursor + 1 < (int)m_UndoStates.size(); }
    void Undo();
    void Redo();
    void SetShadowSettingsPtr(ShadowSettings* s) { m_ShadowSettings = s; }
    using PickCallback = std::function<entt::entity(ImVec2 uv)>;
    void SetPickCallback(PickCallback cb) { m_PickCallback = std::move(cb); }
    entt::entity GetSelectedEntity() const { return m_SelectedEntity; }
    const std::unordered_set<entt::entity>& GetSelectedEntities() const
    {
        return m_SelectedEntities;
    }

private:
    entt::registry& m_Registry;
    entt::entity                     m_SelectedEntity = entt::null;
    std::unordered_set<entt::entity> m_SelectedEntities;
    entt::entity m_PendingDelete    = entt::null;
    entt::entity m_PendingDuplicate = entt::null;
    std::vector<std::function<void()>> m_ExtraPanelDrawers;
    Viewport* m_Viewport = nullptr;
    bool m_ShowHierarchy  = true;
    bool m_ShowInspector  = true;
    bool m_ViewportHovered = false;
    char m_HierarchySearch[128] = {};
    bool m_FocusSearch = false;
    glm::mat4 m_ViewMatrix       = glm::mat4(1.0f);
    glm::mat4 m_ProjectionMatrix = glm::mat4(1.0f);
    int m_GizmoOperation = 0;
    int m_GizmoMode      = 0;
    bool  m_SnapEnabled   = false;
    bool  m_ShowLightGizmos = true;
    float m_SnapTranslate = 0.5f;
    float m_SnapRotate    = 15.0f;
    float m_SnapScale     = 0.1f;
    float m_ViewportScreenX = 0.0f;
    float m_ViewportScreenY = 0.0f;
    float m_ViewportScreenW = 0.0f;
    float m_ViewportScreenH = 0.0f;
    float m_PendingViewportW = 0.0f;
    float m_PendingViewportH = 0.0f;
    MaterialHandle m_EditingMaterial    = kInvalidMaterial;
    bool           m_ShowMaterialEditor = false;
    ShaderEditor m_ShaderEditor;
    Console      m_Console;
    bool         m_ShowViewportStats = false;
    PickCallback m_PickCallback;
    std::vector<std::string> m_UndoStates;
    int  m_UndoCursor          = -1;
    bool m_UndoSuppress        = false;
    bool m_GizmoWasUsing       = false;
    bool m_InspectorPendingRec = false;
    void DrawHierarchy();
    void DrawInspector();
    void DrawViewport();
    void DrawGizmo();
    void DrawLightGizmos();
    void DrawAddComponentMenu();
    void DrawCreateObjectDropdown();
    void DrawMaterialEditor();
    void DrawEntityRow(entt::entity entity,
                       const std::unordered_map<entt::entity, std::vector<entt::entity>>& children,
                       std::unordered_set<entt::entity>& visited);
    void SelectOnly(entt::entity e);
    void ToggleSelect(entt::entity e);
    void ClearSelection();
    void SetParent(entt::entity child, entt::entity parent);
    bool IsDescendantOf(entt::entity candidate, entt::entity ancestor) const;
    std::vector<entt::entity> CollectSubtree(entt::entity root) const;
    float& SnapStep();
    void FocusOnSelection();
    std::string MakeUniqueName(const std::string& sourceName) const;
    bool IsNameTaken(const std::string& name) const;
    void ImportMeshFromFile(const std::string& path);
    void InitComponentFactory();
    void AddComponentToEntity(entt::entity entity, const std::string& componentName);
    void RemoveComponentFromEntity(entt::entity entity, const std::string& componentName);
    entt::entity DuplicateSingle(entt::entity src);
    void DuplicateEntity(entt::entity entity);
    void DeleteEntity(entt::entity entity);
    void SaveScene();
    void LoadScene();
    bool IsEditorCamera(entt::entity entity) const;
    bool IsProtectedEntity(entt::entity entity) const;
    std::string GetEntityName(entt::entity entity) const;
    void ApplyModernDarkStyle();
    void LoadEditorFont();
    bool AnimatedButton(const char* label, const ImVec2& size, bool primary = false);
    bool AnimatedIconButton(const char* icon, const char* tooltip, bool active);
    void ViewportFocusGlow();
    void DrawAnimatedHeader(const char* label, bool* open = nullptr);
    void ResetUndo();
    void RecordUndo();
    std::string CaptureScene() const;
    void RestoreScene(const std::string& state);

    // ---- Import dialog ------------------------------------------------------
    enum class ImportScaleMode { AutoFit, Original, Custom };

    struct PendingImportState
    {
        bool            Active   = false;
        std::string     Path;
        ImportedModel   Model;
        ImportScaleMode ScaleMode = ImportScaleMode::AutoFit;
        float           TargetSize  = 2.0f;
        float           CustomScale = 1.0f;
        bool            Recenter    = true;
        bool            OpenPopupRequested = false;

        bool FlipUV_U    = false;
        bool FlipUV_V    = false;
        bool FlipNormals = false;
    } m_PendingImport;

    void DrawImportDialog();
    void ExecutePendingImport();
    void CreateEmptyEntity();
    void CreateCubeEntity();
    void CreateLightEntity(LightType type);

    ShadowSettings* m_ShadowSettings = nullptr;
    bool m_ShowShadowSettings = false;
    void DrawShadowSettingsWindow();
};

}