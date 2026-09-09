#pragma once
#include <Scene/Api.hpp>

namespace ettycc { struct EditorPropertyVisitor; }

namespace gravity
{
    // Per-entity fusion state -- tracks cooldown to prevent cascade collisions.
    // Registered by GravityModule; queried by GravityDynamicsSystem.
    struct FusionDataComponent
    {
        static constexpr const char*             componentType = "FusionData";
        static constexpr ettycc::ProcessingChannel channel     = ettycc::ProcessingChannel::MAIN;

        float cooldown = 0.f;

        bool  CanFuse() const           { return cooldown <= 0.f; }
        void  SetCooldown(float seconds){ cooldown = seconds; }
        void  Tick(float dt)            { if (cooldown > 0.f) cooldown -= dt; }

        void InspectProperties(ettycc::EditorPropertyVisitor& v);
    };

} // namespace gravity
