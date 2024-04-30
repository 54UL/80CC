#include <gtest/gtest.h>
#include <80CC.hpp>
#include <memory>
#include <spdlog/spdlog.h>
#include <sstream>
#include <fstream>

using namespace ettycc;

std::shared_ptr<App> app_;
std::shared_ptr<Engine> engineInstance_;

class SceneNodeTestFixture : public testing::Test
{
protected:


    static void SetUpTestCase()
    {
        app_ = std::make_shared<SDL2App>("80CC-UNIT-TEST");
        engineInstance_ = std::make_shared<Engine>(app_);

        engineInstance_->mainScene_ = std::make_shared<Scene>("80CC-UNIT-TEST-SCENE");
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
        
        // expecting 3 children on the parent node...
        EXPECT_TRUE(parentNode->children_.size() == 3);
        auto flatSize = readedScene->nodes_flat_.size();

        // expecting 5 nodes overall (parent node, sprite-node-1, child-sprite-1-1, sprite-node-2, camera-node)
        EXPECT_TRUE(flatSize == 5);
    }
}