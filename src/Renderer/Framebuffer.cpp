
#include "Framebuffer.h"
#include "../Utils/Log.h"

namespace Binax {

Framebuffer::Framebuffer(uint32_t width, uint32_t height)
    : m_Width(width), m_Height(height)
{
    glGenFramebuffers(1, &m_RendererID);
    Bind();
}

Framebuffer::~Framebuffer()
{
    for (const auto& att : m_ColorAttachments)
        if (att.TextureID)
            glDeleteTextures(1, &att.TextureID);

    if (m_DepthAttachment.TextureID)
        glDeleteTextures(1, &m_DepthAttachment.TextureID);

    if (m_RendererID)
        glDeleteFramebuffers(1, &m_RendererID);
}

void Framebuffer::Bind() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
}

void Framebuffer::Unbind() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::Resize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;
    if (width == m_Width && height == m_Height)
        return;

    
    GLint prevFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);

    m_Width  = width;
    m_Height = height;

    glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);

    for (size_t i = 0; i < m_ColorAttachments.size(); ++i)
        RecreateColorAttachment(static_cast<uint32_t>(i));

    if (m_DepthAttachment.TextureID != 0)
        RecreateDepthAttachment();

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFBO));
}

void Framebuffer::AttachColorTexture(uint32_t internalFormat, uint32_t format,
                                     uint32_t type, uint32_t index)
{
    if (index >= m_ColorAttachments.size())
        m_ColorAttachments.resize(index + 1);

    auto& att = m_ColorAttachments[index];
    att.InternalFormat = internalFormat;
    att.Format         = format;
    att.Type           = type;

    
    RecreateColorAttachment(index);
}

void Framebuffer::AttachDepthStencilTexture(uint32_t internalFormat, uint32_t format,
                                            uint32_t type)
{
    m_DepthAttachment.InternalFormat = internalFormat;
    m_DepthAttachment.Format         = format;
    m_DepthAttachment.Type           = type;

    
    RecreateDepthAttachment();
}

bool Framebuffer::IsComplete() const
{
    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

void Framebuffer::CheckStatus()
{
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        BINAX_LOG_ERROR("Framebuffer incomplete: {0:x}", status);
    }
}

void Framebuffer::RecreateColorAttachment(uint32_t index)
{
    auto& att = m_ColorAttachments[index];

    if (att.TextureID)
        glDeleteTextures(1, &att.TextureID);

    glGenTextures(1, &att.TextureID);
    glBindTexture(GL_TEXTURE_2D, att.TextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, att.InternalFormat, m_Width, m_Height,
                 0, att.Format, att.Type, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index,
                           GL_TEXTURE_2D, att.TextureID, 0);
}

void Framebuffer::RecreateDepthAttachment()
{
    auto& att = m_DepthAttachment;

    if (att.TextureID)
        glDeleteTextures(1, &att.TextureID);

    glGenTextures(1, &att.TextureID);
    glBindTexture(GL_TEXTURE_2D, att.TextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, att.InternalFormat, m_Width, m_Height,
                 0, att.Format, att.Type, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                           GL_TEXTURE_2D, att.TextureID, 0);
}

} 
