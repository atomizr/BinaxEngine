#pragma once

#include "Reflect/Reflection.h"
#include <functional>
#include <string>
#include <vector>

namespace Binax {

class PropertyWidgets
{
public:
    static bool Draw(void* obj, const Reflect::PropertyInfo& prop);

    // Opens a modal file browser; callback fires when user picks a file.
    static void OpenFileBrowser(const std::string& title,
                                const std::vector<std::string>& extensions,
                                std::function<void(const std::string&)> onSelect);

    // Call once per frame (before ImGui::Render()).
    static void DrawFileBrowser();
};

}