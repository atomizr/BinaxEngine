
#include "Reflect/ComponentRegistry.h"
#include "Utils/Log.h"

namespace Binax::Reflect {

ComponentRegistry& ComponentRegistry::Get()
{
    static ComponentRegistry instance;
    return instance;
}

void ComponentRegistry::WarnDuplicate(const std::string& name) const
{
    BINAX_LOG_WARN("Component '{}' already registered - ignoring", name);
}

const ComponentRegistry::Entry* ComponentRegistry::Find(std::string_view name) const
{
    auto it = m_ByName.find(std::string(name));
    return it != m_ByName.end() ? m_Entries[it->second].get() : nullptr;
}

const ComponentRegistry::Entry* ComponentRegistry::Find(const void* typeId) const
{
    auto it = m_ByTypeId.find(typeId);
    return it != m_ByTypeId.end() ? m_Entries[it->second].get() : nullptr;
}

} 
