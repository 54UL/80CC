#pragma once
#include <ECS/Entity.hpp>
#include <ECS/ComponentPool.hpp>
#include <typeindex>
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <algorithm>

namespace ettycc::ecs {

// Central storage for all component types.
// Each component type gets its own dense ComponentPool<T>.
// Entities are identified by their uint64_t ID.
class Registry
{
public:
    // ── Entity lifecycle ──────────────────────────────────────────────────────

    // Allocate a fresh entity ID.
    Entity Create()
    {
        Entity e = nextId_++;
        alive_.push_back(e);
        return e;
    }

    // Register an externally-assigned ID (deserialization).
    // Ensures the registry tracks the entity and next IDs are higher.
    void Track(Entity e)
    {
        if (!Valid(e)) alive_.push_back(e);
        if (e >= nextId_) nextId_ = e + 1;
    }

    void Destroy(Entity e)
    {
        for (auto& [ti, holder] : pools_)
            holder->Remove(e);
        alive_.erase(std::remove(alive_.begin(), alive_.end(), e), alive_.end());
        typeNames_.erase(e);
    }

    bool Valid(Entity e) const
    {
        return std::find(alive_.begin(), alive_.end(), e) != alive_.end();
    }

    // ── Component add / get / has / remove ────────────────────────────────────

    template<typename T>
    T& Add(Entity e, T comp = T{})
    {
        T& result = GetHolder<T>().pool.Add(e, std::move(comp));

        // Record type name per entity so the editor can enumerate components.
        if constexpr (HasComponentType<T>::value)
            typeNames_[e].push_back(T::componentType);

        return result;
    }

    template<typename T>
    [[nodiscard]] T* Get(Entity e)
    {
        auto* p = FindPool<T>();
        return p ? p->Get(e) : nullptr;
    }

    template<typename T>
    [[nodiscard]] const T* Get(Entity e) const
    {
        const auto* p = FindPool<T>();
        return p ? p->Get(e) : nullptr;
    }

    template<typename T>
    [[nodiscard]] bool Has(Entity e) const
    {
        const auto* p = FindPool<T>();
        return p && p->Has(e);
    }

    template<typename T>
    void Remove(Entity e)
    {
        if (auto* p = FindPool<T>())
            p->Remove(e);

        // Also remove the type name entry
        if constexpr (HasComponentType<T>::value)
        {
            auto nameIt = typeNames_.find(e);
            if (nameIt != typeNames_.end())
            {
                auto& names = nameIt->second;
                names.erase(std::remove(names.begin(), names.end(),
                                        std::string(T::componentType)), names.end());
            }
        }
    }

    // Direct pool access — useful for systems that iterate all components.
    template<typename T>
    ComponentPool<T>& Pool()
    {
        return GetHolder<T>().pool;
    }

    // View: returns all entities that have EVERY one of the listed types.
    template<typename First, typename... Rest>
    [[nodiscard]] std::vector<Entity> View() const
    {
        std::vector<Entity> result;
        const auto* fp = FindPool<First>();
        if (!fp) return result;
        for (Entity e : fp->Entities())
            if ((Has<Rest>(e) && ...))
                result.push_back(e);
        return result;
    }

    // Editor: list of component type names on an entity.
    const std::vector<std::string>& GetComponentTypes(Entity e) const
    {
        static const std::vector<std::string> empty;
        auto it = typeNames_.find(e);
        return it != typeNames_.end() ? it->second : empty;
    }

    const std::vector<Entity>& Entities() const { return alive_; }

    void Clear()
    {
        pools_.clear();
        alive_.clear();
        typeNames_.clear();
        nextId_ = 1;
    }

private:
    // ── Compile-time helpers ──────────────────────────────────────────────────
    template<typename T, typename = void>
    struct HasComponentType : std::false_type {};
    template<typename T>
    struct HasComponentType<T, std::void_t<decltype(T::componentType)>>
        : std::true_type {};

    // ── Type-erased pool wrapper ──────────────────────────────────────────────
    struct IHolder
    {
        virtual ~IHolder() = default;
        virtual void Remove(Entity e) = 0;
    };

    template<typename T>
    struct Holder : IHolder
    {
        ComponentPool<T> pool;
        void Remove(Entity e) override { pool.Remove(e); }
    };

    // ── Pool lookup helpers ───────────────────────────────────────────────────
    // Returns nullptr if no pool for T has been created yet.
    template<typename T>
    ComponentPool<T>* FindPool()
    {
        auto it = pools_.find(std::type_index(typeid(T)));
        return it != pools_.end()
            ? &static_cast<Holder<T>*>(it->second.get())->pool
            : nullptr;
    }

    template<typename T>
    const ComponentPool<T>* FindPool() const
    {
        auto it = pools_.find(std::type_index(typeid(T)));
        return it != pools_.end()
            ? &static_cast<const Holder<T>*>(it->second.get())->pool
            : nullptr;
    }

    template<typename T>
    Holder<T>& GetHolder()
    {
        auto ti = std::type_index(typeid(T));
        auto it = pools_.find(ti);
        if (it == pools_.end())
        {
            pools_[ti] = std::make_unique<Holder<T>>();
            return *static_cast<Holder<T>*>(pools_[ti].get());
        }
        return *static_cast<Holder<T>*>(it->second.get());
    }

    // ── Data ──────────────────────────────────────────────────────────────────
    std::unordered_map<std::type_index, std::unique_ptr<IHolder>> pools_;
    std::vector<Entity>                                            alive_;
    std::unordered_map<Entity, std::vector<std::string>>          typeNames_;
    Entity                                                         nextId_ = 1;
};

} // namespace ettycc::ecs
