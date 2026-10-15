#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <memory>

namespace Binax {

class Cubemap
{
public:
    Cubemap() = default;
    ~Cubemap();

    Cubemap(const Cubemap&) = delete;
    Cubemap& operator=(const Cubemap&) = delete;
    Cubemap(Cubemap&& other) noexcept;
    Cubemap& operator=(Cubemap&& other) noexcept;

    // Face order matches GL_TEXTURE_CUBE_MAP_POSITIVE_X + i:
    //   [0] +X (right)   [1] -X (left)
    //   [2] +Y (top)     [3] -Y (bottom)
    //   [4] +Z (front)   [5] -Z (back)
    // srgb=true for color skyboxes (PNG/JPG), false for HDR-ish data.
    static std::unique_ptr<Cubemap> LoadFromFiles(const std::array<std::string, 6>& paths, bool srgb = true);

    void Bind(uint32_t slot = 0) const;
    void Unbind(uint32_t slot = 0) const;

    uint32_t GetID()   const { return m_ID; }
    int      GetSize() const { return m_Size; }

private:
    uint32_t m_ID = 0;
    int      m_Size = 0;
};

}