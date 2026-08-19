#ifndef ETTYCC_PHYSICS_REGISTRY_HPP
#define ETTYCC_PHYSICS_REGISTRY_HPP

#include <Physics/IPhysicsWorld.hpp>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

namespace ettycc::physics
{
    using PhysicsWorldFactory = std::function<std::unique_ptr<IPhysicsWorld>()>;

    class PhysicsRegistry
    {
    public:
        void Register(const std::string& name, PhysicsWorldFactory factory)
        {
            factories_[name] = std::move(factory);
        }

        std::unique_ptr<IPhysicsWorld> Create(const std::string& name) const
        {
            auto it = factories_.find(name);
            if (it == factories_.end()) return nullptr;
            return it->second();
        }

        std::vector<std::string> GetAvailable() const
        {
            std::vector<std::string> names;
            names.reserve(factories_.size());
            for (auto& [k, _] : factories_)
                names.push_back(k);
            return names;
        }

        bool Has(const std::string& name) const
        {
            return factories_.count(name) > 0;
        }

    private:
        std::unordered_map<std::string, PhysicsWorldFactory> factories_;
    };

} // namespace ettycc::physics

#endif
