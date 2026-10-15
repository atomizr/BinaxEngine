
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Binax {

struct Transform
{
    glm::vec3 Position = glm::vec3(0.0f);
    glm::quat Rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 Scale = glm::vec3(1.0f);

    glm::mat4 GetModelMatrix() const
    {
        glm::mat4 mat = glm::translate(glm::mat4(1.0f), Position);
        mat *= glm::mat4_cast(Rotation);
        mat = glm::scale(mat, Scale);
        return mat;
    }
};

} 