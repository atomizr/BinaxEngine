#pragma once
#include <cstdint>

namespace Binax {

// GL_TEXTURE_CUBE_MAP_ARRAY — по кубу на point light.
// Layer индекс в glFramebufferTextureLayer: cubeIndex * 6 + faceIndex.
class ShadowCubemap
{
public:
    ~ShadowCubemap();

    void Create(uint32_t size, uint32_t cubeCount);
    void Destroy();

    void BindCubeFace(uint32_t cubeIndex, uint32_t faceIndex);
    void Bind(uint32_t slot) const;

    uint32_t GetSize()   const { return m_Size; }
    uint32_t GetCubes()  const { return m_Cubes; }
    uint32_t GetTexture() const { return m_Texture; }

private:
    uint32_t m_Size    = 0;
    uint32_t m_Cubes   = 0;
    uint32_t m_FBO     = 0;
    uint32_t m_Texture = 0;
};

}