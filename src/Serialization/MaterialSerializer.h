#pragma once

#include "Renderer/MaterialLibrary.h"
#include <string>

namespace Binax
{



class MaterialSerializer
{
public:
    static constexpr int kVersion = 1;

    static bool Save(const MaterialLibrary& lib, MaterialHandle handle,
                     const std::string& filepath);

    
    static bool SaveAll(const MaterialLibrary& lib, const std::string& directory);

    static bool Load(MaterialLibrary& lib, const std::string& filepath);

    
    static void LoadAll(MaterialLibrary& lib, const std::string& directory);

private:
    static std::string SanitizeFileName(const std::string& name);
};

}
