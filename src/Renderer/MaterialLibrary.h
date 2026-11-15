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

enum class MaterialBlendMode : uint32_t
{
    Opaque      = 0,   // как сейчас
    AlphaTest   = 1,   // foliage: discard по alpha cutoff
    Transparent = 2,   // стекло/вода: forward pass с blend
};

const char* MaterialBlendModeName(MaterialBlendMode m);
MaterialBlendMode MaterialBlendModeFromInt(int v);

// ---- Shader type --------------------------------------------------------
// Как в Unity: материал ссылается на "shader", который определяет, как
// его освещать. Расширяется добавлением новых значений в enum.
enum class MaterialShaderType : uint32_t
{
    Lit       = 0,   // полный PBR + IBL + карты
    SimpleLit = 1,   // Lambert + Phong, без PBR-карт и IBL
    Unlit     = 2,   // только albedo + emissive, без света
};



const char* MaterialShaderTypeName(MaterialShaderType t);
MaterialShaderType MaterialShaderTypeFromInt(int v);

// ---- GPU layout ---------------------------------------------------------
struct MaterialGPU
{
    glm::vec4     AlbedoRoughness{ 0.8f, 0.8f, 0.8f, 0.5f };
    glm::vec4     MetallicAOEms  { 0.0f, 1.0f, 0.0f, 1.0f };
    glm::vec4     SkyReflParams  { 1.0f, 1.0f, 0.0f, 1.0f };  // .w = AlphaMultiplier
    glm::vec4     StrengthA      { 1.0f };

    TextureHandle AlbedoHandle    = kInvalidTextureHandle;
    TextureHandle NormalHandle    = kInvalidTextureHandle;
    TextureHandle MetallicHandle  = kInvalidTextureHandle;
    TextureHandle RoughnessHandle = kInvalidTextureHandle;
    TextureHandle AOHandle        = kInvalidTextureHandle;
    TextureHandle EmissiveHandle  = kInvalidTextureHandle;

    // .x = albedoStrength, .y = normalStrength
    // .z = AlphaCutoff (для AlphaTest)
    // .w = BlendMode as float (0=Opaque, 1=AlphaTest, 2=Transparent)
    glm::vec4     StrengthB      { 1.0f };
};
static_assert(sizeof(MaterialGPU) == 128,
              "MaterialGPU must be 128 bytes to match std430 layout in geometry.frag");

struct MaterialEditData
{
    std::string       Name;
    MaterialShaderType ShaderType = MaterialShaderType::Lit;

    std::string AlbedoMapPath;
    std::string NormalMapPath;
    std::string MetallicMapPath;
    std::string RoughnessMapPath;
    std::string AOMapPath;
    std::string EmissiveMapPath;
    MaterialBlendMode BlendMode       = MaterialBlendMode::Opaque;
    float             AlphaCutoff     = 0.5f;
    float             AlphaMultiplier = 1.0f;
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

    // ---- Shader type ------------------------------------------------------
    MaterialShaderType GetShaderType(MaterialHandle h) const
    {
        return (h < m_CPU.size()) ? m_Edit[h].ShaderType
                                  : MaterialShaderType::Lit;
    }

    void SetShaderType(MaterialHandle h, MaterialShaderType t)
    {
        if (h >= m_CPU.size()) return;
        m_Edit[h].ShaderType           = t;
        m_CPU[h].SkyReflParams.z       = static_cast<float>(static_cast<int>(t));
        m_Dirty = true;
    }

    // ---- Other ------------------------------------------------------------
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
        MaterialBlendMode GetBlendMode(MaterialHandle h) const
    { return (h < m_CPU.size()) ? m_Edit[h].BlendMode : MaterialBlendMode::Opaque; }

    void SetBlendMode(MaterialHandle h, MaterialBlendMode m)
    {
        if (h >= m_CPU.size()) return;
        m_Edit[h].BlendMode         = m;
        m_CPU[h].StrengthB.w        = static_cast<float>(static_cast<int>(m));
        m_Dirty = true;
    }

    float GetAlphaCutoff(MaterialHandle h) const
    { return (h < m_CPU.size()) ? m_Edit[h].AlphaCutoff : 0.5f; }

    void SetAlphaCutoff(MaterialHandle h, float v)
    {
        if (h >= m_CPU.size()) return;
        m_Edit[h].AlphaCutoff = v;
        m_CPU[h].StrengthB.z  = v;
        m_Dirty = true;
    }

    float GetAlphaMultiplier(MaterialHandle h) const
    { return (h < m_CPU.size()) ? m_Edit[h].AlphaMultiplier : 1.0f; }

    void SetAlphaMultiplier(MaterialHandle h, float v)
    {
        if (h >= m_CPU.size()) return;
        m_Edit[h].AlphaMultiplier = v;
        m_CPU[h].SkyReflParams.w  = v;
        m_Dirty = true;
    }
        // FIX #13: снимок для атомарной загрузки сцены.
    // Восстановление — в точности те же handle'ы (индексы не меняются),
    // поэтому ссылки из MaterialComponent остаются валидными.
    struct Snapshot
    {
        std::vector<MaterialGPU>      CPU;
        std::vector<MaterialEditData> Edit;
        std::unordered_map<std::string, MaterialHandle> ByName;
    };

    Snapshot TakeSnapshot() const;
    void RestoreSnapshot(Snapshot&& s);
    

private:
    MaterialLibrary() = default;

    std::vector<MaterialGPU>      m_CPU;
    std::vector<MaterialEditData> m_Edit;
    std::unordered_map<std::string, MaterialHandle> m_ByName;

    uint32_t m_SSBO  = 0;
    bool     m_Dirty = true;
};

}