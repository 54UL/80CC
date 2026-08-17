#include <UI/ComponentRegistry.hpp>
#include <algorithm>

namespace ettycc
{

ComponentRegistry& ComponentRegistry::Instance()
{
    static ComponentRegistry instance;
    return instance;
}

void ComponentRegistry::Register(ComponentEntry entry)
{
    // Replace existing entry with the same typeName (idempotent -- safe for
    // hot-reload where a module may re-register without unregistering first).
    for (auto& e : entries_)
    {
        if (e.typeName == entry.typeName)
        {
            e = std::move(entry);
            return;
        }
    }
    entries_.push_back(std::move(entry));
}

void ComponentRegistry::Unregister(const std::string& typeName)
{
    entries_.erase(
        std::remove_if(entries_.begin(), entries_.end(),
            [&](const ComponentEntry& e) { return e.typeName == typeName; }),
        entries_.end());
}

const std::vector<ComponentEntry>& ComponentRegistry::Entries() const
{
    return entries_;
}

const ComponentEntry* ComponentRegistry::FindByType(const std::string& typeName) const
{
    for (auto& e : entries_)
        if (e.typeName == typeName) return &e;
    return nullptr;
}

} // namespace ettycc
