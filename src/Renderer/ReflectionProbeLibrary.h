#pragma once

#include <cstdint>
#include <string>

namespace Binax {

class ReflectionProbeLibrary
{
public:
    static ReflectionProbeLibrary& Get();

    static constexpr uint32_t kMaxProbes       = 4;
    static constexpr uint32_t kDefaultResolution = 256;

    // Возвращает 1-based индекс (0 = ошибка/нет места).
    // Первая проба задаёт resolution для всех.
    uint32_t Create(uint32_t resolution);

    void Clear();

    uint32_t GetCount()      const { return m_Count; }
    uint32_t GetResolution() const { return m_Resolution; }

    // Bake target
    bool BindCubeFace(uint32_t probeIdx, uint32_t face);
    void Unbind();

    // Lighting sample
    void BindTextureArray(uint32_t slot) const;
    void GenerateMipmaps();

    uint32_t GetTextureArray() const { return m_TextureArray; }
        // Полная пересоздание массива на новом разрешении — все данные теряются
    void RecreateAtResolution(uint32_t resolution);

    // Persistence
    bool SaveToFile(const std::string& path) const;
    bool LoadFromFile(const std::string& path);

private:
    ReflectionProbeLibrary() = default;
    void RecreateArray(uint32_t resolution, uint32_t count);

    uint32_t m_TextureArray = 0;
    uint32_t m_FBO          = 0;
    uint32_t m_Resolution   = 0;
    uint32_t m_Count        = 0;
};

}