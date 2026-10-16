#include "Serialization/PropertySerializer.h"
#include "Reflect/Reflection.h"
#include "Reflect/ComponentRegistry.h"
#include "Utils/Log.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Binax::Serialization {

using namespace Reflect;

namespace {

nlohmann::json SerializeProperty(const void* obj, const PropertyInfo& p)
{
    using PK = PropertyKind;
    switch (p.Kind)
    {
    case PK::Bool:   return *p.GetPtr<bool>(obj);
    case PK::Int:    return *p.GetPtr<int>(obj);
    case PK::UInt:   return *p.GetPtr<uint32_t>(obj);
    case PK::Float:  return *p.GetPtr<float>(obj);
    case PK::Double: return *p.GetPtr<double>(obj);
    case PK::String: return *p.GetPtr<std::string>(obj);
    case PK::Asset:  return *p.GetPtr<std::string>(obj);
    case PK::Enum:   return *p.GetPtr<int>(obj);
    case PK::Vec2:   { auto* v = p.GetPtr<glm::vec2>(obj); return nlohmann::json{ v->x, v->y }; }
    case PK::Vec3:
    case PK::Color3: { auto* v = p.GetPtr<glm::vec3>(obj); return nlohmann::json{ v->x, v->y, v->z }; }
    case PK::Vec4:
    case PK::Color4: { auto* v = p.GetPtr<glm::vec4>(obj); return nlohmann::json{ v->x, v->y, v->z, v->w }; }
    case PK::Quat:   { auto* q = p.GetPtr<glm::quat>(obj); return nlohmann::json{ q->w, q->x, q->y, q->z }; }
    default:         return nullptr;
    }
}

void DeserializeProperty(void* obj, const PropertyInfo& p, const nlohmann::json& j)
{
    using PK = PropertyKind;
    try {
        switch (p.Kind)
        {
        case PK::Bool:   *p.GetPtr<bool>(obj)        = j.get<bool>();        break;
        case PK::Int:    *p.GetPtr<int>(obj)         = j.get<int>();         break;
        case PK::UInt:   *p.GetPtr<uint32_t>(obj)    = j.get<uint32_t>();    break;
        case PK::Float:  *p.GetPtr<float>(obj)       = j.get<float>();       break;
        case PK::Double: *p.GetPtr<double>(obj)      = j.get<double>();      break;
        case PK::String: *p.GetPtr<std::string>(obj) = j.get<std::string>(); break;
        case PK::Asset:  *p.GetPtr<std::string>(obj) = j.get<std::string>(); break;
        case PK::Enum:   *p.GetPtr<int>(obj)         = j.get<int>();         break;
        case PK::Vec2:   { auto* v = p.GetPtr<glm::vec2>(obj);
                           v->x = j[0]; v->y = j[1]; break; }
        case PK::Vec3:
        case PK::Color3: { auto* v = p.GetPtr<glm::vec3>(obj);
                           v->x = j[0]; v->y = j[1]; v->z = j[2]; break; }
        case PK::Vec4:
        case PK::Color4: { auto* v = p.GetPtr<glm::vec4>(obj);
                           v->x = j[0]; v->y = j[1]; v->z = j[2]; v->w = j[3]; break; }
        case PK::Quat:   { auto* q = p.GetPtr<glm::quat>(obj);
                           q->w = j[0]; q->x = j[1]; q->y = j[2]; q->z = j[3]; break; }
        default: break;
        }
    } catch (const std::exception& e) {
        BINAX_LOG_WARN("Failed to read property '{}': {}", p.Name, e.what());
    }
}

} 

nlohmann::json PropertySerializer::SerializeEntity(entt::registry& reg, entt::entity e)
{
    nlohmann::json out;
    auto& cr = ComponentRegistry::Get();

    for (const auto& entryPtr : cr.All())
    {
        const auto& entry = *entryPtr;
        if (entry.Transient)         continue;
        if (!entry.Has(reg, e))      continue;

        const ClassInfo* info = TypeRegistry::Get().Find(entry.TypeId);
        if (!info) continue;

        const void* obj = entry.GetPtrConst(reg, e);
        if (!obj) continue;

        nlohmann::json comp;
        for (const auto& prop : info->Properties)
        {
            if (prop.Transient) continue;
            comp[prop.Name] = SerializeProperty(obj, prop);
        }
        out[entry.Name] = std::move(comp);
    }
    return out;
}

void PropertySerializer::DeserializeEntity(entt::registry& reg, entt::entity e,
                                           const nlohmann::json& j)
{
    auto& cr = ComponentRegistry::Get();

    for (auto it = j.begin(); it != j.end(); ++it)
    {
        const std::string& compName = it.key();
        const auto* entry = cr.Find(compName);
        if (!entry)
        {
            BINAX_LOG_WARN("Unknown component '{}' in scene file", compName);
            continue;
        }

        if (entry->Transient)
        {
            BINAX_LOG_WARN("Skipping transient component '{}' from scene file", compName);
            continue;
        }

        if (!entry->Has(reg, e))
            entry->AddDefault(reg, e);

        void* obj = entry->GetPtr(reg, e);
        const ClassInfo* info = TypeRegistry::Get().Find(entry->TypeId);
        if (!obj || !info) continue;

        const auto& comp = it.value();
        for (const auto& prop : info->Properties)
        {
            if (prop.Transient) continue;
            if (!comp.contains(prop.Name)) continue;
            DeserializeProperty(obj, prop, comp[prop.Name]);
        }
    }
}

}
