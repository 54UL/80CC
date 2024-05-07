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
    
    }
    
    // TODO: MOVE THIS PICE OF CODE INTO THE GOOGL UNIT TEST...
    void Engine::LoadDefaultScene()
    {
        auto resources_ = GetDependency(Resources);

        const char* engineWorkingFolder = std::getenv("ASSETS_80CC");
        if (engineWorkingFolder == nullptr) 
        {
            spdlog::warn("Engine working folder not set... using '../../assets'");    
            resources_->SetWorkingFolder(std::string("../../../assets") + "/config/");
        }
        else 
        {
            // Assuming the proyect structure is: 80cc/build/Testers
            spdlog::info("Engine working folder '{}'", engineWorkingFolder);
            resources_->SetWorkingFolder(std::string(engineWorkingFolder) + "/config/");
        }

        resources_->Load("80CC.json");
        const std::string scenesPath_ = "../../../assets/scenes/";

        // Scene initialization
        {
            mainScene_.reset();
            mainScene_ = std::make_shared<Scene>("80CC-NULL-SCENE");

            std::ifstream ifs(scenesPath_ + "/default_scene.json"); 

            cereal::JSONInputArchive archive2(ifs);
            archive2(*mainScene_);
        }

        mainScene_->Init();

        //TODO: GET DEFAULT RESOLUTION FROM CONFIG...(USE RESOURCES->GET)
        auto mainWindowSize = appInstance_->GetMainWindowSize();
        renderEngine_.SetScreenSize(mainWindowSize.x, mainWindowSize.y);
    }

    void Engine::LoadScene(const std::string &filePath)
    {

    }

    void Engine::StoreScene(const std::string &filePath)
    {
        
    }

    void Engine::Init()
    {
        //TODO: instead of loading a default scene it should seek for the last saved scene...
        LoadDefaultScene();
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
