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

        std::shared_ptr<Camera> mainCamera = std::make_shared<Camera>(mainWindowSize.x, mainWindowSize.y, 90, 0.01f);
        std::shared_ptr<Sprite> someSprite = std::make_shared<Sprite>();
        someSprite->underylingTransform.setGlobalPosition(glm::vec3(0, 0, -5));

        ghostCamera_ = std::make_shared<GhostCamera>(&inputSystem_, mainCamera);

        renderEngine_.AddRenderable(mainCamera);
        renderEngine_.AddRenderable(someSprite);
    }

    void Engine::Init()
    {
        RenderingEngineDemo();
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
        renderEngine_.Pass(appInstance_->GetCurrentTime());
    }

    void Engine::ProcessInput(PlayerInputType type, uint64_t *data)
    {
        inputSystem_.ProcessInput(type, data);
    }
} // namespace ettycc
