#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "Renderer/TextureHandle.h"

namespace Binax {

using MaterialHandle = uint32_t;
constexpr MaterialHandle kInvalidMaterial = 0xFFFFFFFFu;

struct MaterialGPU
{
    glm::vec4 AlbedoRoughness;
    glm::vec4 MetallicAOEms;
    glm::vec4 SkyReflParams;
    glm::vec4 StrengthA;

    TextureHandle AlbedoHandle    = kInvalidTextureHandle;
    TextureHandle NormalHandle    = kInvalidTextureHandle;
    TextureHandle MetallicHandle  = kInvalidTextureHandle;
    TextureHandle RoughnessHandle = kInvalidTextureHandle;
    TextureHandle AOHandle        = kInvalidTextureHandle;
    TextureHandle EmissiveHandle  = kInvalidTextureHandle;

    glm::vec4 StrengthB;
};
static_assert(sizeof(MaterialGPU) == 128, "MaterialGPU must be 128 bytes");

struct MaterialEditData
{
    std::string Name;
    std::string AlbedoMapPath;
    std::string NormalMapPath;
    std::string MetallicMapPath;
    std::string RoughnessMapPath;
    std::string AOMapPath;
    std::string EmissiveMapPath;
};

class MaterialLibrary
{
public:
    static MaterialLibrary& Get();

    MaterialHandle Add(const std::string& name, const MaterialGPU& mat);
    MaterialHandle Find(const std::string& name) const;
    MaterialHandle Copy(const std::string& newName, MaterialHandle src);
    void Update(MaterialHandle h, const MaterialGPU& mat);

    
    
    
    
    MaterialHandle GetOrCreateDefault();

    void SetAlbedoMap(MaterialHandle h, const std::string& path);
    void SetNormalMap(MaterialHandle h, const std::string& path);
    void SetMetallicMap(MaterialHandle h, const std::string& path);
    void SetRoughnessMap(MaterialHandle h, const std::string& path);
    void SetAOMap(MaterialHandle h, const std::string& path);
    void SetEmissiveMap(MaterialHandle h, const std::string& path);

    
    
    
    
    bool IsValid(MaterialHandle h) const { return h < m_CPU.size(); }

    const MaterialGPU&      Get(MaterialHandle h) const      { return m_CPU[h]; }
    MaterialGPU&            GetMutable(MaterialHandle h)     { return m_CPU[h]; }

    const MaterialEditData& GetEdit(MaterialHandle h) const  { return m_Edit[h]; }
    const std::string&      GetName(MaterialHandle h) const  { return m_Edit[h].Name; }
    uint32_t                GetMaterialCount() const         { return (uint32_t)m_CPU.size(); }

    
    
    const std::vector<MaterialEditData>& GetEditData() const { return m_Edit; }

    uint32_t GetMaterialSSBO() const { return m_SSBO; }
    void MarkDirty() { m_Dirty = true; }
    void Flush();
    void Clear();

private:
    MaterialLibrary() = default;

    std::vector<MaterialGPU>      m_CPU;
    std::vector<MaterialEditData> m_Edit;
    std::unordered_map<std::string, MaterialHandle> m_ByName;

    uint32_t m_SSBO = 0;
    bool m_Dirty = true;
};

} 
