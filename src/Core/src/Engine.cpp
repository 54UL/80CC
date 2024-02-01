#include <Engine.hpp>

namespace ettycc
{
    Engine::Engine(std::shared_ptr<App> appInstance) : appInstance_(appInstance)
    {
    }

    Engine::~Engine()
    {
    }

    void Engine::RenderingEngineDemo()
    {
        auto mainWindowSize = appInstance_->GetMainWindowSize();
        renderEngine_.SetScreenSize(mainWindowSize.x, mainWindowSize.y);

        // below is the editor view port camera
        std::shared_ptr<Camera> mainCamera = std::make_shared<Camera>(200,300, 90, 0.01f);
        std::shared_ptr<Sprite> someSprite = std::make_shared<Sprite>();
        mainCamera->underylingTransform.setGlobalPosition(glm::vec3(0, 0, -2));
       
        renderEngine_.SetViewPortFrameBuffer(mainCamera->offScreenFrameBuffer);
        ghostCamera_ = std::make_shared<GhostCamera>(&inputSystem_, mainCamera);

        renderEngine_.AddRenderable(mainCamera);
        renderEngine_.AddRenderable(someSprite);
    }

    void Engine::Init()
    {
        RenderingEngineDemo();
        spdlog::warn("App and Game engine ready..."); // if it gets here it means that the app was initialized just fine...?
    }

    void Engine::Update()
    {
        // Engine logic goes here
        ghostCamera_->Update(appInstance_->GetDeltaTime());
    }

    void Engine::PrepareFrame()
    {
        
    }

    void Engine::PresentFrame()
    {
        renderEngine_.Pass(appInstance_->GetDeltaTime());
    }

    void Engine::ProcessInput(PlayerInputType type, uint64_t *data)
    {
        inputSystem_.ProcessInput(type, data);
    }
} // namespace ettycc
