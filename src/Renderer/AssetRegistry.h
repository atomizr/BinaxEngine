#pragma once

#include "Texture.h"
#include "Cubemap.h"
#include "Renderer/TextureHandle.h"
#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace Binax {

class AssetRegistry
{
public:
    static AssetRegistry& Get();

    // --- 2D textures ---
    const Texture* GetTexture(const std::string& path, bool srgb = true);

    // Bindless handle (0, если texture не загрузилась или bindless недоступен).
    TextureHandle GetTextureHandle(const std::string& path, bool srgb = true);

    void ReloadTexture(const std::string& path);

    // --- Cubemaps ---
    const Cubemap* GetCubemap(const std::array<std::string, 6>& paths, bool srgb = true);
    void ReloadCubemap(const std::array<std::string, 6>& paths);

    void Clear();

private:
    AssetRegistry() = default;

    static std::string MakeCubemapKey(const std::array<std::string, 6>& paths);

    std::unordered_map<std::string, std::unique_ptr<Texture>> m_Textures;
    std::unordered_set<std::string> m_Failed;

    std::unordered_map<std::string, TextureHandle> m_Handles;

    std::unordered_map<std::string, std::unique_ptr<Cubemap>> m_Cubemaps;
    std::unordered_set<std::string> m_FailedCubemaps;
};

} // namespace Binax
