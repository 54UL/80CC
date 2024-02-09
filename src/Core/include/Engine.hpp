#ifndef ENGINE_80CC_HPP
#define ENGINE_80CC_HPP

#include <App/App.hpp>
#include <App/EnginePipeline.hpp>
#include <Graphics/Rendering.hpp>
#include <Graphics/Rendering/Entities/Camera.hpp>
#include <Graphics/Rendering/Entities/Sprite.hpp>
#include <Input/Controls/GhostCamera.hpp>
#include <Input/PlayerInput.hpp>
#include <Scene/Scene.hpp>

#include <memory>
#include <imgui.h>
#include <spdlog/spdlog.h>

namespace ettycc
{
    class Engine : public EnginePipeline
    {
    public:
        // DRAFT DEPENDENCIES (ONLY INTERNAL SYSTEMS....)
        std::shared_ptr<App>   appInstance_;
        Rendering              renderEngine_;
        PlayerInput            inputSystem_;
        std::shared_ptr<Scene>  mainScene_; // THIS SHOULD BE A MULTI SCENE ARRAY...
        std::shared_ptr<GhostCamera> ghostCamera_; //THIS ONE SHOULD BE ON THE GAME MODULE LAYER... woops
    
    public:
        Engine(std::shared_ptr<App> appInstance);
        ~Engine();

        // TESTING...
        void RenderingEngineDemo();

        // Engine pipeline impl
        void Init() override; 
        void Update() override;
        void PrepareFrame() override;
        void PresentFrame() override;
        void ProcessInput(PlayerInputType type, uint64_t *data) override;
    };   
} // namespace ettycc

#endif