#ifndef ENGINE_80CC_HPP
#define ENGINE_80CC_HPP

#include <App/App.hpp>
#include <Paths.hpp>
#include <App/EnginePipeline.hpp>
#include <Graphics/Rendering.hpp>
#include <Graphics/Rendering/Entities/Camera.hpp>
#include <Graphics/Rendering/Entities/Sprite.hpp>
#include <Input/Controls/GhostCamera.hpp>
#include <Input/PlayerInput.hpp>
#include <Scene/Scene.hpp>
#include <Dependencies/Resources.hpp>
#include <Game/GameModule.hpp>
#include <Physics/PhysicsWorld.hpp>
#include <Networking/NetworkManager.hpp>

#include <memory>
#include <vector>
#include <imgui.h>
#include <spdlog/spdlog.h>


namespace ettycc
{

    class Engine : public EnginePipeline
    {
    public:
        // DEPENDENCIES (ONLY INTERNAL SYSTEMS....)
        std::shared_ptr<App>                        appInstance_;
        std::shared_ptr<Resources>                  engineResources_;
        std::vector<std::shared_ptr<GameModule>>    gameModules_;
        std::shared_ptr<Camera>                     editorCamera_;

        Rendering              renderEngine_;
        PhysicsWorld           physicsWorld_;
        PlayerInput            inputSystem_;
        NetworkManager         networkManager_;
        std::shared_ptr<Scene> mainScene_; // THIS SHOULD BE A MULTI SCENE ARRAY...

    private:
        bool isEditorMode_ = false;
        
    public:
        Engine(std::shared_ptr<App> appInstance);
        ~Engine() override;

        // Call before Init(). True = running inside DevEditor, False = standalone game executable.
        // This is the single source of truth for editor/game mode at runtime.
        void SetEditorMode(bool isEditor) { isEditorMode_ = isEditor; }
        bool IsEditorMode()         const { return isEditorMode_; }

        static void createSprite(std::shared_ptr<SceneNode> rootSceneNode, std::string spriteTexturePath, const glm::vec3 pos);
        void createPhysicsBox(std::shared_ptr<SceneNode> rootSceneNode, const std::string& texPath,
                              float mass, glm::vec3 halfExtents, glm::vec3 pos);
        void createSoftBody(std::shared_ptr<SceneNode> rootSceneNode, std::string texPath,
                            float radius, glm::vec3 pos, float mass = 1.0f);


        // Engine front-end API
        void InitEditorCamera();
        // Scans scene renderables for the first Camera; creates a default free-fly
        // camera if none exists. Ensures the camera is first in the render list.
        // Called automatically after every scene load in standalone mode.
        void EnsureGameCamera();
        void LoadDefaultScene();
        void LoadPhysicsScene();
        // Engine front-end API
        void LoadLastScene();
        void LoadScene(const std::string& sceneName, const bool defaultPath = true);
        void StoreScene(const std::string& sceneName, const bool defaultPath = true) const;


        void RegisterModules(const std::vector<std::shared_ptr<GameModule>>& modules);
        void BuildExecutable(const std::string& outputPath);
        void ConfigResource();

        // Networking helpers — call before Init() if you want a networked scene
        void InitNetwork(bool isHost, uint16_t port = 7777,
                         const std::string& serverAddress = "127.0.0.1");
        void LoadNetworkScene();
        
        // Engine pipeline API (backend)
        void Init() override;
        void Update() override;
        void PrepareFrame() override;
        void PresentFrame() override;
        virtual PlayerInput * GetInputSystem() override;
    };
} // namespace ettycc

#endif