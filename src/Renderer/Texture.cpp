#include "Texture.h"
#include "Utils/Log.h"

#include <glad/glad.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>   // <- thirdparty/stb/stb_image.h

namespace Binax {

Texture::~Texture()
{
    if (m_ID)
        glDeleteTextures(1, &m_ID);
}

Texture::Texture(Texture&& other) noexcept
    : m_ID(other.m_ID)
    , m_Width(other.m_Width)
    , m_Height(other.m_Height)
    , m_Channels(other.m_Channels)
    , m_Path(std::move(other.m_Path))
{
    other.m_ID = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept
{
    if (this != &other)
    {
        if (m_ID) glDeleteTextures(1, &m_ID);
        m_ID       = other.m_ID;
        m_Width    = other.m_Width;
        m_Height   = other.m_Height;
        m_Channels = other.m_Channels;
        m_Path     = std::move(other.m_Path);
        other.m_ID = 0;
    }
    return *this;
}

std::unique_ptr<Texture> Texture::LoadFromFile(const std::string& path, bool srgb)
{
    // Flip so UV (0,0) = bottom-left → matches our mesh UV layout.
    stbi_set_flip_vertically_on_load(1);

    int w = 0, h = 0, ch = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 0);
    if (!data)
    {
        BINAX_LOG_ERROR("Texture load failed '{}': {}", path, stbi_failure_reason());
        return nullptr;
    }

    GLenum internalFmt = GL_RGB8;
    GLenum fmt         = GL_RGB;

    if (ch == 1)      { internalFmt = GL_R8;  fmt = GL_RED;  }
    else if (ch == 3) { internalFmt = srgb ? GL_SRGB8        : GL_RGB8;  fmt = GL_RGB;  }
    else if (ch == 4) { internalFmt = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8; fmt = GL_RGBA; }

    auto tex = std::unique_ptr<Texture>(new Texture());
    glGenTextures(1, &tex->m_ID);
    glBindTexture(GL_TEXTURE_2D, tex->m_ID);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

#ifdef GL_TEXTURE_MAX_ANISOTROPY_EXT
    // Anisotropic filtering (EXT_texture_filter_anisotropic)
    if (GLAD_GL_EXT_texture_filter_anisotropic)
    {
        GLfloat maxAniso = 1.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
        if (maxAniso > 1.0f)
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAniso);
    }
#endif

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);

    tex->m_Width    = w;
    tex->m_Height   = h;
    tex->m_Channels = ch;
    tex->m_Path     = path;
    return tex;
}

std::unique_ptr<Texture> Texture::CreateSolid(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    auto tex = std::unique_ptr<Texture>(new Texture());
    uint8_t px[4] = { r, g, b, a };

    glGenTextures(1, &tex->m_ID);
    glBindTexture(GL_TEXTURE_2D, tex->m_ID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    tex->m_Width    = 1;
    tex->m_Height   = 1;
    tex->m_Channels = 4;
    return tex;
}

void Texture::Bind(uint32_t slot) const
{
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, m_ID);
}

void Texture::Unbind(uint32_t slot) const
{
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, 0);
}

}