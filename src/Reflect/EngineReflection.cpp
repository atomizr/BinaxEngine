#include "Reflect/EngineReflection.h"
#include "Reflect/Reflection.h"
#include "Reflect/ComponentRegistry.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/MeshComponent.h"
#include "ECS/Components/MaterialComponent.h"
#include "ECS/Components/CameraComponent.h"
#include "ECS/Components/LightComponent.h"
#include "ECS/Components/NameComponent.h"
#include "ECS/Components/EditorCameraComponent.h"
#include "ECS/Components/SkyboxComponent.h"
#include "ECS/Components/PostProcessComponent.h"
#include "ECS/Components/EntityFlags.h"
#include "ECS/Components/ReflectionProbeComponent.h"

using namespace Binax;

BINAX_REFLECT(Binax::Transform)
{
    info.Add(&Transform::Position, "Position", PropertyKind::Vec3).SetSpeed(0.1f);
    info.Add(&Transform::Rotation, "Rotation", PropertyKind::Quat);
    info.Add(&Transform::Scale,    "Scale",    PropertyKind::Vec3).SetSpeed(0.1f);
}

BINAX_REFLECT(Binax::CameraComponent)
{
    info.Add(&CameraComponent::Projection, "Projection", PropertyKind::Enum).SetEnums({
        { static_cast<int>(ProjectionType::Perspective),  "Perspective"  },
        { static_cast<int>(ProjectionType::Orthographic), "Orthographic" }
    });
    info.Add(&CameraComponent::FOV,         "FOV",         PropertyKind::Float).SetRange(1.0f, 120.0f);
    info.Add(&CameraComponent::NearPlane,   "Near Plane",  PropertyKind::Float).SetRange(0.001f, 10.0f);
    info.Add(&CameraComponent::FarPlane,    "Far Plane",   PropertyKind::Float).SetRange(1.0f, 10000.0f);
    info.Add(&CameraComponent::OrthoSize,   "Ortho Size",  PropertyKind::Float).SetSpeed(0.1f);
    info.Add(&CameraComponent::AspectRatio, "Aspect",      PropertyKind::Float)
        .SetSpeed(0.01f).SetReadOnly().SetTransient();
    info.Add(&CameraComponent::Primary,     "Primary",     PropertyKind::Bool);

    
    info.Add(&CameraComponent::FXAAEnabled,          "FXAA",                    PropertyKind::Bool);
    info.Add(&CameraComponent::FXAAEdgeThreshold,    "FXAA Edge Threshold",     PropertyKind::Float)
        .SetRange(0.0f, 0.5f).SetSpeed(0.001f);
    info.Add(&CameraComponent::FXAAEdgeThresholdMin, "FXAA Edge Threshold Min", PropertyKind::Float)
        .SetRange(0.0f, 0.2f).SetSpeed(0.001f);
    info.Add(&CameraComponent::FXAASubpixelBlending, "FXAA Subpixel Blending",  PropertyKind::Float)
        .SetRange(0.0f, 1.0f);

    
    info.Add(&CameraComponent::TAAEnabled,       "TAA",                  PropertyKind::Bool);
    info.Add(&CameraComponent::TAAHistoryWeight, "TAA History Weight",   PropertyKind::Float)
        .SetRange(0.0f, 0.98f).SetSpeed(0.01f);
}

BINAX_REFLECT(Binax::EditorCameraComponent)
{
    info.Add(&EditorCameraComponent::MoveSpeed,       "Move Speed",       PropertyKind::Float).SetRange(0.1f, 20.0f);
    info.Add(&EditorCameraComponent::RotationSpeed,   "Rotation Speed",   PropertyKind::Float).SetRange(0.1f, 10.0f);
    info.Add(&EditorCameraComponent::SpeedMultiplier, "Speed Multiplier", PropertyKind::Float).SetRange(0.1f, 10.0f);
}

BINAX_REFLECT(Binax::LightComponent)
{
    info.Add(&LightComponent::Type, "Type", PropertyKind::Enum).SetEnums({
        { static_cast<int>(LightType::Directional), "Directional" },
        { static_cast<int>(LightType::Point),       "Point"       },
        { static_cast<int>(LightType::Spot),        "Spot"        }
    });
    info.Add(&LightComponent::Color,       "Color",        PropertyKind::Color3);
    info.Add(&LightComponent::Intensity,   "Intensity",    PropertyKind::Float).SetRange(0.0f, 10.0f);
    info.Add(&LightComponent::Range,       "Range",        PropertyKind::Float).SetRange(0.1f, 100.0f);
    info.Add(&LightComponent::InnerCutoff, "Inner Cutoff", PropertyKind::Float).SetRange(0.0f, 1.0f);
    info.Add(&LightComponent::OuterCutoff, "Outer Cutoff", PropertyKind::Float).SetRange(0.0f, 1.0f);

    // ---- Shadow fields (Directional / Spot / Point) -------------------
    // VisibleWhen НЕ ставим — поля нужны для всех трёх типов.
    info.Add(&LightComponent::Mode, "Shadow Mode", PropertyKind::Enum)
        .SetEnums({
            { (int)ShadowMode::None, "None" },
            { (int)ShadowMode::Hard, "Hard" },
            { (int)ShadowMode::Soft, "Soft" }
        });

    info.Add(&LightComponent::ResolutionMode, "Shadow Res Mode", PropertyKind::Enum)
        .SetEnums({
            { (int)ShadowResolutionMode::Global, "Global" },
            { (int)ShadowResolutionMode::Custom, "Custom" }
        });

    info.Add(&LightComponent::ShadowResolution, "Shadow Resolution", PropertyKind::Int)
        .SetRange(512.0f, 4096.0f).SetSpeed(256.0f);

    info.Add(&LightComponent::ShadowStrength, "Shadow Strength", PropertyKind::Float)
        .SetRange(0.0f, 1.0f);

    info.Add(&LightComponent::BiasMultiplier, "Bias Multiplier", PropertyKind::Float)
        .SetRange(0.1f, 10.0f).SetSpeed(0.05f);
}

BINAX_REFLECT(Binax::MeshComponent)
{
    
    
    info.Add(&MeshComponent::MeshId, "Mesh Id", PropertyKind::UInt)
        .SetReadOnly().SetTransient();
}

BINAX_REFLECT(Binax::MaterialComponent)
{
    info.Add(&MaterialComponent::MaterialId, "Material Id", PropertyKind::UInt)
        .SetReadOnly().SetTransient();
}

BINAX_REFLECT(Binax::SkyboxComponent)
{
    info.Add(&SkyboxComponent::Enabled,    "Enabled",    PropertyKind::Bool);
    info.Add(&SkyboxComponent::FacePX,     "Face +X",    PropertyKind::Asset).SetAssetType("Texture");
    info.Add(&SkyboxComponent::FaceNX,     "Face -X",    PropertyKind::Asset).SetAssetType("Texture");
    info.Add(&SkyboxComponent::FacePY,     "Face +Y",    PropertyKind::Asset).SetAssetType("Texture");
    info.Add(&SkyboxComponent::FaceNY,     "Face -Y",    PropertyKind::Asset).SetAssetType("Texture");
    info.Add(&SkyboxComponent::FacePZ,     "Face +Z",    PropertyKind::Asset).SetAssetType("Texture");
    info.Add(&SkyboxComponent::FaceNZ,     "Face -Z",    PropertyKind::Asset).SetAssetType("Texture");
    info.Add(&SkyboxComponent::Rotation,   "Rotation",   PropertyKind::Float).SetRange(-180.0f, 180.0f);
    info.Add(&SkyboxComponent::Brightness, "Brightness", PropertyKind::Float).SetRange(0.0f, 10.0f);
}

BINAX_REFLECT(Binax::EntityFlags)
{
    info.Add(&EntityFlags::Enabled, "Enabled", PropertyKind::Bool);
    info.Add(&EntityFlags::Static,  "Static",  PropertyKind::Bool);
}

BINAX_REFLECT(Binax::NameComponent)
{
    info.Add(&NameComponent::Name, "Name", PropertyKind::String);
}

BINAX_REFLECT(Binax::ReflectionProbeComponent)
{
    info.Add(&ReflectionProbeComponent::BoxExtents,    "Box Extents",    PropertyKind::Vec3)
        .SetSpeed(0.1f);
    info.Add(&ReflectionProbeComponent::Realtime,      "Realtime",       PropertyKind::Bool);
    info.Add(&ReflectionProbeComponent::BoxProjection, "Box Projection", PropertyKind::Bool);
    info.Add(&ReflectionProbeComponent::ReflectSkybox, "Reflect Skybox", PropertyKind::Bool);
    info.Add(&ReflectionProbeComponent::Resolution, "Resolution", PropertyKind::Enum)
        .SetEnums({
            { 128,  "128"  },
            { 256,  "256"  },
            { 512,  "512"  },
            { 1024, "1024" }
        })
        .SetVisibleWhen([](const void*) { return false; });
    info.Add(&ReflectionProbeComponent::ProbeIndex, "Probe Index", PropertyKind::UInt)
        .SetReadOnly().SetTransient();
    info.Add(&ReflectionProbeComponent::NeedsBake, "Needs Bake", PropertyKind::Bool)
        .SetReadOnly().SetTransient();
}

BINAX_REFLECT(Binax::PostProcessComponent)
{
    info.Add(&PostProcessComponent::Enabled,  "Enabled",  PropertyKind::Bool);
    info.Add(&PostProcessComponent::Exposure, "Exposure", PropertyKind::Float)
        .SetRange(0.1f, 4.0f).SetSpeed(0.01f);
    info.Add(&PostProcessComponent::Tonemap, "Tonemap", PropertyKind::Enum).SetEnums({
        { static_cast<int>(TonemapMode::ACES),     "ACES"    },
        { static_cast<int>(TonemapMode::Reinhard), "Reinhard" },
        { static_cast<int>(TonemapMode::Filmic),   "Filmic"   },
        { static_cast<int>(TonemapMode::AgX),      "AgX"      },
        { static_cast<int>(TonemapMode::Linear),   "Linear"   }
    });

    
    info.Add(&PostProcessComponent::Contrast,    "Contrast",    PropertyKind::Float).SetRange(0.0f, 2.0f).SetSpeed(0.01f);
    info.Add(&PostProcessComponent::Saturation,  "Saturation",  PropertyKind::Float).SetRange(0.0f, 2.0f).SetSpeed(0.01f);
    info.Add(&PostProcessComponent::Temperature, "Temperature", PropertyKind::Float).SetRange(-1.0f, 1.0f).SetSpeed(0.01f);
    info.Add(&PostProcessComponent::Tint,        "Tint",        PropertyKind::Float).SetRange(-1.0f, 1.0f).SetSpeed(0.01f);
    info.Add(&PostProcessComponent::Lift,  "Lift",  PropertyKind::Color3);
    info.Add(&PostProcessComponent::Gamma, "Gamma", PropertyKind::Color3);
    info.Add(&PostProcessComponent::Gain,  "Gain",  PropertyKind::Color3);

    
    info.Add(&PostProcessComponent::BloomEnabled,   "Bloom",           PropertyKind::Bool);
    info.Add(&PostProcessComponent::BloomThreshold, "Bloom Threshold", PropertyKind::Float)
        .SetRange(0.0f, 10.0f).SetSpeed(0.01f);
    info.Add(&PostProcessComponent::BloomIntensity, "Bloom Intensity", PropertyKind::Float)
        .SetRange(0.0f, 2.0f).SetSpeed(0.005f);

        // ---- SSAO ---------------------------------------------------------
    info.Add(&PostProcessComponent::SSAOEnabled,   "SSAO",           PropertyKind::Bool);
    info.Add(&PostProcessComponent::SSAORadius,    "SSAO Radius",    PropertyKind::Float)
        .SetRange(0.01f, 5.0f).SetSpeed(0.01f);
    info.Add(&PostProcessComponent::SSAOIntensity, "SSAO Intensity", PropertyKind::Float)
        .SetRange(0.0f, 3.0f).SetSpeed(0.01f);
    info.Add(&PostProcessComponent::SSAOBias,      "SSAO Bias",      PropertyKind::Float)
        .SetRange(0.0f, 0.5f).SetSpeed(0.001f);
    info.Add(&PostProcessComponent::SSAOPower,     "SSAO Power",     PropertyKind::Float)
        .SetRange(0.5f, 5.0f).SetSpeed(0.01f);
    info.Add(&PostProcessComponent::SSAOSamples,   "SSAO Samples",   PropertyKind::Int)
        .SetRange(4.0f, 32.0f);

        // ---- Volumetric Fog ------------------------------------------------
    info.Add(&PostProcessComponent::VolumetricFogEnabled, "Volumetric Fog", PropertyKind::Bool);
    info.Add(&PostProcessComponent::VolumetricFogColor,   "Fog Ray Color",  PropertyKind::Color3);
    info.Add(&PostProcessComponent::VolumetricFogIntensity, "Ray Brightness", PropertyKind::Float)
        .SetRange(0.0f, 10.0f).SetSpeed(0.05f);
    info.Add(&PostProcessComponent::VolumetricFogDensity, "Ray Density",    PropertyKind::Float)
        .SetRange(0.0f, 0.5f).SetSpeed(0.001f);
    info.Add(&PostProcessComponent::VolumetricFogMinHeight, "Min Height",   PropertyKind::Float)
        .SetRange(-1000.0f, 1000.0f).SetSpeed(0.5f);
    info.Add(&PostProcessComponent::VolumetricFogMaxHeight, "Max Height",   PropertyKind::Float)
        .SetRange(-1000.0f, 1000.0f).SetSpeed(0.5f);
    info.Add(&PostProcessComponent::VolumetricFogDistance,  "Max Distance", PropertyKind::Float)
        .SetRange(1.0f, 1000.0f).SetSpeed(1.0f);
    info.Add(&PostProcessComponent::VolumetricFogSteps,     "Ray Steps",    PropertyKind::Int)
        .SetRange(8.0f, 128.0f);
    info.Add(&PostProcessComponent::VolumetricFogBlurIterations, "Blur Iterations", PropertyKind::Int)
        .SetRange(0.0f, 8.0f);
    info.Add(&PostProcessComponent::VolumetricFogScattering, "Scattering",  PropertyKind::Float)
        .SetRange(0.0f, 0.95f).SetSpeed(0.01f);
        // ---- Volumetric Fog — Noise ---------------------------------------
    info.Add(&PostProcessComponent::VolumetricFogNoiseEnabled, "Noise", PropertyKind::Bool);
    info.Add(&PostProcessComponent::VolumetricFogNoiseSpeed, "Noise Speed", PropertyKind::Float)
        .SetRange(0.0f, 20.0f).SetSpeed(0.05f);
    info.Add(&PostProcessComponent::VolumetricFogNoiseScale, "Noise Scale", PropertyKind::Float)
        .SetRange(0.001f, 2.0f).SetSpeed(0.001f);
    info.Add(&PostProcessComponent::VolumetricFogNoiseIntensity, "Noise Intensity", PropertyKind::Float)
        .SetRange(0.0f, 1.0f).SetSpeed(0.01f);

    
    info.Add(&PostProcessComponent::MotionBlurEnabled,  "Motion Blur",         PropertyKind::Bool);
    info.Add(&PostProcessComponent::MotionBlurStrength, "Motion Blur Strength", PropertyKind::Float)
        .SetRange(0.0f, 2.0f).SetSpeed(0.01f);
    info.Add(&PostProcessComponent::MotionBlurSamples,  "Motion Blur Samples",  PropertyKind::Int)
        .SetRange(2.0f, 16.0f);

    
    info.Add(&PostProcessComponent::LensDistortionEnabled, "Lens Distortion",    PropertyKind::Bool);
    info.Add(&PostProcessComponent::LensDistortion,        "Distortion Amount",  PropertyKind::Float)
        .SetRange(-0.5f, 0.5f).SetSpeed(0.001f);

    
    info.Add(&PostProcessComponent::ChromaticAberrationEnabled, "Chromatic Aberration", PropertyKind::Bool);
    info.Add(&PostProcessComponent::ChromaticAberration,        "CA Amount",            PropertyKind::Float)
        .SetRange(0.0f, 1.0f).SetSpeed(0.005f);

    
    info.Add(&PostProcessComponent::VignetteEnabled,    "Vignette",            PropertyKind::Bool);
    info.Add(&PostProcessComponent::VignetteIntensity,  "Vignette Intensity",  PropertyKind::Float)
        .SetRange(0.0f, 1.0f).SetSpeed(0.005f);
    info.Add(&PostProcessComponent::VignetteSmoothness, "Vignette Smoothness", PropertyKind::Float)
        .SetRange(0.0f, 1.0f).SetSpeed(0.005f);

    
    info.Add(&PostProcessComponent::FilmGrainEnabled,   "Film Grain",           PropertyKind::Bool);
    info.Add(&PostProcessComponent::FilmGrainIntensity, "Film Grain Intensity", PropertyKind::Float)
        .SetRange(0.0f, 0.5f).SetSpeed(0.001f);

    
    info.Add(&PostProcessComponent::FogEnabled, "Fog", PropertyKind::Bool);
    info.Add(&PostProcessComponent::Fog, "Fog Mode", PropertyKind::Enum).SetEnums({
        { static_cast<int>(FogMode::Linear),             "Linear"             },
        { static_cast<int>(FogMode::Exponential),        "Exponential"        },
        { static_cast<int>(FogMode::ExponentialSquared), "Exponential Squared" }
    });
    info.Add(&PostProcessComponent::FogColor,   "Fog Color",   PropertyKind::Color3);
    info.Add(&PostProcessComponent::FogDensity, "Fog Density", PropertyKind::Float)
        .SetRange(0.0f, 0.5f).SetSpeed(0.001f);
    info.Add(&PostProcessComponent::FogStart,   "Fog Start",   PropertyKind::Float)
        .SetRange(0.0f, 500.0f).SetSpeed(0.1f);
    info.Add(&PostProcessComponent::FogEnd,     "Fog End",     PropertyKind::Float)
        .SetRange(0.0f, 2000.0f).SetSpeed(0.5f);
}

namespace Binax::Reflect {

void RegisterEngineComponents()
{
    static bool done = false;
    if (done) return;
    done = true;

    auto& cr = ComponentRegistry::Get();
    cr.Register<Transform>             ("Transform");
    cr.Register<MeshComponent>         ("Mesh Component");
    cr.Register<MaterialComponent>     ("Material");
    cr.Register<LightComponent>        ("Light Component");
    cr.Register<CameraComponent>       ("Camera Component");
    cr.Register<NameComponent>         ("Name",                  true,  false);
    cr.Register<EntityFlags>("EntityFlags", /*hidden=*/true, /*transient=*/false);
    cr.Register<EditorCameraComponent> ("EditorCameraComponent", true,  true);
    cr.Register<SkyboxComponent>       ("Skybox");
    cr.Register<PostProcessComponent>  ("Post Processing");
    cr.Register<ReflectionProbeComponent>("Reflection Probe");
}

}
