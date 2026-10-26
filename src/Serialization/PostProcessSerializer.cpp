#include "PostProcessSerializer.h"
#include "Serialization/JsonHelpers.h"
#include "ECS/Components/PostProcessComponent.h"
#include "ECS/Components/NameComponent.h"
#include "ECS/Components/SystemEntityTag.h"
#include "ECS/Components/Transform.h"
#include "Utils/Log.h"
#include "Utils/Paths.h"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

namespace Binax
{

using namespace Serialization::Json;

namespace
{

PostProcessComponent* FindSettings(entt::registry& registry)
{
    auto view = registry.view<PostProcessComponent>();
    for (auto entity : view)
        return &view.get<PostProcessComponent>(entity);
    return nullptr;
}



PostProcessComponent* EnsureSettings(entt::registry& registry)
{
    if (auto* existing = FindSettings(registry))
        return existing;

    auto entity = registry.create();
    registry.emplace<NameComponent>(entity, "PostProcessingCfg");
    registry.emplace<Transform>(entity);
    registry.emplace<SystemEntityTag>(entity);
    return &registry.emplace<PostProcessComponent>(entity);
}

} 

bool PostProcessSerializer::Save(entt::registry& registry, const std::string& filepath)
{
    const PostProcessComponent* c = FindSettings(registry);
    if (!c)
    {
        BINAX_LOG_WARN("PostProcessing: no config component to save");
        return false;
    }

    nlohmann::json j;
    j["version"] = kVersion;

    j["enabled"]  = c->Enabled;
    j["exposure"] = c->Exposure;
    j["tonemap"]  = static_cast<int>(c->Tonemap);

    
    j["contrast"]    = c->Contrast;
    j["saturation"]  = c->Saturation;
    j["temperature"] = c->Temperature;
    j["tint"]        = c->Tint;
    SetVec3(j, "lift",  c->Lift);
    SetVec3(j, "gamma", c->Gamma);
    SetVec3(j, "gain",  c->Gain);

    
    j["bloomEnabled"]   = c->BloomEnabled;
    j["bloomThreshold"] = c->BloomThreshold;
    j["bloomIntensity"] = c->BloomIntensity;

    
    j["motionBlurEnabled"]  = c->MotionBlurEnabled;
    j["motionBlurStrength"] = c->MotionBlurStrength;
    j["motionBlurSamples"]  = c->MotionBlurSamples;

    
    j["lensDistortionEnabled"] = c->LensDistortionEnabled;
    j["lensDistortion"]        = c->LensDistortion;
    j["chromaticAberrationEnabled"] = c->ChromaticAberrationEnabled;
    j["chromaticAberration"]        = c->ChromaticAberration;

    
    j["vignetteEnabled"]    = c->VignetteEnabled;
    j["vignetteIntensity"]  = c->VignetteIntensity;
    j["vignetteSmoothness"] = c->VignetteSmoothness;
    j["filmGrainEnabled"]   = c->FilmGrainEnabled;
    j["filmGrainIntensity"] = c->FilmGrainIntensity;

    
    j["fogEnabled"] = c->FogEnabled;
    j["fogMode"]    = static_cast<int>(c->Fog);
    SetVec3(j, "fogColor", c->FogColor);
    j["fogDensity"] = c->FogDensity;
    j["fogStart"]   = c->FogStart;
    j["fogEnd"]     = c->FogEnd;

    const std::filesystem::path path = Paths::FromUtf8(filepath);
    std::error_code ec;
    if (path.has_parent_path())
        std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream file(path);
    if (!file.is_open())
    {
        BINAX_LOG_ERROR("PostProcessing: cannot write '{}'", filepath);
        return false;
    }

    file << j.dump(4);
    file.flush();
    const bool ok = file.good();
    if (ok)
        BINAX_LOG_INFO("Post-processing settings saved to {}", filepath);
    return ok;
}

bool PostProcessSerializer::Load(entt::registry& registry, const std::string& filepath)
{
    std::ifstream file(Paths::FromUtf8(filepath));
    if (!file.is_open())
    {
        BINAX_LOG_ERROR("PostProcessing: cannot open '{}'", filepath);
        return false;
    }

    nlohmann::json j;
    try
    {
        file >> j;
    }
    catch (const std::exception& e)
    {
        BINAX_LOG_ERROR("PostProcessing '{}': parse error: {}", filepath, e.what());
        return false;
    }

    if (!j.is_object() || Int(j, "version", 0) != kVersion)
    {
        BINAX_LOG_ERROR("PostProcessing '{}': unsupported version", filepath);
        return false;
    }

    PostProcessComponent* c = EnsureSettings(registry);

    c->Enabled  = Bool(j, "enabled", true);
    c->Exposure = Number(j, "exposure", 1.0f);

    const int tonemap = Int(j, "tonemap", 0);
    if (tonemap >= static_cast<int>(TonemapMode::ACES) &&
        tonemap <= static_cast<int>(TonemapMode::Linear))
        c->Tonemap = static_cast<TonemapMode>(tonemap);

    c->Contrast    = Number(j, "contrast", 1.0f);
    c->Saturation  = Number(j, "saturation", 1.0f);
    c->Temperature = Number(j, "temperature", 0.0f);
    c->Tint        = Number(j, "tint", 0.0f);
    c->Lift  = Vec3(j, "lift",  glm::vec3(0.0f));
    c->Gamma = Vec3(j, "gamma", glm::vec3(1.0f));
    c->Gain  = Vec3(j, "gain",  glm::vec3(1.0f));

    c->BloomEnabled   = Bool(j, "bloomEnabled", true);
    c->BloomThreshold = Number(j, "bloomThreshold", 1.0f);
    c->BloomIntensity = Number(j, "bloomIntensity", 0.15f);

    c->MotionBlurEnabled  = Bool(j, "motionBlurEnabled", true);
    c->MotionBlurStrength = Number(j, "motionBlurStrength", 0.5f);
    c->MotionBlurSamples  = Int(j, "motionBlurSamples", 8);

    c->LensDistortionEnabled = Bool(j, "lensDistortionEnabled", false);
    c->LensDistortion        = Number(j, "lensDistortion", 0.0f);
    c->ChromaticAberrationEnabled = Bool(j, "chromaticAberrationEnabled", false);
    c->ChromaticAberration        = Number(j, "chromaticAberration", 0.0f);

    c->VignetteEnabled    = Bool(j, "vignetteEnabled", true);
    c->VignetteIntensity  = Number(j, "vignetteIntensity", 0.35f);
    c->VignetteSmoothness = Number(j, "vignetteSmoothness", 0.5f);
    c->FilmGrainEnabled   = Bool(j, "filmGrainEnabled", false);
    c->FilmGrainIntensity = Number(j, "filmGrainIntensity", 0.08f);

    c->FogEnabled = Bool(j, "fogEnabled", false);
    const int fogMode = Int(j, "fogMode", 1);
    if (fogMode >= static_cast<int>(FogMode::Linear) &&
        fogMode <= static_cast<int>(FogMode::ExponentialSquared))
        c->Fog = static_cast<FogMode>(fogMode);
    c->FogColor   = Vec3(j, "fogColor", glm::vec3(0.62f, 0.70f, 0.80f));
    c->FogDensity = Number(j, "fogDensity", 0.04f);
    c->FogStart   = Number(j, "fogStart", 5.0f);
    c->FogEnd     = Number(j, "fogEnd", 60.0f);

    BINAX_LOG_INFO("Post-processing settings loaded from {}", filepath);
    return true;
}

}
