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

// Раскладка должна точно совпадать с struct MaterialGPU в geometry.frag (std430):
//   vec4 AlbedoRoughness;  vec4 MetallicAOEms;  vec4 SkyReflParams;  vec4 StrengthA;
//   uvec2 AlbedoHandle;    uvec2 NormalHandle;  uvec2 MetallicHandle;
//   uvec2 RoughnessHandle; uvec2 AOHandle;      uvec2 EmissiveHandle; vec4 StrengthB;
struct MaterialGPU
{
    glm::vec4     AlbedoRoughness{ 0.8f, 0.8f, 0.8f, 0.5f };
    glm::vec4     MetallicAOEms  { 0.0f, 1.0f, 0.0f, 1.0f };
    glm::vec4     SkyReflParams  { 1.0f, 1.0f, 0.0f, 0.0f };
    glm::vec4     StrengthA      { 1.0f };

    TextureHandle AlbedoHandle    = kInvalidTextureHandle;
    TextureHandle NormalHandle    = kInvalidTextureHandle;
    TextureHandle MetallicHandle  = kInvalidTextureHandle;
    TextureHandle RoughnessHandle = kInvalidTextureHandle;
    TextureHandle AOHandle        = kInvalidTextureHandle;
    TextureHandle EmissiveHandle  = kInvalidTextureHandle;

    glm::vec4     StrengthB      { 1.0f };
};
static_assert(sizeof(MaterialGPU) == 128,
              "MaterialGPU must be 128 bytes to match std430 layout in geometry.frag");

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
    MaterialHandle GetOrCreateDefault();
    MaterialHandle Copy(const std::string& newName, MaterialHandle src);
    MaterialHandle Find(const std::string& name) const;

    bool IsValid(MaterialHandle h) const { return h < m_CPU.size(); }

    const MaterialGPU& Get(MaterialHandle h)        const { return m_CPU[h]; }
    MaterialGPU&       GetMutable(MaterialHandle h)       { return m_CPU[h]; }

    const MaterialEditData& GetEdit(MaterialHandle h) const { return m_Edit[h]; }
    const std::vector<MaterialEditData>& GetEditData() const { return m_Edit; }
    const std::string& GetName(MaterialHandle h) const { return m_Edit[h].Name; }
    uint32_t GetMaterialCount() const { return (uint32_t)m_CPU.size(); }

    void Update(MaterialHandle h, const MaterialGPU& mat);

    void SetAlbedoMap   (MaterialHandle h, const std::string& path);
    void SetNormalMap   (MaterialHandle h, const std::string& path);
    void SetMetallicMap (MaterialHandle h, const std::string& path);
    void SetRoughnessMap(MaterialHandle h, const std::string& path);
    void SetAOMap       (MaterialHandle h, const std::string& path);
    void SetEmissiveMap (MaterialHandle h, const std::string& path);

    void Flush();
    void Clear();
    void MarkDirty() { m_Dirty = true; }

    uint32_t GetMaterialSSBO() const { return m_SSBO; }

private:
    MaterialLibrary() = default;

    std::vector<MaterialGPU>      m_CPU;
    std::vector<MaterialEditData> m_Edit;
    std::unordered_map<std::string, MaterialHandle> m_ByName;

    uint32_t m_SSBO  = 0;
    bool     m_Dirty = true;
};

} // namespace Binax
