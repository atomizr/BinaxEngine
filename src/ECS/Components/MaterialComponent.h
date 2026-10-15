#pragma once

#include <glm/glm.hpp>
#include <string>

namespace Binax {

struct MaterialComponent
{
    // ---- Scalar params ----
    glm::vec3 Albedo    = glm::vec3(1.0f);
    float     Metallic  = 0.0f;
    float     Roughness = 0.5f;
    float     AO        = 1.0f;
    float     Emissive  = 0.0f;

    // ---- Texture paths ----
    std::string AlbedoMapPath;
    std::string NormalMapPath;
    std::string MetallicMapPath;
    std::string RoughnessMapPath;
    std::string AOMapPath;
    std::string EmissiveMapPath;

    // ---- Per-map strengths ----
    float AlbedoStrength    = 1.0f;
    float NormalStrength    = 1.0f;
    float MetallicStrength  = 1.0f;
    float RoughnessStrength = 1.0f;
    float AOStrength        = 1.0f;
    float EmissiveStrength  = 1.0f;

    // ---- UV tiling ----
    float Tiling = 1.0f;

    // ---- Skybox reflections (direct sample of env cubemap, no baking) ----
    bool  SkyReflectionsEnabled = true;
    float SkyReflectionStrength = 1.0f;
};

} // namespace Binax