#include "Renderer/ShadowMap.h"
#include <glad/glad.h>

namespace Binax {

ShadowMap::~ShadowMap() { Destroy(); }

void ShadowMap::Destroy()
{
    if (m_Texture) { glDeleteTextures(1, &m_Texture); m_Texture = 0; }
    if (m_FBO)     { glDeleteFramebuffers(1, &m_FBO); m_FBO = 0; }
    m_Size = 0; m_Layers = 0;
}

void ShadowMap::Create(uint32_t size, uint32_t layers)
{
    Destroy();
    m_Size = size;
    m_Layers = layers;

    glGenTextures(1, &m_Texture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_Texture);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F,
                 (GLsizei)size, (GLsizei)size, (GLsizei)layers,
                 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
    const float border[4] = { 1.0f, 1.0f, 1.0f, 1.0f };   // вне карты = не в тени
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, border);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    glGenFramebuffers(1, &m_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_Texture, 0, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ShadowMap::BindLayer(uint32_t layer)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_Texture, 0, (GLint)layer);
    glViewport(0, 0, (GLsizei)m_Size, (GLsizei)m_Size);
}

void ShadowMap::Bind(uint32_t slot) const
{
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_Texture);
}

}