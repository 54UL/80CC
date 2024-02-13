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
    
    void Engine::TestScene()
    {
        // Scene initialization
        mainScene_ = std::make_shared<Scene>("Test scene");
        mainScene_->Init();
        
        // some resources path lol xd
        const char* loonaImagePath = "D:/repos2/ALPHA_V1/assets/images/loona.jpg";// TODO: FETCH FROM config???
        const char* notFoundTexturePath = "D:/repos2/ALPHA_V1/assets/images/not_found_texture.png";// TODO: FETCH FROM config???

        // basic initialization???
        auto mainWindowSize = appInstance_->GetMainWindowSize();
        renderEngine_.SetScreenSize(mainWindowSize.x, mainWindowSize.y);

        std::shared_ptr<Camera> mainCamera = std::make_shared<Camera>(mainWindowSize.x,  mainWindowSize.y, 90, 0.01f);// editor view port camera
        std::shared_ptr<Sprite> someSprite = std::make_shared<Sprite>(loonaImagePath);
        std::shared_ptr<Sprite> someSprite2 = std::make_shared<Sprite>(notFoundTexturePath);
        std::shared_ptr<Sprite> someSpriteChildren = std::make_shared<Sprite>(notFoundTexturePath);

        // mainCamera->underylingTransform.setGlobalPosition(glm::vec3(0, 0, -2));
        someSprite->underylingTransform.setGlobalPosition(glm::vec3(0, 0, -2));
        someSprite2->underylingTransform.setGlobalPosition(glm::vec3(1, 0, -2));
        someSpriteChildren->underylingTransform.setGlobalPosition(glm::vec3(0, 1, -2));

        renderEngine_.SetViewPortFrameBuffer(mainCamera->offScreenFrameBuffer); // Instead of passing the framebuffer should pass the whole camera refference???
        ghostCamera_ = std::make_shared<GhostCamera>(&inputSystem_, mainCamera); // todo: refactor this into a game module...
    
        std::shared_ptr<SceneNode> rootNode = std::make_shared<SceneNode>("Parent");

        std::shared_ptr<SceneNode> sprite1Node = std::make_shared<SceneNode>(rootNode,"sprite node 1");
        sprite1Node->AddComponent(std::make_shared<RenderableNode>(someSprite));
        
        std::shared_ptr<SceneNode> childrenNode = std::make_shared<SceneNode>(sprite1Node,"children node fr");
        sprite1Node->AddComponent(std::make_shared<RenderableNode>(someSpriteChildren));

        sprite1Node->children_.emplace_back(childrenNode);

        std::shared_ptr<SceneNode> sprite2Node = std::make_shared<SceneNode>(rootNode, "sprite node 2");
        sprite2Node->AddComponent(std::make_shared<RenderableNode>(someSprite2));

        std::shared_ptr<SceneNode> cameraNode = std::make_shared<SceneNode>(rootNode,"cameraNode");
        cameraNode->AddComponent(std::make_shared<RenderableNode>(mainCamera));

        std::vector<std::shared_ptr<SceneNode>> nodes = {rootNode, sprite1Node, sprite2Node, cameraNode};

        mainScene_->root_node_->AddNodes(nodes);
    }

    void Engine::Init()
    {
        // RenderingEngineDemo();
        spdlog::warn("App and Game engine ready...");

        TestScene();
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
