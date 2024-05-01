#include <Engine.hpp>
#include <Dependency.hpp>

// TEST INCLUDE
#include <Scene/Components/RenderableNode.hpp>

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
        // Scene initialization
        // mainScene_ = std::make_shared<Scene>("Test scene");
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
        // RenderingEngineDemo();
        spdlog::warn("App and Game engine ready...");

        LoadDefaultScene();
    }
 
    void Engine::Update()
    {
        // Engine logic goes here
        // ghostCamera_->Update(appInstance_->GetDeltaTime());
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
