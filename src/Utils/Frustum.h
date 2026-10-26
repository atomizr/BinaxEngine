#pragma once

#include <glm/glm.hpp>
#include <array>

namespace Binax {




struct Frustum
{
    
    
    
    std::array<glm::vec4, 6> Planes{};

    static Frustum FromViewProjection(const glm::mat4& viewProj);
};

inline Frustum Frustum::FromViewProjection(const glm::mat4& viewProj)
{
    
    
    const glm::vec4 r0(viewProj[0][0], viewProj[1][0], viewProj[2][0], viewProj[3][0]);
    const glm::vec4 r1(viewProj[0][1], viewProj[1][1], viewProj[2][1], viewProj[3][1]);
    const glm::vec4 r2(viewProj[0][2], viewProj[1][2], viewProj[2][2], viewProj[3][2]);
    const glm::vec4 r3(viewProj[0][3], viewProj[1][3], viewProj[2][3], viewProj[3][3]);

    Frustum f;
    f.Planes[0] = r3 + r0;   
    f.Planes[1] = r3 - r0;   
    f.Planes[2] = r3 + r1;   
    f.Planes[3] = r3 - r1;   
    f.Planes[4] = r3 + r2;   
    f.Planes[5] = r3 - r2;   

    
    
    for (glm::vec4& p : f.Planes)
    {
        const float len = glm::length(glm::vec3(p));
        if (len > 1e-8f)
            p /= len;
    }

    return f;
}

} 
