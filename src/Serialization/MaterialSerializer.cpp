#include "MaterialSerializer.h"
#include "Serialization/JsonHelpers.h"
#include "Utils/AssetPaths.h"
#include "Utils/Log.h"
#include "Utils/Paths.h"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

namespace Binax
{

namespace fs = std::filesystem;
using namespace Serialization::Json;

std::string MaterialSerializer::SanitizeFileName(const std::string& name)
{
    std::string out;
    out.reserve(name.size());
    for (char c : name)
    {
        switch (c)
        {
        case '\\': case '/': case ':': case '*': case '?':
        case '"':  case '<': case '>': case '|':
            out.push_back('_');
            break;
        default:
            out.push_back(c);
            break;
        }
    }
    if (out.empty())
        out = "material";
    return out;
}

bool MaterialSerializer::Save(const MaterialLibrary& lib, MaterialHandle handle,
                              const std::string& filepath)
{
    if (!lib.IsValid(handle))
        return false;

    const MaterialGPU& gpu = lib.Get(handle);
    const MaterialEditData& ed = lib.GetEdit(handle);

    nlohmann::json j;
    j["version"] = kVersion;
    j["name"]    = ed.Name;

    j["albedo"]    = { gpu.AlbedoRoughness.x, gpu.AlbedoRoughness.y, gpu.AlbedoRoughness.z };
    j["roughness"] = gpu.AlbedoRoughness.w;

    j["metallic"] = gpu.MetallicAOEms.x;
    j["ao"]       = gpu.MetallicAOEms.y;
    j["emissive"] = gpu.MetallicAOEms.z;
    j["tiling"]   = gpu.MetallicAOEms.w;

    j["skyReflectionsEnabled"] = (gpu.SkyReflParams.x > 0.5f);
    j["skyReflectionStrength"] = gpu.SkyReflParams.y;

    j["metallicStrength"]  = gpu.StrengthA.x;
    j["roughnessStrength"] = gpu.StrengthA.y;
    j["aoStrength"]        = gpu.StrengthA.z;
    j["emissiveStrength"]  = gpu.StrengthA.w;

    j["albedoStrength"] = gpu.StrengthB.x;
    j["normalStrength"] = gpu.StrengthB.y;

    j["albedoMap"]    = ed.AlbedoMapPath;
    j["normalMap"]    = ed.NormalMapPath;
    j["metallicMap"]  = ed.MetallicMapPath;
    j["roughnessMap"] = ed.RoughnessMapPath;
    j["aoMap"]        = ed.AOMapPath;
    j["emissiveMap"]  = ed.EmissiveMapPath;

    const fs::path path = Paths::FromUtf8(filepath);
    std::error_code ec;
    if (path.has_parent_path())
        fs::create_directories(path.parent_path(), ec);

    std::ofstream file(path);
    if (!file.is_open())
    {
        BINAX_LOG_ERROR("Material: cannot write '{}'", filepath);
        return false;
    }

    file << j.dump(4);
    file.flush();
    const bool ok = file.good();
    return ok;
}

bool MaterialSerializer::SaveAll(const MaterialLibrary& lib, const std::string& directory)
{
    std::error_code ec;
    const fs::path dir = Paths::FromUtf8(directory);
    fs::create_directories(dir, ec);

    bool allOk = true;
    uint32_t saved = 0;
    for (uint32_t i = 0; i < lib.GetMaterialCount(); ++i)
    {
        const MaterialHandle h = i;
        const std::string& name = lib.GetName(h);
        if (name.empty() || name == AssetFiles::kDefaultMaterialName)
            continue;

        const fs::path filePath =
            dir / (SanitizeFileName(name) + AssetFiles::kMaterialExt);

        if (Save(lib, h, Paths::ToUtf8(filePath)))
            ++saved;
        else
            allOk = false;
    }

    BINAX_LOG_INFO("Materials saved: {} -> {}", saved, directory);
    return allOk;
}

bool MaterialSerializer::Load(MaterialLibrary& lib, const std::string& filepath)
{
    std::ifstream file(Paths::FromUtf8(filepath));
    if (!file.is_open())
    {
        BINAX_LOG_ERROR("Material: cannot open '{}'", filepath);
        return false;
    }

    nlohmann::json j;
    try
    {
        file >> j;
    }
    catch (const std::exception& e)
    {
        BINAX_LOG_ERROR("Material '{}': parse error: {}", filepath, e.what());
        return false;
    }

    if (!j.is_object() || Int(j, "version", 0) != kVersion)
    {
        BINAX_LOG_ERROR("Material '{}': unsupported version", filepath);
        return false;
    }

    const std::string name = String(j, "name");
    if (name.empty())
    {
        BINAX_LOG_WARN("Material '{}': empty name — skipped", filepath);
        return false;
    }

    const glm::vec3 albedo = Vec3(j, "albedo", glm::vec3(1.0f));

    MaterialGPU gpu{};
    gpu.AlbedoRoughness = glm::vec4(albedo, Number(j, "roughness", 0.5f));
    gpu.MetallicAOEms   = glm::vec4(Number(j, "metallic", 0.0f),
                                    Number(j, "ao",       1.0f),
                                    Number(j, "emissive", 0.0f),
                                    Number(j, "tiling",   1.0f));
    gpu.SkyReflParams   = glm::vec4(Bool(j, "skyReflectionsEnabled", true) ? 1.0f : 0.0f,
                                    Number(j, "skyReflectionStrength", 1.0f),
                                    0.0f, 0.0f);
    gpu.StrengthA       = glm::vec4(Number(j, "metallicStrength",  1.0f),
                                    Number(j, "roughnessStrength", 1.0f),
                                    Number(j, "aoStrength",        1.0f),
                                    Number(j, "emissiveStrength",  1.0f));
    gpu.StrengthB       = glm::vec4(Number(j, "albedoStrength", 1.0f),
                                    Number(j, "normalStrength", 1.0f),
                                    0.0f, 0.0f);

    const MaterialHandle h = lib.Add(name, gpu);

    lib.SetAlbedoMap   (h, String(j, "albedoMap"));
    lib.SetNormalMap   (h, String(j, "normalMap"));
    lib.SetMetallicMap (h, String(j, "metallicMap"));
    lib.SetRoughnessMap(h, String(j, "roughnessMap"));
    lib.SetAOMap       (h, String(j, "aoMap"));
    lib.SetEmissiveMap (h, String(j, "emissiveMap"));

    return true;
}

void MaterialSerializer::LoadAll(MaterialLibrary& lib, const std::string& directory)
{
    lib.Clear();

    std::error_code ec;
    const fs::path dir = Paths::FromUtf8(directory);
    if (fs::exists(dir, ec) && !ec && fs::is_directory(dir, ec) && !ec)
    {
        try
        {
            for (const auto& entry : fs::directory_iterator(dir, ec))
            {
                if (ec)
                    break;
                std::error_code fileEc;
                if (!entry.is_regular_file(fileEc) || fileEc)
                    continue;
                if (entry.path().extension() != AssetFiles::kMaterialExt)
                    continue;
                Load(lib, Paths::ToUtf8(entry.path()));
            }
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            BINAX_LOG_WARN("Materials: cannot list '{}': {}", directory, e.what());
        }
    }

    lib.GetOrCreateDefault();
    lib.Flush();

    BINAX_LOG_INFO("Materials loaded from {0}: {1}", directory,
                   lib.GetMaterialCount());
}

}
