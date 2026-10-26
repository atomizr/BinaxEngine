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

    
    static void OpenFileBrowser(const std::string& title,
                                const std::vector<std::string>& extensions,
                                std::function<void(const std::string&)> onSelect);

    
    static void DrawFileBrowser();

    
    
    
    static bool IsFileBrowserOpen();
};

}
