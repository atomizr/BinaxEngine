
#include "Reflect/Reflection.h"
#include "Utils/Log.h"

namespace Binax::Reflect {

TypeRegistry& TypeRegistry::Get()
{
    static TypeRegistry instance;
    return instance;
}

void TypeRegistry::Register(const void* typeId, ClassInfo info)
{
    if (m_Types.count(typeId))
        BINAX_LOG_WARN("Type '{}' already registered — overwriting", info.Name);

    m_ByName[info.Name] = typeId;
    m_Types[typeId]     = std::move(info);
}

const ClassInfo* TypeRegistry::Find(const void* typeId) const
{
    auto it = m_Types.find(typeId);
    return it != m_Types.end() ? &it->second : nullptr;
}

const ClassInfo* TypeRegistry::FindByName(std::string_view name) const
{
    auto it = m_ByName.find(std::string(name));
    if (it == m_ByName.end()) return nullptr;
    return Find(it->second);
}

} 