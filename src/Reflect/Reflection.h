
#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>

namespace Binax::Reflect {

template<typename T>
struct TypeIdHolder { static const char Id; };

template<typename T>
const char TypeIdHolder<T>::Id = 0;

template<typename T>
inline const void* GetTypeId() noexcept { return &TypeIdHolder<T>::Id; }

enum class PropertyKind : uint8_t
{
    Bool, Int, UInt, Float, Double,
    Vec2, Vec3, Vec4, Quat,
    Color3, Color4,
    String,
    Enum,
    Entity,
    Asset,
    Custom
};

struct EnumEntry
{
    int Value = 0;
    std::string Name;
};

struct PropertyInfo
{
    std::string Name;
    PropertyKind Kind = PropertyKind::Custom;
    uint32_t Offset = 0;
    uint32_t Size   = 0;

    float Min = 0.0f;
    float Max = 0.0f;
    float Speed = 0.1f;
    bool  HasRange = false;

    bool  ReadOnly  = false;
    bool  Transient = false;

    // Asset hint for PropertyKind::Asset — e.g. "Texture", "Mesh".
    std::string AssetType;

    std::vector<EnumEntry> EnumValues;

    template<typename T> T*       GetPtr(void* obj)       const noexcept
    { return reinterpret_cast<T*>(static_cast<char*>(obj) + Offset); }
    template<typename T> const T* GetPtr(const void* obj) const noexcept
    { return reinterpret_cast<const T*>(static_cast<const char*>(obj) + Offset); }

    PropertyInfo& SetSpeed(float s)                  { Speed = s; return *this; }
    PropertyInfo& SetRange(float mn, float mx)       { Min = mn; Max = mx; HasRange = true; return *this; }
    PropertyInfo& SetEnums(std::vector<EnumEntry> e) { EnumValues = std::move(e); return *this; }
    PropertyInfo& SetReadOnly()                      { ReadOnly  = true; return *this; }
    PropertyInfo& SetTransient()                     { Transient = true; return *this; }
    PropertyInfo& SetAssetType(std::string t)        { AssetType = std::move(t); return *this; }
};

struct ClassInfo
{
    std::string Name;
    std::string DisplayName;
    size_t Size = 0;
    std::vector<PropertyInfo> Properties;

    template<typename C, typename M>
    PropertyInfo& Add(M C::* member, std::string name, PropertyKind kind)
    {
        PropertyInfo p;
        p.Name   = std::move(name);
        p.Kind   = kind;
        p.Size   = static_cast<uint32_t>(sizeof(M));
        p.Offset = MemberOffset(member);
        Properties.push_back(std::move(p));
        return Properties.back();
    }

    const PropertyInfo* Find(std::string_view name) const
    {
        for (const auto& p : Properties)
            if (p.Name == name) return &p;
        return nullptr;
    }

private:
    template<typename C, typename M>
    static uint32_t MemberOffset(M C::* member) noexcept
    {
        constexpr uintptr_t kFake = 0x1000;
        const C* base = reinterpret_cast<const C*>(kFake);
        const M* m    = &(base->*member);
        return static_cast<uint32_t>(reinterpret_cast<const char*>(m)
                                     - reinterpret_cast<const char*>(base));
    }
};

class TypeRegistry
{
public:
    static TypeRegistry& Get();

    void Register(const void* typeId, ClassInfo info);
    const ClassInfo* Find(const void* typeId) const;
    const ClassInfo* FindByName(std::string_view name) const;

    template<typename T>
    const ClassInfo* Find() const { return Find(GetTypeId<T>()); }

    const std::unordered_map<const void*, ClassInfo>& All() const { return m_Types; }

private:
    TypeRegistry() = default;
    TypeRegistry(const TypeRegistry&) = delete;
    TypeRegistry& operator=(const TypeRegistry&) = delete;

    std::unordered_map<const void*, ClassInfo> m_Types;
    std::unordered_map<std::string, const void*> m_ByName;
};

template<typename T>
struct TypeRegistrar;

} 









#define BINAX_REFLECT_CONCAT_(a, b) a##b
#define BINAX_REFLECT_CONCAT(a, b)  BINAX_REFLECT_CONCAT_(a, b)

#define BINAX_REFLECT(Type)                                                      \
    namespace Binax::Reflect {                                                   \
        template<> struct TypeRegistrar<Type> {                                  \
            static void Register(ClassInfo& info);                               \
        };                                                                       \
    }                                                                            \
    namespace {                                                                  \
        [[maybe_unused]]                                                         \
        const bool BINAX_REFLECT_CONCAT(g_ReflectReg_, __LINE__) = []() {        \
            ::Binax::Reflect::ClassInfo info_;                                   \
            std::string full_ = #Type;                                           \
            auto pos_ = full_.rfind("::");                                       \
            info_.DisplayName = (pos_ == std::string::npos)                      \
                                ? full_ : full_.substr(pos_ + 2);                \
            info_.Name = std::move(full_);                                       \
            info_.Size = sizeof(Type);                                           \
            ::Binax::Reflect::TypeRegistrar<Type>::Register(info_);              \
            ::Binax::Reflect::TypeRegistry::Get().Register(                      \
                ::Binax::Reflect::GetTypeId<Type>(), std::move(info_));          \
            return true;                                                         \
        }();                                                                     \
    }                                                                            \
    void Binax::Reflect::TypeRegistrar<Type>::Register(Binax::Reflect::ClassInfo& info)