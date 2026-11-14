#include "Renderer/ShadowCubemap.h"
#include <glad/glad.h>

namespace Binax {

ShadowCubemap::~ShadowCubemap() { Destroy(); }

void ShadowCubemap::Destroy()
{
    if (m_Texture) { glDeleteTextures(1, &m_Texture); m_Texture = 0; }
    if (m_FBO)     { glDeleteFramebuffers(1, &m_FBO); m_FBO = 0; }
    m_Size = 0; m_Cubes = 0;
}

void ShadowCubemap::Create(uint32_t size, uint32_t cubeCount)
{
    Destroy();
    m_Size  = size;
    m_Cubes = cubeCount;

    glGenTextures(1, &m_Texture);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, m_Texture);
    glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_DEPTH_COMPONENT32F,
                 (GLsizei)size, (GLsizei)size, (GLsizei)(6 * cubeCount),
                 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);

    glGenFramebuffers(1, &m_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ShadowCubemap::BindCubeFace(uint32_t cubeIndex, uint32_t faceIndex)
{
    const uint32_t layer = cubeIndex * 6 + faceIndex;
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              m_Texture, 0, (GLint)layer);
    glViewport(0, 0, (GLsizei)m_Size, (GLsizei)m_Size);
}

void ShadowCubemap::Bind(uint32_t slot) const
{
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, m_Texture);
}

}