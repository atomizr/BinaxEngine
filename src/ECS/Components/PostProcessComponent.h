#pragma once

#include <glm/glm.hpp>

namespace Binax
{

enum class TonemapMode
{
    ACES = 0,
    Reinhard,
    Filmic,
    AgX,
    Linear
};

enum class FogMode
{
    Linear = 0,
    Exponential,
    ExponentialSquared
};



struct PostProcessComponent
{
    bool Enabled = true;

    
    float Exposure = 1.0f;
    TonemapMode Tonemap = TonemapMode::ACES;

    
    float Contrast    = 1.0f;   
    float Saturation  = 1.0f;   
    float Temperature = 0.0f;   
    float Tint        = 0.0f;   

    
    glm::vec3 Lift  = glm::vec3(0.0f);
    glm::vec3 Gamma = glm::vec3(1.0f);
    glm::vec3 Gain  = glm::vec3(1.0f);

    
    bool  BloomEnabled   = true;
    float BloomThreshold = 1.0f;
    float BloomIntensity = 0.15f;

    
    bool  MotionBlurEnabled  = true;
    float MotionBlurStrength = 0.5f;
    int   MotionBlurSamples  = 8;

    
    bool  LensDistortionEnabled = false;
    float LensDistortion        = 0.0f;   

    
    bool  ChromaticAberrationEnabled = false;
    float ChromaticAberration        = 0.0f;   

    
    bool  VignetteEnabled    = true;
    float VignetteIntensity  = 0.35f;
    float VignetteSmoothness = 0.5f;

    
    bool  FilmGrainEnabled   = false;
    float FilmGrainIntensity = 0.08f;

    
    bool    FogEnabled = false;
    FogMode Fog        = FogMode::Exponential;
    glm::vec3 FogColor   = glm::vec3(0.62f, 0.70f, 0.80f);
    float   FogDensity   = 0.04f;   
    float   FogStart     = 5.0f;    
    float   FogEnd       = 60.0f;   
};

}
