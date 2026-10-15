#pragma once

#include "Renderer/Mesh.h"

namespace Binax {

struct MeshComponent
{
    const Mesh* MeshPtr = nullptr;
    // Параметры материала переехали в MaterialComponent.
    // MeshComponent теперь отвечает только за геометрию.
};

}