#pragma once
#include <Game/GameModule.hpp>
#include "GravityConfig.hpp"

namespace ettycc { struct EditorPropertyVisitor; }

namespace gravity
{

class GravityGameModule : public ettycc::GameModule
{
public:
    GravityGameModule();

    bool OnStart(const ettycc::Engine* engine) override;
    void OnUpdate(const float deltaTime) override;
    void OnDestroy() override;

    // Build and launch the gravity attractor scene
    void SetupGravityScene(const GravitySceneConfig& cfg = {});

private:
    const ettycc::Engine* engine_ = nullptr;

    // Editor state for the config popup
    bool              popupOpen_ = false;
    GravitySceneConfig pendingConfig_;

    void RegisterComponents();
    void UnregisterComponents();
    void RegisterEditorExtensions();
    void UnregisterEditorExtensions();
};

} // namespace gravity
