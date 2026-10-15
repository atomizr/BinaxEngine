
#pragma once

#include <glm/glm.hpp>

namespace Binax {

enum class ProjectionType
{
    Perspective,
    Orthographic
};

struct CameraComponent
{
    ProjectionType Projection = ProjectionType::Perspective;
    float FOV = 45.0f;                
    float NearPlane = 0.1f;
    float FarPlane = 100.0f;
    float OrthoSize = 5.0f;           
    float AspectRatio = 1.777f;       
    bool Primary = false;             
};

} 