#include "Renderer/ShadowSettings.h"
#include "Utils/Log.h"
#include "Utils/Paths.h"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace Binax {

using nlohmann::json;

namespace {
float N(const json& j, const char* k, float d) {
    auto it = j.find(k); return (it!=j.end() && it->is_number()) ? it->get<float>() : d;
}
int I(const json& j, const char* k, int d) {
    auto it = j.find(k); return (it!=j.end() && it->is_number_integer()) ? it->get<int>() : d;
}
bool B(const json& j, const char* k, bool d) {
    auto it = j.find(k); return (it!=j.end() && it->is_boolean()) ? it->get<bool>() : d;
}
}

bool ShadowSettingsSerializer::Save(const ShadowSettings& s, const std::string& filepath)
{
    json j;
    j["version"]             = ShadowSettings::kVersion;
    j["enabled"]             = s.Enabled;
    j["cascadeCount"]        = s.CascadeCount;
    j["globalResolution"]    = s.GlobalResolution;
    j["maxShadowDistance"]   = s.MaxShadowDistance;
    j["cascadeSplitLambda"]  = s.CascadeSplitLambda;
    j["depthBias"]           = s.DepthBias;
    j["normalBias"]          = s.NormalBias;
    j["shadowSoftness"]      = s.ShadowSoftness;
    j["fadeOutDistance"]     = s.FadeOutDistance;

    const std::filesystem::path path = Paths::FromUtf8(filepath);
    std::error_code ec;
    if (path.has_parent_path())
        std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream f(path);
    if (!f.is_open()) { BINAX_LOG_ERROR("ShadowSettings: cannot write '{}'", filepath); return false; }
    f << j.dump(4); f.flush();
    return f.good();
}

bool ShadowSettingsSerializer::Load(ShadowSettings& s, const std::string& filepath)
{
    std::ifstream f(Paths::FromUtf8(filepath));
    if (!f.is_open()) {
        BINAX_LOG_INFO("ShadowSettings: '{}' not found, using defaults", filepath);
        return false;
    }

    json j;
    try { f >> j; } catch (const std::exception& e) {
        BINAX_LOG_ERROR("ShadowSettings: parse '{}': {}", filepath, e.what());
        return false;
    }

    if (!j.is_object() || I(j, "version", 0) != ShadowSettings::kVersion) {
        BINAX_LOG_WARN("ShadowSettings: unsupported version in '{}'", filepath);
        return false;
    }

    s.Enabled            = B(j, "enabled", true);
    s.CascadeCount       = std::clamp(I(j, "cascadeCount", 3), 1, 4);
    s.GlobalResolution   = I(j, "globalResolution", 2048);
    s.MaxShadowDistance  = N(j, "maxShadowDistance", 80.0f);
    s.CascadeSplitLambda = std::clamp(N(j, "cascadeSplitLambda", 0.75f), 0.0f, 1.0f);
    s.DepthBias          = N(j, "depthBias", 0.002f);
    s.NormalBias         = N(j, "normalBias", 0.02f);
    s.ShadowSoftness     = N(j, "shadowSoftness", 1.0f);
    s.FadeOutDistance    = N(j, "fadeOutDistance", 10.0f);

    return true;
}

}