#include "AssetRegistry.h"
#include "Utils/Log.h"

namespace Binax {

AssetRegistry& AssetRegistry::Get()
{
    static AssetRegistry instance;
    return instance;
}

// ======================= 2D Texture =======================

const Texture* AssetRegistry::GetTexture(const std::string& path, bool srgb)
{
    if (path.empty())
        return nullptr;

    if (m_Failed.count(path))
        return nullptr;

    auto it = m_Textures.find(path);
    if (it != m_Textures.end())
        return it->second.get();

    auto tex = Texture::LoadFromFile(path, srgb);
    if (!tex)
    {
        m_Failed.insert(path);
        return nullptr;
    }

    const Texture* raw = tex.get();
    m_Textures[path] = std::move(tex);
    BINAX_LOG_INFO("AssetRegistry: loaded texture '{}'", path);
    return raw;
}

void AssetRegistry::ReloadTexture(const std::string& path)
{
    m_Failed.erase(path);
    m_Textures.erase(path);
}

// ======================= Cubemap =======================

std::string AssetRegistry::MakeCubemapKey(const std::array<std::string, 6>& paths)
{
    std::string key;
    for (const auto& p : paths)
    {
        key += p;
        key += '|';
    }
    return key;
}

const Cubemap* AssetRegistry::GetCubemap(const std::array<std::string, 6>& paths, bool srgb)
{
    for (const auto& p : paths)
        if (p.empty())
            return nullptr;

    const std::string key = MakeCubemapKey(paths);

    if (m_FailedCubemaps.count(key))
        return nullptr;

    auto it = m_Cubemaps.find(key);
    if (it != m_Cubemaps.end())
        return it->second.get();

    auto cube = Cubemap::LoadFromFiles(paths, srgb);
    if (!cube)
    {
        m_FailedCubemaps.insert(key);
        return nullptr;
    }

    const Cubemap* raw = cube.get();
    m_Cubemaps[key] = std::move(cube);
    BINAX_LOG_INFO("AssetRegistry: loaded cubemap ({} px)", raw->GetSize());
    return raw;
}

void AssetRegistry::ReloadCubemap(const std::array<std::string, 6>& paths)
{
    const std::string key = MakeCubemapKey(paths);
    m_FailedCubemaps.erase(key);
    m_Cubemaps.erase(key);
}

void AssetRegistry::Clear()
{
    m_Textures.clear();
    m_Failed.clear();
    m_Cubemaps.clear();
    m_FailedCubemaps.clear();
}

}