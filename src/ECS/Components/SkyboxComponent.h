#pragma once

#include <string>

namespace Binax {

struct SkyboxComponent
{
    bool Enabled = true;

    // Face order: +X, -X, +Y, -Y, +Z, -Z
    std::string FacePX;
    std::string FaceNX;
    std::string FacePY;
    std::string FaceNY;
    std::string FacePZ;
    std::string FaceNZ;

    // Y-axis rotation, degrees
    float Rotation = 0.0f;

    // HDR multiplier (1.0 = as authored)
    float Brightness = 1.0f;
};

}