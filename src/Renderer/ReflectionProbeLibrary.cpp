#include "Renderer/ReflectionProbeLibrary.h"
#include "Utils/Log.h"

#include <glad/glad.h>
#include <algorithm>
#include <vector>
#include <fstream>

namespace Binax {

ReflectionProbeLibrary& ReflectionProbeLibrary::Get()
{
    static ReflectionProbeLibrary s;
    return s;
}

void ReflectionProbeLibrary::RecreateArray(uint32_t resolution, uint32_t count)
{
    count = std::min<uint32_t>(count, kMaxProbes);
    resolution = std::clamp<uint32_t>(resolution, 16u, 4096u);   // ← FIX #10

    const uint32_t oldCount      = m_Count;
    const uint32_t oldResolution = m_Resolution;
    const uint32_t oldTex        = m_TextureArray;
    const uint32_t oldFBO        = m_FBO;

    m_TextureArray = 0;
    m_FBO = 0;
    m_Count = 0;
    m_Resolution = resolution;

    if (count == 0)
    {
        if (oldTex) glDeleteTextures(1, &oldTex);
        if (oldFBO) glDeleteFramebuffers(1, &oldFBO);
        return;
    }

    glGenTextures(1, &m_TextureArray);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, m_TextureArray);
    glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_RGBA16F,
                 (GLsizei)resolution, (GLsizei)resolution,
                 (GLsizei)(count * 6), 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    // ---- Копируем старые слои, если разрешение совпадает ----------------
    if (oldTex && oldCount > 0 && oldResolution == resolution)
    {
        const uint32_t layersToCopy = std::min(oldCount, count);

        // FIX #21: старый путь делал FBO-switch + blit на каждый layer
        // (24 итерации при 4 пробах). glCopyImageSubData копирует весь
        // массив за один вызов — GL 4.3+ / ARB_copy_image.
        if (GLAD_GL_VERSION_4_3 || GLAD_GL_ARB_copy_image)
        {
            glCopyImageSubData(
                oldTex,         GL_TEXTURE_CUBE_MAP_ARRAY, 0, 0, 0, 0,
                m_TextureArray, GL_TEXTURE_CUBE_MAP_ARRAY, 0, 0, 0, 0,
                (GLsizei)resolution, (GLsizei)resolution,
                (GLsizei)(layersToCopy * 6));
        }
        else
        {
            // Fallback для очень старых драйверов (движку требуется 4.6,
            // но оставляем на случай отладки на чужой машине).
            GLint prevRead = 0, prevDraw = 0;
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevRead);
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw);

            GLuint tmpRead = 0, tmpDraw = 0;
            glGenFramebuffers(1, &tmpRead);
            glGenFramebuffers(1, &tmpDraw);

            for (uint32_t layer = 0; layer < layersToCopy * 6; ++layer)
            {
                glBindFramebuffer(GL_READ_FRAMEBUFFER, tmpRead);
                glFramebufferTextureLayer(GL_READ_FRAMEBUFFER,
                                          GL_COLOR_ATTACHMENT0,
                                          oldTex, 0, (GLint)layer);

                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, tmpDraw);
                glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER,
                                          GL_COLOR_ATTACHMENT0,
                                          m_TextureArray, 0, (GLint)layer);

                glBlitFramebuffer(0, 0, (GLint)oldResolution, (GLint)oldResolution,
                                  0, 0, (GLint)resolution, (GLint)resolution,
                                  GL_COLOR_BUFFER_BIT, GL_NEAREST);
            }

            glDeleteFramebuffers(1, &tmpRead);
            glDeleteFramebuffers(1, &tmpDraw);

            glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)prevRead);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)prevDraw);
        }
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);

    glGenFramebuffers(1, &m_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    m_Count = count;

    if (oldTex) glDeleteTextures(1, &oldTex);
    if (oldFBO) glDeleteFramebuffers(1, &oldFBO);

    BINAX_LOG_INFO("ReflectionProbeLibrary: created {} probes @ {}",
                   count, resolution);
}

uint32_t ReflectionProbeLibrary::Create(uint32_t resolution)
{
    if (m_Count == 0)
    {
        RecreateArray(resolution, 1);
        return 1;
    }

    if (m_Count >= kMaxProbes)
    {
        BINAX_LOG_WARN("ReflectionProbeLibrary: limit {} reached", kMaxProbes);
        return 0;
    }

    // Расширяем массив на 1 пробу, сохраняя resolution
    RecreateArray(m_Resolution, m_Count + 1);
    return m_Count;
}

void ReflectionProbeLibrary::Clear()
{
    if (m_TextureArray) glDeleteTextures(1, &m_TextureArray);
    if (m_FBO)          glDeleteFramebuffers(1, &m_FBO);
    m_TextureArray = 0;
    m_FBO = 0;
    m_Count = 0;
    m_Resolution = 0;
}

bool ReflectionProbeLibrary::BindCubeFace(uint32_t probeIdx, uint32_t face)
{
    if (probeIdx == 0 || probeIdx > m_Count || face >= 6) return false;
    if (!m_FBO || !m_TextureArray) return false;

    const uint32_t layer = (probeIdx - 1) * 6 + face;

    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              m_TextureArray, 0, (GLint)layer);
    glViewport(0, 0, (GLsizei)m_Resolution, (GLsizei)m_Resolution);

    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

void ReflectionProbeLibrary::Unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ReflectionProbeLibrary::BindTextureArray(uint32_t slot) const
{
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, m_TextureArray);
}

void ReflectionProbeLibrary::GenerateMipmaps()
{
    if (!m_TextureArray) return;
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, m_TextureArray);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP_ARRAY);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);
}

void ReflectionProbeLibrary::RecreateAtResolution(uint32_t resolution)
{
    if (m_Count == 0) return;
    RecreateArray(resolution, m_Count);
}

// ---------------------------------------------------------------------------
// Save / Load
// ---------------------------------------------------------------------------
namespace {
constexpr uint32_t kFileMagic   = 0x42505842u;  // "BXPB"
constexpr uint32_t kFileVersion = 1u;
}

bool ReflectionProbeLibrary::SaveToFile(const std::string& path) const
{
    if (m_Count == 0 || m_TextureArray == 0)
    {
        BINAX_LOG_WARN("ReflectionProbeLibrary::SaveToFile: nothing to save");
        return false;
    }

    const uint32_t pixels = m_Resolution * m_Resolution * 6 * m_Count;
    std::vector<uint16_t> data(pixels * 4);   // RGBA half-float

    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, m_TextureArray);
    glGetTexImage(GL_TEXTURE_CUBE_MAP_ARRAY, 0,
                  GL_RGBA, GL_HALF_FLOAT, data.data());
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f.is_open())
    {
        BINAX_LOG_ERROR("ReflectionProbeLibrary::SaveToFile: cannot write '{}'", path);
        return false;
    }

    f.write(reinterpret_cast<const char*>(&kFileMagic),   sizeof(kFileMagic));
    f.write(reinterpret_cast<const char*>(&kFileVersion), sizeof(kFileVersion));
    f.write(reinterpret_cast<const char*>(&m_Resolution), sizeof(m_Resolution));
    f.write(reinterpret_cast<const char*>(&m_Count),      sizeof(m_Count));
    f.write(reinterpret_cast<const char*>(data.data()),
            data.size() * sizeof(uint16_t));

    if (!f.good())
    {
        BINAX_LOG_ERROR("ReflectionProbeLibrary::SaveToFile: write failed '{}'", path);
        return false;
    }

    BINAX_LOG_INFO("ReflectionProbeLibrary: saved {} probes @ {} -> '{}' "
                   "({:.1f} MB)",
                   m_Count, m_Resolution, path,
                   (double)(data.size() * sizeof(uint16_t)) / (1024.0 * 1024.0));
    return true;
}

bool ReflectionProbeLibrary::LoadFromFile(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
    {
        BINAX_LOG_INFO("ReflectionProbeLibrary::LoadFromFile: no file '{}'", path);
        return false;
    }

    uint32_t magic = 0, version = 0, resolution = 0, count = 0;
    f.read(reinterpret_cast<char*>(&magic),      sizeof(magic));
    f.read(reinterpret_cast<char*>(&version),    sizeof(version));
    f.read(reinterpret_cast<char*>(&resolution), sizeof(resolution));
    f.read(reinterpret_cast<char*>(&count),      sizeof(count));

    if (magic != kFileMagic || version != kFileVersion)
    {
        BINAX_LOG_ERROR("ReflectionProbeLibrary::LoadFromFile: bad header in '{}'", path);
        return false;
    }
    if (resolution == 0 || count == 0 || count > kMaxProbes)
    {
        BINAX_LOG_ERROR("ReflectionProbeLibrary::LoadFromFile: invalid params "
                        "res={} count={}", resolution, count);
        return false;
    }

    // FIX #10: защита от переполнения uint32_t и неадекватных размеров.
    // pixels = res * res * 6 * count должен влезть в uint32_t и в память.
    constexpr uint32_t kMaxResolution = 4096;   // 4K кубемап — разумный потолок
    if (resolution > kMaxResolution)
    {
        BINAX_LOG_ERROR("ReflectionProbeLibrary::LoadFromFile: resolution {} "
                        "exceeds safe maximum {} in '{}'",
                        resolution, kMaxResolution, path);
        return false;
    }

    const uint64_t pixels64 =
        static_cast<uint64_t>(resolution) * resolution * 6ull * count;
    if (pixels64 == 0 || pixels64 > (1ull << 28))   // 268M пикселей ≈ 2 GB в half
    {
        BINAX_LOG_ERROR("ReflectionProbeLibrary::LoadFromFile: suspicious pixel "
                        "count {} in '{}'", pixels64, path);
        return false;
    }

    const uint32_t pixels = resolution * resolution * 6 * count;
    std::vector<uint16_t> data(pixels * 4);

    f.read(reinterpret_cast<char*>(data.data()), data.size() * sizeof(uint16_t));
    if (!f.good())
    {
        BINAX_LOG_ERROR("ReflectionProbeLibrary::LoadFromFile: short read '{}'", path);
        return false;
    }

    RecreateArray(resolution, count);

    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, m_TextureArray);
    glTexSubImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0,
                    0, 0, 0,
                    (GLsizei)resolution, (GLsizei)resolution,
                    (GLsizei)(count * 6),
                    GL_RGBA, GL_HALF_FLOAT, data.data());
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);

    GenerateMipmaps();

    BINAX_LOG_INFO("ReflectionProbeLibrary: loaded {} probes @ {} from '{}'",
                   count, resolution, path);
    return true;
}

}