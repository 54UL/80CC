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
#include <Game/ModuleLoader.hpp>
#include <Physics/IPhysicsWorld.hpp>
#include <Physics/PhysicsRegistry.hpp>
#include <Networking/NetworkManager.hpp>
#include <Audio/AudioManager.hpp>
#include <Threading/ThreadRegistry.hpp>

#include <Scene/Assets/AssetRegistry.hpp>
#include <UI/ComponentRegistry.hpp>
#include <UI/AssetDescriptor.hpp>
#include <UI/EditorExtensionRegistry.hpp>
#include <Graphics/Rendering/RenderLayerConfig.hpp>


#include <memory>
#include <vector>
#include <array>
#include <future>
#include <imgui.h>
#include <spdlog/spdlog.h>


namespace ettycc
{
    // -- Per-frame thread timing data ------------------------------------------
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

    enum class SampleScene { Default, SoftBodies, Network };

    // -- Per-scene configurable parameters ------------------------------------
    // Each built-in scene declares a config struct with sensible defaults.
    // The editor shows a popup to let the user tweak these before launching.

    struct SoftBodySceneConfig
    {
        int   bodyCount        = 10;
        float gravity          = -18.81f;
        float softBodyRadius   = 0.7f;
        float softBodyMass     = 10.0f;
    };

    struct NetworkSceneConfig
    {
        int   boxPairs         = 5;
        float gravity          = -9.81f;
    };

    // Union-style holder for any built-in scene config
    struct BuiltInSceneConfig
    {
        SampleScene          scene = SampleScene::Default;
        SoftBodySceneConfig  softBodies;
        NetworkSceneConfig   network;
    };

    class Engine final : public EnginePipeline
    {
    public:
        std::shared_ptr<App>                        appInstance_;
        std::shared_ptr<Globals>                    globals_;
        std::vector<std::shared_ptr<GameModule>>    gameModules_;
        std::shared_ptr<Camera>                     editorCamera_;
        std::shared_ptr<Grid>                       editorGrid_;

        std::shared_ptr<Scene> mainScene_;
        uint32_t               sceneGeneration_ = 0; // bumped on every LoadScene; used by editor to detect stale refs
        Rendering              renderEngine_;
        RenderLayerConfig      renderLayerConfig_;
        std::unique_ptr<physics::IPhysicsWorld> physicsWorld_;
        physics::PhysicsRegistry               physicsRegistry_;
        bool                   simulationPaused_ = false;
        AudioManager           audioManager_;
        PlayerInput            inputSystem_;
        NetworkManager         networkManager_;
        ThreadDebugInfo        threadDebugInfo_;
        ThreadRegistry         threadRegistry_;
        ModuleLoader           moduleLoader_;
        ComponentRegistry          componentRegistry_;
        AssetDescriptorRegistry    assetDescriptors_;
        EditorExtensionRegistry    editorExtensions_;
        std::shared_ptr<AssetRegistry> assetRegistry_;

    private:
        bool isEditorMode_      = false;
        bool isHeadless_        = false;
        bool isPlaying_         = false;
        bool audioInitialized_  = false;

        std::future<float> physicsFuture_;   // This overlaps Bullet simulation with rendering.

        // -- Init sub-phases (called by Init) --
        void InitAssetRegistry();
        void InitPhysics();
        void InitPresentation();   // GPU upload + audio -- only when !headless
        void InitScene();          // scene load + modules -- only when !headless

        // -- Scene switch helpers --
        void PrepareSceneSwitch();   // clears renderables before loading a new scene
        void FinalizeSceneLoad();    // camera setup + preload after scene is ready

    public:
        explicit Engine(std::shared_ptr<App> appInstance);
        ~Engine() override;

        void Init() override;
        void Update() override;
        void PrepareFrame() override;
        void PresentFrame() override;
        PlayerInput * GetInputSystem() override;

        // Engine config
        void LoadGlobals(const std::string &fileName);
        void StoreGlobals(const std::string &fileName) const;
        void ConfigResource();

        void BeginPlay();
        void EndPlay();

        void DrainPhysicsFuture() { if (physicsFuture_.valid()) physicsFuture_.get(); }
        void ReleaseAllPhysicsBodies();

        void SetEditorMode(bool isEditor) { isEditorMode_ = isEditor; if (isEditor) simulationPaused_ = true; }
        bool IsEditorMode()         const { return isEditorMode_; }

        void SetHeadlessMode(bool headless) { isHeadless_ = headless; }
        bool IsHeadless()           const { return isHeadless_; }

        // Scene operations (assets, scene handling and basic initialization)
        void PreloadSceneAssets();
        void LoadBuiltInScene(SampleScene scene = SampleScene::Default);
        void LoadBuiltInScene(const BuiltInSceneConfig& cfg);
        void LoadLastScene();
        void LoadScene(const std::string& sceneName, const bool defaultPath = true);
        void StoreScene(const std::string& sceneName, const bool defaultPath = true) const;
        void InitEditorCamera();
        void EnsureGameCamera();

        void RegisterModules(const std::vector<std::shared_ptr<GameModule>>& modules);
        bool LoadDynamicModule(const std::string& dllPath);
        void RestartDllModules();

        // Network stuff
        void InitNetwork(bool isHost, uint16_t port = 7777,
                         const std::string& serverAddress = "127.0.0.1");
        void StartNetworkWorker();
        void StopNetworkWorker();

        // Sample built-in scenes
        void SoftBodyScene(const SoftBodySceneConfig& cfg = {});
        void NetworkScene(const NetworkSceneConfig& cfg = {});

        void createSprite(const std::shared_ptr<SceneNode>& rootSceneNode, std::string spriteTexturePath, const glm::vec3& pos);
        void createPhysicsBox(std::shared_ptr<SceneNode> rootSceneNode, const std::string& texPath,
                              float mass, glm::vec3 halfExtents, glm::vec3 pos) const;
        void createSoftBody(std::shared_ptr<SceneNode> rootSceneNode, std::string texPath,
                            float radius, glm::vec3 pos, float mass = 1.0f) const;
    };
} // namespace ettycc

#endif
