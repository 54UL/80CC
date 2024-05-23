#include <gtest/gtest.h>
#include <80CC.hpp>
#include <memory>
#include <spdlog/spdlog.h>
#include <sstream>
#include <fstream>
#include <string>

using namespace ettycc;

constexpr const char * UNIT_TEST_SCENE_NAME = "80CC-UNIT-TEST-SCENE";
const std::string scenesPath_ = paths::ASSETS_DEFAULT + "scenes/";// change this trashh

std::shared_ptr<App> app_;
std::shared_ptr<Engine> engineInstance_;
std::shared_ptr<Resources> resources_;
 
class SceneNodeTestFixture : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        app_ = std::make_shared<SDL2App>("80CC-UNIT-TEST");
        engineInstance_ = std::make_shared<Engine>(app_);
        resources_ = std::make_shared<Resources>();

        engineInstance_->mainScene_ = std::make_shared<Scene>(UNIT_TEST_SCENE_NAME);
        engineInstance_->mainScene_->Init();

        resources_->Load(paths::RESOURCES_FILE_NAME);
        
        // Dependency registration
        RegisterDependency(Engine, engineInstance_);
        RegisterDependency(Resources, resources_);

        engineInstance_->SetWorkingResources(resources_);
    }

    void TearDown() override
    {
     
    }
};

TEST_F(SceneNodeTestFixture, basic_scene)
{
    auto mainScene = engineInstance_->mainScene_;

    // Some example parent node
    auto parentNode = std::make_shared<SceneNode>("parent-node");

    // 1st node with one child node (2 in total)
    auto sprite1Node = std::make_shared<SceneNode>("sprite-node-1");
    auto childrenNode = std::make_shared<SceneNode>("child-sprite-1-1");
    sprite1Node->AddChild(childrenNode);

    // const char* loonaImagePath = nullptr; // if null backend stuff wont be initialized and its fine for unit testing, TODO: create resource prefix handler (to retrive paths stored into resources.json)
    std::shared_ptr<Sprite> someSprite = std::make_shared<Sprite>("some_sprite_path.jpg", false);
    sprite1Node->AddComponent(std::make_shared<RenderableNode>(someSprite));

    // Add the node to the parent node...
    parentNode->AddChild(sprite1Node);

    // 2nd node
    auto sprite2Node = std::make_shared<SceneNode>("sprite-node-2");
    parentNode->AddChild(sprite2Node);

    // 3rd node
    auto cameraNode = std::make_shared<SceneNode>("camera-node");
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
        std::ofstream ofs(scenesPath_ + "/scene_unit_test.json"); 

        cereal::JSONOutputArchive archive(ofs);    
        archive(*engineInstance_->mainScene_);
    }

    {
        std::ifstream ifs(scenesPath_ + "/scene_unit_test.json"); 
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
    // TODO: THERE IS A ISSUE WHEN MANIPULATING SCENES, IF THEY ARE NOT THEY MAIN SCENE (WORKING SCENE), IT WILL NOT HAVE FLAT NODES POPULATED!!!
    engineInstance_->mainScene_.reset();
    engineInstance_->mainScene_ = std::make_shared<Scene>("80CC-DEFAULT-SCENE");

    // todo: this must be changed to an asset loading system like assetSystem->getSprite("loona.jpg")
    std::string loonaImagePath = resources_->Get("sprites","loona");
    std::string notFoundTexturePath = resources_->Get("sprites","not-found");

    // log path info
    spdlog::info("loona path: {}", loonaImagePath);
    spdlog::info("notFoundTexturePath: {}", notFoundTexturePath);

    // Game objects initializations...
    std::shared_ptr<Camera> mainCamera = std::make_shared<Camera>(600, 800, 90, 0.01f);
    mainCamera->initializable_ = false; // IMPORTANT: THIS IS USED FOR UNIT TESTING AND SHOULD BE NEVER USED BY THE END USER...
    
    auto someSprite = std::make_shared<Sprite>(loonaImagePath, false);
    auto someSprite2 = std::make_shared<Sprite>(notFoundTexturePath, false);
    auto someSpriteChildren = std::make_shared<Sprite>(notFoundTexturePath, false);

    // Setting some arbitrary values to the node components
    someSprite->underylingTransform.setGlobalPosition(glm::vec3(0, 0, -2));
    someSprite2->underylingTransform.setGlobalPosition(glm::vec3(1, 0, -2));
    someSpriteChildren->underylingTransform.setGlobalPosition(glm::vec3(0, 1, -2));
    
    // Configure parenting herarchy 
    auto someParent = std::make_shared<SceneNode>("parent");
    auto sprite1Node = std::make_shared<SceneNode>("sprite node 1");
    sprite1Node->AddComponent(std::make_shared<RenderableNode>(someSprite));
    someParent->AddChild(sprite1Node);

    auto childrenNode = std::make_shared<SceneNode>("child sprite");
    childrenNode->AddComponent(std::make_shared<RenderableNode>(someSpriteChildren));
    sprite1Node->AddChild(childrenNode);

    auto sprite2Node = std::make_shared<SceneNode>("sprite node 2");
    sprite2Node->AddComponent(std::make_shared<RenderableNode>(someSprite2));
    someParent->AddChild(sprite2Node);

    auto cameraNode = std::make_shared<SceneNode>("cameraNode");
    cameraNode->AddComponent(std::make_shared<RenderableNode>(mainCamera));
    someParent->AddChild(cameraNode);

    engineInstance_->mainScene_->root_node_->AddChild(someParent);

    // Store the scene into the scenes folder
    { 
        std::ofstream ofs(scenesPath_ + "/default_scene.json"); 

        cereal::JSONOutputArchive archive(ofs);    
        archive(*engineInstance_->mainScene_);
    }
}