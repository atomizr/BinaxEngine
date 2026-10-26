#include "GBuffer.h"
#include <glad/glad.h>
#include <array>

namespace Binax {

GBuffer::GBuffer(uint32_t width, uint32_t height)
    : Framebuffer(width, height)
{
    Init(width, height);
}

void GBuffer::Init(uint32_t width, uint32_t height)
{
    Bind();

    
    AttachColorTexture(GL_RGB16F, GL_RGB, GL_FLOAT, POSITION);

    
    
    AttachColorTexture(GL_RGB16F, GL_RGB, GL_FLOAT, NORMAL);

    
    AttachColorTexture(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, ALBEDO_AO);

    
    
    AttachColorTexture(GL_RGBA16F, GL_RGBA, GL_FLOAT, MATERIAL);

    
    
    AttachColorTexture(GL_RG16F, GL_RG, GL_FLOAT, VELOCITY);

    AttachDepthStencilTexture(GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL,
                              GL_UNSIGNED_INT_24_8);

    std::array<GLenum, COUNT> attachments = {
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2,
        GL_COLOR_ATTACHMENT3,
        GL_COLOR_ATTACHMENT4
    };
    glDrawBuffers(COUNT, attachments.data());

    if (!IsComplete())
        CheckStatus();

    Unbind();
}

void GBuffer::BindTextures(uint32_t startSlot, uint32_t count) const
{
    const uint32_t n = (count > COUNT) ? COUNT : count;
    for (uint32_t i = 0; i < n; ++i)
    {
        glActiveTexture(GL_TEXTURE0 + startSlot + i);
        glBindTexture(GL_TEXTURE_2D, GetColorAttachment(i));
    }
}

}
