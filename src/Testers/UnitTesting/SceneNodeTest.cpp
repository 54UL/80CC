#include <gtest/gtest.h>
#include <80CC.hpp>
#include <memory>
#include <spdlog/spdlog.h>
using namespace ettycc;

std::shared_ptr<App> app_;
std::shared_ptr<Engine> engineInstance_;

class SceneNodeTestFixture : public testing::Test
{
protected:
    void SetUp() override
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
    std::shared_ptr<SceneNode> parentNode = std::make_shared<SceneNode>("some parent node");

    // 1st node with one child node (2 in total)
    std::shared_ptr<SceneNode> sprite1Node = std::make_shared<SceneNode>("sprite node 1");
    std::shared_ptr<SceneNode> childrenNode = std::make_shared<SceneNode>("child sprite");

    // create a new child
    sprite1Node->AddChild(sprite1Node, childrenNode);

    // add the new node with a chidl to the main parent
    sprite1Node->AddChild(parentNode, sprite1Node);

    // 2nd node
    std::shared_ptr<SceneNode> sprite2Node = std::make_shared<SceneNode>("sprite node 2");
    sprite2Node->AddChild(parentNode, sprite2Node);

    // 3rd node
    std::shared_ptr<SceneNode> cameraNode = std::make_shared<SceneNode>("cameraNode");
    cameraNode->AddChild(parentNode, cameraNode);

    // Add them to the scene root node...
    mainScene->root_node_->AddChild(mainScene->root_node_, parentNode);
    
    // expecting 3 children on the parent node...
    EXPECT_TRUE(mainScene->root_node_->children_.at(0)->children_.size() == 3);
    auto flatSize = mainScene->nodes_flat_.size();
    // expecting 4 nodes overall
    EXPECT_TRUE( flatSize == 4);
}