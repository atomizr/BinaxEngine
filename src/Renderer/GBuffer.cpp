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

    // POSITION: world-space position (HDR)
    AttachColorTexture(GL_RGB16F, GL_RGB, GL_FLOAT, POSITION);

    // NORMAL: world-space normal (HDR — на будущее, для нормалей достаточно
    // RGB16F, чтобы избежать бэндинга при интерполяции)
    AttachColorTexture(GL_RGB16F, GL_RGB, GL_FLOAT, NORMAL);

    // ALBEDO_AO: RGB = linear base color, A = ambient occlusion
    AttachColorTexture(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, ALBEDO_AO);

    // MATERIAL: R = metallic, G = roughness, B = emissive, A = reserved
    // RGBA16F — чтобы emissive мог быть > 1.0 (HDR / bloom-ready)
    AttachColorTexture(GL_RGBA16F, GL_RGBA, GL_FLOAT, MATERIAL);

    AttachDepthStencilTexture(GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL,
                              GL_UNSIGNED_INT_24_8);

    std::array<GLenum, COUNT> attachments = {
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2,
        GL_COLOR_ATTACHMENT3
    };
    glDrawBuffers(COUNT, attachments.data());

    if (!IsComplete())
        CheckStatus();

    Unbind();
}

void GBuffer::BindTextures(uint32_t startSlot) const
{
    for (uint32_t i = 0; i < COUNT; ++i)
    {
        glActiveTexture(GL_TEXTURE0 + startSlot + i);
        glBindTexture(GL_TEXTURE_2D, GetColorAttachment(i));
    }
}

}