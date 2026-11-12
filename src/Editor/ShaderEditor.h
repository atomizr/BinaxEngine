#pragma once

#include <string>
#include <vector>
#include <functional>
#include <imgui.h>

#include "Editor/TextEditor.h"

namespace Binax {

class ShaderEditor
{
public:
    ShaderEditor();


    void SetReloadCallback(std::function<void(const std::string&)> cb)
    {
        m_ReloadCallback = std::move(cb);
    }

    void SetVisible(bool v) { m_Visible = v; }
    bool IsVisible() const  { return m_Visible; }
    void ToggleVisible()    { m_Visible = !m_Visible; }


    void OpenFile(const std::string& path);


    void Draw();

private:
    struct OpenShader
    {
        std::string Path;
        std::string Name;
        TextEditor  Editor;
        bool        Dirty = false;
        bool        NeedsSelect = false;
    };

    void ScanShaderDirectory();
    void LoadFileInto(OpenShader& s);
    void SaveCurrent();
    void ReloadCurrent();
    bool HasFileOpen(const std::string& path) const;

    void DrawFileList();
    void DrawEditorTabs();

    std::vector<OpenShader> m_Open;
    int                     m_ActiveTab = 0;
    bool                    m_Visible   = false;

    std::string              m_ShaderDir;
    std::vector<std::string> m_AvailableFiles;
    float                    m_ScanTimer = 0.0f;

    std::function<void(const std::string&)> m_ReloadCallback;

    ImVec2 m_WindowSize{900.0f, 560.0f};
};

} 