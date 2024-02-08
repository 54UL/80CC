#include <Scene/Components/RenderableNode.hpp>


namespace ettycc
{
    RenderableNode::RenderableNode()
    {
        std::shared_ptr<Sprite> someSprite = std::make_shared<Sprite>(loonaImagePath);
    }

    RenderableNode::~RenderableNode()
    {
    }

    void RenderableNode::OnStart()
    {
    }

    void RenderableNode::OnUpdate()
    {
    }
}
