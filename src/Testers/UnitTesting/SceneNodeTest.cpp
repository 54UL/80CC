#include <gtest/gtest.h>
#include <80CC.hpp>
#include <memory>
#include <spdlog/spdlog.h>
#include <sstream>
#include <fstream>

using namespace ettycc;

constexpr const char * UNIT_TEST_SCENE_NAME = "80CC-UNIT-TEST-SCENE";

std::shared_ptr<App> app_;
std::shared_ptr<Engine> engineInstance_;
std::shared_ptr<Resources> resources_;

// TODO: IMPROVE THE WAY TO REGISTER THIS SHIT
CEREAL_REGISTER_TYPE(ettycc::RenderableNode);
CEREAL_REGISTER_TYPE(ettycc::Sprite);
CEREAL_REGISTER_TYPE(ettycc::Renderable);

CEREAL_REGISTER_POLYMORPHIC_RELATION(ettycc::NodeComponent, ettycc::RenderableNode)
CEREAL_REGISTER_POLYMORPHIC_RELATION(ettycc::Renderable, ettycc::Sprite)

class SceneNodeTestFixture : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        app_ = std::make_shared<SDL2App>("80CC-UNIT-TEST");
        engineInstance_ = std::make_shared<Engine>(app_);
        resources_ = std::make_shared<Resources>();
        resources_->Load("resources.json");

        engineInstance_->mainScene_ = std::make_shared<Scene>(UNIT_TEST_SCENE_NAME);
        engineInstance_->mainScene_->Init();

        // Dependency registration
        RegisterDependency(Engine, engineInstance_);
    }

    void TearDown() override
    {
     
    }
};

TEST_F(SceneNodeTestFixture, basic_scene)
{
    auto mainScene = engineInstance_->mainScene_;

    // Some example parent node
    std::shared_ptr<SceneNode> parentNode = std::make_shared<SceneNode>("parent-node");

    // 1st node with one child node (2 in total)
    std::shared_ptr<SceneNode> sprite1Node = std::make_shared<SceneNode>("sprite-node-1");
    std::shared_ptr<SceneNode> childrenNode = std::make_shared<SceneNode>("child-sprite-1-1");
    sprite1Node->AddChild(childrenNode);

    const char* loonaImagePath = nullptr; // if null backend stuff wont be initialized and its fine for unit testing, TODO: create resource prefix handler (to retrive paths stored into resources.json)
    std::shared_ptr<Sprite> someSprite = std::make_shared<Sprite>(loonaImagePath);
    sprite1Node->AddComponent(std::make_shared<RenderableNode>(someSprite));

    // Add the node to the parent node...
    parentNode->AddChild(sprite1Node);

    // 2nd node
    std::shared_ptr<SceneNode> sprite2Node = std::make_shared<SceneNode>("sprite-node-2");
    parentNode->AddChild(sprite2Node);

    // 3rd node
    std::shared_ptr<SceneNode> cameraNode = std::make_shared<SceneNode>("camera-node");
    parentNode->AddChild(cameraNode);

    // Add them to the scene root node...
    mainScene->root_node_->AddChild(parentNode);

    // expecting 3 children on the parent node...
    EXPECT_TRUE(parentNode->children_.size() == 3);
    auto flatSize = mainScene->nodes_flat_.size();

    // expecting 5 nodes overall (parent node, sprite-node-1, child-sprite-1-1, sprite-node-2, camera-node)
    EXPECT_TRUE(flatSize == 5);
}


TEST_F(SceneNodeTestFixture, basic_scene_serialization)
{
    { // IMPORTANT USE RAII OR THIS WON'T WORK !!
        std::ofstream ofs("scene_unit_test.json"); 

        cereal::JSONOutputArchive archive(ofs);    
        archive(*engineInstance_->mainScene_);
    }

    {
        std::ifstream ifs("scene_unit_test.json"); 
        std::shared_ptr<Scene> readedScene = std::make_shared<Scene>("NULL");

        cereal::JSONInputArchive archive2(ifs);
        archive2(*readedScene);

        auto parentNode = readedScene->root_node_->children_.at(0); 
        
        // Check the scene name
        EXPECT_TRUE(readedScene->sceneName_.compare(UNIT_TEST_SCENE_NAME) == 0);

        // expecting 3 children on the parent node...
        EXPECT_TRUE(parentNode->children_.size() == 3);
        auto flatSize = readedScene->nodes_flat_.size();

        // expecting 5 nodes overall (parent node, sprite-node-1, child-sprite-1-1, sprite-node-2, camera-node)
        EXPECT_TRUE(flatSize == 5);
    }
}

// Scene by made hand test (this would generate the default scene...)
TEST_F(SceneNodeTestFixture, test_bed_scene_serialization)
{
    auto mainScene = engineInstance_->mainScene_;

    // some resources path lol xd
    // const char* loonaImagePath = ;// TODO: FETCH FROM config???
    // const char* notFoundTexturePath = "D:/repos2/ALPHA_V1/assets/images/not_found_texture.png";// TODO: FETCH FROM config???
    const char* loonaImagePath = resources_->Get("sprites","loona").c_str();
    const char* notFoundTexturePath = resources_->Get("sprites","not-found").c_str();
    
    // basic initialization???
    std::shared_ptr<Camera> mainCamera = std::make_shared<Camera>(600, 800, 90, 0.01f);// editor view port camera
    std::shared_ptr<Sprite> someSprite = std::make_shared<Sprite>(loonaImagePath);
    std::shared_ptr<Sprite> someSprite2 = std::make_shared<Sprite>(notFoundTexturePath);
    std::shared_ptr<Sprite> someSpriteChildren = std::make_shared<Sprite>(notFoundTexturePath);

    // mainCamera->underylingTransform.setGlobalPosition(glm::vec3(0, 0, -2));
    someSprite->underylingTransform.setGlobalPosition(glm::vec3(0, 0, -2));
    someSprite2->underylingTransform.setGlobalPosition(glm::vec3(1, 0, -2));
    someSpriteChildren->underylingTransform.setGlobalPosition(glm::vec3(0, 1, -2));

    // renderEngine_.SetViewPortFrameBuffer(mainCamera->offScreenFrameBuffer); // Instead of passing the framebuffer should pass the whole camera refference???

    std::shared_ptr<SceneNode> someParent = std::make_shared<SceneNode>("parent");

    std::shared_ptr<SceneNode> sprite1Node = std::make_shared<SceneNode>("sprite node 1");
    sprite1Node->AddComponent(std::make_shared<RenderableNode>(someSprite));
    someParent->AddChild(sprite1Node);

    std::shared_ptr<SceneNode> childrenNode = std::make_shared<SceneNode>("child sprite");
    childrenNode->AddComponent(std::make_shared<RenderableNode>(someSpriteChildren));
    sprite1Node->AddNode(childrenNode);
    someParent->AddChild(sprite1Node);

    std::shared_ptr<SceneNode> sprite2Node = std::make_shared<SceneNode>("sprite node 2");
    sprite2Node->AddComponent(std::make_shared<RenderableNode>(someSprite2));
    someParent->AddChild(sprite2Node);

    std::shared_ptr<SceneNode> cameraNode = std::make_shared<SceneNode>("cameraNode");
    cameraNode->AddComponent(std::make_shared<RenderableNode>(mainCamera));
    someParent->AddChild(cameraNode);

    mainScene->root_node_->AddChild(someParent);
    { 
        // IMPORTANT USE RAII OR THIS WON'T WORK !!
        std::ofstream ofs("scene_unit_test.json"); 

        cereal::JSONOutputArchive archive(ofs);    
        archive(*mainScene);
    }
}