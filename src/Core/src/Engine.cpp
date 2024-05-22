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
            engineResources_->Store(paths::RESOURCES_DEFAULT);
            spdlog::warn("Engine config auto-saved");
        }
        else
        {
            spdlog::error("can't auto save engine configuration!!!");
        }
    }
    

    void Engine::LoadLastScene()
    {
        auto lastLoadedScene = engineResources_->Get("state", "last_scene");

        LoadScene(lastLoadedScene);

        // TODO: MOVE THIS CODE BELOW BECAUSE IS GENERIC ENGINE INITIALIZATION
        auto mainWindowSize = appInstance_->GetMainWindowSize();
        renderEngine_.SetScreenSize(mainWindowSize.x, mainWindowSize.y);
    }

    void Engine::LoadScene(const std::string &sceneName)
    {
        // TODO: ADD DEFAULT SCENE WITH THE NO TEXTURE FOND AS DEFAULT SCENE...
        mainScene_.reset();
        mainScene_ = std::make_shared<Scene>("80CC-EMPTY-SCENE");
        std::ifstream ifs(paths::SCENE_DEFAULT + sceneName);

        if (!ifs.is_open())
        {
            spdlog::error("Cannot open file scene {}", sceneName);
            return;
        }

        cereal::JSONInputArchive archive2(ifs);
        archive2(*mainScene_);

        if (mainScene_)
        {
            engineResources_->Set("state", "last_scene", sceneName);
            mainScene_->Init();
        }
        else 
        {
            spdlog::error("Cannot initialize scene {}", sceneName);
        }
    }

    void Engine::StoreScene(const std::string &sceneName)
    {
        std::ofstream ofs(paths::SCENE_DEFAULT + sceneName); 

        if (!ofs.is_open())
        {
            spdlog::error("Cannot store scene {}", sceneName);
            return;
        }

        cereal::JSONOutputArchive archive(ofs);    
        archive(*mainScene_);
    }

    void Engine::RegisterModules(const std::vector<std::shared_ptr<GameModule>>& modules)
    {
        gameModules_ = modules;
        for (const auto& module : gameModules_)
        {
            spdlog::warn("Game module registered {}", module->name_);
        }
    }

    void Engine::Init()
    {
        engineResources_ = GetDependency(Resources);

        const char* engineWorkingFolder = std::getenv("ASSETS_80CC");
        if (engineWorkingFolder == nullptr) 
        {
            spdlog::warn("Engine working folder not set... using: {}", paths::ASSETS_DEFAULT);    
            engineResources_->SetWorkingFolder(paths::CONFIG_DEFAULT);
        }
        else 
        {
            // Assuming the proyect structure is: 80cc/build/Testers
            spdlog::info("Engine working folder '{}'", engineWorkingFolder);
            engineResources_->SetWorkingFolder(std::string(engineWorkingFolder) + "/config/");
        }

        // Load engine resource file
        engineResources_->Load(paths::RESOURCES_DEFAULT);

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

        spdlog::warn("Scene loaded... {}");
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
        
        renderEngine_.Pass(appInstance_->GetCurrentTime());
        inputSystem_.ResetState(); // TODO: FIX THIS THRASH WITH INPUT UP AND DOWN EVENTS (INTERNALLY)
    }

    void Engine::ProcessInput(PlayerInputType type, uint64_t *data) // UNSAFE!!!! FIX THIS TRASH
    {
        inputSystem_.ProcessInput(type, data);
    }

    void Engine::BuildExecutable(const std::string& outputPath)
    {
        // Get (config,images,scenes,shaders,templates) folders inside of assets and copy them into outputPath
        // Compile all assets into a binary form (optional meanwhile)

        // Use an env var to locate the engine source code
        // Compile the assets/src with:
        //  cmake --build ./  --config Debug --target Game -j 4

    }
} // namespace ettycc
