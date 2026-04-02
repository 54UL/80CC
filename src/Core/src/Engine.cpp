#include <Engine.hpp>
#include <Dependency.hpp>
#include <future>
#include <chrono>

#include <Scene/Components/RenderableNode.hpp>
#include <Scene/Components/RigidBodyComponent.hpp>
#include <Scene/Components/SoftBodyComponent.hpp>
#include <Networking/NetworkComponent.hpp>
#include <Scene/Systems/PhysicsSystem.hpp>
#include <Scene/Systems/RenderSystem.hpp>
#include <Scene/Systems/AudioSystem.hpp>
#include <Scene/Systems/NetworkSystem.hpp>
#include <Dependencies/Globals.hpp>
#include <GlobalKeys.hpp>
#include <Graphics/Rendering/Entities/Grid.hpp>
#include <filesystem>
#ifdef _WIN32
#  include <windows.h>
#else
#  include <unistd.h>
#endif

namespace ettycc
{
    Engine::Engine(std::shared_ptr<App> appInstance) : appInstance_(appInstance)
    {
    }

    Engine::~Engine()
    {
        // Reset the scene BEFORE shutting down AudioManager.
        // AudioSourceComponent::~AudioSourceComponent calls DestroySource(), which
        // calls alDeleteSources() — those calls require a live AL context.
        // If Shutdown() ran first the context would already be null and the
        // OpenAL driver would abort().
        mainScene_.reset();

        audioManager_.Shutdown();

        // TODO: Check if this is appropiate to handle here...
        if (globals_)
        {
            spdlog::warn("Engine config auto-saved");
            StoreGlobals(paths::RESOURCES_DEFAULT);
        }
        else
        {
            spdlog::error("can't auto save engine configuration!!!");
        }
    }

    // ── Scene system setup ────────────────────────────────────────────────────
    // Registers the default ECS systems and calls Init so that any already-loaded
    // components are initialized. Call once per scene before adding nodes.
    static void SetupSceneSystems(Scene& scene, Engine& engine)
    {
        scene.systems_.clear();
        scene.RegisterSystem(std::make_unique<RenderSystem>());
        scene.RegisterSystem(std::make_unique<PhysicsSystem>());
        scene.RegisterSystem(std::make_unique<AudioSystem>());
        scene.RegisterSystem(std::make_unique<NetworkSystem>());
        scene.Init(engine);
    }

    // ── Factory helpers ───────────────────────────────────────────────────────
    void Engine::createSprite(const std::shared_ptr<SceneNode> &rootSceneNode,
                              std::string spriteTexturePath,
                              const glm::vec3& pos)
    {
        const auto sprite = std::make_shared<Sprite>(spriteTexturePath);
        sprite->underylingTransform.setGlobalPosition(pos);

        static int index = 0;
        auto node = std::make_shared<SceneNode>(
            std::string("sprite primitive #") + std::to_string(index++));

        mainScene_->registry_.Add<RenderableNode>(node->GetId(), RenderableNode{sprite});
        rootSceneNode->AddChild(node);
    }

    void Engine::createPhysicsBox(std::shared_ptr<SceneNode> rootSceneNode,
                                  const std::string& texPath,
                                  float mass,
                                  glm::vec3 halfExtents,
                                  glm::vec3 pos) const
    {
        auto sprite = std::make_shared<Sprite>(texPath);
        sprite->underylingTransform.setGlobalPosition(pos);
        sprite->underylingTransform.setGlobalScale(halfExtents);

        static int index = 0;
        auto node = std::make_shared<SceneNode>("physics-box-" + std::to_string(index++));

        // Add components BEFORE AddChild so OnEntityAdded sees both at once.
        mainScene_->registry_.Add<RenderableNode>(node->GetId(), RenderableNode{sprite});
        mainScene_->registry_.Add<RigidBodyComponent>(
            node->GetId(), RigidBodyComponent{mass, halfExtents, pos});
        rootSceneNode->AddChild(node);
    }

    void Engine::createSoftBody(std::shared_ptr<SceneNode> rootSceneNode,
                                std::string texPath,
                                float radius, glm::vec3 pos, float mass) const {
        static int softBodyIndex = 0;
        auto node = std::make_shared<SceneNode>("soft-body-" + std::to_string(softBodyIndex++));

        mainScene_->registry_.Add<SoftBodyComponent>(
            node->GetId(), SoftBodyComponent{radius, pos, mass, std::move(texPath)});
        rootSceneNode->AddChild(node);
    }

    void Engine::LoadDefaultScene()
    {
        spdlog::warn("[Engine] loading built-in fallback scene...");

        renderEngine_.ClearRenderables();
        mainScene_ = std::make_shared<Scene>("default-scene");
        SetupSceneSystems(*mainScene_, *this);

        const std::string tex = globals_->Get(gk::prefix::SPRITES, gk::key::SPRITE_NOT_FOUND);
        auto root = mainScene_->root_node_;

        // ── Static boundaries ─────────────────────────────────────────────────
        createPhysicsBox(root, tex, 0.0f, glm::vec3(9.0f, 0.3f, 0.5f), glm::vec3( 0.0f, -5.0f, 0.0f)); // ground
        createPhysicsBox(root, tex, 0.0f, glm::vec3(0.3f, 5.5f, 0.5f), glm::vec3(-9.3f,  0.0f, 0.0f)); // left wall
        createPhysicsBox(root, tex, 0.0f, glm::vec3(0.3f, 5.5f, 0.5f), glm::vec3( 9.3f,  0.0f, 0.0f)); // right wall

        // ── Two staggered columns of dynamic rigid boxes ──────────────────────
        for (int i = 0; i < 4; ++i)
        {
            createPhysicsBox(root, tex, 1.0f, glm::vec3(0.5f, 0.5f, 0.5f),
                             glm::vec3(-1.1f, -3.8f + i * 1.15f, 0.0f));
            createPhysicsBox(root, tex, 1.0f, glm::vec3(0.5f, 0.5f, 0.5f),
                             glm::vec3( 1.1f, -3.8f + i * 1.15f + 0.55f, 0.0f));
        }

        // ── Soft body discs dropped from above ────────────────────────────────
        createSoftBody(root, tex, 0.7f, glm::vec3(-2.5f, 3.5f, 0.0f), 1.0f);
        createSoftBody(root, tex, 0.5f, glm::vec3( 0.0f, 4.5f, 0.0f), 1.0f);
        createSoftBody(root, tex, 0.9f, glm::vec3( 2.5f, 3.5f, 0.0f), 1.5f);

        if (!isEditorMode_)
            EnsureGameCamera();
    }

    void Engine::InitEditorCamera()
    {
        editorCamera_ = std::make_shared<Camera>(1200, 800);
        editorCamera_->AttachEditorControl(&inputSystem_);
        editorCamera_->underylingTransform.setGlobalPosition({0.0f, 0.0f, -1.0f});
        editorCamera_->Init(GetDependency(Engine));
        renderEngine_.SetViewPortFrameBuffer(editorCamera_->offScreenFrameBuffer);
        renderEngine_.AddRenderable(editorCamera_);

        auto grid = std::make_shared<Grid>();
        grid->Init(GetDependency(Engine));
        renderEngine_.AddRenderable(grid);

        spdlog::info("Editor camera initialized [1200x800]");
    }

    void Engine::EnsureGameCamera()
    {
        // Find the first Camera already in the render list (added by a scene node).
        std::shared_ptr<Camera> cam;
        for (const auto& r : renderEngine_.GetRenderables())
        {
            if (auto c = std::dynamic_pointer_cast<Camera>(r)) { cam = c; break; }
        }

        if (!cam)
        {
            // No camera in scene — spawn a default free-fly camera.
            auto sz = appInstance_->GetMainWindowSize();
            cam = std::make_shared<Camera>(sz.x, sz.y);
            cam->underylingTransform.setGlobalPosition({0.0f, 0.0f, -1.0f});
            cam->Init(GetDependency(Engine));
            editorCamera_ = cam;
            spdlog::info("[Engine] No scene camera — spawned default [{}x{}]", sz.x, sz.y);
        }

        // Attach pan/zoom/scroll controls so the player can navigate in standalone.
        cam->AttachEditorControl(&inputSystem_);

        // Camera must be first so it populates ctx matrices before sprites draw.
        renderEngine_.EnsureFirst(cam);
    }

    void Engine::LoadLastScene()
    {
        auto lastLoadedScene = globals_->Get(gk::prefix::STATE, gk::key::STATE_LAST_SCENE);
        spdlog::warn("Loading last scene: {}", lastLoadedScene);

        LoadScene(lastLoadedScene);

        // TODO: MOVE THIS CODE BELOW BECAUSE IS GENERIC ENGINE INITIALIZATION
        auto mainWindowSize = appInstance_->GetMainWindowSize();
        renderEngine_.SetScreenSize(mainWindowSize.x, mainWindowSize.y);
    }

    //TODO: INSERT HERE ASSET MANAGEMENT AND NODE BUILDER (TO BUILD TEMPLATES FROM THE ASSET MANAGEMENT)
    void Engine::LoadScene(const std::string &sceneName, const bool defaultPath)
    {
        auto path = sceneName;

        if (defaultPath)
        {
            path= globals_->GetWorkingFolder() + paths::SCENE_DEFAULT + sceneName;
        }

        std::ifstream ifs(path);
        bool loadedFromFile = false;

        if (!ifs.is_open())
        {
            spdlog::error("Can't open file scene [{}]", path);
            LoadDefaultScene();
        }
        else
        {
            renderEngine_.ClearRenderables(); // TODO: MAKE SURE WE CLEAN UP EVERYTHING....

            cereal::JSONInputArchive archive2(ifs);
            try
            {
                {
                    mainScene_ = std::make_shared<Scene>("");
                    archive2(*mainScene_);
                    SetupSceneSystems(*mainScene_, *this);
                    loadedFromFile = true;
                    spdlog::info("Scene loaded successfully [{}]", mainScene_->sceneName_);
                }
            } catch (const std::exception& e) {
                spdlog::error("Can't deserialize scene reason: [{}]", e.what());
                LoadDefaultScene();
            }
        }

        if (mainScene_)
        {
            if (loadedFromFile)
            {
                // Store the actual file path so LoadLastScene can reopen it on next boot.
                globals_->Set(gk::prefix::STATE, gk::key::STATE_LAST_SCENE, path);
            }
            spdlog::info("Active scene [{}]", mainScene_->sceneName_);
        }
        else
        {
            spdlog::error("Errors occurred when processing scene...");
        }

        if (!isEditorMode_)
            EnsureGameCamera();
    }

    void Engine::StoreScene(const std::string &sceneName, const bool defaultPath) const
    {
        // Use std::string (not string_view) — the concatenated path is a temporary.
        std::string path = defaultPath
            ? globals_->GetWorkingFolder() + paths::SCENE_DEFAULT + sceneName
            : sceneName;

        std::ofstream ofs(path);

        if (!ofs.is_open()) {
            spdlog::error("Cannot store scene {}", path);
            return;
        }

        spdlog::info("Storing scene {}", path);

        {
            cereal::JSONOutputArchive archive(ofs);
            archive(*mainScene_);
        }

        // Store the actual file path so LoadLastScene can reopen it directly.
        globals_->Set(gk::prefix::STATE, gk::key::STATE_LAST_SCENE, path);
        spdlog::info("Scene stored successfully {}", path);
    }

    void Engine::RegisterModules(const std::vector<std::shared_ptr<GameModule>> &modules)
    {
        gameModules_ = modules;
        for (const auto &module : gameModules_)
        {
            spdlog::warn("Game module registered {}", module->name_);
        }
    }

    // ── Persistence ────────────────────────────────────────────────────
    void Engine::LoadGlobals(const std::string& fileName) {
        const auto filePath = globals_->GetWorkingFolder() + fileName;

        std::ifstream file(filePath);
        if (!file.is_open())
        {
            spdlog::error("Globals file not found: '{}'", filePath);
            return;
        }

        // Strip UTF-8 BOM (EF BB BF) written by some Windows editors —
        // RapidJSON does not skip it and will fail to parse the document.
        {
            unsigned char bom[3] = {};
            file.read(reinterpret_cast<char*>(bom), 3);
            if (!(bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF))
                file.seekg(0); // not a BOM — rewind to start
        }

        try
        {
            cereal::JSONInputArchive ar(file);
            ar(*globals_);
        }
        catch (const std::exception& e)
        {
            spdlog::error("Failed to parse globals '{}': {}", filePath, e.what());
        }
    }

    void Engine::StoreGlobals(const std::string& fileName) const
    {
        const auto filePath = globals_->GetWorkingFolder() + fileName;

        std::ofstream file(filePath);
        if (!file.is_open())
        {
            spdlog::warn("Cannot write globals file: '{}'", filePath);
            return;
        }

        cereal::JSONOutputArchive ar(file);
        ar(*globals_);
    }

    void Engine::ConfigResource()
    {
        globals_ = GetDependency(Globals);
        globals_->AutoSetWorkingFolder();

        // Seed structural defaults first — engine works even if the file is missing/corrupt.
        globals_->SetupDefaults();

        // Load 80CC.json; successful load overwrites the defaults above.
        LoadGlobals(paths::RESOURCES_DEFAULT);

        // Always write back so the file is in clean cereal format for next run
        // (fixes corrupt/BOM files automatically).
        StoreGlobals(paths::RESOURCES_DEFAULT);

        // Register runtime engine paths so any system can read them via Globals.
        globals_->Set(gk::prefix::ENGINE, gk::key::ENGINE_WORKING_DIR,
                              globals_->GetWorkingFolder());
//TODO: REFACTOR
#ifdef _WIN32
        char exeBuf[512] = {};
        if (GetModuleFileNameA(nullptr, exeBuf, sizeof(exeBuf)))
        {
            const std::string exeDir =
                std::filesystem::path(exeBuf).parent_path().string();
            globals_->Set(gk::prefix::ENGINE, gk::key::ENGINE_EXE_DIR, exeDir);
        }
#else
        {
            char exeBuf[512] = {};
            ssize_t len = ::readlink("/proc/self/exe", exeBuf, sizeof(exeBuf) - 1);
            if (len > 0)
            {
                exeBuf[len] = '\0';
                const std::string exeDir =
                    std::filesystem::path(exeBuf).parent_path().string();
                globals_->Set(gk::prefix::ENGINE, gk::key::ENGINE_EXE_DIR, exeDir);
            }
        }
#endif
    }

    void Engine::Init()
    {
        ConfigResource();
        physicsWorld_.Init();
        audioManager_.Init();

        if (isEditorMode_)
            audioManager_.PlayStartupChime();

        // isEditorMode_ is set by main.cpp (or any host) before Init() is called.
        // Editor  -> always restore the last scene; game modules are not run at start.
        // Standalone -> run game modules (they own scene loading); fall back to last scene.
        if (!isEditorMode_ && !gameModules_.empty())
        {
            for (const auto& module : gameModules_)
            {
                if (!module)
                {
                    spdlog::error("Null game module in registration list, skipping");
                    continue;
                }
                module->OnStart(this);
                spdlog::info("Game module initialized [{}]", module->name_);
            }
        }
        else
        {
            if (!isEditorMode_ && gameModules_.empty())
                spdlog::warn("[Engine] No game modules registered — loading last scene");

            LoadLastScene();
        }

        if (mainScene_)
            spdlog::info("[Engine] Active scene [{}]", mainScene_->sceneName_);
        else
            spdlog::error("[Engine] No scene available after Init");
    }

    void Engine::Update()
    {
        using Clock = std::chrono::high_resolution_clock;
        using Ms    = std::chrono::duration<float, std::milli>;

        const float dt        = appInstance_->GetDeltaTime();
        const auto  frameStart = Clock::now();

        // ── Phase 1: serial systems that WRITE to scene state ─────────────────
        auto t0 = Clock::now();
        physicsWorld_.Step(dt);
        networkManager_.Poll();
        threadDebugInfo_.physics.durationMs = Ms(Clock::now() - t0).count();
        threadDebugInfo_.physics.async      = false;

        auto t1 = Clock::now();
        mainScene_->Process(dt, ProcessingChannel::MAIN);
        threadDebugInfo_.main.durationMs = Ms(Clock::now() - t1).count();
        threadDebugInfo_.main.async      = false;

        for (const auto& module : gameModules_)
            module->OnUpdate(dt);

        threadDebugInfo_.updatePhaseMs = Ms(Clock::now() - frameStart).count();
    }

    void Engine::PrepareFrame()
    {
    }

    void Engine::PresentFrame()
    {
        using Clock = std::chrono::high_resolution_clock;
        using Ms    = std::chrono::duration<float, std::milli>;

        const float dt         = appInstance_->GetDeltaTime();
        const auto  frameStart = Clock::now();

        auto scene = mainScene_;

        // Audio timing is written to threadDebugInfo_ (a member, not a stack var)
        // so there is no lifetime hazard if an exception unwinds before get().
        threadDebugInfo_.audio.durationMs = 0.f;

        auto audioFuture = std::async(std::launch::async, [this, scene, dt]()
        {
            auto t = Clock::now();
            audioManager_.Update();
            scene->Process(dt, ProcessingChannel::AUDIO);
            // safe: this is a member write; join in get() provides happens-before
            threadDebugInfo_.audio.durationMs = Ms(Clock::now() - t).count();
        });

        auto t2 = Clock::now();
        scene->Process(dt, ProcessingChannel::RENDERING);
        renderEngine_.Pass(dt);
        threadDebugInfo_.rendering.durationMs = Ms(Clock::now() - t2).count();
        threadDebugInfo_.rendering.async      = false;

        audioFuture.get(); // join — threadDebugInfo_.audio.durationMs written by worker

        threadDebugInfo_.audio.async = true;

        threadDebugInfo_.presentPhaseMs = Ms(Clock::now() - frameStart).count();
        threadDebugInfo_.PushSample();

        inputSystem_.ResetState();
    }

    PlayerInput * Engine::GetInputSystem()
    {
        return &inputSystem_;
    }

    // void Engine::ProcessInput(PlayerInputType type, uint64_t *data) // UNSAFE!!!! FIX THIS TRASH
    // {
    //     inputSystem_.ProcessInput(type, data);
    // }

    void Engine::InitNetwork(bool isHost, uint16_t port, const std::string& serverAddress)
    {
        if (isHost)
            networkManager_.InitHost(port);
        else
            networkManager_.InitClient(serverAddress, port);
    }

    // Builds the physics arena with a NetworkComponent on every dynamic body.
    //
    // IMPORTANT: all three components (RenderableNode, RigidBodyComponent,
    // NetworkComponent) must be added to the SceneNode BEFORE AddChild is
    // called.  AddChild -> AddNode fires OnStart once for every component
    // already present on the node.  Adding a component after AddChild skips
    // OnStart entirely, leaving syncTransform_ / rigidBody_ as nullptr and
    // the component never registered with NetworkManager.
    void Engine::LoadNetworkScene()
    {
        spdlog::info("[Engine] building network physics scene...");

        renderEngine_.ClearRenderables();
        mainScene_ = std::make_shared<Scene>("network-scene");
        SetupSceneSystems(*mainScene_, *this);

        const std::string tex = globals_->Get(gk::prefix::SPRITES, gk::key::SPRITE_NOT_FOUND);
        auto root = mainScene_->root_node_;

        // Static boundaries — no network sync needed
        createPhysicsBox(root, tex, 0.0f, glm::vec3(9.0f, 0.3f, 0.5f), glm::vec3( 0.0f, -5.0f, 0.0f));
        createPhysicsBox(root, tex, 0.0f, glm::vec3(0.3f, 5.5f, 0.5f), glm::vec3(-9.3f,  0.0f, 0.0f));
        createPhysicsBox(root, tex, 0.0f, glm::vec3(0.3f, 5.5f, 0.5f), glm::vec3( 9.3f,  0.0f, 0.0f));

        // Pre-allocate pool capacity so vector::push_back never reallocates.
        // Reallocation would move components to new addresses, invalidating
        // the raw pointers that NetworkManager::registry_ caches.
        constexpr int kNetBoxCount = 10;   // 5 iterations × 2 boxes
        constexpr int kTotalRB     = kNetBoxCount + 3;  // +3 static walls
        mainScene_->registry_.Pool<RenderableNode>().Reserve(kTotalRB);
        mainScene_->registry_.Pool<RigidBodyComponent>().Reserve(kTotalRB);
        mainScene_->registry_.Pool<NetworkComponent>().Reserve(kNetBoxCount);

        // Build a replicated box: ALL components are added before AddChild so
        // that OnEntityAdded sees all three components at once.
        uint32_t netId = 1;
        static int netBoxIdx = 0;
        auto makeNetBox = [&](glm::vec3 halfExt, glm::vec3 pos)
        {
            auto sprite = std::make_shared<Sprite>(tex);
            sprite->underylingTransform.setGlobalPosition(pos);
            sprite->underylingTransform.setGlobalScale(glm::vec3(halfExt.x, halfExt.y, 1.0f));

            auto node = std::make_shared<SceneNode>("net-box-" + std::to_string(netBoxIdx++));
            mainScene_->registry_.Add<RenderableNode>(node->GetId(), RenderableNode{sprite});
            mainScene_->registry_.Add<RigidBodyComponent>(node->GetId(),
                RigidBodyComponent{1.0f, halfExt, pos});
            mainScene_->registry_.Add<NetworkComponent>(node->GetId(),
                NetworkComponent{netId++});
            root->AddChild(node);
        };

        for (int i = 0; i < 5; ++i)
        {
            makeNetBox(glm::vec3(0.5f), glm::vec3(-1.1f, -3.8f + i * 1.15f,        0.0f));
            makeNetBox(glm::vec3(0.5f), glm::vec3( 1.1f, -3.8f + i * 1.15f + 0.55f, 0.0f));
        }

        // Do NOT call mainScene_->Init() here — AddNode already called OnStart
        // for every component.  Calling Init() again would double-init
        // RigidBodyComponent, leaking btRigidBody instances into the world.
        spdlog::info("[Engine] network scene ready — {} replicated bodies", netId - 1);
    }

    void Engine::BuildExecutable(const std::string &outputPath)
    {
        // Get (config,images,scenes,shaders,templates) folders inside of assets and copy them into outputPath
        // Compile all assets into a binary form (optional meanwhile)

        // Use an env var to locate the engine source code
        // Compile the assets/src with:
        //  cmake --build ./  --config Debug --target Game -j 4
    }
} // namespace ettycc
