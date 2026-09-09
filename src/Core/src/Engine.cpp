#include <Engine.hpp>
#include <Physics/Bullet/BulletPhysicsWorld.hpp>
#include <Physics/Box2D/Box2DPhysicsWorld.hpp>
#include <Dependency.hpp>
#include <Benchmark/Benchmark.hpp>
#include <Scene/Assets/AssetRegistry.hpp>
#include <future>
#include <chrono>
#include <Math/Constants.hpp>
#include <cmath>
#include <algorithm>
#include <set>

#include <Scene/Components/RenderableNode.hpp>
#include <Scene/Components/RigidBodyComponent.hpp>
#include <Scene/Components/SoftBodyComponent.hpp>
#include <Scene/Components/CameraControllerComponent.hpp>
#include <Input/Controls/EditorCamera.hpp>
#include <Networking/NetworkComponent.hpp>
#include <Scene/Systems/PhysicsSystem.hpp>
#include <Scene/Systems/RenderSystem.hpp>
#include <Scene/Systems/AudioSystem.hpp>
#include <Scene/Systems/NetworkSystem.hpp>
#include <Dependencies/Globals.hpp>
#include <GlobalKeys.hpp>
#include <Graphics/Rendering/Entities/Grid.hpp>
#include <filesystem>

#include <Random.hpp>
#ifdef _WIN32
#  include <windows.h>
#else
#  include <unistd.h>
#endif

namespace ettycc {
    Engine::Engine(std::shared_ptr<App> appInstance) : appInstance_(appInstance) {
    }

    Engine::~Engine() {
        // Stop all worker threads and pool tasks FIRST.
        // The network worker and any in-flight physics tasks must finish
        // before we destroy the subsystems they reference.
        threadRegistry_.Shutdown();

        // Wait for any in-flight physics step.
        if (physicsFuture_.valid())
            physicsFuture_.get();

        // Reset the scene BEFORE shutting down AudioManager.
        // AudioSourceComponent::~AudioSourceComponent calls DestroySource(), which
        // calls alDeleteSources() -- those calls require a live AL context.
        // If Shutdown() ran first the context would already be null and the
        // OpenAL driver would abort().
        mainScene_.reset();

        if (audioInitialized_)
            audioManager_.Shutdown();

        if (globals_) {
            spdlog::warn("Engine config auto-saved");
            StoreGlobals(paths::RESOURCES_DEFAULT);
        } else {
            spdlog::error("can't auto save engine configuration!!!");
        }
    }

    // -- Scene system setup ------
    // Registers the default ECS systems and calls Init so that any already-loaded
    // components are initialized. Call once per scene before adding nodes.
    static void SetupSceneSystems(Scene &scene, Engine &engine) {
        scene.systems_.clear();
        if (!engine.IsHeadless())
        {
            scene.RegisterSystem(std::make_unique<RenderSystem>());
            scene.RegisterSystem(std::make_unique<AudioSystem>());
        }
        scene.RegisterSystem(std::make_unique<PhysicsSystem>());
        scene.RegisterSystem(std::make_unique<NetworkSystem>());

        scene.Init(engine);
    }

    // -- Factory helpers ---------
    void Engine::createSprite(const std::shared_ptr<SceneNode> &rootSceneNode,
                              std::string spriteTexturePath,
                              const glm::vec3 &pos) {
        const auto sprite = std::make_shared<Sprite>(spriteTexturePath);
        sprite->underylingTransform.setGlobalPosition(pos);

        static int index = 0;
        auto node = std::make_shared<SceneNode>(
            std::string("sprite primitive #") + std::to_string(index++));

        mainScene_->registry_.Add<RenderableNode>(node->GetId(), RenderableNode{sprite});
        rootSceneNode->AddChild(node);
    }

    void Engine::createPhysicsBox(std::shared_ptr<SceneNode> rootSceneNode,
                                  const std::string &texPath,
                                  float mass,
                                  glm::vec3 halfExtents,
                                  glm::vec3 pos) const {
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

    void Engine::SoftBodyScene(const SoftBodySceneConfig& cfg) {
        mainScene_->sceneName_ = "soft-body-scene";
        physicsWorld_->SetGravity({0.f, cfg.gravity, 0.f});

        const std::string tex = assetRegistry_->GetSpritePath("not_found_texture");
        auto root = mainScene_->root_node_;
        // -- Static boundaries ---
        createPhysicsBox(root, tex, 0.0f, glm::vec3(9.0f, 0.3f, 0.5f), glm::vec3(0.0f, -5.0f, 0.0f));
        createPhysicsBox(root, tex, 0.0f, glm::vec3(0.3f, 5.5f, 0.5f), glm::vec3(-9.3f, 0.0f, 0.0f));
        createPhysicsBox(root, tex, 0.0f, glm::vec3(0.3f, 5.5f, 0.5f), glm::vec3(9.3f, 0.0f, 0.0f));

        for (int i = 0; i < cfg.bodyCount; ++i) {
            createSoftBody(root, tex, cfg.softBodyRadius, glm::vec3(-2.5f, i * 1.5f, 0.0f), cfg.softBodyMass);
            createSoftBody(root, tex, cfg.softBodyRadius * 0.7f, glm::vec3(0.0f, i * 1.5f, 0.0f), cfg.softBodyMass);
        }
    }

    // Builds the physics arena with a NetworkComponent on every dynamic body.
    //
    // IMPORTANT: all three components (RenderableNode, RigidBodyComponent,
    // NetworkComponent) must be added to the SceneNode BEFORE AddChild is
    // called.  AddChild -> AddNode fires OnStart once for every component
    // already present on the node.  Adding a component after AddChild skips
    // OnStart entirely, leaving syncTransform_ / rigidBody_ as nullptr and
    // the component never registered with NetworkManager.
    void Engine::NetworkScene(const NetworkSceneConfig& cfg)
    {
        mainScene_->sceneName_ = "network-scene";
        physicsWorld_->SetGravity({0.f, cfg.gravity, 0.f});

        const std::string tex = assetRegistry_->GetSpritePath("not_found_texture");
        auto root = mainScene_->root_node_;

        // Static boundaries -- no network sync needed
        createPhysicsBox(root, tex, 0.0f, glm::vec3(9.0f, 0.3f, 0.5f), glm::vec3(0.0f, -5.0f, 0.0f));
        createPhysicsBox(root, tex, 0.0f, glm::vec3(0.3f, 5.5f, 0.5f), glm::vec3(-9.3f, 0.0f, 0.0f));
        createPhysicsBox(root, tex, 0.0f, glm::vec3(0.3f, 5.5f, 0.5f), glm::vec3(9.3f, 0.0f, 0.0f));

        // Pre-allocate pool capacity so vector::push_back never reallocates.
        // Reallocation would move components to new addresses, invalidating
        // the raw pointers that NetworkManager::registry_ caches.
        const int kNetBoxCount = cfg.boxPairs * 2;
        const int kTotalRB = kNetBoxCount + 3; // +3 static walls
        mainScene_->registry_.Pool<RenderableNode>().Reserve(kTotalRB);
        mainScene_->registry_.Pool<RigidBodyComponent>().Reserve(kTotalRB);
        mainScene_->registry_.Pool<NetworkComponent>().Reserve(kNetBoxCount);

        // Build a replicated box: ALL components are added before AddChild so
        // that OnEntityAdded sees all three components at once.
        uint32_t netId = 1;
        static int netBoxIdx = 0;
        auto makeNetBox = [&](glm::vec3 halfExt, glm::vec3 pos) {
            auto node = std::make_shared<SceneNode>("net-box-" + std::to_string(netBoxIdx++));

            // Renderable is always created -- RenderSystem (which is NOT registered
            // in headless mode) is the only thing that calls Sprite::Init() / GL code.
            auto sprite = std::make_shared<Sprite>(tex);
            sprite->underylingTransform.setGlobalPosition(pos);
            sprite->underylingTransform.setGlobalScale(glm::vec3(halfExt.x, halfExt.y, 1.0f));
            mainScene_->registry_.Add<RenderableNode>(node->GetId(), RenderableNode{sprite});

            mainScene_->registry_.Add<RigidBodyComponent>(node->GetId(),
                                                          RigidBodyComponent{1.0f, halfExt, pos});
            mainScene_->registry_.Add<NetworkComponent>(node->GetId(),
                                                        NetworkComponent{netId++});
            root->AddChild(node);
        };

        for (int i = 0; i < cfg.boxPairs; ++i) {
            makeNetBox(glm::vec3(0.5f), glm::vec3(-1.1f, -3.8f + i * 1.15f, 0.0f));
            makeNetBox(glm::vec3(0.5f), glm::vec3(1.1f, -3.8f + i * 1.15f + 0.55f, 0.0f));
        }

        // Do NOT call mainScene_->Init() here -- AddNode already called OnStart
        // for every component.  Calling Init() again would double-init
        // RigidBodyComponent, leaking btRigidBody instances into the world.
        spdlog::info("[Engine] network scene ready -- {} replicated bodies", netId - 1);
    }

    void Engine::LoadBuiltInScene(SampleScene scene)
    {
        ++sceneGeneration_;
        spdlog::warn("Loading built-in scene (fallback)");

        PrepareSceneSwitch();

        mainScene_ = std::make_shared<Scene>("empty-scene");
        SetupSceneSystems(*mainScene_, *this);

        // Create a default game camera before the sample scene adds nodes.
        if (!isHeadless_)
        {
            auto sz  = appInstance_->GetMainWindowSize();
            auto cam = std::make_shared<Camera>(sz.x, sz.y);
            cam->underylingTransform.setGlobalPosition({0.0f, 0.0f, -1.0f});

            auto cameraNode = std::make_shared<SceneNode>("game-camera");
            mainScene_->registry_.Add<RenderableNode>(cameraNode->GetId(), RenderableNode{cam});
            mainScene_->root_node_->AddChild(cameraNode);
        }

        switch (scene)
        {
            case SampleScene::Network:    NetworkScene();  break;
            case SampleScene::SoftBodies: SoftBodyScene(); break;
            default:
                spdlog::warn("Empty scene loaded");
                break;
        }

        FinalizeSceneLoad();

        if (isEditorMode_)
            InitEditorCamera();
    }

    void Engine::LoadBuiltInScene(const BuiltInSceneConfig& cfg)
    {
        ++sceneGeneration_;
        spdlog::warn("Loading built-in scene (configured)");

        PrepareSceneSwitch();

        mainScene_ = std::make_shared<Scene>("empty-scene");
        SetupSceneSystems(*mainScene_, *this);

        if (!isHeadless_)
        {
            auto sz  = appInstance_->GetMainWindowSize();
            auto cam = std::make_shared<Camera>(sz.x, sz.y);
            cam->underylingTransform.setGlobalPosition({0.0f, 0.0f, -1.0f});

            auto cameraNode = std::make_shared<SceneNode>("game-camera");
            mainScene_->registry_.Add<RenderableNode>(cameraNode->GetId(), RenderableNode{cam});
            mainScene_->root_node_->AddChild(cameraNode);
        }

        switch (cfg.scene)
        {
            case SampleScene::Network:    NetworkScene(cfg.network);      break;
            case SampleScene::SoftBodies: SoftBodyScene(cfg.softBodies);  break;
            default:
                spdlog::warn("Empty scene loaded");
                break;
        }

        FinalizeSceneLoad();

        if (isEditorMode_)
            InitEditorCamera();
    }

    void Engine::InitEditorCamera() {
        auto sz = appInstance_->GetMainWindowSize();
        editorCamera_ = std::make_shared<Camera>(sz.x, sz.y);
        editorCamera_->frustumCullingEnabled_ = false; // editor shows everything by default
        editorCamera_->AttachEditorControl(&inputSystem_);
        editorCamera_->underylingTransform.setGlobalPosition({0.0f, 0.0f, -1.0f});
        editorCamera_->Init(GetDependency(Engine));
        renderEngine_.SetViewPortFrameBuffer(editorCamera_->offScreenFrameBuffer);

        editorGrid_ = std::make_shared<Grid>();
        editorGrid_->Init(GetDependency(Engine));

        // Editor camera + grid live outside the renderables list so they
        // never interfere with scene/game cameras.
        renderEngine_.SetEditorOverlay(editorCamera_, editorGrid_);

        spdlog::info("Editor camera initialized [{}x{}]", sz.x, sz.y);
    }

    void Engine::EnsureGameCamera() {
        // camera if none exists. Ensures the camera is first in the render list.
        // Called automatically after every scene load in standalone mode.
        std::shared_ptr<Camera> cam;
        for (const auto &r: renderEngine_.GetRenderables()) {
            if (auto c = std::dynamic_pointer_cast<Camera>(r)) {
                cam = c;
                break;
            }
        }

        if (!cam) {
            // No camera in scene -- spawn a default free-fly camera as a proper
            // SceneNode so it is part of the scene hierarchy and gets serialized.
            auto sz = appInstance_->GetMainWindowSize();
            cam = std::make_shared<Camera>(sz.x, sz.y);
            cam->underylingTransform.setGlobalPosition({0.0f, 0.0f, -1.0f});

            if (mainScene_)
            {
                auto cameraNode = std::make_shared<SceneNode>("game-camera");
                mainScene_->registry_.Add<RenderableNode>(cameraNode->GetId(), RenderableNode{cam});
                mainScene_->root_node_->AddChild(cameraNode);
                // RenderSystem::OnEntityAdded will call InitRenderable and AddRenderable
            }
            else
            {
                // Fallback if no scene is loaded yet
                cam->Init(GetDependency(Engine));
                renderEngine_.AddRenderable(cam);
            }

            editorCamera_ = cam;
            spdlog::info("[Engine] No scene camera -- spawned default [{}x{}] as scene node", sz.x, sz.y);
        }

        cam->AttachEditorControl(&inputSystem_);
        cam->editorCameraControl_->enabled = true; // standalone game: always enabled

        // Bind the SceneNode's transform + CameraControllerComponent so the
        // control operates on the canonical transform (single source of truth).
        if (mainScene_)
        {
            auto* rnPool = mainScene_->registry_.TryGetPool<RenderableNode>();
            if (rnPool)
            {
                for (auto e : rnPool->Entities())
                {
                    auto* rn = rnPool->Get(e);
                    if (rn && rn->renderable_ == cam)
                    {
                        auto* node = mainScene_->GetNode(e);
                        if (node)
                            cam->editorCameraControl_->BindTransform(&node->transform_);

                        auto* ccPool = mainScene_->registry_.TryGetPool<CameraControllerComponent>();
                        if (ccPool)
                        {
                            auto* cc = ccPool->Get(e);
                            if (cc)
                                cam->editorCameraControl_->BindComponent(cc);
                        }
                        break;
                    }
                }
            }
        }

        // Camera must be first so it populates ctx matrices before sprites draw.
        renderEngine_.EnsureFirst(cam);
    }

    void Engine::LoadLastScene() {
        auto lastLoadedScene = globals_->Get(gk::prefix::STATE, gk::key::STATE_LAST_SCENE);
        spdlog::warn("Loading last scene: {}", lastLoadedScene);

        LoadScene(lastLoadedScene, false);
    }

    void Engine::LoadScene(const std::string &sceneName, const bool defaultPath) {
        ++sceneGeneration_;

        auto path = sceneName;

        if (defaultPath) {
            path = globals_->GetWorkingFolder() + paths::SCENE_DEFAULT + sceneName;
        }

        std::ifstream ifs(path);

        if (!ifs.is_open())
        {
            spdlog::error("Can't open file scene [{}]", path);
            // LoadBuiltInScene handles its own Prepare/Finalize -- just delegate.
            LoadBuiltInScene();
            return;
        }

        PrepareSceneSwitch();

        cereal::JSONInputArchive archive2(ifs);
        try {
            mainScene_ = std::make_shared<Scene>("");
            archive2(*mainScene_);

            PreloadSceneAssets();

            SetupSceneSystems(*mainScene_, *this);

            globals_->Set(gk::prefix::STATE, gk::key::STATE_LAST_SCENE, path);
            spdlog::info("Scene loaded successfully [{}]", mainScene_->sceneName_);
        } catch (const std::exception &e) {
            spdlog::error("Can't deserialize scene reason: [{}]", e.what());
            // LoadBuiltInScene handles its own Prepare/Finalize -- just delegate.
            LoadBuiltInScene();
            return;
        }

        FinalizeSceneLoad();
    }

    void Engine::StoreScene(const std::string &sceneName, const bool defaultPath) const {
        // Use std::string (not string_view) -- the concatenated path is a temporary.
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

    void Engine::RegisterModules(const std::vector<std::shared_ptr<GameModule> > &modules) {
        gameModules_ = modules;
        for (const auto &module: gameModules_) {
            spdlog::warn("Game module registered {}", module->name_);
        }
    }

    bool Engine::LoadDynamicModule(const std::string& dllPath) {
        return moduleLoader_.LoadModule(dllPath, this);
    }

    // -- Persistence ------
    void Engine::LoadGlobals(const std::string &fileName) {
        const auto filePath = globals_->GetWorkingFolder() + fileName;

        std::ifstream file(filePath);
        if (!file.is_open()) {
            spdlog::error("Globals file not found: '{}'", filePath);
            return;
        }

        // Strip UTF-8 BOM (EF BB BF) written by some Windows editors --
        // RapidJSON does not skip it and will fail to parse the document.
        {
            unsigned char bom[3] = {};
            file.read(reinterpret_cast<char *>(bom), 3);
            if (!(bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF))
                file.seekg(0); // not a BOM -- rewind to start
        }

        try {
            cereal::JSONInputArchive ar(file);
            ar(*globals_);
        } catch (const std::exception &e) {
            spdlog::error("Failed to parse globals '{}': {}", filePath, e.what());
        }
    }

    void Engine::StoreGlobals(const std::string &fileName) const {
        const auto filePath = globals_->GetWorkingFolder() + fileName;

        std::ofstream file(filePath);
        if (!file.is_open()) {
            spdlog::warn("Cannot write globals file: '{}'", filePath);
            return;
        }

        cereal::JSONOutputArchive ar(file);
        ar(*globals_);
    }

    void Engine::ConfigResource() {
        globals_ = GetDependency(Globals);
        globals_->AutoSetWorkingFolder();

        // Seed structural defaults first -- engine works even if the file is missing/corrupt.
        globals_->SetupDefaults();

        // Load 80CC.json; successful load overwrites the defaults above.
        LoadGlobals(paths::RESOURCES_DEFAULT);

        // Always write back so the file is in clean cereal format for next run
        // (fixes corrupt/BOM files automatically).
        StoreGlobals(paths::RESOURCES_DEFAULT);

        // Load render layer config
        renderLayerConfig_.Load(globals_->GetWorkingFolder() + "config/render_layers.json");

        // Register runtime engine paths so any system can read them via Globals.
        globals_->Set(gk::prefix::ENGINE, gk::key::ENGINE_WORKING_DIR,
                      globals_->GetWorkingFolder());
#ifdef _WIN32
        char exeBuf[512] = {};
        if (GetModuleFileNameA(nullptr, exeBuf, sizeof(exeBuf))) {
            const std::string exeDir =
                    std::filesystem::path(exeBuf).parent_path().string();
            globals_->Set(gk::prefix::ENGINE, gk::key::ENGINE_EXE_DIR, exeDir);
        }
#else
        {
            char exeBuf[512] = {};
            ssize_t len = ::readlink("/proc/self/exe", exeBuf, sizeof(exeBuf) - 1);
            if (len > 0) {
                exeBuf[len] = '\0';
                const std::string exeDir =
                        std::filesystem::path(exeBuf).parent_path().string();
                globals_->Set(gk::prefix::ENGINE, gk::key::ENGINE_EXE_DIR, exeDir);
            }
        }
#endif
    }

    // -- Async asset preloading -
    // Collects all unique texture paths and shader names from the current scene,
    // reads the raw data on a background thread, then uploads GL objects on the
    // main thread.  Call after scene deserialization but BEFORE SetupSceneSystems
    // so that Renderable::Init() finds everything already cached.
    void Engine::PreloadSceneAssets()
    {
        if (!mainScene_ || !assetRegistry_)
            return;

        const std::string workDir = globals_->GetWorkingFolder();
        const std::string shadersPath = workDir + globals_->Get(gk::prefix::PATHS, gk::key::PATH_SHADERS);

        // -- Collect unique texture paths -------------------------------------
        std::set<std::string> texPaths;
        for (ecs::Entity e: mainScene_->registry_.Pool<RenderableNode>().Entities()) {
            auto *rn = mainScene_->registry_.Get<RenderableNode>(e);
            if (!rn || !rn->renderable_) continue;
            if (auto sprite = std::dynamic_pointer_cast<Sprite>(rn->renderable_)) {
                const auto &rel = sprite->GetTexturePath();
                if (!rel.empty())
                    texPaths.insert(workDir + rel);
            }
        }

        // -- Collect shader names ---------------------------------------------
        std::vector<std::string> shaderNames = {"sprite", "sprite_instanced", "softbody", "grid"};

        spdlog::info("[Engine] Preloading {} textures, {} shaders async...",
                     texPaths.size(), shaderNames.size());

        // -- Fire async preloads
        std::vector<std::string> texVec(texPaths.begin(), texPaths.end());
        auto imageFuture = AssetRegistry::PreloadImagesAsync(texVec);
        auto shaderFuture = AssetRegistry::PreloadShadersAsync(shadersPath, shaderNames);

        // -- Upload shaders (GL calls -- main thread) --------------------------
        auto shaderSources = shaderFuture.get();
        assetRegistry_->UploadShaders(shaderSources);

        // -- Upload textures (GL calls -- main thread) -------------------------
        auto images = imageFuture.get();
        for (auto &img: images) {
            if (img.pixels)
            {
                // Derive relative path from absolute
                std::string relPath = img.path;
                if (relPath.find(workDir) == 0)
                    relPath = relPath.substr(workDir.size());
                for (char& c : relPath) if (c == '\\') c = '/';
                assetRegistry_->UploadTexture(img, relPath);
            }
        }

        spdlog::info("[Engine] Asset preload complete -- {} shaders, {} textures cached",
                     assetRegistry_->GetShaderCount(),
                     assetRegistry_->GetTextureCount());
    }

    void Engine::InitAssetRegistry()
    {
        assetRegistry_ = std::make_shared<AssetRegistry>();

        const std::string workDir     = globals_->GetWorkingFolder();
        const std::string shadersPath = workDir + globals_->Get(gk::prefix::PATHS, gk::key::PATH_SHADERS);
        const std::string imagesRel   = globals_->Get(gk::prefix::PATHS, gk::key::PATH_IMAGES);
        const std::string matsRel     = globals_->Get(gk::prefix::PATHS, gk::key::PATH_MATERIALS);

        assetRegistry_->Init(workDir, shadersPath);
        assetRegistry_->ScanAndLoad(imagesRel, matsRel);  // metadata only -- no GL calls

        RegisterDependency(AssetRegistry, assetRegistry_);
    }

    void Engine::InitPhysics()
    {
        physicsRegistry_.Register("Bullet", []() {
            return std::make_unique<physics::BulletPhysicsWorld>();
        });
        physicsRegistry_.Register("Box2D", []() {
            return std::make_unique<physics::Box2DPhysicsWorld>();
        });

        std::string backend = globals_->Get(gk::prefix::STATE, gk::key::STATE_PHYSICS_BACKEND);
        if (backend.empty() || !physicsRegistry_.Has(backend))
            backend = "Box2D";
        physicsWorld_ = physicsRegistry_.Create(backend);
        physicsWorld_->Init();
        spdlog::info("[Engine] Physics backend: {}", backend);
    }

    void Engine::InitPresentation()
    {
        // GPU: upload all scanned textures to the GPU
        assetRegistry_->UploadAllTextures();

        // GPU: precompile essential shaders (so built-in scenes don't rely on lazy compilation)
        for (const auto& name : {"sprite", "sprite_instanced", "softbody", "grid"})
            assetRegistry_->GetShader(name);

        // Audio
        audioManager_.Init();
        audioInitialized_ = true;

        if (isEditorMode_)
            audioManager_.PlayStartupChime();

        // Screen size
        auto sz = appInstance_->GetMainWindowSize();
        renderEngine_.SetScreenSize(sz.x, sz.y);
    }

    void Engine::InitScene()
    {
        if (!isEditorMode_ && !gameModules_.empty())
        {
            for (const auto &module: gameModules_) {
                if (!module) {
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
                spdlog::warn("[Engine] No game modules registered -- loading last scene");

            LoadLastScene();
        }
    }

    void Engine::PrepareSceneSwitch()
    {
        // Drain all in-flight async work that references the current scene
        // or the physics world before we destroy anything.
        DrainPhysicsFuture();

        // Clear network component registry so inbound transforms (from the
        // network worker thread) don't dereference dead component pointers
        // while the old scene is being torn down.
        networkManager_.ClearComponentRegistry();

        if (isHeadless_) return;
        renderEngine_.ClearRenderables();
    }

    void Engine::FinalizeSceneLoad()
    {
        if (isHeadless_) return;

        if (!isEditorMode_)
            EnsureGameCamera();
    }

    void Engine::Init()
    {
        Benchmark bench;
        bench.Begin();

        // -- Phase 1: Configuration & core systems (always) --------------------
        ConfigResource();
        bench.Mark("config_resources");

        InitAssetRegistry();
        bench.Mark("asset_registry_init");

        InitPhysics();
        bench.Mark("physics_init");

        // -- Phase 2: Presentation -- GPU upload + audio (skip for headless) ----
        if (!isHeadless_)
        {
            InitPresentation();
            bench.Mark("presentation_init");
        }

        // -- Phase 3: Scene loading (headless defers to server main.cpp) -------
        if (!isHeadless_)
        {
            InitScene();
            bench.Mark("scene_load");
        }

        if (mainScene_)
            spdlog::info("[Engine] Active scene [{}]", mainScene_->sceneName_);

        // -- Phase 4: Modules & network ----------------------------------------
        {
            const std::string modulesDir = globals_->GetWorkingFolder() + "modules/";
            int loaded = moduleLoader_.LoadModulesFromDirectory(modulesDir, this);
            bench.Mark("dll_modules_load");
            if (loaded > 0)
                spdlog::info("[Engine] {} DLL module(s) loaded from '{}'", loaded, modulesDir);
        }

        // Write benchmark results
        const std::string benchPath = globals_->GetWorkingFolder() + "config/startup_benchmark.csv";
        bench.WriteToFile(benchPath);

        // Network worker: standalone mode auto-starts; editor defers to BeginPlay;
        // headless defers to InitNetwork().
        if (!isHeadless_ && !isEditorMode_)
            StartNetworkWorker();
    }

    void Engine::Update()
    {
        using Clock = std::chrono::high_resolution_clock;
        using Ms = std::chrono::duration<float, std::milli>;

        const float dt = appInstance_->GetDeltaTime();
        const auto frameStart = Clock::now();

        // -- 1. Wait for previous frame's physics step (pipelining) -----------
        auto t0 = Clock::now();
        if (physicsFuture_.valid())
        {
            // The future returns the step duration in ms (measured on the worker)
            threadDebugInfo_.physics.durationMs = physicsFuture_.get();
            threadDebugInfo_.physics.async = true;
        }

        // -- 2. Drain inbound network transforms (lock-free queue) ------------
        auto tNet = Clock::now();
        if (!simulationPaused_ || !isEditorMode_)
        {
            networkManager_.HandlePendingDisconnect();
            networkManager_.ApplyInboundTransforms();
        }
        threadDebugInfo_.network.durationMs = Ms(Clock::now() - tNet).count();
        threadDebugInfo_.network.async = true; // polling runs on network worker

        // -- 3. Scene MAIN processing (gravity, fusion, sync, broadcast) ------
        auto t1 = Clock::now();
        if (!simulationPaused_)
            mainScene_->Process(dt, ProcessingChannel::MAIN);
        threadDebugInfo_.main.durationMs = Ms(Clock::now() - t1).count();
        threadDebugInfo_.main.async = false;

        if (!simulationPaused_)
        {
            for (const auto &module: gameModules_)
                module->OnUpdate(dt);

            // DLL modules loaded via ModuleLoader
            for (auto* mod : moduleLoader_.GetModules())
                mod->OnUpdate(dt);
        }

        // Poll for DLL hot-reloads (internally throttled to ~1 check/sec)
        moduleLoader_.PollForReloads(this, dt);

        // -- 4. Kick physics step for THIS frame (runs during PresentFrame) ---
        // Bullet Step overlaps with rendering.  Safe because all Bullet
        // reads/writes in Process(MAIN) are done before this point, and the
        // next frame's Update waits for this future before touching Bullet.
        if (!simulationPaused_)
        {
            physicsFuture_ = threadRegistry_.Submit([this, dt]() {
                auto t = Clock::now();
                physicsWorld_->Step(dt);
                return Ms(Clock::now() - t).count();
            });
        }

        threadDebugInfo_.updatePhaseMs = Ms(Clock::now() - frameStart).count();
    }

    void Engine::PrepareFrame()
    {
    }

    void Engine::PresentFrame()
    {
        using Clock = std::chrono::high_resolution_clock;
        using Ms = std::chrono::duration<float, std::milli>;

        const float dt = appInstance_->GetDeltaTime();
        const auto frameStart = Clock::now();

        auto scene = mainScene_;

        // -- Audio on a pool thread (via ThreadRegistry instead of raw async) -
        threadDebugInfo_.audio.durationMs = 0.f;

        auto audioFuture = threadRegistry_.Submit([this, scene, dt]() {
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
        threadDebugInfo_.rendering.async = false;

        audioFuture.get(); // join -- threadDebugInfo_.audio.durationMs written by worker

        threadDebugInfo_.audio.async = true;

        threadDebugInfo_.presentPhaseMs = Ms(Clock::now() - frameStart).count();
        threadDebugInfo_.PushSample();

        // NOTE: inputSystem_.ResetState() is now called at the start of
        // AppInput() so both the render pass and the UI pass (game-view
        // camera controller, etc.) can read the same input snapshot.
    }

    PlayerInput *Engine::GetInputSystem()
    {
        return &inputSystem_;
    }

    void Engine::StartNetworkWorker()
    {
        auto* existing = threadRegistry_.GetWorker("network");
        if (existing && existing->GetState() == WorkerThread::State::Running)
            return; // already running

        auto& netWorker = threadRegistry_.CreateWorker("network");
        netWorker.Start([this]() {
            networkManager_.Poll();
            networkManager_.SendOutbound();
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        });
        spdlog::info("[Engine] Network worker started");
    }

    void Engine::StopNetworkWorker()
    {
        auto* w = threadRegistry_.GetWorker("network");
        if (w && w->GetState() == WorkerThread::State::Running)
        {
            w->Stop();
            spdlog::info("[Engine] Network worker stopped");
        }
    }

    void Engine::InitNetwork(bool isHost, uint16_t port, const std::string &serverAddress)
    {
        if (isHost)
            networkManager_.InitHost(port);
        else
            networkManager_.InitClient(serverAddress, port);

        // For headless mode the worker is deferred until here so the
        // host/client is fully set up before polling begins.
        if (isHeadless_)
            StartNetworkWorker();
    }

    // -- Editor playback lifecycle --------------------------------------------

    void Engine::BeginPlay()
    {
        // -- Editor playback lifecycle ----------------------------------------
        // Called by the editor to cleanly start/stop the simulation loop.
        // Handles module restart, network worker, and simulation pause state.

        if (isPlaying_) return;
        isPlaying_ = true;
        spdlog::info("[Engine] BeginPlay");

        // Re-init bodies that were released by EndPlay.
        if (mainScene_)
            mainScene_->Init(*this);

        simulationPaused_ = false;

        // Restart DLL modules so they register systems / components fresh.
        RestartDllModules();

        // Start the network worker so Poll/Send actually run.
        StartNetworkWorker();
    }

    void Engine::EndPlay()
    {
        if (!isPlaying_) return;
        isPlaying_ = false;
        spdlog::info("[Engine] EndPlay");
        simulationPaused_ = true;

        // Wait for any in-flight async physics step before touching bodies.
        DrainPhysicsFuture();

        // Release all physics bodies so they don't reference the world during teardown.
        // Soft bodies first -- their clusters hold contacts to rigid bodies.
        if (mainScene_)
        {
            auto& sbPool = mainScene_->registry_.Pool<SoftBodyComponent>();
            for (size_t i = 0; i < sbPool.Size(); ++i)
            {
                auto renderable = sbPool.Components()[i].GetRenderable();
                if (renderable)
                    renderEngine_.RemoveRenderable(renderable);
                sbPool.Components()[i].ReleaseBody();
            }

            auto& rbPool = mainScene_->registry_.Pool<RigidBodyComponent>();
            for (size_t i = 0; i < rbPool.Size(); ++i)
                rbPool.Components()[i].ReleaseBody();
        }

        // Tear down DLL module instances (systems, components they added).
        for (auto* mod : moduleLoader_.GetModules())
            mod->OnDestroy();

        // Stop network activity while the editor is idle.
        StopNetworkWorker();
        networkManager_.Shutdown();
    }

    void Engine::RestartDllModules()
    {
        for (auto* mod : moduleLoader_.GetModules())
            mod->OnDestroy();

        // Purge empty pools so any vtable references from module code are released.
        if (mainScene_)
            mainScene_->registry_.PurgeEmptyPools();

        for (auto* mod : moduleLoader_.GetModules())
            mod->OnStart(this);

        spdlog::info("[Engine] Restarted {} DLL module(s)", moduleLoader_.GetModules().size());
    }
} // namespace ettycc
