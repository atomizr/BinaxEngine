#pragma once

#include <string>

namespace Binax {

struct ShadowSettings
{
    bool  Enabled            = true;
    int   CascadeCount       = 3;        // 1..4
    int   GlobalResolution   = 2048;     // 512/1024/2048/4096
    float MaxShadowDistance  = 80.0f;
    float CascadeSplitLambda = 0.75f;
    float DepthBias          = 0.002f;
    float NormalBias         = 0.02f;
    float ShadowSoftness     = 1.0f;
    float FadeOutDistance    = 10.0f;

    static constexpr int kVersion = 1;
};

class ShadowSettingsSerializer
{
public:
    static bool Save(const ShadowSettings& s, const std::string& filepath);
    static bool Load(ShadowSettings& s, const std::string& filepath);
};

}