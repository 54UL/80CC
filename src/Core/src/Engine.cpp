#include <Engine.hpp>
#include <Dependency.hpp>

// TEST INCLUDE
#include <Scene/Components/RenderableNode.hpp>
#include <Scene/Components/RigidBodyComponent.hpp>
#include <Scene/Components/SoftBodyComponent.hpp>
#include <Networking/NetworkComponent.hpp>
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
        // TODO: Check if this is appropiate to handle here...
        if (engineResources_)
        {
            spdlog::warn("Engine config auto-saved");
            engineResources_->Store(paths::RESOURCES_DEFAULT);
        }
        else
        {
            spdlog::error("can't auto save engine configuration!!!");
        }
    }

    // TODO: THIS SHOULD BE SOMEWHERE IN A ASSET BULDER IN ORDDER TO CREATE STUFF LIKE THIS FROM TEMPLATES
    void Engine::createSprite(std::shared_ptr<SceneNode> rootSceneNode, std::string spriteTexturePath, const glm::vec3 pos) {
        auto spriteComponent = std::make_shared<Sprite>(spriteTexturePath);
        static int index = 0;
        auto spriteNode = std::make_shared<SceneNode>(std::string("sprite primitive #").append(std::to_string(index++)));

        spriteNode->AddComponent(std::make_shared<RenderableNode>(spriteComponent));
        spriteComponent->underylingTransform.setGlobalPosition(pos);
        rootSceneNode->AddChild(spriteNode);
    }

    void Engine::createPhysicsBox(std::shared_ptr<SceneNode> rootSceneNode, const std::string& texPath,
                                   float mass, glm::vec3 halfExtents, glm::vec3 pos)
    {
        auto sprite = std::make_shared<Sprite>(texPath);
        // The sprite quad has local vertices at ±1, so scale = halfExtents gives
        // a world-space box of ±halfExtents — matching the Bullet btBoxShape exactly.
        sprite->underylingTransform.setGlobalPosition(pos);
        sprite->underylingTransform.setGlobalScale(halfExtents); // full 3D scale = exact hull half-extents

        static int index = 0;
        auto node = std::make_shared<SceneNode>("physics-box-" + std::to_string(index++));
        // RenderableNode must be added first — RigidBodyComponent finds it via ownerNode_ in OnStart
        node->AddComponent(std::make_shared<RenderableNode>(sprite));
        node->AddComponent(std::make_shared<RigidBodyComponent>(mass, halfExtents, pos));
        rootSceneNode->AddChild(node);
    }

    void Engine::createSoftBody(std::shared_ptr<SceneNode> rootSceneNode, std::string texPath,
                                float radius, glm::vec3 pos, float mass)
    {
        static int softBodyIndex = 0;
        auto node = std::make_shared<SceneNode>("soft-body-" + std::to_string(softBodyIndex++));
        node->AddComponent(std::make_shared<SoftBodyComponent>(radius, pos, mass, std::move(texPath)));
        rootSceneNode->AddChild(node);
    }

    void Engine::LoadPhysicsScene()
    {
        spdlog::info("[Engine] building physics scene...");

        renderEngine_.ClearRenderables();
        mainScene_ = std::make_shared<Scene>("physics-scene");

        const std::string tex = engineResources_->Get(gk::prefix::SPRITES, gk::key::SPRITE_NOT_FOUND);
        auto root = mainScene_->root_node_;

        // ── Static boundaries ─────────────────────────────────────────────────
        // Ground
        createPhysicsBox(root, tex, 0.0f, glm::vec3(9.0f, 0.3f, 0.5f),  glm::vec3( 0.0f, -5.0f, 0.0f));
        // Left wall
        createPhysicsBox(root, tex, 0.0f, glm::vec3(0.3f, 5.5f, 0.5f),  glm::vec3(-9.3f,  0.0f, 0.0f));
        // Right wall
        createPhysicsBox(root, tex, 0.0f, glm::vec3(0.3f, 5.5f, 0.5f),  glm::vec3( 9.3f,  0.0f, 0.0f));

        // ── Dynamic boxes: two columns, staggered ──────────────────────────────
        for (int i = 0; i < 5; ++i)
        {
            // Left column
            createPhysicsBox(root, tex, 1.0f, glm::vec3(0.5f, 0.5f, 0.5f),
                             glm::vec3(-1.1f, -3.8f + i * 1.15f, 0.0f));
            // Right column — offset slightly so they topple on each other
            createPhysicsBox(root, tex, 1.0f, glm::vec3(0.5f, 0.5f, 0.5f),
                             glm::vec3( 1.1f, -3.8f + i * 1.15f + 0.55f, 0.0f));
        }

        // ── Soft body rubber discs — dropped from above ────────────────────────
        createSoftBody(root, tex, 0.8f, glm::vec3(-3.0f,  3.0f, 0.0f), 1.0f);
        createSoftBody(root, tex, 0.6f, glm::vec3( 0.0f,  4.5f, 0.0f), 1.0f);
        createSoftBody(root, tex, 1.0f, glm::vec3( 3.0f,  3.0f, 0.0f), 1.5f);
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

    void Engine::LoadDefaultScene()
    {
        spdlog::warn("[Engine] loading fallback physics scene...");

        renderEngine_.ClearRenderables();
        mainScene_ = std::make_shared<Scene>("default-scene");

        auto root = mainScene_->root_node_;
        const std::string tex = engineResources_->Get(gk::prefix::SPRITES, gk::key::SPRITE_NOT_FOUND);

        // Static ground platform — wide and thin
        createPhysicsBox(root, tex, 0.0f, glm::vec3(9.0f, 0.3f, 0.5f), glm::vec3(0.0f, -4.5f, 0.0f));

        // Mixed pyramid — rigid boxes and soft-body circles alternating
        const float UNIT = 0.5f;
        const float STEP = UNIT * 2.1f;

        // row 0 (bottom, 4): box  soft  box  soft
        // row 1 (mid,    3): soft box   soft
        // row 2 (top,    2): box  soft
        struct { int count; float y; } rows[] = {
            { 4, -4.5f + 0.3f + UNIT       },
            { 3, -4.5f + 0.3f + UNIT * 3.f },
            { 2, -4.5f + 0.3f + UNIT * 5.f },
        };
        for (int r = 0; r < 3; ++r)
        {
            auto& row   = rows[r];
            float startX = -(row.count - 1) * STEP * 0.5f;
            for (int j = 0; j < row.count; ++j)
            {
                glm::vec3 pos(startX + j * STEP, row.y, 0.0f);
                bool useSoft = (r == 0) ? (j % 2 != 0)   // row 0: odd indices -> soft
                             : (r == 1) ? (j % 2 == 0)   // row 1: even indices -> soft
                             :            (j % 2 != 0);  // row 2: odd index -> soft
                if (useSoft)
                    createSoftBody(root, tex, UNIT, pos, 1.0f);
                else
                    createPhysicsBox(root, tex, 1.0f, glm::vec3(UNIT, UNIT, 0.5f), pos);
            }
        }

        if (!isEditorMode_)
            EnsureGameCamera();
    }

    void Engine::LoadLastScene()
    {
        auto lastLoadedScene = engineResources_->Get(gk::prefix::STATE, gk::key::STATE_LAST_SCENE);
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
            path= engineResources_->GetWorkingFolder() + paths::SCENE_DEFAULT + sceneName;
        }

        std::ifstream ifs(path);

        if (!ifs.is_open())
        {
            spdlog::error("Can't open file scene [{}]", sceneName);

            LoadDefaultScene();
        }
        else
        {
            renderEngine_.ClearRenderables();
            cereal::JSONInputArchive archive2(ifs);
            try
            {
                {
                    mainScene_ = std::make_shared<Scene>("");
                    archive2(*mainScene_);
                    mainScene_->Init();
                    spdlog::info("Scene loaded successfully [{}]", mainScene_->sceneName_);
                }
            } catch (const std::exception& e) {
                spdlog::error("Can't deserialize scene reason: [{}]", e.what());
                LoadDefaultScene();
            }
        }

        if (mainScene_)
        {
            engineResources_->Set(gk::prefix::STATE, gk::key::STATE_LAST_SCENE, mainScene_->sceneName_);

            spdlog::info("Scene loaded successfully [{}]", mainScene_->sceneName_);
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
        std::string_view path = sceneName;

        if (defaultPath)
        {
            path= engineResources_->GetWorkingFolder() + paths::SCENE_DEFAULT + sceneName;
        }

        std::ofstream ofs(path.data());

        if (!ofs.is_open()) {
            spdlog::error("Cannot store scene {}", sceneName);
            return;
        }

        spdlog::info("Storing scene {}", sceneName);

        {
            cereal::JSONOutputArchive archive(ofs);
            archive(*mainScene_);
        }

        spdlog::info("Scene stored successfully {}", sceneName);
    }

    void Engine::RegisterModules(const std::vector<std::shared_ptr<GameModule>> &modules)
    {
        gameModules_ = modules;
        for (const auto &module : gameModules_)
        {
            spdlog::warn("Game module registered {}", module->name_);
        }
    }

    void Engine::ConfigResource()
    {
        engineResources_ = GetDependency(Globals);
        engineResources_->AutoSetWorkingFolder();

        // Load 80CC.json, the working folder should point to the 80CC.JSON (important as hell...)
        engineResources_->Load(paths::RESOURCES_DEFAULT);

        // Register runtime engine paths so any system can read them via Globals.
        engineResources_->Set(gk::prefix::ENGINE, gk::key::ENGINE_WORKING_DIR,
                              engineResources_->GetWorkingFolder());
//TODO: REFACTOR
#ifdef _WIN32
        char exeBuf[512] = {};
        if (GetModuleFileNameA(nullptr, exeBuf, sizeof(exeBuf)))
        {
            const std::string exeDir =
                std::filesystem::path(exeBuf).parent_path().string();
            engineResources_->Set(gk::prefix::ENGINE, gk::key::ENGINE_EXE_DIR, exeDir);
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
                engineResources_->Set(gk::prefix::ENGINE, gk::key::ENGINE_EXE_DIR, exeDir);
            }
        }
#endif
    }

    void Engine::Init()
    {
        ConfigResource();
        physicsWorld_.Init();

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
        auto deltaTime = appInstance_->GetDeltaTime();
        physicsWorld_.Step(deltaTime);
        networkManager_.Poll();
        mainScene_->Process(deltaTime, ProcessingChannel::MAIN);

        // Update game modules state...
        if (gameModules_.size() > 0)
        {
            for (const auto &module : gameModules_)
            {
                module->OnUpdate(deltaTime);
            }
        }
    }

    void Engine::PrepareFrame()
    {
    }

    void Engine::PresentFrame()
    {
        mainScene_->Process(appInstance_->GetDeltaTime(), ProcessingChannel::RENDERING);

        renderEngine_.Pass(appInstance_->GetDeltaTime()); // this just broke the shader animations lol
        inputSystem_.ResetState(); // TODO: CHECK IF IS MORE SUITIABLE TO HAVING IT ON THE DEFAULT CASE WHEN PROCESSING INPUT (ALWAYS ENTERS THERE)
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

        const std::string tex = engineResources_->Get(gk::prefix::SPRITES, gk::key::SPRITE_NOT_FOUND);
        auto root = mainScene_->root_node_;

        // Static boundaries — no network sync needed
        createPhysicsBox(root, tex, 0.0f, glm::vec3(9.0f, 0.3f, 0.5f), glm::vec3( 0.0f, -5.0f, 0.0f));
        createPhysicsBox(root, tex, 0.0f, glm::vec3(0.3f, 5.5f, 0.5f), glm::vec3(-9.3f,  0.0f, 0.0f));
        createPhysicsBox(root, tex, 0.0f, glm::vec3(0.3f, 5.5f, 0.5f), glm::vec3( 9.3f,  0.0f, 0.0f));

        // Build a replicated box: ALL components are added before AddChild so
        // that AddNode -> OnStart runs once for all of them together.
        uint32_t netId = 1;
        static int netBoxIdx = 0;
        auto makeNetBox = [&](glm::vec3 halfExt, glm::vec3 pos)
        {
            auto sprite = std::make_shared<Sprite>(tex);
            sprite->underylingTransform.setGlobalPosition(pos);
            sprite->underylingTransform.setGlobalScale(glm::vec3(halfExt.x, halfExt.y, 1.0f));

            auto node = std::make_shared<SceneNode>("net-box-" + std::to_string(netBoxIdx++));
            node->AddComponent(std::make_shared<RenderableNode>(sprite));
            node->AddComponent(std::make_shared<RigidBodyComponent>(1.0f, halfExt, pos));
            node->AddComponent(std::make_shared<NetworkComponent>(netId++));
            root->AddChild(node); // OnStart fired once for all three ^
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
