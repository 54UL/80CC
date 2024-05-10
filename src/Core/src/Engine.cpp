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
            engineResources_->Store(ENGINE_RESOURCES_PATH);
            spdlog::warn("Engine config auto-saved");
        }
        else
        {
            spdlog::error("can't auto save engine configuration!!!");
        }
    }
    
    // TODO: MOVE THIS PICE OF CODE INTO THE GOOGL UNIT TEST...
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
        mainScene_.reset();
        mainScene_ = std::make_shared<Scene>("80CC-EMPTY-SCENE");
        std::ifstream ifs(scenesPath_ + sceneName);

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
        std::ofstream ofs(scenesPath_ + sceneName); 

        if (!ofs.is_open())
        {
            spdlog::error("Cannot store scene {}", sceneName);
            return;
        }

        cereal::JSONOutputArchive archive(ofs);    
        archive(*mainScene_);
    }

    void Engine::Init()
    {
        engineResources_ = GetDependency(Resources);

        const char* engineWorkingFolder = std::getenv("ASSETS_80CC");
        if (engineWorkingFolder == nullptr) 
        {
            spdlog::warn("Engine working folder not set... using '../../assets'");    
            engineResources_->SetWorkingFolder(std::string("../../../assets") + "/config/");
        }
        else 
        {
            // Assuming the proyect structure is: 80cc/build/Testers
            spdlog::info("Engine working folder '{}'", engineWorkingFolder);
            engineResources_->SetWorkingFolder(std::string(engineWorkingFolder) + "/config/");
        }

        // Load engine resource file
        engineResources_->Load(ENGINE_RESOURCES_PATH);

        // Load the last scene provied from the state of the last scene...
        LoadLastScene();

        spdlog::warn("Scene loaded... be aware!!!");
    }
 
    void Engine::Update()
    {
        // Engine logic goes here
        mainScene_->Process(appInstance_->GetDeltaTime(), ProcessingChannel::MAIN);
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
} // namespace ettycc
