
#pragma once

#include <glad/glad.h>
#include <vector>
#include <cstdint>

namespace Binax {

class Framebuffer
{
public:
    Framebuffer(uint32_t width, uint32_t height);
    virtual ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    void Bind() const;
    void Unbind() const;

    
    
    
    void Resize(uint32_t width, uint32_t height);

    void AttachColorTexture(uint32_t internalFormat, uint32_t format,
                            uint32_t type, uint32_t index);
    void AttachDepthStencilTexture(uint32_t internalFormat, uint32_t format,
                                   uint32_t type);

    uint32_t GetColorAttachment(uint32_t index) const { return m_ColorAttachments[index].TextureID; }
    uint32_t GetDepthAttachment() const { return m_DepthAttachment.TextureID; }

    uint32_t GetWidth()  const { return m_Width; }
    uint32_t GetHeight() const { return m_Height; }

    bool IsComplete() const;
    void CheckStatus();

protected:
    struct ColorAttachmentDesc
    {
        uint32_t TextureID      = 0;
        uint32_t InternalFormat = 0;
        uint32_t Format         = 0;
        uint32_t Type           = 0;
    };

    struct DepthAttachmentDesc
    {
        uint32_t TextureID      = 0;
        uint32_t InternalFormat = 0;
        uint32_t Format         = 0;
        uint32_t Type           = 0;
    };

    uint32_t m_RendererID = 0;
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;

    std::vector<ColorAttachmentDesc> m_ColorAttachments;
    DepthAttachmentDesc m_DepthAttachment;

private:
    
    
    void RecreateColorAttachment(uint32_t index);
    void RecreateDepthAttachment();
};

} 
