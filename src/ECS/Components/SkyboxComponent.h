#pragma once

#include <string>

namespace Binax {

struct SkyboxComponent
{
    bool Enabled = true;

    
    std::string FacePX;
    std::string FaceNX;
    std::string FacePY;
    std::string FaceNY;
    std::string FacePZ;
    std::string FaceNZ;

    
    float Rotation = 0.0f;

    
    float Brightness = 1.0f;
};

}