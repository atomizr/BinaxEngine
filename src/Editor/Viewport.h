
#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace Binax {

class Viewport
{
public:
    Viewport(uint32_t width, uint32_t height);
    ~Viewport();

    void Resize(uint32_t width, uint32_t height);
    void Bind() const;
    void Unbind() const;
    uint32_t GetTextureID() const { return m_TextureID; }
    uint32_t GetFBO() const { return m_FBO; }
    glm::vec2 GetSize() const { return glm::vec2(m_Width, m_Height); }
    float GetAspect() const { return m_Height > 0 ? static_cast<float>(m_Width) / static_cast<float>(m_Height) : 1.0f; }

private:
    uint32_t m_FBO;
    uint32_t m_TextureID;
    uint32_t m_RBO;
    uint32_t m_Width, m_Height;
};

} 