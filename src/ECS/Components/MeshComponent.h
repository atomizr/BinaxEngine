#pragma once

#include "Renderer/MeshLibrary.h"
#include <string>

namespace Binax {

struct MeshComponent
{
    MeshHandle  MeshId     = kInvalidMesh;
    // Путь к кэш-файлу (.bxmesh) для импортированных мешей.
    // Пустой для встроенных (Cube, FullscreenQuad). НЕ транзиентный.
    std::string SourcePath;
};

}