#pragma once
#include <Game/GameModule.hpp>
#include <ECS/ISystem.hpp>
#include <ECS/Entity.hpp>

namespace ettycc { struct EditorPropertyVisitor; }

namespace sample
{

// ── Custom component: pure data, lives in the ECS registry ───────────────────
// Add this to any entity via the editor's "Add Component" button.
// The SpinnerSystem will rotate any entity that carries it.
struct SpinnerComponent
{
    static constexpr const char* componentType = "Spinner";

    float speed       = 90.0f;  // degrees per second
    float accumulated = 0.0f;   // current angle (degrees)

    void InspectProperties(ettycc::EditorPropertyVisitor& v);
};

// ── Custom system: iterates SpinnerComponent pool each frame ─────────────────
class SpinnerSystem : public ettycc::ISystem
{
public:
    ettycc::ProcessingChannel Channel() const override;
    void OnStart(ettycc::Scene& scene, ettycc::Engine& engine) override;
    void OnEntityAdded(ettycc::Scene& scene, ettycc::Engine& engine,
                       ettycc::ecs::Entity entity) override;
    void OnUpdate(ettycc::Scene& scene, float dt) override;
};

// ── Module entry point ───────────────────────────────────────────────────────
class SampleGameModule : public ettycc::GameModule
{
public:
    SampleGameModule();

    bool OnStart(const ettycc::Engine* engine) override;
    void OnUpdate(const float deltaTime) override;
    void OnDestroy() override;

private:
    const ettycc::Engine* engine_ = nullptr;

    void RegisterSpinnerComponent();
    void UnregisterSpinnerComponent();
};

} // namespace sample
