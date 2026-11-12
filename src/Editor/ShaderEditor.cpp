#include "Editor/ShaderEditor.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <imgui.h>
#include <imgui_internal.h>    // ImHashStr

#include "Renderer/im_anim.h"
#include "Utils/Log.h"
#include "Utils/Paths.h"

namespace Binax {

namespace fs = std::filesystem;

ShaderEditor::ShaderEditor()
{
    m_ShaderDir = Paths::Resolve("shaders/");
    ScanShaderDirectory();
}

void ShaderEditor::ScanShaderDirectory()
{
    m_AvailableFiles.clear();

    std::error_code ec;
    const fs::path dir = Paths::FromUtf8(m_ShaderDir);
    if (!fs::exists(dir, ec) || ec || !fs::is_directory(dir, ec))
        return;

    for (auto it = fs::directory_iterator(dir, ec);
         !ec && it != fs::directory_iterator(); it.increment(ec))
    {
        if (!it->is_regular_file()) continue;
        const std::string ext = it->path().extension().string();
        if (ext == ".vert" || ext == ".frag" || ext == ".comp"
            || ext == ".geom" || ext == ".tesc" || ext == ".tese"
            || ext == ".glsl")
        {
            m_AvailableFiles.push_back(Paths::ToUtf8(it->path()));
        }
    }

    std::sort(m_AvailableFiles.begin(), m_AvailableFiles.end());
}

bool ShaderEditor::HasFileOpen(const std::string& path) const
{
    for (const auto& s : m_Open)
        if (s.Path == path) return true;
    return false;
}

void ShaderEditor::OpenFile(const std::string& path)
{
    for (size_t i = 0; i < m_Open.size(); ++i)
    {
        if (m_Open[i].Path == path)
        {
            m_ActiveTab = (int)i;
            m_Open[i].NeedsSelect = true;   // <-- форсим переключение
            m_Visible = true;
            return;
        }
    }

    OpenShader s;
    s.Path = path;
    s.Name = Paths::ToUtf8(Paths::FromUtf8(path).filename());
    s.NeedsSelect = true;                   // <-- форсим активацию новой вкладки

    s.Editor.SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());
    s.Editor.SetPalette(TextEditor::GetDarkPalette());
    s.Editor.SetShowWhitespaces(false);
    s.Editor.SetTabSize(4);
    s.Editor.SetHandleKeyboardInputs(true);
    s.Editor.SetHandleMouseInputs(true);

    LoadFileInto(s);

    m_Open.push_back(std::move(s));
    m_ActiveTab = (int)m_Open.size() - 1;
    m_Visible = true;

    BINAX_LOG_INFO("ShaderEditor: opened '{}' ({} lines)",
                   path, m_Open.back().Editor.GetTotalLines());
}

void ShaderEditor::LoadFileInto(OpenShader& s)
{
    std::ifstream file(Paths::FromUtf8(s.Path), std::ios::binary);
    if (!file.is_open())
    {
        BINAX_LOG_ERROR("ShaderEditor: cannot open '{}'", s.Path);
        s.Editor.SetText("// Failed to open file: " + s.Path);
        s.Dirty = false;
        return;
    }

    std::stringstream ss;
    ss << file.rdbuf();
    const std::string text = ss.str();

    BINAX_LOG_INFO("ShaderEditor: read '{}' ({} bytes, {} lines)",
                   s.Path, (int)text.size(),
                   (int)std::count(text.begin(), text.end(), '\n') + 1);

    s.Editor.SetText(text);
    s.Dirty = false;
}

void ShaderEditor::SaveCurrent()
{
    if (m_ActiveTab < 0 || m_ActiveTab >= (int)m_Open.size())
        return;

    OpenShader& s = m_Open[m_ActiveTab];

    std::ofstream file(Paths::FromUtf8(s.Path),
                       std::ios::binary | std::ios::trunc);
    if (!file.is_open())
    {
        BINAX_LOG_ERROR("ShaderEditor: cannot write '{}'", s.Path);
        return;
    }

    file << s.Editor.GetText();
    file.flush();
    if (!file.good())
    {
        BINAX_LOG_ERROR("ShaderEditor: write failed for '{}'", s.Path);
        return;
    }

    s.Dirty = false;
    BINAX_LOG_INFO("ShaderEditor: saved '{}'", s.Path);

    if (m_ReloadCallback)
        m_ReloadCallback(s.Path);
}

void ShaderEditor::ReloadCurrent()
{
    if (m_ActiveTab < 0 || m_ActiveTab >= (int)m_Open.size())
        return;

    OpenShader& s = m_Open[m_ActiveTab];
    LoadFileInto(s);

    if (m_ReloadCallback)
        m_ReloadCallback(s.Path);
}

void ShaderEditor::Draw()
{
    if (!m_Visible)
        return;

    m_ScanTimer += ImGui::GetIO().DeltaTime;
    if (m_ScanTimer > 2.0f)
    {
        m_ScanTimer = 0.0f;
        ScanShaderDirectory();
    }

    const float dt = ImGui::GetIO().DeltaTime;
    const float anim = iam_tween_float(
        ImHashStr("shader_editor_open"), ImHashStr("open"),
        m_Visible ? 1.0f : 0.0f,
        0.25f, iam_ease_preset(iam_ease_out_cubic),
        iam_policy_crossfade, dt);

    // Не рисуем окно, если оно ещё почти невидимо
    if (anim < 0.01f)
        return;

    ImGui::SetNextWindowSize(m_WindowSize, ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, anim);

    bool open = true;
    if (ImGui::Begin("Shader Editor", &open))
    {
        m_WindowSize = ImGui::GetWindowSize();

        if (ImGui::Button("Save"))   SaveCurrent();
        ImGui::SameLine();
        if (ImGui::Button("Reload")) ReloadCurrent();
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::TextDisabled("%d shaders · %d open",
                            (int)m_AvailableFiles.size(),
                            (int)m_Open.size());

        ImGui::Separator();

        const float listW = 220.0f;

        ImGui::BeginChild("##shaderlist", ImVec2(listW, 0), true);
        DrawFileList();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("##editortabs", ImVec2(0, 0), true);
        DrawEditorTabs();
        ImGui::EndChild();
    }
    ImGui::End();
    ImGui::PopStyleVar();

    if (!open)
        m_Visible = false;
}

void ShaderEditor::DrawFileList()
{
    ImGui::TextDisabled("Shaders");
    ImGui::Separator();

    for (const auto& path : m_AvailableFiles)
    {
        const std::string name = Paths::ToUtf8(Paths::FromUtf8(path).filename());
        const bool isOpen = HasFileOpen(path);

        if (isOpen)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 1.0f, 1.0f));

        if (ImGui::Selectable(name.c_str(), false))
            OpenFile(path);

        if (isOpen)
            ImGui::PopStyleColor();

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", path.c_str());
    }

    if (m_AvailableFiles.empty())
        ImGui::TextDisabled("(no shader files found in shaders/)");
}

void ShaderEditor::DrawEditorTabs()
{
    if (m_Open.empty())
    {
        ImGui::TextDisabled("Open a shader file from the list on the left.");
        return;
    }

    if (!ImGui::BeginTabBar("##shadertabs", ImGuiTabBarFlags_Reorderable))
    {
        // На первом кадре после BeginTabBar ImGui может вернуть false.
        // Просто пропускаем кадр, вкладки появятся со следующего.
        return;
    }

    bool anyTabDrawn = false;

    for (int i = 0; i < (int)m_Open.size(); ++i)
    {
        OpenShader& s = m_Open[i];
        std::string label = s.Name;
        if (s.Dirty) label += " *";

        ImGuiTabItemFlags flags = 0;
        if (s.NeedsSelect)
            flags |= ImGuiTabItemFlags_SetSelected;

        bool wantClose = false;
        if (ImGui::BeginTabItem(label.c_str(), &wantClose, flags))
        {
            anyTabDrawn = true;
            s.NeedsSelect = false;      // флаг израсходован
            m_ActiveTab = i;

            ImGui::TextDisabled("%s", s.Path.c_str());
            ImGui::Separator();

            // Гарантируем ненулевой размер, иначе TextEditor не отрисуется
            ImVec2 avail = ImGui::GetContentRegionAvail();
            if (avail.x < 100.0f) avail.x = 400.0f;
            if (avail.y < 100.0f) avail.y = 300.0f;

            s.Editor.Render("##editor", avail, false);

            if (s.Editor.IsTextChanged())
                s.Dirty = true;

            ImGui::EndTabItem();
        }

        if (wantClose)
        {
            m_Open.erase(m_Open.begin() + i);
            if (m_ActiveTab >= (int)m_Open.size())
                m_ActiveTab = (int)m_Open.size() - 1;
            ImGui::EndTabBar();
            return;
        }
    }

    // Fallback: если ImGui почему-то не выбрал ни одну вкладку — 
    // принудительно рисуем активную, чтобы пользователь точно увидел код.
    if (!anyTabDrawn && m_ActiveTab >= 0 && m_ActiveTab < (int)m_Open.size())
    {
        OpenShader& s = m_Open[m_ActiveTab];
        ImGui::TextDisabled("%s", s.Path.c_str());
        ImGui::Separator();

        ImVec2 avail = ImGui::GetContentRegionAvail();
        if (avail.x < 100.0f) avail.x = 400.0f;
        if (avail.y < 100.0f) avail.y = 300.0f;

        s.Editor.Render("##editor_fallback", avail, false);
    }

    ImGui::EndTabBar();
}

} // namespace Binax