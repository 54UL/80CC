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
        const char* loonaImagePath = "D:/repos2/ALPHA_V1/assets/images/loona.jpg";// TODO: FETCH FROM config???
        const char* notFoundTexturePath = "D:/repos2/ALPHA_V1/assets/images/not_found_texture.png";// TODO: FETCH FROM config???

        auto mainWindowSize = appInstance_->GetMainWindowSize();

        // TODO: THIS IS A REAL MF ISSUE, PLEASE DEFINE THE SCENE BUFFER,AND GAME BUFFER, AND TAKE THE RESOLUTION VALUES FROM THERE INSTEAD OF THE MAIN WINDOW...
        // PLEASE FIX ME REAL SOON
        renderEngine_.SetScreenSize(mainWindowSize.x, mainWindowSize.y);

        // below is the editor view port camera
        std::shared_ptr<Camera> mainCamera = std::make_shared<Camera>(1080,720, 90, 0.01f);
        std::shared_ptr<Sprite> someSprite = std::make_shared<Sprite>(loonaImagePath);
        std::shared_ptr<Sprite> someSprite2 = std::make_shared<Sprite>(notFoundTexturePath);
          
        // mainCamera->underylingTransform.setGlobalPosition(glm::vec3(0, 0, -2));
        someSprite->underylingTransform.setGlobalPosition(glm::vec3(0, 0, -2));
        someSprite2->underylingTransform.setGlobalPosition(glm::vec3(1, 0, -2));

        renderEngine_.SetViewPortFrameBuffer(mainCamera->offScreenFrameBuffer); // Instead of passing the framebuffer should pass the whole camera refference???
        
        ghostCamera_ = std::make_shared<GhostCamera>(&inputSystem_, mainCamera);

        renderEngine_.AddRenderable(mainCamera);
        renderEngine_.AddRenderable(someSprite);
        renderEngine_.AddRenderable(someSprite2);
    }

    void Engine::Init()
    {
        RenderingEngineDemo();
        spdlog::warn("App and Game engine ready...");
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
        inputSystem_.ResetState();// TODO: FIX THIS THRASH WITH INPUT UP AND DOWN EVENTS (INTERNALLY)
    }

    void Engine::ProcessInput(PlayerInputType type, uint64_t *data) // UNSAFE!!!! FIX THIS TRASH
    {
        inputSystem_.ProcessInput(type, data);
    }

} // namespace ettycc
