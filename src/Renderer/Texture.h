#pragma once

#include <cstdint>
#include <string>
#include <memory>

namespace Binax {

class Texture
{
public:
    Texture() = default;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    // Loads via stb_image. srgb=true for color textures (albedo), false for data (normal).
    // Returns nullptr on failure.
    static std::unique_ptr<Texture> LoadFromFile(const std::string& path, bool srgb);

    // 1x1 solid-color texture (used for fallbacks).
    static std::unique_ptr<Texture> CreateSolid(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);

    void Bind(uint32_t slot = 0) const;
    void Unbind(uint32_t slot = 0) const;

    uint32_t GetID() const { return m_ID; }
    int GetWidth()  const { return m_Width; }
    int GetHeight() const { return m_Height; }
    const std::string& GetPath() const { return m_Path; }

private:
    uint32_t m_ID = 0;
    int m_Width = 0;
    int m_Height = 0;
    int m_Channels = 0;
    std::string m_Path;
};

}