#pragma once

#include "Reflect/Reflection.h"

namespace Binax::Reflect {

class ComponentRegistry
{
public:
    struct Entry
    {
        std::string Name;               
        const void* TypeId = nullptr;

        
        
        bool Hidden    = false;
        bool Transient = false;

        std::function<bool(entt::registry&, entt::entity)> Has;
        std::function<void(entt::registry&, entt::entity)> AddDefault;
        std::function<void(entt::registry&, entt::entity)> Remove;
        std::function<void*(entt::registry&, entt::entity)> GetPtr;
        std::function<const void*(const entt::registry&, entt::entity)> GetPtrConst;
    };

    static ComponentRegistry& Get();

    template<typename T>
    void Register(std::string uiName, bool hidden = false, bool transient = false);

    const Entry* Find(std::string_view name) const;
    const Entry* Find(const void* typeId) const;

    const std::vector<std::unique_ptr<Entry>>& All() const { return m_Entries; }

private:
    ComponentRegistry() = default;
    ComponentRegistry(const ComponentRegistry&) = delete;
    ComponentRegistry& operator=(const ComponentRegistry&) = delete;

    
    
    void WarnDuplicate(const std::string& name) const;

    std::vector<std::unique_ptr<Entry>> m_Entries;
    std::unordered_map<std::string, size_t> m_ByName;
    std::unordered_map<const void*, size_t> m_ByTypeId;
};

template<typename T>
void ComponentRegistry::Register(std::string uiName, bool hidden, bool transient)
{
    
    
    
    if (m_ByName.count(uiName) || m_ByTypeId.count(GetTypeId<T>()))
    {
        WarnDuplicate(uiName);
        return;
    }

    auto e = std::make_unique<Entry>();
    e->Name      = std::move(uiName);
    e->TypeId    = GetTypeId<T>();
    e->Hidden    = hidden;
    e->Transient = transient;

    e->Has        = [](entt::registry& r, entt::entity en) { return r.any_of<T>(en); };
    e->AddDefault = [](entt::registry& r, entt::entity en) { if (!r.any_of<T>(en)) r.emplace<T>(en); };
    e->Remove     = [](entt::registry& r, entt::entity en) { if (r.any_of<T>(en))  r.remove<T>(en); };
    e->GetPtr     = [](entt::registry& r, entt::entity en) -> void* {
        return r.any_of<T>(en) ? static_cast<void*>(&r.get<T>(en)) : nullptr;
    };
    e->GetPtrConst = [](const entt::registry& r, entt::entity en) -> const void* {
        return r.any_of<T>(en) ? static_cast<const void*>(&r.get<T>(en)) : nullptr;
    };

    size_t idx = m_Entries.size();
    m_ByName[e->Name]     = idx;
    m_ByTypeId[e->TypeId] = idx;
    m_Entries.push_back(std::move(e));
}

}
