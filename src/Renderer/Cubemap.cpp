#include "Cubemap.h"
#include "Utils/Log.h"
#include "Utils/FileIO.h"

#include <glad/glad.h>
#include <stb/stb_image.h>

namespace Binax {

Cubemap::~Cubemap()
{
    if (m_ID) glDeleteTextures(1, &m_ID);
}

Cubemap::Cubemap(Cubemap&& other) noexcept
    : m_ID(other.m_ID), m_Size(other.m_Size)
{
    other.m_ID = 0;
}

Cubemap& Cubemap::operator=(Cubemap&& other) noexcept
{
    if (this != &other)
    {
        if (m_ID) glDeleteTextures(1, &m_ID);
        m_ID       = other.m_ID;
        m_Size     = other.m_Size;
        other.m_ID = 0;
    }
    return *this;
}

std::unique_ptr<Cubemap> Cubemap::LoadFromFiles(const std::array<std::string, 6>& paths, bool srgb)
{
    stbi_set_flip_vertically_on_load(0);

    auto cube = std::unique_ptr<Cubemap>(new Cubemap());
    glGenTextures(1, &cube->m_ID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube->m_ID);

    int faceSize = 0;

    for (int i = 0; i < 6; ++i)
    {
        if (paths[i].empty())
        {
            BINAX_LOG_ERROR("Cubemap face {} path is empty", i);
            glDeleteTextures(1, &cube->m_ID);
            cube->m_ID = 0;
            return nullptr;
        }

        
        std::FILE* file = OpenFileUtf8(paths[i]);
        if (!file)
        {
            BINAX_LOG_ERROR("Cubemap face {}: cannot open '{}'", i, paths[i]);
            glDeleteTextures(1, &cube->m_ID);
            cube->m_ID = 0;
            return nullptr;
        }

        int w = 0, h = 0, ch = 0;
        unsigned char* data = stbi_load_from_file(file, &w, &h, &ch, 0);
        std::fclose(file);

        if (!data)
        {
            BINAX_LOG_ERROR("Cubemap face {} load failed '{}': {}",
                            i, paths[i], stbi_failure_reason());
            glDeleteTextures(1, &cube->m_ID);
            cube->m_ID = 0;
            return nullptr;
        }

        if (w != h)
        {
            BINAX_LOG_ERROR("Cubemap face {} not square ({}x{})", i, w, h);
            stbi_image_free(data);
            glDeleteTextures(1, &cube->m_ID);
            cube->m_ID = 0;
            return nullptr;
        }

        if (faceSize == 0)
            faceSize = w;
        else if (w != faceSize)
        {
            
            
            BINAX_LOG_ERROR("Cubemap face {} size {}x{} != first face size {}x{}",
                            i, w, h, faceSize, faceSize);
            stbi_image_free(data);
            glDeleteTextures(1, &cube->m_ID);
            cube->m_ID = 0;
            return nullptr;
        }

        GLenum internalFmt = GL_RGB8;
        GLenum fmt         = GL_RGB;
        
        
        if (ch == 1)      { internalFmt = GL_R8;  fmt = GL_RED; }
        else if (ch == 2) { internalFmt = GL_RG8; fmt = GL_RG;  }
        else if (ch == 3) { internalFmt = srgb ? GL_SRGB8        : GL_RGB8;  fmt = GL_RGB;  }
        else if (ch == 4) { internalFmt = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8; fmt = GL_RGBA; }

        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0,
                     internalFmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

#ifdef GL_TEXTURE_MAX_ANISOTROPY_EXT
    if (GLAD_GL_EXT_texture_filter_anisotropic)
    {
        GLfloat maxAniso = 1.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
        if (maxAniso > 1.0f)
            glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAniso);
    }
#endif

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    cube->m_Size = faceSize;
    return cube;
}

void Cubemap::Bind(uint32_t slot) const
{
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_ID);
}

void Cubemap::Unbind(uint32_t slot) const
{
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

} 
