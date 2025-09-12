#include <Engine.hpp>
#include <Dependency.hpp>

// TEST INCLUDE
#include <Scene/Components/RenderableNode.hpp>
#include <Dependencies/Resources.hpp>

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

    void Engine::LoadDefaultScene()
    {
        // default scene construction (basic sprite with not found texture...)
        mainScene_ = std::make_shared<Scene>("default-scene");

        auto rootSceneNode = mainScene_->root_node_;
        
        // not found sprite
        std::string notFoundTexturePath = engineResources_->Get("sprites", "not-found");
        
        createSprite(rootSceneNode, notFoundTexturePath,glm::vec3(-1, 0, 0));
        createSprite(rootSceneNode, notFoundTexturePath,glm::vec3(2, 0, 0));

        // default camera
        std::shared_ptr<Camera> mainCamera = std::make_shared<Camera>(1200,800);
        mainCamera->AttachEditorControl(&inputSystem_);

        auto cameraNode = std::make_shared<SceneNode>("cameraNode-editor-camera");
        mainCamera->mainCamera_ = true;
        editorCamera_ = mainCamera;

        cameraNode->AddComponent(std::make_shared<RenderableNode>(mainCamera));
        rootSceneNode->AddChild(cameraNode);
    }

    void Engine::LoadLastScene()
    {
        auto lastLoadedScene = engineResources_->Get("state", "last_scene");
        spdlog::warn("Loading last scene: {}", lastLoadedScene);

        LoadScene(lastLoadedScene);

        // TODO: MOVE THIS CODE BELOW BECAUSE IS GENERIC ENGINE INITIALIZATION
        auto mainWindowSize = appInstance_->GetMainWindowSize();
        renderEngine_.SetScreenSize(mainWindowSize.x, mainWindowSize.y);
    }

    //TODO: INSERT HERE ASSET MANAGEMENT AND NODE BUILDER (TO BUILD TEMPLATES FROM THE ASSET MANAGEMENT)
    void Engine::LoadScene(const std::string &sceneName)
    {
        // TODO: ADD DEFAULT SCENE WITH THE NO TEXTURE FOND AS DEFAULT SCENE...
        std::ifstream ifs(engineResources_->GetWorkingFolder() + paths::SCENE_DEFAULT + sceneName);

        if (!ifs.is_open())
        {
            // if not open loads default scene
            spdlog::error("Cannot open file scene [{}]; loading fall-back scene [default]", sceneName);
            LoadDefaultScene();
        }
        else
        {
            // FIX THIS (DOES NOT WORK...)
            cereal::JSONInputArchive archive2(ifs);
            archive2(*mainScene_);
        }

        if (mainScene_)
        {
            engineResources_->Set("state", "last_scene", mainScene_->sceneName_);
            // mainScene_->Init();

            spdlog::info("Scene loaded successfully [{}]", mainScene_->sceneName_);
        }
        else
        {
            spdlog::error("Cannot initialize scene {}", sceneName);
        }
    }

    void Engine::StoreScene(const std::string &sceneName)
    {
        std::ofstream ofs(engineResources_->GetWorkingFolder() + paths::SCENE_DEFAULT + sceneName);

        if (!ofs.is_open())
        {
            spdlog::error("Cannot store scene {}", sceneName);
            return;
        }

        cereal::JSONOutputArchive archive(ofs);
        archive(*mainScene_);
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
        engineResources_ = GetDependency(Resources);
        engineResources_->AutoSetWorkingFolder();

        // Load 80CC.json, the working folder should point to the 80CC.JSON (important as hell...)
        engineResources_->Load(paths::RESOURCES_DEFAULT);
    }

    void Engine::Init()
    {
        ConfigResource();

        // If game modules present then they have to load the scene from there, otherwise it will load the last loaded scene (thus due to editor logic)
        // TODO: This condition is wrong, it needs to know if is an editor or game executable
        if (gameModules_.size() > 0)
        {
            for (const auto &module : gameModules_)
            {
                if (!module)
                {
                    spdlog::error("Cannot initialize [{}] game module", module->name_);
                    continue;
                }
                module->OnStart(this);
                spdlog::info("Game module initialized [{}]", module->name_);
            }
        }
        else
        {
            LoadLastScene();
        }

        spdlog::warn("Scene loaded [{}]", mainScene_->sceneName_);
    }

    void Engine::Update()
    {
        // Engine logic goes here
        auto deltaTime = appInstance_->GetDeltaTime();
        mainScene_->Process(appInstance_->GetDeltaTime(), ProcessingChannel::MAIN);

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

    void Engine::BuildExecutable(const std::string &outputPath)
    {
        // Get (config,images,scenes,shaders,templates) folders inside of assets and copy them into outputPath
        // Compile all assets into a binary form (optional meanwhile)

        // Use an env var to locate the engine source code
        // Compile the assets/src with:
        //  cmake --build ./  --config Debug --target Game -j 4
    }
} // namespace ettycc
