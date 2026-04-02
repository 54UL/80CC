#pragma once
#include <ECS/Entity.hpp>
#include <vector>
#include <unordered_map>
#include <cassert>

namespace ettycc::ecs {

// Dense-packed component storage with O(1) add / remove / lookup.
// Swap-and-pop keeps the array contiguous for cache-friendly iteration.
template<typename T>
class ComponentPool
{
public:
    T& Add(const Entity e, T comp = T{})
    {
        assert(!Has(e) && "Entity already has this component type");
        sparse_[e] = dense_.size();
        entities_.push_back(e);
        dense_.push_back(std::move(comp));
        return dense_.back();
    }

    [[nodiscard]] T* Get(Entity e)
    {
        auto it = sparse_.find(e);
        return it != sparse_.end() ? &dense_[it->second] : nullptr;
    }

    [[nodiscard]] const T* Get(Entity e) const
    {
        auto it = sparse_.find(e);
        return it != sparse_.end() ? &dense_[it->second] : nullptr;
    }

    [[nodiscard]] bool Has(Entity e) const { return sparse_.count(e) > 0; }

    void Remove(Entity e)
    {
        auto it = sparse_.find(e);
        if (it == sparse_.end()) return;

        const size_t idx  = it->second;
        const size_t last = dense_.size() - 1;

        if (idx != last)
        {
            dense_[idx]              = std::move(dense_[last]);
            entities_[idx]           = entities_[last];
            sparse_[entities_[idx]]  = idx;
        }

        dense_.pop_back();
        entities_.pop_back();
        sparse_.erase(e);
    }

    // Contiguous iteration
    [[nodiscard]] std::vector<T>&            Components() { return dense_; }
    [[nodiscard]] std::vector<Entity>&       Entities()   { return entities_; }
    [[nodiscard]] const std::vector<T>&      Components() const { return dense_; }
    [[nodiscard]] const std::vector<Entity>& Entities()   const { return entities_; }

    [[nodiscard]] std::size_t Size()  const { return dense_.size(); }
    [[nodiscard]] bool        Empty() const { return dense_.empty(); }

    void Reserve(std::size_t n) { dense_.reserve(n); entities_.reserve(n); }
    void Clear()                { dense_.clear(); entities_.clear(); sparse_.clear(); }

private:
    std::vector<T>                     dense_;
    std::vector<Entity>                entities_;
    std::unordered_map<Entity, size_t> sparse_;
};

} // namespace ettycc::ecs
