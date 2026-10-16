#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "ECS/Components/CameraComponent.h"
#include "ECS/Components/Transform.h"

namespace Binax {

struct CameraData
{
    glm::mat4 View;
    glm::mat4 Projection;
    glm::vec3 Position;
    bool IsValid = false;
};

class CameraSystem
{
public:
    static CameraData GetActiveCamera(entt::registry& registry, float defaultAspect)
    {
        CameraData result;
        result.IsValid = false;

        auto view = registry.view<Transform, CameraComponent>();
        for (auto entity : view)
        {
            const auto& transform = view.get<Transform>(entity);
            const auto& camera    = view.get<CameraComponent>(entity);

            if (camera.Primary || !result.IsValid)
            {
                // ВАЖНО: учитываем поворот Transform. Раньше камера всегда
                // смотрела в -Z независимо от Rotation — это был баг.
                const glm::vec3 forward = transform.Rotation * glm::vec3(0.0f, 0.0f, -1.0f);
                const glm::vec3 up      = transform.Rotation * glm::vec3(0.0f, 1.0f,  0.0f);

                result.Position = transform.Position;
                result.View = glm::lookAt(transform.Position,
                                          transform.Position + forward,
                                          up);

                float aspect = camera.AspectRatio > 0.0f ? camera.AspectRatio : defaultAspect;
                if (camera.Projection == ProjectionType::Perspective)
                {
                    result.Projection = glm::perspective(glm::radians(camera.FOV), aspect,
                                                         camera.NearPlane, camera.FarPlane);
                }
                else
                {
                    float halfSize = camera.OrthoSize;
                    float width  = halfSize * aspect;
                    float height = halfSize;
                    result.Projection = glm::ortho(-width, width, -height, height,
                                                   camera.NearPlane, camera.FarPlane);
                }

                result.IsValid = true;

                if (camera.Primary)
                    break;
            }
        }
        return result;
    }
};

} // namespace Binax
