#include "Renderer/MaterialLibrary.h"
#include "Renderer/AssetRegistry.h"
#include "Utils/Log.h"

#include <glad/glad.h>

namespace Binax {

MaterialLibrary& MaterialLibrary::Get()
{
    static MaterialLibrary instance;
    return instance;
}

MaterialHandle MaterialLibrary::Add(const std::string& name, const MaterialGPU& mat)
{
    auto it = m_ByName.find(name);
    if (it != m_ByName.end())
    {
        Update(it->second, mat);
        return it->second;
    }

    MaterialHandle h = (MaterialHandle)m_CPU.size();
    m_CPU.push_back(mat);
    MaterialEditData ed; ed.Name = name;
    m_Edit.push_back(std::move(ed));
    m_ByName[name] = h;
    m_Dirty = true;
    return h;
}

MaterialHandle MaterialLibrary::GetOrCreateDefault()
{
    
    
    constexpr const char* kName = "__Default";

    auto it = m_ByName.find(kName);
    if (it != m_ByName.end())
        return it->second;

    MaterialGPU def{};
    def.AlbedoRoughness = glm::vec4(0.8f, 0.8f, 0.8f, 0.5f);
    def.MetallicAOEms   = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
    def.SkyReflParams   = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
    def.StrengthA       = glm::vec4(1.0f);
    def.StrengthB       = glm::vec4(1.0f);
    return Add(kName, def);
}

MaterialHandle MaterialLibrary::Copy(const std::string& newName, MaterialHandle src)
{
    if (src >= m_CPU.size())
        return kInvalidMaterial;

    MaterialHandle h = Add(newName, m_CPU[src]);

    MaterialEditData ed = m_Edit[src];
    ed.Name = newName;
    m_Edit[h] = std::move(ed);
    m_Dirty = true;
    return h;
}

MaterialHandle MaterialLibrary::Find(const std::string& name) const
{
    auto it = m_ByName.find(name);
    return it == m_ByName.end() ? kInvalidMaterial : it->second;
}

void MaterialLibrary::Update(MaterialHandle h, const MaterialGPU& mat)
{
    if (h >= m_CPU.size()) return;
    m_CPU[h] = mat;
    m_Dirty = true;
}

void MaterialLibrary::SetAlbedoMap(MaterialHandle h, const std::string& path)
{
    if (h >= m_CPU.size()) return;
    m_Edit[h].AlbedoMapPath = path;
    m_CPU[h].AlbedoHandle = AssetRegistry::Get().GetTextureHandle(path,  true);
    m_Dirty = true;
}

void MaterialLibrary::SetNormalMap(MaterialHandle h, const std::string& path)
{
    if (h >= m_CPU.size()) return;
    m_Edit[h].NormalMapPath = path;
    m_CPU[h].NormalHandle = AssetRegistry::Get().GetTextureHandle(path,  false);
    m_Dirty = true;
}

void MaterialLibrary::SetMetallicMap(MaterialHandle h, const std::string& path)
{
    if (h >= m_CPU.size()) return;
    m_Edit[h].MetallicMapPath = path;
    m_CPU[h].MetallicHandle = AssetRegistry::Get().GetTextureHandle(path, false);
    m_Dirty = true;
}

void MaterialLibrary::SetRoughnessMap(MaterialHandle h, const std::string& path)
{
    if (h >= m_CPU.size()) return;
    m_Edit[h].RoughnessMapPath = path;
    m_CPU[h].RoughnessHandle = AssetRegistry::Get().GetTextureHandle(path, false);
    m_Dirty = true;
}

void MaterialLibrary::SetAOMap(MaterialHandle h, const std::string& path)
{
    if (h >= m_CPU.size()) return;
    m_Edit[h].AOMapPath = path;
    m_CPU[h].AOHandle = AssetRegistry::Get().GetTextureHandle(path, false);
    m_Dirty = true;
}

void MaterialLibrary::SetEmissiveMap(MaterialHandle h, const std::string& path)
{
    if (h >= m_CPU.size()) return;
    m_Edit[h].EmissiveMapPath = path;
    m_CPU[h].EmissiveHandle = AssetRegistry::Get().GetTextureHandle(path,  true);
    m_Dirty = true;
}

void MaterialLibrary::Flush()
{
    if (!m_Dirty && m_SSBO != 0) return;

    if (m_CPU.empty())
    {
        if (m_SSBO == 0) glGenBuffers(1, &m_SSBO);
        MaterialGPU dummy{};
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(MaterialGPU), &dummy, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        m_Dirty = false;
        return;
    }

    if (m_SSBO == 0) glGenBuffers(1, &m_SSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 (GLsizeiptr)(m_CPU.size() * sizeof(MaterialGPU)),
                 m_CPU.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    m_Dirty = false;
}

void MaterialLibrary::Clear()
{
    m_CPU.clear();
    m_Edit.clear();
    m_ByName.clear();
    m_Dirty = true;
    if (m_SSBO) { glDeleteBuffers(1, &m_SSBO); m_SSBO = 0; }
}

} 
