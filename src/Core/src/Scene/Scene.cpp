#include <Scene/Scene.hpp>
#include <Scene/Components/RigidBodyComponent.hpp>
#include <Scene/Components/RenderableNode.hpp>
#include <Scene/Components/SoftBodyComponent.hpp>
#include <Scene/Components/AudioSourceComponent.hpp>
#include <Scene/Components/AudioListenerComponent.hpp>
#include <Networking/NetworkComponent.hpp>
#include <Dependency.hpp>

#include <cereal/types/utility.hpp>  // std::pair serialization
#include <cereal/types/vector.hpp>

namespace ettycc
{
    Scene::Scene(const std::string& name) : sceneName_(name)
    {
        root_node_ = std::make_shared<SceneNode>(sceneName_);
        root_node_->scene_ = this;
    }

    Scene::~Scene() {}

    // ── Initialization ─────────────────────────────────────────────────────────
    auto Scene::Init(Engine& engine) -> void
    {
        RebuildIndex();

        for (auto& sys : systems_)
            sys->OnStart(*this, engine);
    }

    // ── Private: rebuild fast lookup from nodes_flat_ ─────────────────────────
    void Scene::RebuildIndex()
    {
        nodeIndex_.clear();
        for (auto& node : nodes_flat_)
        {
            if (!node) continue;
            node->scene_ = this;
            nodeIndex_[node->GetId()] = node.get();
            registry_.Track(node->GetId());
        }
        // Also track root node
        if (root_node_)
        {
            root_node_->scene_ = this;
            nodeIndex_[root_node_->GetId()] = root_node_.get();
            registry_.Track(root_node_->GetId());
        }
    }

    // ── Node lookup ────────────────────────────────────────────────────────────
    auto Scene::GetNode(ecs::Entity id) -> SceneNode*
    {
        auto it = nodeIndex_.find(id);
        return it != nodeIndex_.end() ? it->second : nullptr;
    }

    auto Scene::GetNodesByName(const std::string& name)
        -> std::vector<std::shared_ptr<SceneNode>>
    {
        std::vector<std::shared_ptr<SceneNode>> result;
        for (auto& node : nodes_flat_)
            if (node && node->GetName() == name)
                result.push_back(node);
        return result;
    }

    auto Scene::GetAllNodes() -> std::vector<std::shared_ptr<SceneNode>>
    {
        return nodes_flat_;
    }

    auto Scene::GetTransform(ecs::Entity e) -> Transform*
    {
        auto* node = GetNode(e);
        return node ? &node->transform_ : nullptr;
    }

    // ── ECS ────────────────────────────────────────────────────────────────────
    auto Scene::RegisterSystem(std::unique_ptr<ISystem> system) -> void
    {
        systems_.push_back(std::move(system));
    }

    auto Scene::NotifyEntityAdded(ecs::Entity e, Engine& engine) -> void
    {
        for (auto& sys : systems_)
            sys->OnEntityAdded(*this, engine, e);
    }

    // ── Per-frame update ───────────────────────────────────────────────────────
    auto Scene::Process(float dt, ProcessingChannel channel) -> void
    {
        for (auto& sys : systems_)
            if (sys->Channel() == channel)
                sys->OnUpdate(*this, dt);
    }

    // ── Serialization (explicit specialisations to avoid linker issues) ────────
    template<>
    void Scene::serialize<cereal::JSONOutputArchive>(cereal::JSONOutputArchive& ar)
    {
        // Snapshot the current counter so a reload can fast-forward past it.
        maxEntityId_ = Utils::EntityCounter() - 1;
        ar(CEREAL_NVP(sceneName_), CEREAL_NVP(maxEntityId_),
           CEREAL_NVP(root_node_), CEREAL_NVP(nodes_flat_));
        SerializeComponents(ar);
    }

    template<>
    void Scene::serialize<cereal::JSONInputArchive>(cereal::JSONInputArchive& ar)
    {
        ar(CEREAL_NVP(sceneName_));
        try { ar(CEREAL_NVP(maxEntityId_)); } catch (...) {}   // missing in old scenes
        ar(CEREAL_NVP(root_node_), CEREAL_NVP(nodes_flat_));
        SerializeComponents(ar);
        // Prevent new nodes from colliding with loaded entity IDs.
        Utils::FastForwardEntityCounter(maxEntityId_);
    }

    // ── Component pool serialization ───────────────────────────────────────────
    // Each pool is stored as a vector of {entity, componentData} pairs.
    // We use cereal::make_nvp for clear JSON keys.

    void Scene::SerializeComponents(cereal::JSONOutputArchive& ar)
    {
        // Components like RigidBodyComponent are move-only (copy deleted), so we
        // can't build vector<pair<Entity,T>>.  Instead we build a vector of
        // non-owning proxy views that serialize as {"first":e,"second":{...}} —
        // the exact same JSON layout cereal produces for pair<Entity,T>, so the
        // JSONInputArchive load path (which uses vector<pair<Entity,T>>) is compatible.
        auto save = [&](auto& pool, const char* key)
        {
            using ComponentVec = std::remove_reference_t<decltype(pool.Components())>;
            using T = typename ComponentVec::value_type;

            std::vector<EntryView<T>> views;
            views.reserve(pool.Size());
            for (size_t i = 0; i < pool.Size(); ++i)
                views.emplace_back(EntryView<T>{pool.Entities()[i], &pool.Components()[i]});
            ar(cereal::make_nvp(key, views));
        };

        save(registry_.Pool<RigidBodyComponent>(),    "rigid_bodies");
        save(registry_.Pool<RenderableNode>(),         "renderables");
        save(registry_.Pool<SoftBodyComponent>(),      "soft_bodies");
        save(registry_.Pool<AudioSourceComponent>(),   "audio_sources");
        save(registry_.Pool<AudioListenerComponent>(), "audio_listeners");
        save(registry_.Pool<NetworkComponent>(),       "network_components");
    }

    void Scene::SerializeComponents(cereal::JSONInputArchive& ar)
    {
        // Null-pointer tag carries the component type into the generic lambda (C++17).
        auto loadPool = [&](auto* tag, const char* key)
        {
            using T = std::remove_pointer_t<decltype(tag)>;
            try
            {
                std::vector<std::pair<ecs::Entity, T>> data;
                ar(cereal::make_nvp(key, data));
                for (auto& [e, c] : data) { registry_.Track(e); registry_.Add<T>(e, std::move(c)); }
            }
            catch (...) { /* component pool missing in older scene files */ }
        };

        loadPool(static_cast<RigidBodyComponent*>(nullptr),    "rigid_bodies");
        loadPool(static_cast<RenderableNode*>(nullptr),         "renderables");
        loadPool(static_cast<SoftBodyComponent*>(nullptr),      "soft_bodies");
        loadPool(static_cast<AudioSourceComponent*>(nullptr),   "audio_sources");
        loadPool(static_cast<AudioListenerComponent*>(nullptr), "audio_listeners");
        loadPool(static_cast<NetworkComponent*>(nullptr),       "network_components");
    }

} // namespace ettycc
