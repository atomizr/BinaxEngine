#include "AssetRegistry.h"
#include "Utils/Log.h"

#include <glad/glad.h>

namespace Binax {

AssetRegistry& AssetRegistry::Get()
{
    static AssetRegistry instance;
    return instance;
}



std::string AssetRegistry::MakeTextureKey(const std::string& path, bool srgb)
{
    return path + (srgb ? "|sRGB" : "|Linear");
}

const Texture* AssetRegistry::GetTexture(const std::string& path, bool srgb)
{
    if (path.empty())
        return nullptr;

    const std::string key = MakeTextureKey(path, srgb);

    if (m_Failed.count(key))
        return nullptr;

    auto it = m_Textures.find(key);
    if (it != m_Textures.end())
        return it->second.get();

    auto tex = Texture::LoadFromFile(path, srgb);
    if (!tex)
    {
        m_Failed.insert(key);
        return nullptr;
    }

    const Texture* raw = tex.get();
    m_Textures[key] = std::move(tex);
    BINAX_LOG_INFO("AssetRegistry: loaded texture '{}'", path);
    return raw;
}

TextureHandle AssetRegistry::GetTextureHandle(const std::string& path, bool srgb)
{
    if (path.empty())
        return kInvalidTextureHandle;

    const std::string key = MakeTextureKey(path, srgb);

    auto it = m_Handles.find(key);
    if (it != m_Handles.end())
        return it->second;

    if (!GLAD_GL_ARB_bindless_texture)
    {
        BINAX_LOG_WARN("AssetRegistry: bindless texture unsupported — "
                       "texture '{}' will be ignored", path);
        m_Handles[key] = kInvalidTextureHandle;
        return kInvalidTextureHandle;
    }

    const Texture* tex = GetTexture(path, srgb);
    if (!tex || !tex->GetID())
    {
        m_Handles[key] = kInvalidTextureHandle;
        return kInvalidTextureHandle;
    }

    GLuint64 h = glGetTextureHandleARB(tex->GetID());
    if (h != 0 && !glIsTextureHandleResidentARB(h))
        glMakeTextureHandleResidentARB(h);

    m_Handles[key] = (TextureHandle)h;
    BINAX_LOG_INFO("AssetRegistry: bindless handle for '{}' = {}", path,
                   (unsigned long long)h);
    return m_Handles[key];
}

void AssetRegistry::ReloadTexture(const std::string& path)
{
    if (path.empty())
        return;

    
    
    for (const bool srgb : { true, false })
    {
        const std::string key = MakeTextureKey(path, srgb);

        auto hit = m_Handles.find(key);
        if (hit != m_Handles.end())
        {
            if (hit->second != kInvalidTextureHandle && GLAD_GL_ARB_bindless_texture)
            {
                GLuint64 h = (GLuint64)hit->second;
                if (glIsTextureHandleResidentARB(h))
                    glMakeTextureHandleNonResidentARB(h);
            }
            m_Handles.erase(hit);
        }

        m_Failed.erase(key);
        m_Textures.erase(key);
    }
}



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
    
    if (GLAD_GL_ARB_bindless_texture)
    {
        for (auto& [path, h] : m_Handles)
        {
            if (h != kInvalidTextureHandle)
            {
                GLuint64 raw = (GLuint64)h;
                if (glIsTextureHandleResidentARB(raw))
                    glMakeTextureHandleNonResidentARB(raw);
            }
        }
    }
    m_Handles.clear();

    m_Textures.clear();
    m_Failed.clear();

    m_Cubemaps.clear();
    m_FailedCubemaps.clear();
}

} 
