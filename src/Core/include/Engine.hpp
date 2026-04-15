#ifndef ENGINE_80CC_HPP
#define ENGINE_80CC_HPP

#include <App/App.hpp>
#include <Paths.hpp>
#include <App/EnginePipeline.hpp>
#include <Graphics/Rendering.hpp>
#include <Graphics/Rendering/Entities/Camera.hpp>
#include <Graphics/Rendering/Entities/Sprite.hpp>
#include <Graphics/Rendering/Entities/Grid.hpp>
#include <Input/Controls/GhostCamera.hpp>
#include <Input/PlayerInput.hpp>
#include <Scene/Scene.hpp>
#include <Dependencies/Globals.hpp>
#include <Game/GameModule.hpp>
#include <Physics/PhysicsWorld.hpp>
#include <Networking/NetworkManager.hpp>
#include <Audio/AudioManager.hpp>
#include <Threading/ThreadRegistry.hpp>

#include <Scene/Assets/ResourceCache.hpp>

#include <memory>
#include <vector>
#include <array>
#include <future>
#include <imgui.h>
#include <spdlog/spdlog.h>


namespace ettycc
{
    // ── Per-frame thread timing data ──────────────────────────────────────────
    // Written by Engine each frame, read by DevEditor for the Threads debug tab.
    // All values are in milliseconds.
    struct ChannelSample
    {
        float durationMs = 0.f;
        bool  async      = false; // true when the channel ran on a worker thread
    };

    struct ThreadDebugInfo
    {
        static constexpr int kHistorySize = 96;

        ChannelSample physics;    // physicsWorld_.Step
        ChannelSample network;    // networkManager_ Poll + apply
        ChannelSample main;       // Scene::Process(MAIN)
        ChannelSample audio;      // audioManager_.Update + Scene::Process(AUDIO)
        ChannelSample rendering;  // Scene::Process(RENDERING) + renderEngine_.Pass
        float updatePhaseMs  = 0.f; // total Engine::Update duration
        float presentPhaseMs = 0.f; // total Engine::PresentFrame duration

        // Rolling histories for sparkline plots (ring-buffer, newest at tail)
        std::array<float, kHistorySize> mainHistory      {};
        std::array<float, kHistorySize> audioHistory     {};
        std::array<float, kHistorySize> renderingHistory {};
        std::array<float, kHistorySize> frameHistory     {};
        int historyOffset = 0; // oldest-entry index into ring buffer

        void PushSample()
        {
            mainHistory     [historyOffset] = main.durationMs;
            audioHistory    [historyOffset] = audio.durationMs;
            renderingHistory[historyOffset] = rendering.durationMs;
            frameHistory    [historyOffset] = updatePhaseMs + presentPhaseMs;
            historyOffset = (historyOffset + 1) % kHistorySize;
        }
    };

    class Engine final : public EnginePipeline
    {
    public:
        // DEPENDENCIES (ONLY INTERNAL SYSTEMS....)
        std::shared_ptr<App>                        appInstance_;
        std::shared_ptr<Globals>                    globals_;
        std::vector<std::shared_ptr<GameModule>>    gameModules_;
        std::shared_ptr<Camera>                     editorCamera_;
        std::shared_ptr<Grid>                       editorGrid_;

        Rendering              renderEngine_;
        PhysicsWorld           physicsWorld_;
        bool                   simulationPaused_ = false;
        AudioManager           audioManager_;
        std::shared_ptr<ResourceCache> resourceCache_;
        PlayerInput            inputSystem_;
        NetworkManager         networkManager_;
        std::shared_ptr<Scene> mainScene_;// THIS SHOULD BE A MULTI SCENE ARRAY...
        ThreadDebugInfo        threadDebugInfo_;
        ThreadRegistry         threadRegistry_;

    private:
        bool isEditorMode_ = false;
        bool isHeadless_   = false;

        // ── Async physics pipelining ─────────────────────────────────────────
        // Physics Step runs on a pool thread.  We kick it at the end of Update()
        // and wait for it at the START of the next frame's Update().
        // This overlaps Bullet simulation with rendering.
        std::future<float> physicsFuture_;

    public:
        explicit Engine(std::shared_ptr<App> appInstance);
        ~Engine() override;

        // Call before Init(). True = running inside DevEditor, False = standalone game executable.
        // This is the single source of truth for editor/game mode at runtime.
        void SetEditorMode(bool isEditor) { isEditorMode_ = isEditor; if (isEditor) simulationPaused_ = true; }
        bool IsEditorMode()         const { return isEditorMode_; }

        // Call before Init(). True = headless dedicated server (no rendering, no audio, no input).
        void SetHeadlessMode(bool headless) { isHeadless_ = headless; }
        bool IsHeadless()           const { return isHeadless_; }

        // TODO: Implement some pattern to create this from other place
        // ALL THIS STUFF BELOW IS ONLY FOR TESTING...
        void createSprite(const std::shared_ptr<SceneNode>& rootSceneNode, std::string spriteTexturePath, const glm::vec3& pos);
        void createPhysicsBox(std::shared_ptr<SceneNode> rootSceneNode, const std::string& texPath,
                              float mass, glm::vec3 halfExtents, glm::vec3 pos) const;
        void createSoftBody(std::shared_ptr<SceneNode> rootSceneNode, std::string texPath,
                            float radius, glm::vec3 pos, float mass = 1.0f) const;

        void BoxesScene();


        // Engine front-end API
        void InitEditorCamera();
        // Scans scene renderables for the first Camera; creates a default free-fly
        // camera if none exists. Ensures the camera is first in the render list.
        // Called automatically after every scene load in standalone mode.
        void EnsureGameCamera();

        void GravityScene();

        void LoadDefaultScene();
        void CreateEmptyScene(const std::string& name = "untitled");
        // Engine front-end API
        void LoadLastScene();
        void LoadScene(const std::string& sceneName, const bool defaultPath = true);
        void StoreScene(const std::string& sceneName, const bool defaultPath = true) const;


        void RegisterModules(const std::vector<std::shared_ptr<GameModule>>& modules);
        void LoadGlobals(const std::string &fileName);
        void StoreGlobals(const std::string &fileName) const;
        void ConfigResource();

        // Networking helpers
        void InitNetwork(bool isHost, uint16_t port = 7777,
                         const std::string& serverAddress = "127.0.0.1");
        void StartNetworkWorker();
        void LoadNetworkScene();

        // Async asset preloading — reads images/shaders on a worker thread,
        // then uploads GL objects on the main thread.  Call after scene
        // deserialization but before SetupSceneSystems().
        void PreloadSceneAssets();

        // Engine pipeline API (backend)
        void Init() override;
        void Update() override;
        void PrepareFrame() override;
        void PresentFrame() override;
        virtual PlayerInput * GetInputSystem() override;
    };
} // namespace ettycc

#endif
