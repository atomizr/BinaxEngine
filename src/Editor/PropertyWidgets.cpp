#include "Editor/PropertyWidgets.h"
#include "Renderer/AssetRegistry.h"
#include "Renderer/Texture.h"
#include "Utils/Log.h"

#include <glad/glad.h>
#include <stb/stb_image.h>

#include <imgui.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Binax {

using namespace Reflect;

namespace {

// -------------------------------------------------------------------------
// [FIX] Безопасные преобразования path <-> UTF-8.
//
// На MSVC std::filesystem::path хранит wide-строку, а .string() пытается
// сконвертировать её обратно в CP_ACP. Если символ не представим в
// текущей ANSI-кодировке, бросается std::system_error 1113
// "No mapping for the Unicode character exists in the target multi-byte
//  code page".
//
// u8string() всегда даёт UTF-8 без потерь; u8string-конструктор
// интерпретирует входные байты как UTF-8, а не как ANSI.
// -------------------------------------------------------------------------

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

struct FileBrowserState
{
    bool Open = false;
    bool RequestOpen = false;
    std::string Title = "Select File";
    std::filesystem::path CurrentDir;
    std::vector<std::string> Extensions;
    std::function<void(const std::string&)> OnSelect;
};

FileBrowserState g_Browser;

bool ExtensionMatches(const std::filesystem::path& p,
                      const std::vector<std::string>& exts)
{
    if (exts.empty()) return true;
    std::string e = PathToUtf8(p.extension());           // [FIX]
    std::transform(e.begin(), e.end(), e.begin(),
                   [](unsigned char c){ return (char)std::tolower(c); });
    for (const auto& x : exts)
        if (e == x) return true;
    return false;
}

ImTextureID ToImTex(uint32_t id)
{
    return reinterpret_cast<ImTextureID>(static_cast<intptr_t>(id));
}

uint32_t LoadUIIcon(const std::string& path)
{
    static std::unordered_map<std::string, uint32_t> s_Cache;

    auto it = s_Cache.find(path);
    if (it != s_Cache.end())
        return it->second;

    int w = 0, h = 0, ch = 0;
    stbi_set_flip_vertically_on_load(1);
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
    if (!data)
    {
        BINAX_LOG_WARN("[PropertyWidgets] UI icon not found at '{}' (stb: {})",
                       path, stbi_failure_reason());
        s_Cache[path] = 0;
        return 0;
    }

    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);

    s_Cache[path] = id;
    return id;
}

bool IconButton(const char* id, uint32_t iconID, const ImVec2& size, const char* fallbackLabel)
{
    if (iconID != 0)
    {
        return ImGui::ImageButton(
            id,
            ToImTex(iconID),
            size,
            ImVec2(0, 1), ImVec2(1, 0),
            ImVec4(0, 0, 0, 0),
            ImVec4(1, 1, 1, 1));
    }
    return ImGui::Button(fallbackLabel);
}

void DrawFileBrowserPopup()
{
    if (g_Browser.RequestOpen)
    {
        ImGui::OpenPopup(g_Browser.Title.c_str());
        g_Browser.RequestOpen = false;
        g_Browser.Open = true;
    }
    if (!g_Browser.Open) return;

    ImGui::SetNextWindowSize(ImVec2(640, 420), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal(g_Browser.Title.c_str(), nullptr,
                               ImGuiWindowFlags_NoSavedSettings))
    {
        const uint32_t folderIconID = LoadUIIcon("resources/icons/folder.png");
        const uint32_t upIconID     = LoadUIIcon("resources/icons/up.png");
        const uint32_t cancelIconID = LoadUIIcon("resources/icons/cancel.png");

        const float frameH    = ImGui::GetFrameHeight();
        const ImVec2 upSize   (frameH * 1.6f, frameH);
        const ImVec2 cancelSize(frameH * 2.4f, frameH);

        // ---------- Top bar ----------
        ImGui::PushID("UpBtn");
        if (IconButton("##up", upIconID, upSize, "Up"))
        {
            auto parent = g_Browser.CurrentDir.parent_path();
            std::error_code ec;
            if (!parent.empty() && std::filesystem::exists(parent, ec) && !ec)
                g_Browser.CurrentDir = parent;
        }
        ImGui::PopID();

        ImGui::SameLine();
        ImGui::TextWrapped("%s",
            PathToUtf8(g_Browser.CurrentDir).c_str());   // [FIX]
        ImGui::Separator();

        // ---------- Listing ----------
        if (ImGui::BeginChild("##listing", ImVec2(0, -32)))
        {
            const bool hasFolderIcon = (folderIconID != 0);

            std::error_code ec;
            if (std::filesystem::exists(g_Browser.CurrentDir, ec) && !ec)
            {
                std::vector<std::filesystem::path> dirs, files;
                for (const auto& entry :
                     std::filesystem::directory_iterator(g_Browser.CurrentDir, ec))
                {
                    if (ec) break;
                    std::error_code ec2;
                    if (entry.is_directory(ec2)) {
                        if (!ec2) dirs.push_back(entry.path());
                    } else if (entry.is_regular_file(ec2)) {
                        if (!ec2 && ExtensionMatches(entry.path(), g_Browser.Extensions))
                            files.push_back(entry.path());
                    }
                }
                std::sort(dirs.begin(), dirs.end());
                std::sort(files.begin(), files.end());

                const float lineH    = ImGui::GetTextLineHeight();
                const float iconSize = lineH + 4.0f;

                // ---- Directories ----
                for (const auto& d : dirs)
                {
                    const std::string name = PathToUtf8(d.filename());  // [FIX]

                    ImGui::PushID(name.c_str());
                    ImGui::PushStyleColor(ImGuiCol_Text,
                                          ImVec4(0.25f, 0.5f, 0.9f, 1.0f));

                    if (hasFolderIcon)
                    {
                        ImGui::Image(ToImTex(folderIconID),
                                     ImVec2(iconSize, iconSize),
                                     ImVec2(0, 1), ImVec2(1, 0));
                        ImGui::SameLine();
                    }

                    const std::string label = hasFolderIcon ? name : ("[D] " + name);

                    if (ImGui::Selectable(label.c_str(), false,
                                          ImGuiSelectableFlags_AllowDoubleClick))
                    {
                        if (ImGui::IsMouseDoubleClicked(0))
                            g_Browser.CurrentDir = d;
                    }

                    ImGui::PopStyleColor();
                    ImGui::PopID();
                }

                // ---- Files ----
                for (const auto& f : files)
                {
                    const std::string name = PathToUtf8(f.filename());  // [FIX]
                    ImGui::PushID(name.c_str());

                    if (ImGui::Selectable(name.c_str(), false,
                                          ImGuiSelectableFlags_AllowDoubleClick))
                    {
                        if (ImGui::IsMouseDoubleClicked(0) && g_Browser.OnSelect)
                        {
                            auto cb = g_Browser.OnSelect;
                            g_Browser.OnSelect = nullptr;
                            g_Browser.Open = false;
                            ImGui::CloseCurrentPopup();
                            cb(PathToUtf8(f));   // [FIX] возвращаем UTF-8
                        }
                    }
                    ImGui::PopID();
                }
            }
            else
            {
                ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Directory not found.");
            }
            ImGui::EndChild();
        }

        // ---------- Bottom bar ----------
        ImGui::PushID("CancelBtn");
        if (IconButton("##cancel", cancelIconID, cancelSize, "Cancel"))
        {
            g_Browser.Open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopID();

        ImGui::EndPopup();
    }
}

} // anon namespace

// ============================================================
// Public API
// ============================================================

void PropertyWidgets::OpenFileBrowser(const std::string& title,
                                      const std::vector<std::string>& extensions,
                                      std::function<void(const std::string&)> onSelect)
{
    g_Browser.Title = title;
    g_Browser.Extensions = extensions;
    g_Browser.OnSelect = std::move(onSelect);
    g_Browser.RequestOpen = true;

    if (g_Browser.CurrentDir.empty())
    {
        std::error_code ec;
        auto cwd = std::filesystem::current_path(ec);
        if (!ec) g_Browser.CurrentDir = cwd;
    }
}

void PropertyWidgets::DrawFileBrowser()
{
    DrawFileBrowserPopup();
}

bool PropertyWidgets::Draw(void* obj, const PropertyInfo& prop)
{
    if (prop.ReadOnly) ImGui::BeginDisabled();

    bool changed = false;
    const char* label = prop.Name.c_str();

    switch (prop.Kind)
    {
    case PropertyKind::Bool:
        changed = ImGui::Checkbox(label, prop.GetPtr<bool>(obj));
        break;

    case PropertyKind::Int:
    {
        int* v = prop.GetPtr<int>(obj);
        if (prop.HasRange) changed = ImGui::SliderInt(label, v, (int)prop.Min, (int)prop.Max);
        else               changed = ImGui::DragInt  (label, v, prop.Speed);
        break;
    }
    case PropertyKind::UInt:
    {
        uint32_t* v = prop.GetPtr<uint32_t>(obj);
        changed = ImGui::DragScalar(label, ImGuiDataType_U32, v, prop.Speed);
        break;
    }
    case PropertyKind::Float:
    {
        float* v = prop.GetPtr<float>(obj);
        if (prop.HasRange) changed = ImGui::SliderFloat(label, v, prop.Min, prop.Max);
        else               changed = ImGui::DragFloat  (label, v, prop.Speed);
        break;
    }
    case PropertyKind::Double:
    {
        double* v = prop.GetPtr<double>(obj);
        float tmp = static_cast<float>(*v);
        if (ImGui::DragFloat(label, &tmp, prop.Speed))
        {
            *v = static_cast<double>(tmp);
            changed = true;
        }
        break;
    }
    case PropertyKind::Vec2:
    {
        glm::vec2* v = prop.GetPtr<glm::vec2>(obj);
        changed = ImGui::DragFloat2(label, &v->x, prop.Speed);
        break;
    }
    case PropertyKind::Vec3:
    {
        glm::vec3* v = prop.GetPtr<glm::vec3>(obj);
        changed = ImGui::DragFloat3(label, &v->x, prop.Speed);
        break;
    }
    case PropertyKind::Vec4:
    {
        glm::vec4* v = prop.GetPtr<glm::vec4>(obj);
        changed = ImGui::DragFloat4(label, &v->x, prop.Speed);
        break;
    }
    case PropertyKind::Quat:
    {
        glm::quat* q = prop.GetPtr<glm::quat>(obj);
        glm::vec3 euler = glm::degrees(glm::eulerAngles(*q));
        if (ImGui::DragFloat3(label, &euler.x, 1.0f))
        {
            *q = glm::normalize(glm::quat(glm::radians(euler)));
            changed = true;
        }
        break;
    }
    case PropertyKind::Color3:
    {
        glm::vec3* v = prop.GetPtr<glm::vec3>(obj);
        changed = ImGui::ColorEdit3(label, &v->x);
        break;
    }
    case PropertyKind::Color4:
    {
        glm::vec4* v = prop.GetPtr<glm::vec4>(obj);
        changed = ImGui::ColorEdit4(label, &v->x);
        break;
    }
    case PropertyKind::String:
    {
        std::string* s = prop.GetPtr<std::string>(obj);
        std::vector<char> buf(s->begin(), s->end());
        buf.resize(s->size() + 512, '\0');
        if (ImGui::InputText(label, buf.data(), buf.size()))
        {
            *s = buf.data();
            changed = true;
        }
        break;
    }
    case PropertyKind::Enum:
    {
        int* v = prop.GetPtr<int>(obj);
        std::vector<const char*> names;
        names.reserve(prop.EnumValues.size());
        int current = 0;
        for (size_t i = 0; i < prop.EnumValues.size(); ++i)
        {
            names.push_back(prop.EnumValues[i].Name.c_str());
            if (prop.EnumValues[i].Value == *v) current = static_cast<int>(i);
        }
        if (!names.empty() &&
            ImGui::Combo(label, &current, names.data(), static_cast<int>(names.size())))
        {
            *v = prop.EnumValues[current].Value;
            changed = true;
        }
        break;
    }
    case PropertyKind::Asset:
    {
        std::string* path = prop.GetPtr<std::string>(obj);

        ImGui::TextUnformatted(label);
        ImGui::SameLine(170.0f);

        if (prop.AssetType == "Texture" && !path->empty())
        {
            const Texture* tex = AssetRegistry::Get().GetTexture(*path, /*srgb*/ true);
            if (tex && tex->GetID())
            {
                float h = 28.0f;
                float w = h * (float)tex->GetWidth() / (float)tex->GetHeight();
                if (w > 48.0f) w = 48.0f;
                ImGui::Image(ToImTex(tex->GetID()), ImVec2(w, h),
                             ImVec2(0, 1), ImVec2(1, 0));
                ImGui::SameLine();
            }
        }

        // [FIX] .filename() возвращает path, а не string.
        std::string display = path->empty()
            ? "(none)"
            : PathToUtf8(PathFromUtf8(*path).filename());

        ImGui::TextUnformatted(display.c_str());
        ImGui::SameLine();

        ImGui::PushID(label);

        if (ImGui::SmallButton("..."))
        {
            std::vector<std::string> exts = { ".png", ".jpg", ".jpeg",
                                              ".tga", ".bmp", ".hdr" };
            OpenFileBrowser("Select Texture", exts,
                [path](const std::string& chosen)
                {
                    *path = chosen;
                    AssetRegistry::Get().ReloadTexture(chosen);
                });
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("X"))
        {
            path->clear();
            changed = true;
        }
        ImGui::PopID();
        break;
    }
    default:
        ImGui::TextDisabled("%s (unsupported kind %d)", label, (int)prop.Kind);
        break;
    }

    if (prop.ReadOnly) ImGui::EndDisabled();
    return changed;
}

} // namespace Binax
