#include "SampleModule.hpp"

#include <Engine.hpp>
#include <Scene/Scene.hpp>
#include <Scene/SceneNode.hpp>
#include <UI/ComponentRegistry.hpp>
#include <UI/EditorPropertyVisitor.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>

// ── DLL exports (one per module DLL) ─────────────────────────────────────────
ETTYCC_MODULE(sample::SampleGameModule)
ETTYCC_MODULE_IMGUI()

namespace sample
{

// ─── SpinnerComponent ─────────────────────────────────────────────────────────

void SpinnerComponent::InspectProperties(ettycc::EditorPropertyVisitor& v)
{
    PROP(speed,       "Speed (deg/s)");
    PROP(accumulated, "Angle");
}

// ─── SpinnerSystem ────────────────────────────────────────────────────────────

ettycc::ProcessingChannel SpinnerSystem::Channel() const
{
    return ettycc::ProcessingChannel::MAIN;
}

void SpinnerSystem::OnStart(ettycc::Scene& /*scene*/, ettycc::Engine& /*engine*/)
{
    spdlog::info("[SpinnerSystem] OnStart");
}

void SpinnerSystem::OnEntityAdded(ettycc::Scene& /*scene*/, ettycc::Engine& /*engine*/,
                                  ettycc::ecs::Entity /*entity*/)
{
    // Intentionally empty — components are added explicitly by the user
    // through the editor's "Add Component" menu, not auto-attached.
}

void SpinnerSystem::OnUpdate(ettycc::Scene& scene, float dt)
{
    auto* pool = scene.registry_.TryGetPool<SpinnerComponent>();
    if (!pool) return;

    auto& comps    = pool->Components();
    auto& entities = pool->Entities();

    for (size_t i = 0; i < comps.size(); ++i)
    {
        auto& spinner = comps[i];
        spinner.accumulated += spinner.speed * dt;

        if (spinner.accumulated > 360.0f)
            spinner.accumulated -= 360.0f;

        auto* node = scene.GetNode(entities[i]);
        if (node)
        {
            auto euler = glm::degrees(glm::eulerAngles(node->transform_.getGlobalRotation()));
            euler.z = spinner.accumulated;
            node->transform_.set(euler);
        }
    }
}

// ─── SampleGameModule ─────────────────────────────────────────────────────────

SampleGameModule::SampleGameModule()
{
    name_ = "SampleModule";
}

bool SampleGameModule::OnStart(const ettycc::Engine* engine)
{
    engine_ = engine;
    spdlog::info("[SampleModule] OnStart — registering SpinnerSystem & SpinnerComponent");

    auto scene = engine->mainScene_;
    if (!scene)
    {
        spdlog::warn("[SampleModule] No active scene — nothing to do");
        return true;
    }

    // Register our system so it processes SpinnerComponents each frame.
    scene->RegisterSystem(std::make_unique<SpinnerSystem>());

    // Register SpinnerComponent in the editor so it appears in "Add Component"
    // menus and can be added/removed/inspected like any built-in component.
    RegisterSpinnerComponent();

    spdlog::info("[SampleModule] Ready — add Spinner via 'Add Component' in the editor");
    return true;
}

void SampleGameModule::OnUpdate(const float /*deltaTime*/)
{
    // Module-level per-frame logic goes here if needed.
    // The heavy lifting is done by SpinnerSystem::OnUpdate via the ECS.
}

void SampleGameModule::OnDestroy()
{
    spdlog::info("[SampleModule] OnDestroy — cleaning up");

    // Unregister from the editor first (before removing components)
    UnregisterSpinnerComponent();

    if (!engine_ || !engine_->mainScene_) return;

    auto& registry = engine_->mainScene_->registry_;

    // Remove all SpinnerComponents we may have.
    // Use TryGetPool to avoid creating a Holder from DLL code on a fresh scene.
    if (auto* pool = registry.TryGetPool<SpinnerComponent>())
    {
        auto entities = pool->Entities(); // copy — Remove() invalidates iterators
        for (auto e : entities)
            registry.Remove<SpinnerComponent>(e);
    }

    // Remove our SpinnerSystem from the scene
    auto& systems = engine_->mainScene_->systems_;
    systems.erase(
        std::remove_if(systems.begin(), systems.end(),
            [](const std::unique_ptr<ettycc::ISystem>& sys) {
                return dynamic_cast<SpinnerSystem*>(sys.get()) != nullptr;
            }),
        systems.end());

    spdlog::info("[SampleModule] Cleanup complete");
}

// ── Component registration helpers ───────────────────────────────────────────

void SampleGameModule::RegisterSpinnerComponent()
{
    auto& reg = const_cast<ettycc::Engine*>(engine_)->componentRegistry_;

    reg.Register({
        SpinnerComponent::componentType, "Spinner", "Module",
        ettycc::badge::kModule,
        // hasFn
        [](const std::shared_ptr<ettycc::SceneNode>& n) {
            return n->HasComponent<SpinnerComponent>();
        },
        // addFn
        [](const std::shared_ptr<ettycc::SceneNode>& n, ettycc::Engine& eng) {
            n->AddComponent<SpinnerComponent>(SpinnerComponent{});
            eng.mainScene_->NotifyEntityAdded(n->GetId(), eng);
        },
        // removeFn
        [](const std::shared_ptr<ettycc::SceneNode>& n, ettycc::Engine&) {
            n->RemoveComponent<SpinnerComponent>();
        },
        // inspectFn
        [](const std::shared_ptr<ettycc::SceneNode>& n, ettycc::EditorPropertyVisitor& v) {
            if (auto* c = n->GetComponent<SpinnerComponent>()) c->InspectProperties(v);
        }
    });
}

void SampleGameModule::UnregisterSpinnerComponent()
{
    const_cast<ettycc::Engine*>(engine_)->componentRegistry_.Unregister(SpinnerComponent::componentType);
}

} // namespace sample
