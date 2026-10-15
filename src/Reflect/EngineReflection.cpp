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
    info.Add(&CameraComponent::FXAAEnabled,          "FXAA",                   PropertyKind::Bool);
    info.Add(&CameraComponent::FXAAEdgeThreshold,    "FXAA Edge Threshold",    PropertyKind::Float)
        .SetRange(0.0f, 0.5f).SetSpeed(0.001f);
    info.Add(&CameraComponent::FXAAEdgeThresholdMin, "FXAA Edge Threshold Min",PropertyKind::Float)
        .SetRange(0.0f, 0.2f).SetSpeed(0.001f);
    info.Add(&CameraComponent::FXAASubpixelBlending, "FXAA Subpixel Blending", PropertyKind::Float)
        .SetRange(0.0f, 1.0f);
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
}

BINAX_REFLECT(Binax::MeshComponent)
{
}

BINAX_REFLECT(Binax::MaterialComponent)
{
    // ---- Scalar params ----
    info.Add(&MaterialComponent::Albedo,    "Albedo",    PropertyKind::Color3);
    info.Add(&MaterialComponent::Metallic,  "Metallic",  PropertyKind::Float).SetRange(0.0f, 1.0f);
    info.Add(&MaterialComponent::Roughness, "Roughness", PropertyKind::Float).SetRange(0.04f, 1.0f);
    info.Add(&MaterialComponent::AO,        "AO",        PropertyKind::Float).SetRange(0.0f, 1.0f);
    info.Add(&MaterialComponent::Emissive,  "Emissive",  PropertyKind::Float).SetRange(0.0f, 5.0f);

    // ---- Albedo ----
    info.Add(&MaterialComponent::AlbedoMapPath,  "Albedo Map",       PropertyKind::Asset).SetAssetType("Texture");
    info.Add(&MaterialComponent::AlbedoStrength, "Albedo Strength",  PropertyKind::Float).SetRange(0.0f, 10.0f);

    // ---- Normal ----
    info.Add(&MaterialComponent::NormalMapPath,  "Normal Map",       PropertyKind::Asset).SetAssetType("Texture");
    info.Add(&MaterialComponent::NormalStrength, "Normal Strength",  PropertyKind::Float).SetRange(0.0f, 10.0f);

    // ---- Metallic ----
    info.Add(&MaterialComponent::MetallicMapPath,  "Metallic Map",      PropertyKind::Asset).SetAssetType("Texture");
    info.Add(&MaterialComponent::MetallicStrength, "Metallic Strength", PropertyKind::Float).SetRange(0.0f, 10.0f);

    // ---- Roughness ----
    info.Add(&MaterialComponent::RoughnessMapPath,  "Roughness Map",      PropertyKind::Asset).SetAssetType("Texture");
    info.Add(&MaterialComponent::RoughnessStrength, "Roughness Strength", PropertyKind::Float).SetRange(0.0f, 10.0f);

    // ---- AO ----
    info.Add(&MaterialComponent::AOMapPath,  "AO Map",       PropertyKind::Asset).SetAssetType("Texture");
    info.Add(&MaterialComponent::AOStrength, "AO Strength",  PropertyKind::Float).SetRange(0.0f, 10.0f);

    // ---- Emissive ----
    info.Add(&MaterialComponent::EmissiveMapPath,  "Emissive Map",      PropertyKind::Asset).SetAssetType("Texture");
    info.Add(&MaterialComponent::EmissiveStrength, "Emissive Strength", PropertyKind::Float).SetRange(0.0f, 10.0f);

    // ---- Tiling ----
    info.Add(&MaterialComponent::Tiling, "Tiling", PropertyKind::Float).SetRange(0.01f, 20.0f);

    // ---- Skybox reflections ----
    info.Add(&MaterialComponent::SkyReflectionsEnabled,
             "Sky Reflections", PropertyKind::Bool);

    info.Add(&MaterialComponent::SkyReflectionStrength,
             "Sky Reflection Strength", PropertyKind::Float)
        .SetRange(0.0f, 5.0f);
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

BINAX_REFLECT(Binax::NameComponent)
{
    info.Add(&NameComponent::Name, "Name", PropertyKind::String);
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
    cr.Register<NameComponent>         ("Name",                    true,  false);
    cr.Register<EditorCameraComponent> ("EditorCameraComponent",   true,  true);
    cr.Register<SkyboxComponent>("Skybox");
}

}
