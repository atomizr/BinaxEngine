#pragma once

#include <entt/entt.hpp>
#include <string>
#include <glm/glm.hpp>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Renderer/MaterialLibrary.h"
#include "Renderer/MeshLibrary.h"

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

private:
    entt::registry& m_Registry;
    entt::entity m_SelectedEntity = entt::null;
    
    
    entt::entity m_PendingDelete = entt::null;
    entt::entity m_PendingDuplicate = entt::null;
    std::vector<std::function<void()>> m_ExtraPanelDrawers;
    Viewport* m_Viewport = nullptr;

    bool m_ShowHierarchy = true;
    bool m_ShowInspector = true;
    bool m_ViewportHovered = false;

    
    char m_HierarchySearch[128] = {};
    bool m_FocusSearch = false;

    glm::mat4 m_ViewMatrix = glm::mat4(1.0f);
    glm::mat4 m_ProjectionMatrix = glm::mat4(1.0f);
    int m_GizmoOperation = 0;
    int m_GizmoMode = 0;

    
    
    
    bool  m_SnapEnabled   = false;
    float m_SnapTranslate = 0.5f;
    float m_SnapRotate    = 15.0f;
    float m_SnapScale     = 0.1f;

    float m_ViewportScreenX = 0.0f;
    float m_ViewportScreenY = 0.0f;
    float m_ViewportScreenW = 0.0f;
    float m_ViewportScreenH = 0.0f;

    
    
    float m_PendingViewportW = 0.0f;
    float m_PendingViewportH = 0.0f;

    
    MaterialHandle m_EditingMaterial = kInvalidMaterial;
    bool           m_ShowMaterialEditor = false;

    
    void DrawHierarchy();
    void DrawInspector();
    void DrawViewport();
    void DrawGizmo();
    void DrawAddComponentMenu();
    void DrawCreateObjectDropdown();
    void DrawMaterialEditor();

    
    void DrawEntityRow(entt::entity entity,
                       const std::unordered_map<entt::entity, std::vector<entt::entity>>& children,
                       std::unordered_set<entt::entity>& visited);
    void SetParent(entt::entity child, entt::entity parent);
    bool IsDescendantOf(entt::entity candidate, entt::entity ancestor) const;
    std::vector<entt::entity> CollectSubtree(entt::entity root) const;

    
    float& SnapStep();

    
    void FocusOnSelection();

    
    std::string MakeUniqueName(const std::string& sourceName) const;
    bool IsNameTaken(const std::string& name) const;

    
    void CreateEmptyEntity();
    void CreateCubeEntity();
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

    void ApplyQtGrayStyle();
    void LoadEditorFont();
};

}
