#pragma once
#include <cstdint>

namespace Binax {

class ShadowMap
{
public:
    ~ShadowMap();

    void Create(uint32_t size, uint32_t layers);
    void Destroy();

    void BindLayer(uint32_t layer);   // привязка FBO + viewport
    void Bind(uint32_t slot) const;   // привязка текстуры (sampler2DArray)

    uint32_t GetSize()   const { return m_Size; }
    uint32_t GetLayers() const { return m_Layers; }
    uint32_t GetTexture() const { return m_Texture; }

private:
    uint32_t m_Size = 0;
    uint32_t m_Layers = 0;
    uint32_t m_FBO = 0;
    uint32_t m_Texture = 0;
};

}