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
#include <Dependencies/Resources.hpp>
#include <Game/GameModule.hpp>

#include <memory>
#include <vector>
#include <imgui.h>
#include <spdlog/spdlog.h>

#define DEFAULT_ASSETS_FOLDER "../../../assets/scenes/"
#define ENGINE_RESOURCES_PATH "80CC.json"

namespace ettycc
{

    class Engine : public EnginePipeline
    {
    public:
        // DEPENDENCIES (ONLY INTERNAL SYSTEMS....)
        std::shared_ptr<App>   appInstance_;
        std::shared_ptr<Resources> engineResources_;
        std::vector<std::shared_ptr<GameModule>> gameModules_;

        Rendering              renderEngine_;
        PlayerInput            inputSystem_;
        std::shared_ptr<Scene> mainScene_; // THIS SHOULD BE A MULTI SCENE ARRAY...
        
    public:
        Engine(std::shared_ptr<App> appInstance);
        ~Engine();

        // Engine front-end API        
        void LoadLastScene();
        void LoadScene(const std::string& sceneName);
        void StoreScene(const std::string& sceneName);
        void RegisterModules(const std::vector<std::shared_ptr<GameModule>>& modules);

        // Engine pipeline API
        void Init() override; 
        void Update() override;
        void PrepareFrame() override;
        void PresentFrame() override;
        void ProcessInput(PlayerInputType type, uint64_t *data) override;
    };   
} // namespace ettycc

#endif