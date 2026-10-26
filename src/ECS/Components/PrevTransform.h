#pragma once

#include <glm/glm.hpp>

namespace Binax {




struct PrevTransform
{
    glm::mat4 PrevModel = glm::mat4(1.0f);
    bool      Valid     = false;
};

} 
