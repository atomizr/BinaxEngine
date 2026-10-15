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

    // ---- Post-processing ----
    bool  FXAAEnabled          = true;    // включить FXAA для этой камеры
    float FXAAEdgeThreshold    = 0.063f;  // относительный порог (0.05..0.15)
    float FXAAEdgeThresholdMin = 0.0312f; // абсолютный порог  (0.01..0.05)
    float FXAASubpixelBlending = 1.0f;    // 0..1, борьба с субпиксельным алиасингом
};

}
