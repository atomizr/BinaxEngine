#pragma once

#include <glm/glm.hpp>

namespace Binax {


struct Vertex
{
    glm::vec3 Position;        
    glm::vec3 Normal;          
    glm::vec2 UV;              
    glm::u8vec4 BoneIndices;   
    glm::u8vec4 BoneWeights;   
};

static_assert(sizeof(Vertex) == 40, "Vertex must be exactly 40 bytes");

} 
