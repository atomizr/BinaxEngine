#include "Editor/Console.h"
#include "Utils/LogSink.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace Binax {

namespace {

ImU32 LevelColor(spdlog::level::level_enum lvl)
{
    using L = spdlog::level::level_enum;
    switch (lvl)
    {
        case L::trace:    return IM_COL32(150, 150, 150, 255);
        case L::debug:    return IM_COL32(170, 200, 230, 255);
        case L::info:     return IM_COL32(220, 220, 220, 255);
        case L::warn:     return IM_COL32(240, 200, 100, 255);
        case L::err:      return IM_COL32(240, 100, 100, 255);
        case L::critical: return IM_COL32(255, 80, 200, 255);
        default:          return IM_COL32(200, 200, 200, 255);
    }
}

const char* LevelName(spdlog::level::level_enum lvl)
{
    using L = spdlog::level::level_enum;
    switch (lvl)
    {
        case L::trace:    return "TRACE";
        case L::debug:    return "DEBUG";
        case L::info:     return "INFO";
        case L::warn:     return "WARN";
        case L::err:      return "ERROR";
        case L::critical: return "CRIT";
        default:          return "?";
    }
}

std::string ToLower(const std::string& s)
{
    std::string out = s;
    for (char& c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

} // namespace

void Console::Draw()
{
    if (!m_Visible)
        return;

    if (!ImGui::Begin("Console", &m_Visible, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    // === Тулбар ===
    if (ImGui::Button("Clear"))
        LogSink::Instance()->Clear();

    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &m_AutoScroll);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputTextWithHint("##filter", "Filter...", m_Filter, sizeof(m_Filter));

    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    const char* levels[] = { "Trace", "Debug", "Info", "Warn", "Error", "Critical" };
    ImGui::Combo("##level", &m_MinLevel, levels, 6);

    ImGui::Separator();

    // === Лог-область ===
    const float footer = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();

    if (ImGui::BeginChild("##log", ImVec2(0, -footer), false,
                          ImGuiWindowFlags_HorizontalScrollbar))
    {
        const auto entries = LogSink::Instance()->Snapshot();
        const std::string filter_lower = ToLower(m_Filter);

        for (const auto& e : entries)
        {
            if (static_cast<int>(e.Level) < m_MinLevel)
                continue;

            if (!filter_lower.empty() &&
                ToLower(e.Message).find(filter_lower) == std::string::npos)
                continue;

            ImGui::PushStyleColor(ImGuiCol_Text,
                ImGui::ColorConvertU32ToFloat4(LevelColor(e.Level)));
            ImGui::TextUnformatted(("[ " + std::string(LevelName(e.Level)) + " ] " +
                                    e.Message).c_str());
            ImGui::PopStyleColor();
        }

        if (m_AutoScroll)
            ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    ImGui::End();
}

} // namespace Binax